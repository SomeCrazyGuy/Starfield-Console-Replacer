#pragma once

#include "../betterapi.h"


//logging system
extern void Log(const char* file, const char* func, const int line, const char* fmt, ...);
#define LOG(...) do { Log(__FILE__, __func__, __LINE__, " " __VA_ARGS__); } while(0);


extern void RegisterInternalPlugin(const BetterAPI* api);