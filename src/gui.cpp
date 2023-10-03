#include "main.h"

#include "gui.h"

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

                //put the window in the middle of the screen at 50% windows hidth and height
                ImGui::SetNextWindowPos(ImVec2{ width / 2, height / 2 });
                ImGui::SetNextWindowSize(ImVec2{ width, height });

                //scale up the font based on 1920x1080 = 100%
                float hf = size.y / 1080.f;
                if (hf > 1.f) {
                        FontScalePercent = (int)(hf * FontScalePercent);
                }

                //force font size based on fontscaleoverride parameter
                if (GetSettings()->FontScaleOverride) {
                        FontScalePercent = GetSettings()->FontScaleOverride;
                }
        }

        auto imgui_context = ImGui::GetCurrentContext();
        ImGui::Begin("Mod Menu");
        ImGui::SetWindowFontScale(FontScalePercent / 100.f);
        ImGui::BeginTabBar("tab items");
        if (ImGui::BeginTabItem("Logs")) {
                ImGui::BeginTabBar("log tabs");
                auto logs = GetCallbackHead();
                while (logs) {
                        if (logs->DrawLog) {
                                ImGui::PushID(logs);
                                if (ImGui::BeginTabItem(logs->Name)) {
                                        logs->DrawLog(imgui_context);
                                        ImGui::EndTabItem();
                                }
                                ImGui::PopID();
                        }
                        logs = logs->ll_next;
                }
                ImGui::EndTabBar();
                ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
                ImGui::BeginTabBar("settings tabs");
                auto settings = GetCallbackHead();
                while (settings) {
                        if (settings->DrawSettings) {
                                ImGui::PushID(settings);
                                if (ImGui::BeginTabItem(settings->Name)) {
                                        settings->DrawSettings(imgui_context);
                                        ImGui::EndTabItem();
                                }
                                ImGui::PopID();
                        }
                        settings = settings->ll_next;
                }
                ImGui::EndTabBar();
                ImGui::EndTabItem();
        }
        auto tabs = GetCallbackHead();
        while (tabs) {
                if (tabs->DrawTab) {
                        ImGui::PushID(tabs);
                        if (ImGui::BeginTabItem(tabs->Name)) {
                                tabs->DrawTab(imgui_context);
                                ImGui::EndTabItem();
                        }
                        ImGui::PopID();
                }
                tabs = tabs->ll_next;
        }
        ImGui::EndTabBar();
        ImGui::End();

        // Draw all registered windows
        auto windows = GetCallbackHead();
        while (windows) {
                if (windows->DrawWindow) {
                        ImGui::Begin(windows->Name);
                        windows->DrawWindow(imgui_context);
                        ImGui::End();
                }
                windows = windows->ll_next;
        }
}