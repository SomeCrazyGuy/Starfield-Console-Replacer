#pragma once

#define MODMENU_DEBUG
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN

#include <stdlib.h>
#include <stdint.h>

#define BETTERAPI_ENABLE_SFSE_MINIMAL
#include "../betterapi.h"

#include "debug_log.h"
#include "callback.h"
#include "hook_api.h"
#include "simpledraw.h"
#include "log_buffer.h"
#include "settings.h"

#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#include "../imgui/imgui.h"


#define BETTERCONSOLE_VERSION "1.4.3"
#define COMPATIBLE_GAME_VERSION "1.13.61"

constexpr uint32_t GAME_VERSION = BC_MAKE_VERSION(1, 13, 61);

struct ModMenuSettings {
        uint32_t FontScaleOverride = 0;
};

extern const ModMenuSettings* GetSettings();
extern ModMenuSettings* GetSettingsMutable();
extern char* GetPathInDllDir(char* path_max_buffer, const char* filename);
