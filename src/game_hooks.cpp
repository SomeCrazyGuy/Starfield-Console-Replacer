#include "main.h"
#include "hook_api.h"
#include "game_hooks.h"

#include <stdio.h>

static const auto HookAPI = GetHookAPI();

static void (*Game_ConsoleRun)(void* consolemgr, char* cmd) = nullptr;
static void(*Game_ConsolePrintV)(void* consolemgr, const char* fmt, va_list args) = nullptr;
static void(*Game_StartingConsoleCommand)(void* A, uint32_t* type) = nullptr;

static const auto LogBuffer = GetLogBufferAPI();

static GameHookData HookData{};

extern const GameHookData* GameHook_GetData(void) {
        return &HookData;
}


static void HookedConsoleRun(void* consolemgr, char* cmd) {
        if (HookData.ConsoleReadyFlag && !*HookData.ConsoleReadyFlag) {
                DEBUG("StartingConsoleCommand was not called, not ready for console command '%s'", cmd);
        }

        Game_ConsoleRun(consolemgr, cmd);
        
        if (!HookData.ConsoleInput) {
                DEBUG("ConsoleInput logbuffer not ready when running command: '%s'", cmd);
                return;
        }
        LogBuffer->Append(HookData.ConsoleInput, cmd);
}


static void HookedConsolePrintV(void* consolemgr, const char* fmt, va_list args) {
        Game_ConsolePrintV(consolemgr, fmt, args);

        if (!HookData.ConsoleOutput) {
                DEBUG("ConsoleOutput logbuffer not ready");
                return;
        }

        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        LogBuffer->Append(HookData.ConsoleOutput, buffer);
}


static void HookedStartingConsoleCommand(void* A, uint32_t* type) {
        static bool ready = false;
        HookData.ConsoleReadyFlag = &ready;
        if (*type == 5) ready = true;
        Game_StartingConsoleCommand(A, type);
}


static void ConsoleRun(const char* cmd) {
        char command[512];
        strncpy_s(command, sizeof(command), cmd, sizeof(command));
        HookedConsoleRun(NULL, command);
}


static bool* GetGamePausedFlag() {
        static bool* flag_ptr = nullptr;
        if (flag_ptr) return flag_ptr;

        static bool func_addr_initialized = false;
        static unsigned char* func_addr = nullptr;

        if (func_addr_initialized && func_addr == nullptr) return nullptr;

        DEBUG("Hooking is_game_paused function");
        func_addr = (unsigned char*)GetHookAPI()->AOBScanEXE(
                "48 8b 0d ?? ?? ?? ?? " // RCX,QWORD PTR [rip+0x????????]
                "80 79 ?? 00 "          // CMP BYTE PTR [rcx+0x??],0x00 
                "0f 94 c0 "             // SETZ AL (AL = ZF)
                "88 41 ?? "             // MOV BYTE PTR [RCX + 0x??],AL
                "b0 01 "                // MOV AL,0x1
                "C3"                    // RET
        );
        func_addr_initialized = true;

        if (!func_addr) {
                DEBUG("is_game_paused function not found");
                return nullptr;
        }

        uint32_t offset;
        uint8_t displacement;
        memcpy(&offset, &func_addr[3], sizeof(uint32_t));
        memcpy(&displacement, &func_addr[9], sizeof(uint8_t));
        // 7 is because RIP relative displacement is calculated from the following instruction
        const auto ptr1 = (unsigned char**)(func_addr + offset + 7);

        if (*ptr1 == nullptr) {
                DEBUG("is_game_paused data not ready");
                return nullptr;
        }

        flag_ptr = (bool*)(*ptr1 + displacement);
        return flag_ptr;
}

extern void GameHook_Init() {
        char path_tmp[260];
        HookData.ConsoleInput = LogBuffer->Restore("ConsoleInput", GetPathInDllDir(path_tmp, "BetterConsoleInput.txt"));
        HookData.ConsoleOutput = LogBuffer->Create("ConsoleOutput", GetPathInDllDir(path_tmp, "BetterConsoleOutput.txt"));

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
                HookData.ConsoleRun = &ConsoleRun;
        }
        

        DEBUG("Hooking ConsolePrint");
        auto OldConsolePrintV = (FUNC_PTR) HookAPI->AOBScanEXE(
                "48 89 5c 24 ?? " // MOV QWORD PTR [RSP+0x??], RBX
                "48 89 6c 24 ?? " // MOV QWORD PTR [RSP+0x??], RBP
                "48 89 74 24 ?? " // MOV QWORD PTR [RSP+0x??], RSI
                "57 "             // PUSH RDI
                "b8 30 10 00 00 " // MOV EAX, 0x1030
                "e8 ?? ?? ?? ?? " // CALL ?? ?? ?? ??
                "48 2b e0 "       // SUB RSP, RAX
                "49 8b f8 "       // MOV RDI, R8
        );
        if (!OldConsolePrintV) {
                DEBUG("Failed to find ConsolePrint");
        }
        else {
                Game_ConsolePrintV = (decltype(Game_ConsolePrintV))HookAPI->HookFunction(
                        OldConsolePrintV,
                        (FUNC_PTR)HookedConsolePrintV
                );
                HookData.ConsoleOutputHooked = true;
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

        HookData.GetGamePausedFlag = &GetGamePausedFlag;

        DEBUG("Hooking GetFormByID");
        HookData.GetFormByID = (decltype(HookData.GetFormByID))GetHookAPI()->AOBScanEXE(
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

        if (!HookData.GetFormByID) {
                DEBUG("Failed to find GetFormByID");
        }


        DEBUG("Hooking GetFormName");
        HookData.GetFormName = (decltype(HookData.GetFormName))GetHookAPI()->AOBScanEXE(
                "48 89 4c 24 ?? " // MOV QWORD PTR [RSP+0x??], RCX
                "53 "             // PUSH RBX
                "48 83 EC ?? "    // SUB RSP, 0x??
                "48 8D 1D ?? ?? ?? ?? " // LEA RBX, QWORD PTR [rip+0x???]
                "48 85 C9 "       // TEST RCX, RCX
        );
        if (!HookData.GetFormName) {
                DEBUG("Failed to find GetFormName");
        }
}