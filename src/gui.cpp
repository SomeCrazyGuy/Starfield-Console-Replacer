#include "main.h"
#include "gui.h"
#include "hotkeys.h"
#include "simpledraw.h"

static const auto UI = GetSimpleDrawAPI();

extern void draw_gui() {
        static bool once = false;
        if (!once) {
                const auto screen = ImGui::GetIO().DisplaySize;
                ImGui::SetNextWindowPos(ImVec2{ screen.x / 4, screen.y / 4 });
                ImGui::SetNextWindowSize(ImVec2{ screen.x / 2 , screen.y / 2 });
                once = true;
        }

        auto imgui_context = ImGui::GetCurrentContext();
        ImGui::Begin("BetterConsole");
        ImGui::BeginTabBar("mod tabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs);

        if (GetSettings()->FontScaleOverride == 0) {
                GetSettingsMutable()->FontScaleOverride = 100;
        }
        ImGui::SetWindowFontScale((float) GetSettings()->FontScaleOverride / 100.f);

        //TODO: the internal modmenu tab could be part of the internal plugin
        //      and thus called by the tab callback iteration routine like any other plugin
        if (ImGui::BeginTabItem("Mod Menu")) {
                ImGui::BeginTabBar("mod menu tabs");
                if (ImGui::BeginTabItem("Settings")) {
                        // imlemented in settings.cpp - i don't want to export the internals of the settings imlpementation
                        // just to use them in one other place, instead i'd rather add a ui function to the settings code
                        draw_settings_tab();
                        ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Hotkeys")) {
                        // implemented in hotkeys.cpp
                        draw_hotkeys_tab();
                        ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Help")) {
                        UI->Text("BetterConsole Version %s, Compatible with Starfield 1.11.36 - %s", BETTERCONSOLE_VERSION, COMPATIBLE_GAME_VERSION);
                        UI->LinkButton("NexusMods", "https://www.nexusmods.com/starfield/mods/3683");
                        UI->LinkButton("Reddit (not checked often)", "https://www.reddit.com/user/linuxversion/");
                        UI->LinkButton("Constellation by V2 (discord)", "https://discord.gg/v2-s-collections-1076179431195955290");
                        UI->LinkButton("Github", "https://github.com/SomeCrazyGuy/Starfield-Console-Replacer");
                        char path[260];
                        UI->LinkButton("Open Log File", GetPathInDllDir(path, "BetterConsoleLog.txt"));
                        UI->LinkButton("Open Config File", GetPathInDllDir(path, "BetterConsoleConfig.txt"));
                        ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
                ImGui::EndTabItem();
        }


        uint32_t draw_count = 0;
        const auto draw_callback = CallbackGetHandles(CALLBACKTYPE_DRAW, &draw_count);
        for (uint32_t i = 0; i < draw_count; ++i) {
                const auto handle = draw_callback[i];
                ImGui::PushID(handle);
                static bool focus_tab = false;
                if (ImGui::BeginTabItem(CallbackGetName(handle), nullptr, (!focus_tab)? ImGuiTabItemFlags_SetSelected : 0)) {
                        focus_tab = true;
                        CallbackGetCallback(CALLBACKTYPE_DRAW, handle).draw_callback(imgui_context);
                        ImGui::EndTabItem();
                }
                ImGui::PopID();
        }
        ImGui::EndTabBar();
        ImGui::End();
}
