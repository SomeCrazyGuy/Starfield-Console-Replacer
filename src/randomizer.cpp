#include "main.h"
#include "util.h"

#include <varargs.h>
#include <vector>


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

        const int max_weight = (int)Commands.back().weight_total;
        const int chosen_weight = (int)random_in_range(0, max_weight);

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

        enum column_type { 
                COL_DELETE, COL_WEIGHT, COL_COMMAND,
                COL_COUNT
        };

        static const char* table_headers[COL_COUNT] = {
                "Delete", "Weight", "*Command"
        };

        //Roundabout way because simpledraw tables cannot change row count while rendering
        // because it could iterate beyond the new end of the table, or a realloc could invalidate the pointer.
        //Also, it needs to be static so that i can use a lambda, but needs to be set to -1 every frame anyway... meh
        static int request_delete_command_index;
        request_delete_command_index = -1;

        SimpleDraw->Table(
                table_headers,
                COL_COUNT,
                (void*)Commands.data(),
                (uint32_t)Commands.size(),
                [](void* userdata, int row, int id) {
                        auto cmds = reinterpret_cast<RandomizerCommand*>(userdata);
                        auto cmd = &cmds[row];
                        switch ((enum column_type) id)
                        {
                        case COL_DELETE:
                                if (SimpleDraw->Button("Delete")) {
                                        request_delete_command_index = row;
                                }
                                break;
                        case COL_WEIGHT: 
                                if (ImGui::DragInt("##weight", &cmd->weight, 10, 0, 1000)) {
                                        WeightsAreProcessed = false;
                                }
                                break;
                        case COL_COMMAND:
                                SimpleDraw->InputText("##command", cmd->command, sizeof(cmd->command), false);
                                break;
                        default:
                                ASSERT(false && "Invalid column number for cell draw callback!");
                                break;
                        }
                }
        );

        if (request_delete_command_index != -1) {
                Commands.erase(Commands.begin() + request_delete_command_index);
        }
}


static boolean FilterHotkey(uint32_t vk, boolean shift, boolean ctrl) {
        (void)shift;
        (void)ctrl;

        if (vk == 0x87) { //VK_F24
                RunRandomCommand();
                return true;
        }

        return false;
}


extern void RegisterRandomizer(const BetterAPI* api) {
        init_randomizer(api);

        static ModInfo callback{};

        callback.Name = "F24 Randomizer";
        callback.DrawTab = RandomizerTab;
        callback.HotkeyCallback = FilterHotkey;

        api->Callback->RegisterModInfo(callback);
}