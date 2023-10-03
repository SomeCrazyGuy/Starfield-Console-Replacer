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


static void simple_hbox_left(float split, float min_size) {
        auto size = ImGui::GetContentRegionAvail();
        auto w = size.x * split;
        w = (w < min_size) ? min_size : w;

        // weird code, i use the width available as the unique id 
        // because nested hboxes will only reduce available width and thus be unique
        ImGui::PushID((int)(size.x));
        ImGui::BeginChild("##hbox left", ImVec2{ w, size.y });
}


static void simple_hbox_right() {
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##hbox right");
}

static void simple_hbox_end() {
        ImGui::EndChild();
        ImGui::PopID();
}


static void simple_vbox_top(float split, float min_size) {
        auto size = ImGui::GetContentRegionAvail();
        auto h = size.y * split;
        h = (h < min_size) ? min_size : h;
        ImGui::PushID((int)(size.y));
        ImGui::BeginChild("##vbox top", ImVec2{ size.x, h });
}


static void simple_vbox_bottom() {
        ImGui::EndChild();
        ImGui::BeginChild("##vbox bottom");
}

static void simple_vbox_end() {
        ImGui::EndChild();
        ImGui::PopID();
}


static float simple_current_font_size() {
        return ImGui::GetFontSize();
}


static boolean simple_drag_int(const char* name, int* value, int min, int max) {
        return ImGui::DragInt(name, value, 1.f, min, max);
}

static boolean simple_drag_float(const char* name, float* value, float min, float max) {
        return ImGui::DragFloat(name, value, 1.f, min, max);
}


// "simple" draw api
static void simple_show_filtered_log_buffer(LogBufferHandle handle, const uint32_t* lines, uint32_t line_count, boolean scroll_to_bottom) {
        static const auto LogAPI = GetLogBufferAPI();
        char temp_line[4096];

        auto count = (lines) ? line_count : LogAPI->GetLineCount(handle);

        ImGuiListClipper clip;
        clip.Begin(count);

        ImGui::BeginChild("scrolling region");
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 2.f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });
        while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        const auto line_num = (lines) ? lines[i] : i;
                        const char* line_str = LogAPI->GetLine(handle, line_num);

                        ImGui::PushID(i);
                        ImGui::SetNextItemWidth(-1.f);
                        strncpy_s(temp_line, sizeof(temp_line), line_str, strlen(line_str));
                        ImGui::InputText("#buffer", temp_line, sizeof(temp_line), ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopID();
                }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        ImGui::TextUnformatted(" "); //more padding to fix scrolling at bottom
        if (scroll_to_bottom) {
                ImGui::SetScrollHereY(1.f);
        }
        ImGui::EndChild();
}



static void simple_render_log_buffer(LogBufferHandle handle, boolean scrolltobottom) {
        simple_show_filtered_log_buffer(handle, NULL, 0, scrolltobottom);
}


static constexpr simple_draw_t SimpleDraw {
        simple_separator,
        simple_text,
        simple_button,
        simple_checkbox,
        simple_input_text,
        simple_hbox_left,
        simple_hbox_right,
        simple_hbox_end,
        simple_vbox_top,
        simple_vbox_bottom,
        simple_vbox_end,
        simple_current_font_size,
        simple_drag_int,
        simple_drag_float,
        simple_render_log_buffer,
        simple_show_filtered_log_buffer
};


extern constexpr const struct simple_draw_t* GetSimpleDrawAPI() {
        return &SimpleDraw;
}



