#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#include "../minhook/MinHook.h"

#include "../imgui/imgui.h"

#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"

#include "callback.h"
#include "hook_api.h"
#include "simpledraw.h"
#include "log_buffer.h"



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

