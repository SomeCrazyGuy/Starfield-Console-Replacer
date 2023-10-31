#pragma once

#define MODMENU_DEBUG

#ifndef MODMENU_DEBUG
#ifdef _DEBUG
#define MODMENU_DEBUG
#endif // _DEBUG
#endif // !MODMENU_DEBUG

#ifdef MODMENU_DEBUG
extern void DebugImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept;
extern void AssertImpl [[noreturn]] (const char* const filename, const char* const func, int line, const char* const text) noexcept;
inline constexpr auto file_name_only(const char* const in) noexcept -> const char* const {
        const char* i = in;
        const char* ret = in;
        while (*i) {
                if (*i == '\\') ret = i;
                ++i;
        }
        return ++ret;
}
#define DEBUG(...) do { DebugImpl(file_name_only(__FILE__), __func__, __LINE__, " " __VA_ARGS__); } while(0)
#define ASSERT(CONDITION) do { if (!(CONDITION)) { AssertImpl(file_name_only(__FILE__), __func__, __LINE__, " " #CONDITION); } } while(0)
#define IMGUI_DEBUG_PARANOID
#else
#define DEBUG(...) do { } while(0)
#define ASSERT(...) do { } while(0)
#endif // MODMENU_DEBUG


#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN
#define STRICT


#define BETTERAPI_ENABLE_STD
#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"


#include "callback.h"
#include "hook_api.h"
#include "simpledraw.h"
#include "log_buffer.h"

#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#include "../imgui/imgui.h"


// --------------------------------------------------------------------
// ---- Change these offsets for each game update                  ----
// --------------------------------------------------------------------
constexpr uint32_t GAME_VERSION = MAKE_VERSION(1, 7, 36);



//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
struct ModMenuSettings {
        int HotkeyModifier = 0;
        int ConsoleHotkey = 112; //VK_F1
        int FontScaleOverride = 0;
};

extern const ModMenuSettings* GetSettings();
extern ModMenuSettings* GetSettingsMutable();

