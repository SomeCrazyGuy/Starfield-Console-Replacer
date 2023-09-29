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
constexpr uint64_t OFFSET_console_vprint = 0x2886AD8; //stolen from sfse, void ConsolePrintV(ConsoleMgr*, const char* fmt, va_list args)



//48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d
/*
check for this in ghidra:

_strnicmp((char *)local_838,"ForEachRef[",0xb)
...
memset(local_c38,0,0x400);
pcVar15 = "float fresult\nref refr\nset refr to GetSelectedRef\nset fresult to ";

*/
constexpr uint64_t OFFSET_console_run = 0x2881064; //void ConsoleRun(NULL, char* cmd)




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
static void display_search_lines(LogBufferHandle handle, const std::vector<uint32_t>& lines);
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
static FILE* HistoryFile = nullptr;
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
        HistoryHandle = LogBuffer->Create("Command History", NULL);

        OLD_ConsolePrintV = (decltype(OLD_ConsolePrintV))HookAPI->HookFunction(
                (FUNC_PTR)HookAPI->Relocate(OFFSET_console_vprint),
                (FUNC_PTR)console_print
        );

        OLD_ConsoleRun = (decltype(OLD_ConsoleRun))HookAPI->HookFunction(
                (FUNC_PTR)HookAPI->Relocate(OFFSET_console_run),
                (FUNC_PTR)console_run
        );

        fopen_s(&HistoryFile, HISTORY_FILE_PATH, "ab+");
        ASSERT(HistoryFile != NULL);

        //load history file from last session
        fseek(HistoryFile, 0, SEEK_SET);
        while (fgets(IOBuffer, sizeof(IOBuffer), HistoryFile)) {
                for (uint32_t i = 0; i < sizeof(IOBuffer); ++i) {
                        if (!IOBuffer[i]) break;
                        if (IOBuffer[i] == '\n') {
                                IOBuffer[i] = '\0';
                                LogBuffer->Append(HistoryHandle, IOBuffer);
                                break;
                        }
                }
        }
        HistoryIndex = LogBuffer->GetLineCount(HistoryHandle);

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
        fputs(cmd, HistoryFile);
        fputc('\n', HistoryFile);
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
                        fflush(HistoryFile);
                        UpdateFocus = true;
                }
                //display_lines(OutputHandle);
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
                display_search_lines(OutputHandle, SearchOutputLines);
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
                display_search_lines(HistoryHandle, SearchHistoryLines);
        }

        UpdateScroll = false;
}


static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                const auto HistoryMax = LogBuffer->GetLineCount(HistoryHandle);

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



static void display_search_lines(LogBufferHandle handle, const std::vector<uint32_t>& lines) {
        char temp_line[4096];
        ImGui::BeginChild("scrolling region");
        ImGuiListClipper clip{};
        clip.Begin((int)lines.size());
        while (clip.Step()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 2.f });
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });
                for (auto i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        ImGui::PushID(i);
                        ImGui::SetNextItemWidth(-1.f);
                        const char* cur_line = LogBuffer->GetLine(handle, lines[i]);
                        strncpy_s(temp_line, sizeof(temp_line), cur_line, strlen(cur_line));
                        ImGui::InputText("#buffer", temp_line, sizeof(temp_line), ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopID();
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleVar();
        }
        ImGui::TextUnformatted(" "); //more padding to fix scrolling at bottom
        ImGui::EndChild();
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
                s--;
        }
        return true;
}




// this should be the only interface between the console replacer and the mod menu code
extern void setup_console(const BetterAPI* api) {
        static DrawCallbacks callbacks;
 
        callbacks.Name = "BetterConsole";
        callbacks.DrawTab = &draw_console_window;
        api->Callback->RegisterDrawCallbacks(&callbacks);
        API = api;
}