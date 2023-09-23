#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#include "../minhook/MinHook.h"

#include "../imgui/imgui.h"

#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"

/* TODO List:
        -dynamic arrays for buffers
        -documentation
*/


// --------------------------------------------------------------------
// ---- Change these offsets for each game update, need signatures ----
// --------------------------------------------------------------------
constexpr uint32_t GAME_VERSION = MAKE_VERSION(1, 7, 29);



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
constexpr uint64_t OFFSET_console_vprint = 0x2883978; //stolen from sfse, void ConsolePrintV(ConsoleMgr*, const char* fmt, va_list args)



//48 8b c4 48 89 50 ?? 4c 89 40 ?? 4c 89 48 ?? 55 53 56 57 41 55 41 56 41 57 48 8d
/*
check for this in ghidra:

_strnicmp((char *)local_838,"ForEachRef[",0xb)
...
memset(local_c38,0,0x400);
pcVar15 = "float fresult\nref refr\nset refr to GetSelectedRef\nset fresult to ";

*/
constexpr uint64_t OFFSET_console_run = 0x287df04; //void ConsoleRun(NULL, char* cmd)


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------


#ifdef DISABLE_ASSERT
#define ASSERT(X) do{}while(0)
#else
#define ASSERT(X) do{if(!(X)){char MSG[1024];auto len=snprintf(MSG,1024,"%s:%s:%u> ",__FILE__,__func__,__LINE__);snprintf(&MSG[len],1024-len,""#X" ");MessageBoxA(NULL,MSG,"ASSERTION FAILURE",0);ExitProcess(1);}}while(0)
#endif