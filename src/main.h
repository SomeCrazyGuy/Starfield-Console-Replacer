#pragma once

#include <stdlib.h>
#include <stdint.h>

#define MODMENU_DEBUG

#ifndef MODMENU_DEBUG
#ifdef _DEBUG
#define MODMENU_DEBUG
#endif // _DEBUG
#endif // !MODMENU_DEBUG

#ifdef MODMENU_DEBUG
extern void DebugImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept;
extern void AssertImpl(const char* const filename, const char* const func, int line, const char* const text) noexcept;
extern void TraceImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept;
#define DEBUG(...) do { DebugImpl(__FILE__, __func__, __LINE__, " " __VA_ARGS__); } while(0)
#define ASSERT(CONDITION) do { if (!(CONDITION)) { AssertImpl(__FILE__, __func__, __LINE__, " " #CONDITION); } } while(0)
#define TRACE(...) do { TraceImpl(__FILE__, __func__, __LINE__, " " __VA_ARGS__); } while(0)
#define IMGUI_DEBUG_PARANOID
#else
#define DEBUG(...) do { } while(0)
#define ASSERT(...) do { } while(0)
#defince TRACE(...) do { } while(0)
#endif // MODMENU_DEBUG


#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"


#include "callback.h"
#include "hook_api.h"
#include "simpledraw.h"
#include "log_buffer.h"
#include "settings.h"

#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#include "../imgui/imgui.h"


#define BETTERCONSOLE_VERSION "1.3.1"

// --------------------------------------------------------------------
// ---- Change these offsets for each game update                  ----
// --------------------------------------------------------------------
constexpr uint32_t GAME_VERSION = MAKE_VERSION(1, 11, 36);



//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
struct ModMenuSettings {
        uint32_t FontScaleOverride = 0;
        uint32_t CreateSwapChainForHwndSoft = 0;
        uint32_t SwapchainPresentHard = 0;
};

extern const ModMenuSettings* GetSettings();
extern ModMenuSettings* GetSettingsMutable();
extern char* GetPathInDllDir(char* path_max_buffer, const char* filename);
