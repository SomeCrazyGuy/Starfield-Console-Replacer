#pragma once

#include "main.h"

//this should move elsewhere
extern const struct log_api_t* GetLogAPI();

extern void draw_console_window();

//this is hooked in main.cpp
extern void console_print(void* consolemgr, const char* fmt, va_list args);