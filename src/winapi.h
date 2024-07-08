#pragma once
#include "main.h"
extern const struct windows_api_t* GetWindowsAPI();

//internal api

// Free after use!
extern char* WideStringToUTF8(const wchar_t* wideStr);

// Free after use!
extern wchar_t* UTF8ToWideString(const char* utf8Str);

// Free after use!
extern char* GetLastErrorString();