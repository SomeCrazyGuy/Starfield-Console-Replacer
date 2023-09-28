#include "simpledraw.h"

#include "main.h"

#include "log_buffer.h"


static void simple_separator() {
        ImGui::Separator();
}


static void simple_text(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextV(fmt, args);
        va_end(args);
}


static boolean simple_button(const char* text) {
        return ImGui::Button(text);
}


static boolean simple_checkbox(const char* text, boolean* value) {
        return ImGui::Checkbox(text, (bool*)value);
}


static boolean simple_input_text(const char* name, char* buffer, uint32_t buffer_size, boolean true_on_enter) {
        return ImGui::InputText(name, buffer, buffer_size, (true_on_enter) ? ImGuiInputTextFlags_EnterReturnsTrue : 0);
}


// "simple"
static void simple_render_log_buffer(LogBufferHandle handle, boolean scrolltobottom) {
        static const auto LogAPI = GetLogBufferAPI();

        static const auto string_copy = [](char* dest, uint32_t dest_size, const char* src) {
                auto len = strlen(src);
                len = (len >= dest_size) ? dest_size - 1 : len;
                memcpy(dest, src, len);
                dest[len] = '\0';
        };

        char temp_line[4096];
        ImGui::PushID(handle);
        ImGui::BeginChild("scrolling region");
        ImGuiListClipper clip{};
        clip.Begin((int)LogAPI->GetLineCount(handle));
        while (clip.Step()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 2.f });
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });
                for (auto i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        ImGui::PushID(i);
                        ImGui::SetNextItemWidth(-1.f);
                        string_copy(temp_line, sizeof(temp_line), LogAPI->GetLine(handle, i));
                        ImGui::InputText("#buffer", temp_line, sizeof(temp_line), ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopID();
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleVar();
        }
        ImGui::TextUnformatted(" "); //more padding to fix scrolling at bottom
        if (scrolltobottom) {
                ImGui::SetScrollHereY(1.f);
        }
        ImGui::EndChild();
        ImGui::PopID();
}


static constexpr simple_draw_t SimpleDraw {
        &simple_separator,
        &simple_text,
        &simple_button,
        &simple_checkbox,
        &simple_input_text,
        &simple_render_log_buffer
};


extern constexpr const struct simple_draw_t* GetSimpleDrawAPI() {
        return &SimpleDraw;
}



