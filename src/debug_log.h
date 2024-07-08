#pragma once

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
#define TRACE(...) do {} while (0)
#endif // MODMENU_DEBUG
