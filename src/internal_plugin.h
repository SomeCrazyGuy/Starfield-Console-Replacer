#pragma once

#include "../betterapi.h"


//logging system
extern void Log(const char* file, const char* func, const int line, const char* fmt, ...);

extern void RegisterInternalPlugin(const BetterAPI* api);