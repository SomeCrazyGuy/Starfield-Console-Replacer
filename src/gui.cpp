#include "main.h"
#include "gui.h"
#include "settings.h"
#include "simpledraw.h"

static const struct simple_draw_t* const SimpleDraw{GetSimpleDrawAPI()};

//linked list is too much pointer chasing
//theory - mod menu can register its own draw callbacks...

#include <Windows.h>
#include <shellapi.h>


extern void draw_gui() {
        static ImGuiTabItemFlags ConsoleDefaultFlags = ImGuiTabItemFlags_SetSelected;
        static bool initpos = false;

        if (!initpos) {
                initpos = true;
                
                const auto size = ImGui::GetIO().DisplaySize;
                const float width = size.x / 2;
                const float height = size.y / 2;

                //put the window in the middle of the screen at 50% game width and height
                ImGui::SetNextWindowPos(ImVec2{ width / 2, height / 2 });
                ImGui::SetNextWindowSize(ImVec2{ width, height });
        }

        auto imgui_context = ImGui::GetCurrentContext();
        ImGui::Begin(BetterAPIName " Mod Menu");
        ImGui::BeginTabBar("mod tabs");

        size_t infos_count = 0;
        auto infos = GetModInfo(&infos_count);

        if (GetSettings()->FontScaleOverride == 0) {
                GetSettingsMutable()->FontScaleOverride = 100;
        }

        //todo: make the help text always visible with font size override?
        ImGui::SetWindowFontScale(1);
        if (ImGui::TabItemButton("Help", ImGuiTabItemFlags_Leading)) {
                ImGui::OpenPopup("HelpLinks");
        }
        if (ImGui::BeginPopup("HelpLinks")) {
                ImGui::SeparatorText("Get Help");
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
                ImGui::SeparatorText("Common Fixes");
                if (ImGui::Button("Reset Font Scale")) {
                        GetSettingsMutable()->FontScaleOverride = 100;
                }
                if (ImGui::Button("Reset Hotkey to F1")) {
                        GetSettingsMutable()->ConsoleHotkey = 112;
                        GetSettingsMutable()->HotkeyModifier = 0;
                }
                if (ImGui::Button("Open Log File")) {
                        char path[260];
                        ShellExecuteA(NULL, "open", GetPathInDllDir(path, "BetterConsoleLog.txt"), NULL, NULL, 1);
                }
                if (ImGui::Button("Open Config File")) {
                        char path[260];
                        ShellExecuteA(NULL, "open", GetPathInDllDir(path, "MiniModMenuRegistry.txt"), NULL, NULL, 1);
                }
                ImGui::EndPopup();
        }
        ImGui::SetWindowFontScale((float) GetSettings()->FontScaleOverride / 100.f);
        

        //TODO: the internal modmenu tab could be part of the internal plugin
        //      and thus called by the tab callback iteration routine like any other plugin
        if (ImGui::BeginTabItem("Mod Menu")) {
                ImGui::BeginTabBar("mod menu tabs");
                if (ImGui::BeginTabItem("Logs")) {
                        static const auto LogAPI = GetLogBufferAPI();
                        static int selected_log = -1;
                        static LogBufferHandle selected_handle;

                        static UIDataList datalist;
                        datalist.UserData = infos;
                        datalist.Count = (uint32_t)infos_count;
                        datalist.ToString = [](const void* userdata, uint32_t index, char* fmtbuffer, uint32_t fmtbuffer_size) -> const char* {
                                (void)fmtbuffer;
                                (void)fmtbuffer_size;
                                const ModInfo* infos = (const ModInfo*)userdata;
                                return infos[index].Name;
                        };

                        SimpleDraw->HboxLeft(0.f, 12.f);
                        if (SimpleDraw->SelectionList(&datalist, &selected_log)) {
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
