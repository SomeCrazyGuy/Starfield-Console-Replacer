#include "main.h"
#include "hook_api.h"
#include "game_hooks.h"

#include <stdio.h>

static const auto HookAPI = GetHookAPI();
static const auto LogBuffer = GetLogBufferAPI();

static LogBufferHandle ConsoleInput = 0;
static LogBufferHandle ConsoleOutput = 0;

static void* ConsoleManager = nullptr;
static unsigned char* is_game_paused_address = nullptr;
static bool is_console_ready = false;

static void (*Game_ConsolePrint)(void* consolemgr, const char* message) = nullptr;
static void (*Game_ConsoleRun)(void* consolemgr, char* cmd) = nullptr;
static void (*Game_StartingConsoleCommand)(void* A, uint32_t* type) = nullptr;
static void* (*Game_GetFormByID)(const char* identifier) = nullptr;
static const char* (*Game_GetFormName)(void* form) = nullptr;




extern LogBufferHandle GameHook_GetConsoleInputHandle() {
        if (!ConsoleInput) {
                ConsoleInput = LogBuffer->Restore("ConsoleInput", "BetterConsoleInput.txt");
        }
        return ConsoleInput;
}

extern LogBufferHandle GameHook_GetConsoleOutputHandle() {
        if (!ConsoleOutput) {
                ConsoleOutput = LogBuffer->Create("ConsoleOutput", "BetterConsoleOutput.txt");
        }
        return ConsoleOutput;
}


static void HookedConsoleRun(void* consolemgr, char* cmd) {
        if (!is_console_ready) {
                DEBUG("StartingConsoleCommand was not called, not ready for console command '%s'", cmd);
        }
        
        if (!ConsoleInput) {
                DEBUG("ConsoleInput logbuffer not ready when running command: '%s'", cmd);
                return;
        }
        LogBuffer->Append(ConsoleInput, cmd);
        
        if (Game_ConsoleRun) {
                Game_ConsoleRun(consolemgr, cmd);
        }
        else {
                DEBUG("Game_ConsoleRun was null when running command: '%s'", cmd);
        }
}


static void HookedConsolePrint(void* consolemgr, const char* message) {
        if (!ConsoleManager) {
                ConsoleManager = consolemgr;
        }

        if (!ConsoleOutput) {
                DEBUG("ConsoleInput logbuffer not ready when printing '%s'", message);
                return;
        }
        else {
                LogBuffer->Append(ConsoleOutput, message);
        }

        if (consolemgr) {
                if (Game_ConsolePrint) {
                        Game_ConsolePrint(consolemgr, message);
                }
                else {
                        DEBUG("Game_ConsolePrint was null when printing '%s'", message);
                }
        }
        else {
                DEBUG("consolemgr was null when printing '%s'", message);
        }
}

static void HookedStartingConsoleCommand(void* A, uint32_t* type) {
        if (*type == 5) is_console_ready = true;
        Game_StartingConsoleCommand(A, type);
}


static void ConsoleRun(const char* cmd) {
        char command[512];
        strncpy_s(command, sizeof(command), cmd, sizeof(command));
        HookedConsoleRun(NULL, command);
}


static void ConsolePrint(const char* message) {
        HookedConsolePrint(ConsoleManager, message);
}


static bool* GetGamePausedFlag() {
        static bool* flag_ptr = nullptr;
        if (flag_ptr) return flag_ptr;

        if (!is_game_paused_address) return nullptr;

        uint32_t offset;
        uint8_t displacement;
        memcpy(&offset, &is_game_paused_address[3], sizeof(uint32_t));
        memcpy(&displacement, &is_game_paused_address[9], sizeof(uint8_t));
        // 7 is because RIP relative displacement is calculated from the following instruction
        const auto ptr1 = (unsigned char**)(is_game_paused_address + offset + 7);

        if (*ptr1 == nullptr) {
                DEBUG("is_game_paused data not ready");
                return nullptr;
        }

        flag_ptr = (bool*)(*ptr1 + displacement);
        return flag_ptr;
}


static bool IsGamePaused() {
        if (!GetGamePausedFlag()) return false;
        return *GetGamePausedFlag();
}


static void SetGamePaused(bool paused) {
        if (!GetGamePausedFlag()) return;
        *GetGamePausedFlag() = paused;
}


static void* GetFormByID(const char* identifier) {
        if (!Game_GetFormByID) return nullptr;
        return Game_GetFormByID(identifier);
}


static const char* GetFormName(void* form) {
        if (!Game_GetFormName) return nullptr;
        return Game_GetFormName(form);
}


static bool IsConsoleReady() {
        return is_console_ready;
}


extern void GameHook_Init() {
        //char path_tmp[260];
        

        DEBUG("Hooking ExecuteCommand");
        auto OldConsoleRun = (FUNC_PTR) HookAPI->AOBScanEXE(
                "48 8b c4 "      // MOV RAX, RSP
                "48 89 50 ?? "   // MOV QWORD PTR [RAX+0x??], RDX
                "4c 89 40 ?? "   // MOV QWORD PTR [RAX+0x??], R8
                "4c 89 48 ?? "   // MOV QWORD PTR [RAX+0x??], R9
                "55 "            // PUSH RBP
                "53 "            // PUSH RBX
                "56 "            // PUSH RSI
                "57 "            // PUSH RDI
                "41 55 "         // PUSH R13
                "41 56 "         // PUSH R14
                "41 57 "         // PUSH R15
                "48 8d"          // LEA <clipped>
        );
        if (!OldConsoleRun) {
                DEBUG("Failed to find ExecuteCommand");
        }
        else {
                Game_ConsoleRun = (decltype(Game_ConsoleRun))HookAPI->HookFunction(
                        OldConsoleRun,
                        (FUNC_PTR)HookedConsoleRun
                );
        }


        DEBUG("Hooking ConsolePrint");
        auto OldConsolePrintV = (FUNC_PTR) HookAPI->AOBScanEXE(
                "48 85 D2 "                //TEST   RDX, RDX
                "0F 84 ?? ?? 00 00 "       //JE     0X0000????
                "48 89 5C 24 ?? "          //MOV    QWORD PTR[RSP + 0X??], RBX
                "55 "                      //PUSH   RBP
                "56 "                      //PUSH   RSI
                "57 "                      //PUSH   RDI
                "41 56 "                   //PUSH   R14
                "41 57 "                   //PUSH   R15
                "48 8B EC "                //MOV    RBP, RSP
                "48 83 EC ?? "             //SUB    RSP, 0X??
                "4C 8B FA"                 //MOV    R15, RDX
        );
        if (!OldConsolePrintV) {
                DEBUG("Failed to find ConsolePrint");
        }
        else {
                Game_ConsolePrint = (decltype(Game_ConsolePrint))HookAPI->HookFunction(
                        OldConsolePrintV,
                        (FUNC_PTR)HookedConsolePrint
                );
        }


        DEBUG("Hooking is_game_paused");
        is_game_paused_address = (unsigned char*)GetHookAPI()->AOBScanEXE(
                "48 8b 0d ?? ?? ?? ?? " // RCX,QWORD PTR [rip+0x????????]
                "80 79 ?? 00 "          // CMP BYTE PTR [rcx+0x??],0x00 
                "0f 94 c0 "             // SETZ AL (AL = ZF)
                "88 41 ?? "             // MOV BYTE PTR [RCX + 0x??],AL
                "b0 01 "                // MOV AL,0x1
                "C3"                    // RET
        );
        if (!is_game_paused_address) {
                DEBUG("Failed to find is_game_paused");
        }

        DEBUG("Hooking OnStartingConsoleCommand");
        auto OldStartingConsoleCommand = (FUNC_PTR)HookAPI->AOBScanEXE(
                "40 53 "        // PUSH RBX
                "48 83 EC ?? "  // SUB RSP,0x??
                "8B 02 "        // MOV EAX,DWORD PTR [RDX]
                "48 8B D9 "     // MOV RBX,RCX
                "83 F8 ?? "     // CMP EAX,0x??
                "75 ?? "        // JNE 0x??
                "48"            // MOV <clipped>
        );

        if (!OldStartingConsoleCommand) {
                DEBUG("Failed to hook OnStartingConsoleCommand");
        } else {
                Game_StartingConsoleCommand = (decltype(Game_StartingConsoleCommand)) HookAPI->HookFunction(
                        (FUNC_PTR)OldStartingConsoleCommand,
                        (FUNC_PTR)HookedStartingConsoleCommand
                );
        } 

        DEBUG("Hooking GetFormByID");
        Game_GetFormByID = (decltype(Game_GetFormByID))GetHookAPI()->AOBScanEXE(
                "88 54 24 ?? "    // MOV BYTE PTR [RSP+0x??],DL
                "55 "             // PUSH RBP
                "53 "             // PUSH RBX
                "56 "             // PUSH RSI
                "57 "             // PUSH RDI
                "48 8b ec "       // MOV RBP,RSP
                "48 83 ec ?? "    // SUB RSP, 0x??
                "48 8b f9 "       // MOV RDI,RCX
                "33 f6 "          // XOR ESI,ESI
                "48 85 c9 "       // TEST RCX,RCX
        );
        if (!Game_GetFormByID) {
                DEBUG("Failed to find GetFormByID");
        }


        DEBUG("Hooking GetFormName");
        Game_GetFormName = (decltype(Game_GetFormName))GetHookAPI()->AOBScanEXE(
                "48 89 4c 24 ?? " // MOV QWORD PTR [RSP+0x??], RCX
                "53 "             // PUSH RBX
                "48 83 EC ?? "    // SUB RSP, 0x??
                "48 8D 1D ?? ?? ?? ?? " // LEA RBX, QWORD PTR [rip+0x???]
                "48 85 C9 "       // TEST RCX, RCX
        );
        if (!Game_GetFormName) {
                DEBUG("Failed to find GetFormName");
        }
}

BC_EXPORT const struct gamehook_api_t* GetGameHookAPI() {
        static constexpr const struct gamehook_api_t GameHookAPI = {
                ConsoleRun,
                ConsolePrint,
                IsConsoleReady,
                SetGamePaused,
                IsGamePaused,
                GetFormByID,
                GetFormName,
                GameHook_GetConsoleOutputHandle,
        };
        return &GameHookAPI;
}
