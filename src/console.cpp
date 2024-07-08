#include "main.h"
#include "game_hooks.h"

#include <vector>
#include <cctype>

#define NUM_HOTKEY_COMMANDS 16

enum class InputMode : uint32_t {
        Command,
        SearchOutput,
        SearchHistory,
};

static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data);
static int CALLBACK_inputtext_switch_mode(ImGuiInputTextCallbackData* data);
static bool strcasestr(const char* haystack, const char* needle);

static bool UpdateScroll = false;
static bool UpdateFocus = true;
static int HistoryIndex = -1;
static InputMode CommandMode = InputMode::Command;

static const BetterAPI* API = nullptr;
static const hook_api_t* HookAPI = nullptr;
static const log_buffer_api_t* LogBuffer = nullptr;
static const simple_draw_t* SimpleDraw = nullptr;
static const config_api_t* Config = nullptr;
static const auto GameHook = GameHook_GetData();

static char IOBuffer[256 * 1024];
static std::vector<uint32_t> SearchOutputLines{};
static std::vector<uint32_t> SearchHistoryLines{};

static uint32_t NumHotkeys = 0;
static RegistrationHandle ModHandle = 0;

struct Command {
        char name[32];
        char data[512];
} HotkeyCommands[NUM_HOTKEY_COMMANDS];


static void draw_console_window(void*) {
        if (!(GameHook->ConsoleRun && GameHook->ConsoleOutputHooked)) {
                if (!GameHook->ConsoleOutputHooked) {
                        SimpleDraw->Text("Cannot hook console print function.");
                }

                if (!GameHook->ConsoleRun) {
                        SimpleDraw->Text("Cannot hook console run function.");
                }

                SimpleDraw->Text("Incompatible mod loaded or incompatible game version");
                SimpleDraw->Text("BetterConsole '%s' is compatible with game version '%s'", BETTERCONSOLE_VERSION, COMPATIBLE_GAME_VERSION);
                return;
        }


        static bool ConsoleReadyIgnored = false;
        if (!GameHook->ConsoleReadyFlag && !ConsoleReadyIgnored) {
                SimpleDraw->Text("Cannot detect if the console is ready. Proceed anyway?");
                if (SimpleDraw->Button("Ignore and continue")) {
                        ConsoleReadyIgnored = true;
                }
                return;
        }


        static bool GamePausedIgnored = false;
        if (!GameHook->GetGamePausedFlag() && !GamePausedIgnored) {
                SimpleDraw->Text("Cannot detect if the game is paused. Proceed anyway?");
                if (SimpleDraw->Button("Ignore and continue")) {
                        GamePausedIgnored = true;
                }
                return;
        }


        if(GameHook->ConsoleReadyFlag && !*GameHook->ConsoleReadyFlag) {
                SimpleDraw->Text("Waiting for console to become ready...");
                return;
        }


        if (GamePausedIgnored) {
                if (SimpleDraw->Button("Toggle Game Pause")) {
                        GameHook->ConsoleRun("ToggleGamePause");
                }
        }
        else {
                bool* flag = GameHook->GetGamePausedFlag();
                const char* const GamePauseText = *flag ? "Resume Game" : "Pause Game";
                if (SimpleDraw->Button(GamePauseText)) {
                        *flag = !*flag;
                }
        }
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(-(ImGui::GetFontSize() * 12.0f));

        static uint32_t line_count = 0;
        const auto cur_lines = LogBuffer->GetLineCount(GameHook->ConsoleOutput);
        if (line_count != cur_lines) {
                line_count = cur_lines;
                UpdateScroll = true;
        }

        if (UpdateFocus) {
                ImGui::SetKeyboardFocusHere();
                UpdateFocus = false;
        }

        if (CommandMode == InputMode::Command) {
                if (ImGui::InputText("Command Mode  ", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_cmdline)) {
                        HistoryIndex = LogBuffer->GetLineCount(GameHook->ConsoleInput);
                        GameHook->ConsoleRun(IOBuffer);
                        IOBuffer[0] = 0;
                        UpdateFocus = true;
                }
                SimpleDraw->SameLine();
                if (SimpleDraw->Button("Clear")) {
                        LogBuffer->Clear(GameHook->ConsoleOutput);
                }
                SimpleDraw->ShowLogBuffer(GameHook->ConsoleOutput, UpdateScroll);
                
        }
        else if (CommandMode == InputMode::SearchOutput) {
                if (ImGui::InputText("Search Output ", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_switch_mode)) {
                        SearchOutputLines.clear();
                        for (uint32_t i = 0; i < LogBuffer->GetLineCount(GameHook->ConsoleOutput); ++i) {
                                if (strcasestr(LogBuffer->GetLine(GameHook->ConsoleOutput, i), IOBuffer)) {
                                        SearchOutputLines.push_back(i);
                                }
                        }
                }
                SimpleDraw->SameLine();
                if (SimpleDraw->Button("Clear")) {
                        LogBuffer->Clear(GameHook->ConsoleOutput);
                }
                SimpleDraw->ShowFilteredLogBuffer(GameHook->ConsoleOutput, SearchOutputLines.data(), (uint32_t)SearchOutputLines.size(), UpdateScroll);
        }
        else if (CommandMode == InputMode::SearchHistory) {
                if (ImGui::InputText("Search History", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_switch_mode)) {
                        SearchHistoryLines.clear();
                        for (uint32_t i = 0; i < LogBuffer->GetLineCount(GameHook->ConsoleInput); ++i) {
                                if (strcasestr(LogBuffer->GetLine(GameHook->ConsoleInput, i), IOBuffer)) {
                                        SearchHistoryLines.push_back(i);
                                }
                        }
                }
                SimpleDraw->SameLine();
                if (SimpleDraw->Button("Clear")) {
                        LogBuffer->Clear(GameHook->ConsoleInput);
                }
                SimpleDraw->ShowFilteredLogBuffer(GameHook->ConsoleInput, SearchHistoryLines.data(), (uint32_t)SearchHistoryLines.size(), UpdateScroll);
        }
        else {
                ASSERT(false && "Invalid command mode");
        }

        if (UpdateScroll) {
                ImGui::SetScrollHereY(1.0f);
                UpdateScroll = false;
        }
}


static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                const auto HistoryMax = (int)LogBuffer->GetLineCount(GameHook->ConsoleInput);

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
                        data->InsertChars(0, LogBuffer->GetLine(GameHook->ConsoleInput, HistoryIndex));
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
                GameHook->ConsoleRun(hotkey);
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
        
        IOBuffer[0] = 0;
}