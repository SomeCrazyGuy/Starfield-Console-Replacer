#include "main.h"
#include "gui.h"
#include "settings.h"
#include "simpledraw.h"

static const struct simple_draw_t* const SimpleDraw{GetSimpleDrawAPI()};

//linked list is too much pointer chasing
//theory - mod menu can register its own draw callbacks...

extern void draw_gui() {
        static ImGuiTabItemFlags ConsoleDefaultFlags = ImGuiTabItemFlags_SetSelected;
        static int FontScalePercent = 100;
        static bool initpos = false;


        if (!initpos) {
                initpos = true;
                
                const auto size = ImGui::GetIO().DisplaySize;
                const float width = size.x / 2;
                const float height = size.y / 2;

                //put the window in the middle of the screen at 50% game width and height
                ImGui::SetNextWindowPos(ImVec2{ width / 2, height / 2 });
                ImGui::SetNextWindowSize(ImVec2{ width, height });

                //scale up the font based on 1920x1080 = 100%
                float hf = size.y / 1080.f;
                if (hf > 1.f) {
                        FontScalePercent = (int)(hf * FontScalePercent);
                }
        }

        //force font size based on fontscaleoverride parameter if set
        if (GetSettings()->FontScaleOverride) {
                FontScalePercent = GetSettings()->FontScaleOverride;
        }

        auto imgui_context = ImGui::GetCurrentContext();
        ImGui::Begin("Mod Menu");
        ImGui::SetWindowFontScale(FontScalePercent / 100.f);
        ImGui::BeginTabBar("mod tabs");

        size_t infos_count = 0;
        auto infos = GetModInfo(&infos_count);

        //TODO: the internal modmenu tab could be part of the internal plugin
        //      and thus called by the tab callback iteration routine like any other plugin
        if (ImGui::BeginTabItem("Mod Menu")) {
                ImGui::BeginTabBar("mod menu tabs");
                if (ImGui::BeginTabItem("Logs")) {
                        static const auto LogAPI = GetLogBufferAPI();
                        static int selected_log = -1;
                        static LogBufferHandle selected_handle;

                        SimpleDraw->HboxLeft(0.f, 12.f);
                        const auto selection_changed = SimpleDraw->SelectionList(
                                &selected_log,
                                infos,
                                (int)infos_count,
                                [](const void* item, uint32_t index, char* out, uint32_t out_size) -> const char* {
                                        (void)out;
                                        (void)out_size;
                                        const auto mod = (ModInfo*)item;
                                        return (mod[index].PluginLog) ? mod[index].Name : nullptr;
                                }
                        );
                        if (selection_changed) {
                                selected_handle = infos[selected_log].PluginLog;
                        }
                        SimpleDraw->HBoxRight();
                        if (selected_log != -1) {
                                SimpleDraw->ShowLogBuffer(selected_handle, false);
                        }
                        SimpleDraw->HBoxEnd();
                        ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Settings")) {
                        // imlemented in settings.cpp - i don't want to export the internals of the settings imlpementation
                        // just to use them in one other place, instead i'd rather add a ui function to the settings code
                        draw_settings_tab();
                        ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
                ImGui::EndTabItem();
        }
        

        for (uint32_t i = 0; i < infos_count; ++i) {
                const auto mod = infos[i];
                if (mod.DrawTab) {
                        ImGui::PushID(mod.DrawTab);
                        if (ImGui::BeginTabItem(mod.Name)) {
                                mod.DrawTab(imgui_context);
                                ImGui::EndTabItem();
                        }
                        ImGui::PopID();
                }
        }
        ImGui::EndTabBar();
        ImGui::End();

        for (uint32_t i = 0; i < infos_count; ++i) {
                const auto mod = infos[i];
                if (mod.DrawWindow) {
                        mod.DrawWindow(imgui_context, true); //TODO: fix & query the should_show_ui here
                }
        }
}
