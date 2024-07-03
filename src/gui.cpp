#include "main.h"
#include "gui.h"
#include "hotkeys.h"

// maybe more the shellopen stuff to a utility header?
#include <Windows.h>
#include <shellapi.h>


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
                        if (ImGui::Button("NexusMods")) {
                                ShellExecuteA(NULL, "open", "https://www.nexusmods.com/starfield/mods/3683", NULL, NULL, 1);
                        }
                        if (ImGui::Button("Reddit (not checked often)")) {
                                ShellExecuteA(NULL, "open", "https://www.reddit.com/user/linuxversion/", NULL, NULL, 1);
                        }
                        if (ImGui::Button("Constellation by V2 (discord)")) {
                                ShellExecuteA(NULL, "open", "https://discord.gg/v2-s-collections-1076179431195955290", NULL, NULL, 1);
                        }
                        if (ImGui::Button("Discord (not ready yet)")) {
                                ImGui::OpenPopup("NoDiscordServer");
                        }
                        if (ImGui::BeginPopup("NoDiscordServer")) {
                                char message[] = "Sorry, no discord server has been setup yet!\n"
                                        "I wouldn't have the time to moderate it anyway.\n"
                                        "But you can direct message me, my username is: linuxversion\n"
                                        "If you installed this mod as part of the Constellation by V2 collection,\n"
                                        "then you can report betterconsole issues on that discord server.";
                                ImGui::Text(message);
                                ImGui::EndPopup();
                        }
                        if (ImGui::Button("Open Log File")) {
                                char path[260];
                                ShellExecuteA(NULL, "open", GetPathInDllDir(path, "BetterConsoleLog.txt"), NULL, NULL, 1);
                        }
                        if (ImGui::Button("Open Config File")) {
                                char path[260];
                                ShellExecuteA(NULL, "open", GetPathInDllDir(path, "BetterConsoleConfig.txt"), NULL, NULL, 1);
                        }
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
