#include "console.h"

#include "../imgui/imgui.h"

#include <varargs.h>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN
#include <Windows.h>

//copying these from main.h to test console code running completely isolated from the rest of the modmenu
static inline constexpr const char* GetRelativeProjectDir(const char* file_path) noexcept {
        if (!file_path) return nullptr;
        const char* x = file_path;
        while (*x) ++x;
        while ((x != file_path) && (*x != '\\')) --x;
        --x;
        while ((x != file_path) && (*x != '\\')) --x;
        ++x;
        return x;
}

#define HERE_MSG(BUFFER, BUFFER_SIZE) do{snprintf((BUFFER), (BUFFER_SIZE), "[ERROR] %s:%s:%d:", GetRelativeProjectDir(__FILE__), __func__, __LINE__);}while(0)
#define FATAL_ERROR(FORMAT) do{char msg[1024]; HERE_MSG(msg, 256); snprintf(msg+strlen(msg), 768, " " FORMAT); MessageBoxA(NULL, msg, "Fatal Error", 0); abort(); }while(0)
#define ASSERT(CONDITION) do{if(!(CONDITION)){FATAL_ERROR(#CONDITION);}}while(0)
#define NOT_NULL(POINTER) ASSERT((POINTER) != NULL)


//48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ?? 57 b8 30 10 -- the 0x1030 stack size might be too unique
/*
  undefined4 uVar1;
  int iVar2;
  char local_1010 [4096];
  undefined8 local_10;

  local_10 = 0x142883992;
  uVar1 = FUN_140839080();
  FUN_140587784(0x40);

  //calling vsnprintf() with 4096 buffer and using the retrn value to append "\n\0"
  iVar2 = FUN_1412518e8(local_1010,0x1000,param_2,param_4,param_3);
  if (0 < iVar2) {
    local_1010[iVar2] = '\n';           //note the behavior of adding "\n\0"
    local_1010[iVar2 + 1] = '\0';
    FUN_14288379c(param_1,local_1010,param_2);
  }
  FUN_140587784(uVar1);
  return;
*/
constexpr uint64_t OFFSET_console_vprint = 0x02886F18; //stolen from sfse, void ConsolePrintV(ConsoleMgr*, const char* fmt, va_list args)



//48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d
/*
check for this in ghidra:

_strnicmp((char *)local_838,"ForEachRef[",0xb)
...
memset(local_c38,0,0x400);
pcVar15 = "float fresult\nref refr\nset refr to GetSelectedRef\nset fresult to ";

*/
constexpr uint64_t OFFSET_console_run = 0x28814a4; //void ConsoleRun(NULL, char* cmd)




#define OUTPUT_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleOutput.txt"
#define HISTORY_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleHistory.txt"


enum class InputMode : uint32_t {
        Command,
        SearchOutput,
        SearchHistory
};

static void console_run(void* consolemgr, char* cmd);
static void console_print(void* consolemgr, const char* fmt, va_list args);

static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data);
static int CALLBACK_inputtext_switch_mode(ImGuiInputTextCallbackData* data);
static bool strcasestr(const char* haystack, const char* needle);

static void(*OLD_ConsolePrintV)(void*, const char*, va_list) = nullptr;    
static void(*OLD_ConsoleRun)(void*, char*) = nullptr;

static bool UpdateScroll = false;
static bool UpdateFocus = true;
static int HistoryIndex = -1;
static InputMode CommandMode = InputMode::Command;

static bool ConsoleInit = false;
static const BetterAPI* API = nullptr;
static const hook_api_t* HookAPI = nullptr;
static const log_buffer_api_t* LogBuffer = nullptr;
static const simple_draw_t* SimpleDraw = nullptr;
static char IOBuffer[256 * 1024];
static LogBufferHandle OutputHandle;
static LogBufferHandle HistoryHandle;
static std::vector<uint32_t> SearchOutputLines{};
static std::vector<uint32_t> SearchHistoryLines{};


static void init_console() {
        if (ConsoleInit) return;
        ConsoleInit = true;

        ASSERT(API != nullptr);

        HookAPI = API->Hook;
        LogBuffer = API->LogBuffer;
        SimpleDraw = API->SimpleDraw;

        OutputHandle = LogBuffer->Create("Console Output", OUTPUT_FILE_PATH);
        HistoryHandle = LogBuffer->Restore("Command History", HISTORY_FILE_PATH);

        OLD_ConsolePrintV = (decltype(OLD_ConsolePrintV))HookAPI->HookFunction(
                (FUNC_PTR)HookAPI->Relocate(OFFSET_console_vprint),
                (FUNC_PTR)console_print
        );

        OLD_ConsoleRun = (decltype(OLD_ConsoleRun))HookAPI->HookFunction(
                (FUNC_PTR)HookAPI->Relocate(OFFSET_console_run),
                (FUNC_PTR)console_run
        );

        IOBuffer[0] = 0;
}


static void forward_to_old_consoleprint(void* consolemgr, const char* fmt, ...) {
        if (!consolemgr) return;
        va_list args;
        va_start(args, fmt);
        OLD_ConsolePrintV(consolemgr, fmt, args);
        va_end(args);
}


static void console_print(void* consolemgr, const char* fmt, va_list args) {
        auto size = vsnprintf(IOBuffer, sizeof(IOBuffer), fmt, args);
        if (size <= 0) return;
        if (!ConsoleInit) init_console();
        forward_to_old_consoleprint(consolemgr, "%s", IOBuffer); //send it already converted
        LogBuffer->Append(OutputHandle, IOBuffer);
        IOBuffer[0] = 0;
        UpdateScroll = true;
}


static void console_run(void* consolemgr, char* cmd) {
        LogBuffer->Append(HistoryHandle, cmd);
        OLD_ConsoleRun(consolemgr, cmd);
}

static void draw_console_window(void* imgui_context) {
        (void)imgui_context;

        if (!ConsoleInit) init_console();

        static bool GameState = true;
        static const char* GameStates[] = { "Paused", "Running" };
        if (ImGui::Button(GameStates[GameState])) {
                char tgp[] = "ToggleGamePause";
                console_run(NULL, tgp);
                GameState = !GameState;
        }
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(-(ImGui::GetFontSize() * 20));
        if (UpdateFocus) {
                ImGui::SetKeyboardFocusHere();
                UpdateFocus = false;
        }

        if (CommandMode == InputMode::Command) {
                if (ImGui::InputText("Command Mode", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_cmdline)) {
                        HistoryIndex = LogBuffer->GetLineCount(HistoryHandle);
                        console_run(NULL, IOBuffer);
                        IOBuffer[0] = 0;
                        UpdateFocus = true;
                }
                SimpleDraw->ShowLogBuffer(OutputHandle, UpdateScroll);
        }
        else if (CommandMode == InputMode::SearchOutput) {
                if (ImGui::InputText("Search Output", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_switch_mode)) {
                        SearchOutputLines.clear();
                        for (uint32_t i = 0; i < LogBuffer->GetLineCount(OutputHandle); ++i) {
                                if (strcasestr(LogBuffer->GetLine(OutputHandle, i), IOBuffer)) {
                                        SearchOutputLines.push_back(i);
                                }
                        }
                }
                SimpleDraw->ShowFilteredLogBuffer(OutputHandle, SearchOutputLines.data(), (uint32_t)SearchOutputLines.size(), UpdateScroll);
        }
        else if (CommandMode == InputMode::SearchHistory) {
                if (ImGui::InputText("Search History", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_switch_mode)) {
                        SearchHistoryLines.clear();
                        for (uint32_t i = 0; i < LogBuffer->GetLineCount(HistoryHandle); ++i) {
                                if (strcasestr(LogBuffer->GetLine(HistoryHandle, i), IOBuffer)) {
                                        SearchHistoryLines.push_back(i);
                                }
                        }
                }
                SimpleDraw->ShowFilteredLogBuffer(HistoryHandle, SearchHistoryLines.data(), (uint32_t)SearchHistoryLines.size(), UpdateScroll);
        }

        UpdateScroll = false;
}


static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                const auto HistoryMax = (int)LogBuffer->GetLineCount(HistoryHandle);

                if (data->EventKey == ImGuiKey_UpArrow) {
                        --HistoryIndex;
                }
                else if (data->EventKey == ImGuiKey_DownArrow) {
                        ++HistoryIndex;
                }

                if (HistoryIndex < 0) {
                        HistoryIndex = 0;
                }

                if (HistoryIndex >= HistoryMax) {
                        HistoryIndex = HistoryMax - 1;
                }

                if (HistoryMax) {
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, LogBuffer->GetLine(HistoryHandle, HistoryIndex));
                }
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                return CALLBACK_inputtext_switch_mode(data);
        }

        return 0;
}

static int CALLBACK_inputtext_switch_mode(ImGuiInputTextCallbackData* data) {
        (void)data;
        SearchOutputLines.clear();
        SearchHistoryLines.clear();
        UpdateFocus = true;

        switch (CommandMode)
        {
        case InputMode::Command:
                CommandMode = InputMode::SearchOutput;
                break;
        case InputMode::SearchOutput:
                CommandMode = InputMode::SearchHistory;
                break;
        case InputMode::SearchHistory:
                //fallthrough
        default:
                CommandMode = InputMode::Command;
                //when switching back to comand mode, scroll output to bottom
                UpdateScroll = true;
                break;
        }
        return 0;
}


static bool strcasestr(const char* s, const char* find) {
        char c, sc;
        size_t len;
        if ((c = *find++) != 0) {
                c = (char)tolower((unsigned char)c);
                len = strlen(find);
                do {
                        do {
                                if ((sc = *s++) == 0)
                                        return false;
                        } while ((char)tolower((unsigned char)sc) != c);
                } while (_strnicmp(s, find, len) != 0);
        }
        return true;
}

static void DrawHotkeyTab(void* imgui);
static boolean RunHotkey(uint32_t vk_keycode, boolean shift, boolean ctrl);

static void DrawTestLog(void* imgui);

// this should be the only interface between the console replacer and the mod menu code
extern void setup_console(const BetterAPI* api) {
        static DrawCallbacks callbacks;
 
        callbacks.Name = "BetterConsole";
        callbacks.DrawTab = &draw_console_window;
        callbacks.DrawSettings = &DrawHotkeyTab;
        callbacks.DrawLog = *DrawTestLog;
        api->Callback->RegisterDrawCallbacks(&callbacks);
        api->Callback->RegisterHotkey("BetterConsole", RunHotkey);
        API = api;
}


#define HOTKEY_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleHotkeys.txt"
struct HotkeyBuffer { char text[512]; };
static HotkeyBuffer Hotkeys[12];
static LogBufferHandle HotkeyHandle = 0;
static bool hotkey_init = false;


static void init_hotkeys() {
        hotkey_init = true;
        HotkeyHandle = LogBuffer->Restore("Hotkeys", HOTKEY_FILE_PATH);
        LogBuffer->CloseFile(HotkeyHandle);
        for (uint32_t i = 0; i < LogBuffer->GetLineCount(HotkeyHandle); ++i) {
                if (i >= 12) break; //to many lines
                const char* line = LogBuffer->GetLine(HotkeyHandle, i);
                strncpy_s(Hotkeys[i].text, sizeof(Hotkeys[i].text), line, strlen(line));
        }
}


static void DrawHotkeyTab(void* imgui) {
        (void)imgui;
        if (!ConsoleInit) init_console();
        if (!hotkey_init) init_hotkeys();

        if (ImGui::Button("Save to " HOTKEY_FILE_PATH)) {
                LogBuffer->Clear(HotkeyHandle);
                for (uint32_t i = 0; i < 12; ++i) {
                        LogBuffer->Append(HotkeyHandle, Hotkeys[i].text);
                }
                LogBuffer->Save(HotkeyHandle, HOTKEY_FILE_PATH);
        }

        char label[32];
        for (uint32_t i = 0; i < 11; ++i) {
                ImGui::PushID(i);
                snprintf(label, sizeof(label), "F%d", i + 13);
                ImGui::InputText(label, Hotkeys[i].text, sizeof(Hotkeys[i].text));
                ImGui::PopID();
        }
}

static boolean RunHotkey(uint32_t vk_keycode, boolean shift, boolean ctrl) {
        (void) shift;
        (void) ctrl;

        if (!ConsoleInit) init_console();
        if (!hotkey_init) init_hotkeys();

        const auto VKStart = VK_F13;
        const auto VKEnd = VK_F23;

        if ((vk_keycode >= VKStart) && (vk_keycode <= VKEnd)) {
                uint32_t hotkey_index = vk_keycode - VKStart;
                console_run(NULL, Hotkeys[hotkey_index].text);
                return true;
        }

        return false;
}



static void DrawTestLog(void* imgui) {
        (void)imgui;
        if (!ConsoleInit) init_console();

        SimpleDraw->VboxTop(1 / 3.0, 0);
        SimpleDraw->HboxLeft(1 / 3.0, 0);
        SimpleDraw->Text("Top Left");
        SimpleDraw->HBoxRight();
        SimpleDraw->HboxLeft(1 / 2.0, 0);
        SimpleDraw->Text("Top Center");
        SimpleDraw->HBoxRight();
        SimpleDraw->Text("Top Right");
        SimpleDraw->HBoxEnd();
        SimpleDraw->HBoxEnd();

        SimpleDraw->VBoxBottom();
        SimpleDraw->VboxTop(1.0 / 2, 0);
        SimpleDraw->HboxLeft(1.0 / 3, 0);
        SimpleDraw->Text("Middle Left");
        SimpleDraw->HBoxRight();
        SimpleDraw->HboxLeft(1.0 / 2, 0);
        SimpleDraw->Text("Layout Test Code");
        SimpleDraw->HBoxRight();
        SimpleDraw->Text("Middle Right");
        SimpleDraw->HBoxEnd();
        SimpleDraw->HBoxEnd();

        SimpleDraw->VBoxBottom();
        SimpleDraw->HboxLeft(1.0 / 3, 0);
        SimpleDraw->Text("Bottom Left");
        SimpleDraw->HBoxRight();
        SimpleDraw->HboxLeft(1.0 / 2, 0);
        SimpleDraw->Text("Bottom Center");
        SimpleDraw->HBoxRight();
        SimpleDraw->Text("Bottom Right");
        SimpleDraw->HBoxEnd();
        SimpleDraw->HBoxEnd();

        SimpleDraw->VBoxEnd();
        SimpleDraw->VBoxEnd();
}