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

constexpr const char* next_slash(const char* const path) {
        return (!path)
                ? nullptr
                : (*path == '/' || *path == '\\')
                ? path
                : (*path == 0)
                ? nullptr
                : next_slash(path + 1);
}

constexpr const char* filename_only(const char* const path) {
        return (next_slash(path))
                ? filename_only(next_slash(path + 1))
                : path;
}

extern void DebugImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept;
extern void AssertImpl(const char* const filename, const char* const func, int line, const char* const text) noexcept;
extern void TraceImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept;
#define DEBUG(...) do { DebugImpl(filename_only(__FILE__), __func__, __LINE__, " " __VA_ARGS__); } while(0)
#define ASSERT(CONDITION) do { if (!(CONDITION)) { AssertImpl(filename_only(__FILE__), __func__, __LINE__, " " #CONDITION); } } while(0)
#define TRACE(...) do { TraceImpl(filename_only(__FILE__), __func__, __LINE__, " " __VA_ARGS__); } while(0)
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


#define BETTERCONSOLE_VERSION "1.4.0"

// --------------------------------------------------------------------
// ---- Change these offsets for each game update                  ----
// --------------------------------------------------------------------
constexpr uint32_t GAME_VERSION = MAKE_VERSION(1, 12, 32);



//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
struct ModMenuSettings {
        uint32_t FontScaleOverride = 0;
};

extern const ModMenuSettings* GetSettings();
extern ModMenuSettings* GetSettingsMutable();
extern char* GetPathInDllDir(char* path_max_buffer, const char* filename);
