#include "simpledraw.h"

extern const struct simple_draw_t* GetSimpleDrawAPI() {
        static bool init = false;
        static simple_draw_t ret;

        if (!init) {
                init = true;
                ret.Separator = [](void) -> void {
                        ImGui::Separator();
                };
                ret.Text = [](const char* fmt, ...) -> void {
                        va_list args;
                        va_start(args, fmt);
                        ImGui::TextV(fmt, args);
                        va_end(args);
                };
                ret.Button = [](const char* text) -> boolean {
                        return ImGui::Button(text);
                };
                ret.Checkbox = [](const char* text, boolean* state) -> boolean {
                        return ImGui::Checkbox(text, (bool*)state);
                };
        }
        return &ret;
}