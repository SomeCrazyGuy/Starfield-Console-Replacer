#include "../betterapi.h"

#include "../imgui/imgui.h"

#include <random>

#include <varargs.h>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN
#include <Windows.h>




//48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d
/*
check for this in ghidra:

_strnicmp((char *)local_838,"ForEachRef[",0xb)
...
memset(local_c38,0,0x400);
pcVar15 = "float fresult\nref refr\nset refr to GetSelectedRef\nset fresult to ";

*/
constexpr uint64_t OFFSET_console_run = 0x28814a4; //void ConsoleRun(NULL, char* cmd)



struct RandomizerCommand {
        int weight_total;
        int weight;
        char command[512];
};


static std::default_random_engine Engine;
static std::vector<RandomizerCommand> Commands{};
static bool WeightsAreProcessed = false;


// directly write the struct to disk (lazy mode)? maybe header for count, but not necessary
// utilize std lower_bound?
// normalize weight to 10? 1 = 10% normal change, 10 = normal, 1000 = 10x chance?
//
// header | add command button
//[Command     |   Weight   | Delete ]



#define RANDOMIZER_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleRandomizer.bin"

const struct hook_api_t* HookAPI = nullptr;
const struct simple_draw_t* SimpleDraw = nullptr;


static void init_randomizer(const BetterAPI* api) {
        if (HookAPI) return;

        HookAPI = api->Hook;
        SimpleDraw = api->SimpleDraw;

        std::random_device dev{};
        std::seed_seq seq{dev(), dev()};
        std::default_random_engine rand{seq};
        Engine = rand;


        FILE* f = nullptr;
        fopen_s(&f, RANDOMIZER_FILE_PATH, "rb");
        if (f != NULL) {
                RandomizerCommand cmd;
                while (fread(&cmd, sizeof(cmd), 1, f)) {
                        Commands.push_back(cmd);
                }
                fclose(f);
        }
}

static void ConsoleRun(const char* command) {
        static void(*RunCommand)(void* consolemgr, const char* command) = (decltype(RunCommand)) HookAPI->Relocate(OFFSET_console_run);
        RunCommand(NULL, command);
}


static void RunRandomCommand() {
        if (WeightsAreProcessed == false) {
                WeightsAreProcessed = true;

                int weight = 0;
                for (auto& x : Commands) {
                        weight += x.weight;
                        x.weight_total = weight;
                }
        }

        const int max_weight = Commands.back().weight_total;
        std::uniform_int_distribution<int> Dist(0, max_weight);
        const int chosen_weight = Dist(Engine);

        auto pick = std::lower_bound(
                Commands.begin(),
                Commands.end(),
                chosen_weight,
                [](const RandomizerCommand& rc, int weight) -> bool { 
                        return rc.weight_total < weight; 
                }
        );

        ConsoleRun(pick->command);
}


static void RandomizerTab(void*) {

        ImGui::Text("Commands: %d", (int)Commands.size());

        ImGui::SameLine();
        
        if (ImGui::Button("Add")) {
                RandomizerCommand cmd{};
                cmd.command[0] = 0;
                cmd.weight = 100;
                cmd.weight_total = 0;
                Commands.push_back(cmd);
                WeightsAreProcessed = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Save")) {
                FILE* f = nullptr;
                fopen_s(&f, RANDOMIZER_FILE_PATH, "wb");
                ASSERT(f != NULL);
                for (auto& x : Commands) {
                        fwrite(&x, sizeof(RandomizerCommand), 1, f);
                }
                fclose(f);
        }

        if (ImGui::BeginTable("commands", 3, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Command Text", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (auto it = Commands.begin(); it != Commands.end(); ++it) {
                        ImGui::PushID(&it->command);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::SetNextItemWidth(50.f);
                        if (ImGui::Button("Delete")) {
                                WeightsAreProcessed = false;
                                Commands.erase(it);
                                break; // iterators are invalid now
                        }
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(50.f);
                        if (ImGui::DragInt("##weight", &it->weight, 10.f, 10, 1000)) {
                                WeightsAreProcessed = false;
                        }
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1.f);
                        ImGui::InputText("##Command", it->command, sizeof(it->command));
                        ImGui::PopID();
                }

                ImGui::EndTable();
        }

}

static boolean FilterHotkey(uint32_t vk, boolean shift, boolean ctrl) {
        (void)shift;
        (void)ctrl;

        if (vk == VK_F24) {
                RunRandomCommand();
                return true;
        }

        return false;
}

extern void RegisterRandomizer(const BetterAPI* api) {
        init_randomizer(api);

        static DrawCallbacks callback{};

        callback.Name = "F24 Randomizer";
        callback.DrawTab = RandomizerTab;

        api->Callback->RegisterDrawCallbacks(&callback);
        api->Callback->RegisterHotkey("Randomizer", FilterHotkey);
}