#include "main.h"
#include "console.h"

#include <varargs.h>

#include <vector>
#include <string>

#include "hook_api.h"


#define OUTPUT_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleOutput.txt"
#define HISTORY_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleHistory.txt"


struct Line {
        uint32_t start;
        uint32_t len;
};

enum class InputMode : uint32_t {
        Command,
        SearchOutput,
        SearchHistory
};

static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data);
static int CALLBACK_inputtext_switch_mode(ImGuiInputTextCallbackData* data);
static void display_lines(const std::vector<Line>& lines, char* buffer);
static const char* stristr(const char* haystack, const char* needle);

static void(*OLD_ConsolePrintV)(void*, const char*, va_list) = nullptr;
static const hook_api_t* HookAPI = nullptr;
static constexpr auto NumberOfWordsInWarAndPeace = 587287;
static constexpr auto AverageWordLengthCharacters = 5;
static char OutputBuffer[NumberOfWordsInWarAndPeace * AverageWordLengthCharacters];             
static char InputBuffer[512 * 1024];
static uint32_t OutputPos = 0;
static uint32_t InputPos = 0;
static int UpdateScroll = 0;
static bool UpdateFocus = true;
static std::vector<Line> OutputLines{};
static std::vector<Line> InputLines{};
static std::vector<Line> SearchOutputLines{};
static std::vector<Line> SearchHistoryLines{};
static int HistoryIndex = -1;
static FILE* OutputFile = nullptr;
static FILE* HistoryFile = nullptr;
static InputMode CommandMode = InputMode::Command;
static char SmallBuffer[4096];

static void WriteOutput() {
        if (!OutputFile) {
                fopen_s(&OutputFile, OUTPUT_FILE_PATH, "ab");
                assert(OutputFile != NULL);
        }
        const auto last = OutputLines.back();
        const char* buff = &OutputBuffer[last.start];
        fwrite(buff, 1, strlen(buff), OutputFile);
        fputc('\n', OutputFile);
}

static void LoadHistory() {
        fopen_s(&HistoryFile, HISTORY_FILE_PATH, "ab+");
        assert(HistoryFile != NULL);

        fseek(HistoryFile, 0, SEEK_END);
        uint32_t len = ftell(HistoryFile);
        assert(len < sizeof(InputBuffer));
        fseek(HistoryFile, 0, SEEK_SET);
        fread(InputBuffer, 1, len, HistoryFile);

        Line line;
        line.start = InputPos;
        for (InputPos = 0; InputPos < len; ++InputPos) {
                if (InputBuffer[InputPos] == '\n') {
                        line.len = InputPos - line.start;
                        InputBuffer[InputPos] = '\0';
                        InputLines.push_back(line);
                        line.start = InputPos + 1;
                }
        }
        InputBuffer[InputPos++] = '\0';
}

static void WriteHistory() {
        const auto last = InputLines.back();
        const char* buff = &InputBuffer[last.start];
        fwrite(buff, 1, strlen(buff), HistoryFile);
        fputc('\n', HistoryFile);
}


static void forward_to_old_consoleprint(void* consolemgr, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        OLD_ConsolePrintV(consolemgr, fmt, args);
        va_end(args);
}


extern void console_print(void* consolemgr, const char* fmt, va_list args) {
        (void)consolemgr;
        auto size = vsnprintf(&OutputBuffer[OutputPos], sizeof(OutputBuffer) - OutputPos, fmt, args);
        if (size <= 0) {
                return;
        }
        if (consolemgr) {
                forward_to_old_consoleprint(consolemgr, "%s", &OutputBuffer[OutputPos]); //send it already converted
        }
        OutputLines.push_back(Line{OutputPos, (uint32_t)size});
        OutputPos += size + 1; //keep the null terminator in the buffer
        UpdateScroll = 3;
        WriteOutput();
}

static void console_run(char* cmd) {
        static void(*RunConsoleCommand)(void*, char*) = nullptr;
        if (!RunConsoleCommand) {
                RunConsoleCommand = (decltype(RunConsoleCommand))HookAPI->Relocate(OFFSET_console_run);
        }
        RunConsoleCommand(NULL, cmd);
}

static void LogWrapper(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        console_print(NULL, fmt, args);
        va_end(args);
}

extern const struct log_api_t* GetLogAPI() {
        static bool init = false;
        static struct log_api_t ret;
        if (!init) {
                init = true;
                ret.Log = &LogWrapper;
        }
        return &ret;
}

extern void draw_console_window() {
        if (!HistoryFile) {
                LoadHistory();
        }

        if (!HookAPI) {
                HookAPI = GetHookAPI();
        }

        if (!OLD_ConsolePrintV) {
                OLD_ConsolePrintV = (decltype(OLD_ConsolePrintV)) HookAPI->HookFunction(
                        (FUNC_PTR)HookAPI->Relocate(OFFSET_console_vprint),
                        (FUNC_PTR)console_print
                );
        }

        static bool GameState = true;
        static const char* GameStates[] = { "Paused", "Running" };
        if (ImGui::Button(GameStates[GameState])) {
                char tgp[] = "ToggleGamePause";
                console_run(tgp);
                GameState = !GameState;
        }
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(-(ImGui::GetFontSize() * 20));
        if (UpdateFocus) {
                ImGui::SetKeyboardFocusHere();
                UpdateFocus = false;
        }
        char* input = &InputBuffer[InputPos];
        if (CommandMode == InputMode::Command) {
                if (ImGui::InputText("Command Mode", input, sizeof(InputBuffer) - InputPos, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_cmdline)) {
                        HistoryIndex = -1;
                        auto len = (uint32_t) strlen(input);
                        InputLines.push_back(Line{ InputPos, (uint32_t)len });
                        InputPos += len + 1;
                        InputBuffer[InputPos] = '\0';
                        console_run(input);
                        WriteHistory();
                        fflush(OutputFile);
                        fflush(HistoryFile);
                        UpdateFocus = true;
                }
                display_lines(OutputLines, OutputBuffer);
        }
        else if (CommandMode == InputMode::SearchOutput) {
                if (ImGui::InputText("Search Output", SmallBuffer, sizeof(SmallBuffer), ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_switch_mode)) {
                        SearchOutputLines.clear();
                        for (auto x : OutputLines) {
                                if (stristr(&OutputBuffer[x.start], SmallBuffer)) {
                                        SearchOutputLines.push_back(x);
                                }
                        }
                }
                display_lines(SearchOutputLines, OutputBuffer);
        }
        else if (CommandMode == InputMode::SearchHistory) {
                if (ImGui::InputText("Search History", SmallBuffer, sizeof(SmallBuffer), ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_switch_mode)) {
                        SearchHistoryLines.clear();
                        for (auto x : InputLines) {
                                if (stristr(&InputBuffer[x.start], SmallBuffer)) {
                                        SearchHistoryLines.push_back(x);
                                }
                        }
                }
                display_lines(SearchHistoryLines, InputBuffer);
        }
}


static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data) {
        
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                if (data->EventKey == ImGuiKey_UpArrow) {
                        if (HistoryIndex < 0) {
                                //we are on the bottom line and the user pressed up
                                //TODO: snapshot the current linebuffer when going up in history for the first time
                                HistoryIndex = (int) InputLines.size() - 1;
                        }
                        else if (HistoryIndex > 0) {
                                --HistoryIndex;
                        }
                }
                else if (data->EventKey == ImGuiKey_DownArrow) {
                        if (HistoryIndex < 0) {
                                return 0;
                        }

                        if(HistoryIndex < InputLines.size())
                                ++HistoryIndex;
                }

                if (HistoryIndex >= InputLines.size()) {
                        HistoryIndex = (int) InputLines.size() - 1;
                }
                
                const auto line = InputLines[HistoryIndex];
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, &InputBuffer[line.start]);
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                return CALLBACK_inputtext_switch_mode(data);
        }

        return 0;
}

static int CALLBACK_inputtext_switch_mode(ImGuiInputTextCallbackData* data) {
        (void)data;
        SmallBuffer[0] = 0;
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
                UpdateScroll = 3;
                break;
        }
        return 0;
}

static void display_lines(const std::vector<Line>& lines, char* buffer) {
        ImGui::BeginChild("scrolling region");
        ImGuiListClipper clip{};
        clip.Begin((int)lines.size());
        while (clip.Step()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 2.f });
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });
                for (auto i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        ImGui::PushID(i);
                        const auto line = lines[i];
                        ImGui::SetNextItemWidth(-1.f);
                        assert(line.len < 4096); //TODO: what was that weird bug i found....
                        ImGui::InputText("#buffer", &buffer[line.start], line.len, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopID();
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleVar();
        }
        ImGui::TextUnformatted(" "); //more padding to fix scrolling at bottom
        while (UpdateScroll) { //loop because we might be adding more console output across frames
                ImGui::SetScrollHereY(1.f);
                --UpdateScroll;
        }
        ImGui::EndChild();
}


static const char* stristr(const char* haystack, const char* needle) {
        if (haystack == NULL) return NULL;
        if (needle == NULL) return NULL;
        if (!haystack[0]) return NULL;
        if (!needle[0]) return haystack;

        const char* p1 = haystack;
        const char* p2 = needle;
        const char* r = NULL;

        while (*p1 != 0 && *p2 != 0) {
                if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
                        if (r == 0) {
                                r = p1;
                        }
                        p2++;
                } else {
                        p2 = needle;
                        if (r != 0) {
                                p1 = r + 1;
                        }

                        if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
                                r = p1;
                                p2++;
                        } else {
                                r = 0;
                        }
                }
                p1++;
        }

        return *p2 == 0 ? r : 0;
}