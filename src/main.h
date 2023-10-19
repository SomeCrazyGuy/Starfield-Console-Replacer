#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#include "../minhook/MinHook.h"

#include "../imgui/imgui.h"

#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"


static inline constexpr const char* GetRelativeProjectDir(const char* file_path) noexcept {
        if (!file_path) return nullptr;
        const char* x = file_path;
        while (*x) ++x;
        while ((x != file_path) && (*x != '\\')) --x;
        ++x;
        return x;
}

// This is defined in internal_plugin.cpp, but needs to be used everywhere
extern void Log(const char* file, const char* func, const int line, const char* fmt, ...);
#define LOG(...) do { Log(GetRelativeProjectDir(__FILE__), __func__, __LINE__, " " __VA_ARGS__); } while(0)


#include "callback.h"
#include "hook_api.h"
#include "simpledraw.h"
#include "log_buffer.h"



// --------------------------------------------------------------------
// ---- Change these offsets for each game update                  ----
// --------------------------------------------------------------------
constexpr uint32_t GAME_VERSION = MAKE_VERSION(1, 7, 36);



//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------

struct ModMenuSettings {
        int HotkeyModifier = 0;
        int ConsoleHotkey = VK_F1;
        int FontScaleOverride = 0;
};

extern const ModMenuSettings* GetSettings();
extern ModMenuSettings* GetSettingsMutable();

