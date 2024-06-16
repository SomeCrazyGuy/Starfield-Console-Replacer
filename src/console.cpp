#include "main.h"

#include <varargs.h>
#include <vector>
#include <ctype.h>

// Console print interface:
//48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ?? 57 b8 30 10 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 49 - new sig with 1 match
//48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ?? 57 b8 30 10 -- the 0x1030 stack size might be too unique
/*
  undefined4 uVar1;
  int iVar2;
  char local_1010 [4096];
  undefined8 local_10;

  local_10 = 0x142883992;
  uVar1 = FUN_140839080();
  FUN_140587784(0x40);

  //calling vsnprintf() with 4096 buffer and using the return value to append "\n\0"
  iVar2 = FUN_1412518e8(local_1010,0x1000,param_2,param_4,param_3);
  if (0 < iVar2) {
    local_1010[iVar2] = '\n';           //note the behavior of adding "\n\0"
    local_1010[iVar2 + 1] = '\0';
    FUN_14288379c(param_1,local_1010,param_2);
  }
  FUN_140587784(uVar1);
  return;
*/

// Console run command interface:
//48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d
/*
check for this in ghidra:

_strnicmp((char *)local_838,"ForEachRef[",0xb)
...
memset(local_c38,0,0x400);
pcVar15 = "float fresult\nref refr\nset refr to GetSelectedRef\nset fresult to ";

*/

// Forced offsets 1.12.32
#define OFFSET_CONSOLE_PRINT 0x29cd9b8
#define OFFSET_CONSOLE_RUN 0x29c7f44

#define OUTPUT_FILE_PATH "BetterConsoleOutput.txt"
#define HISTORY_FILE_PATH "BetterConsoleHistory.txt"
#define NUM_HOTKEY_COMMANDS 16


enum class InputMode : uint32_t {
        Command,
        SearchOutput,
        SearchHistory,
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

static const BetterAPI* API = nullptr;
static const hook_api_t* HookAPI = nullptr;
static const log_buffer_api_t* LogBuffer = nullptr;
static const simple_draw_t* SimpleDraw = nullptr;
static const config_api_t* Config = nullptr;

static char IOBuffer[256 * 1024];
static LogBufferHandle OutputHandle;
static LogBufferHandle HistoryHandle;
static std::vector<uint32_t> SearchOutputLines{};
static std::vector<uint32_t> SearchHistoryLines{};

static uint32_t NumHotkeys = 0;
static RegistrationHandle ModHandle = 0;

struct Command {
        char name[32];
        char data[512];
} HotkeyCommands[NUM_HOTKEY_COMMANDS];


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
        forward_to_old_consoleprint(consolemgr, "%s", IOBuffer); //send it already converted
        LogBuffer->Append(OutputHandle, IOBuffer);
        IOBuffer[0] = 0;
        UpdateScroll = true;
}


static void console_run(void* consolemgr, char* cmd) {
        LogBuffer->Append(HistoryHandle, cmd);
        if (OLD_ConsoleRun) {
                OLD_ConsoleRun(consolemgr, cmd);
        }
        else {
                DEBUG("console hook not ready when running command: %s", cmd);
        }
}

static void draw_console_window(void* imgui_context) {
        (void)imgui_context;

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
        else {
                ASSERT(false && "Invalid command mode");
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



static void CALLBACK_console_settings(enum ConfigAction action) {
        char keyname[32];
        for (uint32_t i = 0; i < 16; ++i) {
                auto& hc = HotkeyCommands[i];
                snprintf(keyname, sizeof(keyname), "Hotkey Command%u", i);
                Config->ConfigString(action, keyname, hc.data, sizeof(hc.data));
        }
}

static void CALLBACK_console_hotkey(uintptr_t userdata) {
        auto index = (uint32_t)userdata;
        char* hotkey = HotkeyCommands[index].data;
        if (hotkey[0]) {
                API->Console->RunCommand(hotkey);
        }
}


// this should be the only interface between the console replacer and the mod menu code
extern void setup_console(const BetterAPI* api) {
        API = api;

        const auto CB = API->Callback;
        ModHandle = CB->RegisterMod("BetterConsole");
        CB->RegisterDrawCallback(ModHandle, draw_console_window);
        CB->RegisterConfigCallback(ModHandle, CALLBACK_console_settings);
        CB->RegisterHotkeyCallback(ModHandle, CALLBACK_console_hotkey);

        for (uint32_t i = 0; i < NUM_HOTKEY_COMMANDS; ++i) {
                char key_name[32];
                snprintf(key_name, sizeof(key_name), "HotkeyCommand %u", i);
                CB->RequestHotkey(ModHandle, key_name, i);
        }

        LogBuffer = api->LogBuffer;
        HookAPI = API->Hook;
        SimpleDraw = API->SimpleDraw;
        Config = API->Config;

        OutputHandle = LogBuffer->Create("Console Output", OUTPUT_FILE_PATH);
        HistoryHandle = LogBuffer->Restore("Command History", HISTORY_FILE_PATH);

        DEBUG("Hooking print function using AOB method");
        auto hook_print_aob = HookAPI->AOBScanEXE("48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ?? 57 b8 30 10 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 49");
        
        if (!hook_print_aob) {
                DEBUG("hook_print_aob AOB method failed, (incompatible mod or game update?)");
                hook_print_aob = HookAPI->Relocate(OFFSET_CONSOLE_PRINT);
        }

        OLD_ConsolePrintV = (decltype(OLD_ConsolePrintV))HookAPI->HookFunction(
                (FUNC_PTR)hook_print_aob,
                (FUNC_PTR)console_print
        );

        DEBUG("Hooking run function using AOB method");
        auto hook_run_aob = HookAPI->AOBScanEXE("48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d");
        
        if (!hook_run_aob) {
                DEBUG("hook_run_aob AOB method failed, (incompatible mod or game update?)");
                hook_run_aob = HookAPI->Relocate(OFFSET_CONSOLE_RUN);
        }

        OLD_ConsoleRun = (decltype(OLD_ConsoleRun))HookAPI->HookFunction(
                (FUNC_PTR)hook_run_aob,
                (FUNC_PTR)console_run
        );

        IOBuffer[0] = 0;
}


static void run_comand(char* command) {
        console_run(NULL, command);
}


static constexpr struct console_api_t Console {
        run_comand
};

extern constexpr const struct console_api_t* GetConsoleAPI() {
        return &Console;
}