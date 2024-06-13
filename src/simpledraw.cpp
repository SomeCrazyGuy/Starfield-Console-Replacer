#include "main.h"

#include <ctype.h>

static void simple_separator() {
        ImGui::Separator();
}


static void simple_text(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextV(fmt, args);
        va_end(args);
}


static bool simple_button(const char* text) {
        return ImGui::Button(text);
}


static bool simple_checkbox(const char* text, bool* value) {
        return ImGui::Checkbox(text, (bool*)value);
}


static bool simple_input_text(const char* name, char* buffer, uint32_t buffer_size, bool true_on_enter) {
        return ImGui::InputText(name, buffer, buffer_size, (true_on_enter) ? ImGuiInputTextFlags_EnterReturnsTrue : 0);
}


//NOTE: this uses em size which is roughly font size + imgui padding and border size
//      (min_size_em * font_size) + padding + border
static void simple_hbox_left(float split, float min_size_em) {
        auto size = ImGui::GetContentRegionAvail();
        auto w = size.x * split;

        const float min_size = (ImGui::GetFontSize() * min_size_em);

        w = (w < min_size) ? min_size : w;

        // weird code, i use the width available as the unique id 
        // because nested hboxes will only reduce available width and thus be unique
        ImGui::PushID((int)(size.x));
        ImGui::BeginChild("##hbox left", ImVec2{ w, size.y }, true);
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


//NOTE: this uses em size which is roughly font size + imgui padding and border size
static void simple_vbox_top(float split, float min_size_em) {
        auto size = ImGui::GetContentRegionAvail();
        auto h = size.y * split;

        const auto min_size = (ImGui::GetTextLineHeightWithSpacing() * min_size_em);
        h = (h < min_size) ? min_size : h;
        ImGui::PushID((int)(size.y));
        ImGui::BeginChild("##vbox top", ImVec2{ size.x, h }, true);
}


static void simple_vbox_bottom() {
        ImGui::EndChild();
        ImGui::BeginChild("##vbox bottom");
}


static void simple_vbox_end() {
        ImGui::EndChild();
        ImGui::PopID();
}


static bool simple_drag_int(const char* name, int* value, int min, int max) {
        return ImGui::DragInt(name, value, 1.f, min, max);
}


static bool simple_drag_float(const char* name, float* value, float min, float max) {
        return ImGui::DragFloat(name, value, 1.f, min, max);
}


// "simple" draw api
static void simple_show_filtered_log_buffer(LogBufferHandle handle, const uint32_t* lines, uint32_t line_count, bool scroll_to_bottom) {
        if (!handle) return; //resist invalid logbuffers
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


static void simple_render_log_buffer(LogBufferHandle handle, bool scrolltobottom) {
        simple_show_filtered_log_buffer(handle, NULL, 0, scrolltobottom);
}


static bool simple_selection_list(uint32_t* selection, const void* userdata, uint32_t count, CALLBACK_SELECTIONLIST to_string) {
        const uint32_t sel = *selection;
        
        bool ret = false;
        ImGuiListClipper clip;
        char str[128];

        clip.Begin(count);
        while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        ImGui::PushID(i);
                        const char* text = to_string(userdata, i, str, sizeof(str));
                        if (ImGui::Selectable((text) ? text : "NULL", (sel == i))) {
                                *selection = i;
                                ret = true;
                        }
                        ImGui::PopID();
                }
        }
        return ret;
}


static void simple_draw_table(const char * const * const headers, uint32_t header_count, void* rows_userdata, uint32_t row_count, CALLBACK_TABLE draw_cell) {
        if (ImGui::BeginTable("SimpleDrawTable", header_count, ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable)) {
                for (uint32_t i = 0; i < header_count; ++i) {
                        ImGui::TableSetupColumn(headers[i]);
                }
                ImGui::TableHeadersRow();

                ImGuiListClipper clip;
                clip.Begin(row_count);
                while (clip.Step()) {
                        for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                                ImGui::PushID(row);
                                ImGui::TableNextRow();
                                for (uint32_t col = 0; col < header_count; ++col) {
                                        if (ImGui::TableSetColumnIndex(col)) {
                                                ImGui::SetNextItemWidth(-1.f); // fill table cell
                                                draw_cell(rows_userdata, row, col);
                                        }
                                }
                                ImGui::PopID();
                        }
                }
                ImGui::EndTable();
        }
}


static void simple_tab_bar(const char* const* const headers, uint32_t header_count, int* state) {
        ImGui::PushID(state);
        if (ImGui::BeginTabBar("tabbarwidget")) {
                for (uint32_t i = 0; i < header_count; ++i) {
                        ImGui::PushID(i);
                        if (ImGui::BeginTabItem(headers[i])) {
                                *state = i;
                                ImGui::EndTabItem();
                        }
                        ImGui::PopID();
                }
                ImGui::EndTabBar();
        }
        ImGui::PopID();
}


static int simple_button_bar(const char* const* const labels, uint32_t label_count) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 1.f, 1.f });
        const auto& style = ImGui::GetStyle();
        const auto frame_size = style.FramePadding;
        const auto item_spacing = style.ItemSpacing;
        const auto button_width_frame = (frame_size.x * 2) + item_spacing.x;
        const auto area = ImGui::GetContentRegionAvail();

        int ret = -1;
        float cur_x = 0.f;
        for (uint32_t i = 0; i < label_count; ++i) {
                const auto label_size = ImGui::CalcTextSize(labels[i]).x;
                const auto button_width = label_size + button_width_frame;
                const bool fits = ((cur_x + button_width) < area.x);

                if (fits) {
                        if (i) ImGui::SameLine();
                }
                else {
                        cur_x = 0.f;
                }

                ImGui::PushID(i);
                if (ImGui::Button(labels[i])) {
                        ret = (int)i;
                }
                ImGui::PopID();

                cur_x += button_width;
        }
        ImGui::PopStyleVar();
        return ret;
}


static void simple_tip(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip()) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(desc);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
        }
}

static void simple_sameline() {
        ImGui::SameLine();
}


// I find it strange that imgui didn't go with this pattern for grouping radiobuttons
// Also, text on the left or right of the arguments list?
// left matches other apis, right means the active and current ids line up. hmm....
static bool simple_radio(const char* text, uint32_t* group_id, uint32_t* button_id) {
        const auto cur = *button_id;
        const bool active = (*group_id == cur);
        const bool pressed = ImGui::RadioButton(text, active);
        if (pressed) {
                *group_id = cur;
        }
        *button_id = cur + 1;
        return pressed;
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
        simple_drag_int,
        simple_drag_float,
        simple_render_log_buffer,
        simple_show_filtered_log_buffer,
        simple_selection_list,
        simple_draw_table,
        simple_tab_bar,
        simple_button_bar,
        simple_tip,
        simple_sameline,
        simple_radio
};


extern constexpr const struct simple_draw_t* GetSimpleDrawAPI() {
        return &SimpleDraw;
}



