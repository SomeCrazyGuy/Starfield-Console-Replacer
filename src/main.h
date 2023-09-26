#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#include "../minhook/MinHook.h"

#include "../imgui/imgui.h"

#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"

#include <cstdio>

/* TODO List:
        -documentation

        nexusmods todo list:
        ====================
        Easy - option to pass through input to game while console is open

        Easy - allow >1 lines to be pasted into the command input (like paste a whole bat file)

        Medium - get/set selected ref

        Medium - get the ref of the object you are looking at

        Medium - once selected ref is working, add get base id

        Unknown - 1 report of incompatibility with reactor count mod

        Unknown - Game crashes at startup for some users (might be dlss mod related?)

        Unknown - Game crashes when F1 is pressed for some users (might be dlss mod related?)

        Unknown - puredark dlss is not compatible, but only for reading keyboard and mouse input

        Unknown - "Streamline Native (Frame Gen - DLSS - Reflex Integration)" causes crash on F1

        Unknown - "Starfield Frame Generation - Replacing FSR2 with DLSS-G" causes instant CTD
*/


// --------------------------------------------------------------------
// ---- Change these offsets for each game update, need signatures ----
// --------------------------------------------------------------------
constexpr uint32_t GAME_VERSION = MAKE_VERSION(1, 7, 33);



//48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ?? 57 b8 30 10 -- the 0x1030 stack size might be too unique
/*
  undefined4 uVar1;
  int iVar2;
  char local_1010 [4096];
  undefined8 local_10;

  local_10 = 0x142883992;
  uVar1 = FUN_140839080();
  FUN_140587784(0x40);

  //calling vsnprintf() with 4096 buffer and using the retrn value to append "\n\0"
  iVar2 = FUN_1412518e8(local_1010,0x1000,param_2,param_4,param_3);
  if (0 < iVar2) {
    local_1010[iVar2] = '\n';           //note the behavior of adding "\n\0"
    local_1010[iVar2 + 1] = '\0';
    FUN_14288379c(param_1,local_1010,param_2);
  }
  FUN_140587784(uVar1);
  return;
*/
constexpr uint64_t OFFSET_console_vprint = 0x2886AD8; //stolen from sfse, void ConsolePrintV(ConsoleMgr*, const char* fmt, va_list args)



//48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d
/*
check for this in ghidra:

_strnicmp((char *)local_838,"ForEachRef[",0xb)
...
memset(local_c38,0,0x400);
pcVar15 = "float fresult\nref refr\nset refr to GetSelectedRef\nset fresult to ";

*/
constexpr uint64_t OFFSET_console_run = 0x2881064; //void ConsoleRun(NULL, char* cmd)


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------


// adding heavy debugging asserts until crashes are fixed
static inline constexpr const char* GetRelativeProjectDir(const char* file_path) noexcept {
        if (!file_path) return nullptr;
        const char* x = file_path;
        while (*x) ++x;
        while ((x != file_path) && (*x != '\\')) --x;
        --x;
        while ((x != file_path) && (*x != '\\')) --x;
        ++x;
        return x;
}

#define HERE_MSG(BUFFER, BUFFER_SIZE) do{snprintf((BUFFER), (BUFFER_SIZE), "[ERROR] %s:%s:%d:", GetRelativeProjectDir(__FILE__), __func__, __LINE__);}while(0)
#define FATAL_ERROR(FORMAT) do{char msg[1024]; HERE_MSG(msg, 256); snprintf(msg+strlen(msg), 768, " " FORMAT); MessageBoxA(NULL, msg, "Fatal Error", 0); abort(); }while(0)
#define ASSERT(CONDITION) do{if(!(CONDITION)){FATAL_ERROR(#CONDITION);}}while(0)
#define NOT_NULL(POINTER) ASSERT((POINTER) != NULL)