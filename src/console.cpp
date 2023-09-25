#include "main.h"
#include "console.h"

#include <varargs.h>

#include <vector>
#include <string>

#include "hook_api.h"
#include "log_buffer.h"


#define OUTPUT_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleOutput.txt"
#define HISTORY_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleHistory.txt"


enum class InputMode : uint32_t {
        Command,
        SearchOutput,
        SearchHistory
};

static int CALLBACK_inputtext_cmdline(ImGuiInputTextCallbackData* data);
static int CALLBACK_inputtext_switch_mode(ImGuiInputTextCallbackData* data);
static void display_search_lines(LogBufferHandle handle, const std::vector<uint32_t>& lines);
static void display_lines(LogBufferHandle handle);
static bool strcasestr(const char* haystack, const char* needle);

static void(*OLD_ConsolePrintV)(void*, const char*, va_list) = nullptr;          
static int UpdateScroll = 0;
static bool UpdateFocus = true;
static int HistoryIndex = -1;
static InputMode CommandMode = InputMode::Command;


static const hook_api_t* HookAPI = nullptr;
static const log_buffer_api_t* LogBuffer = nullptr;
static char IOBuffer[256 * 1024];
static LogBufferHandle OutputHandle;
static LogBufferHandle HistoryHandle;
static FILE* OutputFile = nullptr;
static FILE* HistoryFile = nullptr;
static std::vector<uint32_t> SearchOutputLines{};
static std::vector<uint32_t> SearchHistoryLines{};


static void init_console() {
        static bool init = false;
        if (init) return; else init = true;

        HookAPI = GetHookAPI();
        LogBuffer = GetLogBufferAPI();
        OutputHandle = LogBuffer->Create("Console Output");
        HistoryHandle = LogBuffer->Create("Command History");

        OLD_ConsolePrintV = (decltype(OLD_ConsolePrintV))HookAPI->HookFunction(
                (FUNC_PTR)HookAPI->Relocate(OFFSET_console_vprint),
                (FUNC_PTR)console_print
        );

        fopen_s(&OutputFile, OUTPUT_FILE_PATH, "ab");
        ASSERT(OutputFile != NULL);

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


extern void console_print(void* consolemgr, const char* fmt, va_list args) {
        auto size = vsnprintf(IOBuffer, sizeof(IOBuffer), fmt, args);
        if (size <= 0) return;
        if (!LogBuffer) init_console();
        forward_to_old_consoleprint(consolemgr, "%s", IOBuffer); //send it already converted
        fputs(IOBuffer, OutputFile);
        fputc('\n', OutputFile);
        LogBuffer->Append(OutputHandle, IOBuffer);
        IOBuffer[0] = 0;
        UpdateScroll = 3;
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
        if (!LogBuffer) init_console();

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

        if (CommandMode == InputMode::Command) {
                if (ImGui::InputText("Command Mode", IOBuffer, sizeof(IOBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion, CALLBACK_inputtext_cmdline)) {
                        HistoryIndex = LogBuffer->GetLineCount(HistoryHandle);
                        fputs(IOBuffer, HistoryFile);
                        fputc('\n', HistoryFile);
                        console_run(IOBuffer);
                        IOBuffer[0] = 0;
                        fflush(OutputFile);
                        fflush(HistoryFile);
                        UpdateFocus = true;
                }
                display_lines(OutputHandle);
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
                UpdateScroll = 3;
                break;
        }
        return 0;
}


static uint32_t string_copy(char* dest, uint32_t dest_len, const char* src) {
        for (uint32_t i = 0; i < dest_len; ++i) {
                dest[i] = src[i];
                if (src[i] == '\0') {
                        return i;
                }

        }
        dest[dest_len - 1] = '\0';
        return dest_len;
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
                        string_copy(temp_line, sizeof(temp_line), LogBuffer->GetLine(handle, lines[i]));
                        ImGui::InputText("#buffer", temp_line, sizeof(temp_line), ImGuiInputTextFlags_ReadOnly);
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


static void display_lines(LogBufferHandle handle) {
        char temp_line[4096];
        ImGui::BeginChild("scrolling region");
        ImGuiListClipper clip{};
        clip.Begin((int)LogBuffer->GetLineCount(handle));
        while (clip.Step()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 2.f });
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });
                for (auto i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        ImGui::PushID(i);
                        ImGui::SetNextItemWidth(-1.f);
                        string_copy(temp_line, sizeof(temp_line), LogBuffer->GetLine(handle, i));
                        ImGui::InputText("#buffer", temp_line, sizeof(temp_line), ImGuiInputTextFlags_ReadOnly);
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