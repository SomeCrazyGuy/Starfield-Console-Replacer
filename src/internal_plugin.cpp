#include "main.h"

#include <stdarg.h>
#include <stdio.h>



static const struct simple_draw_t* SimpleDraw = nullptr;
static LogBufferHandle InternalLog = 0;
static const struct log_buffer_api_t* LogBuffer{ GetLogBufferAPI() };

static const char* just_file_name(const char* str) {
        const char* ret = str;
        while (*str) {
                if (*str == '\\') ret = str;
                ++str;
        }
        return ++ret;
}

extern void Log(const char* file, const char* func, const int line, const char* fmt, ...) {
        static char formatbuff[4096];
        auto size = snprintf(formatbuff, sizeof(formatbuff), "%s:%s:%d> ", just_file_name(file), func, line);
        va_list args;
        va_start(args, fmt);
        vsnprintf(&formatbuff[size], sizeof(formatbuff) - size, fmt, args);
        va_end(args);
        if (!InternalLog) {
                InternalLog = LogBuffer->Create("(internal)", nullptr);
        }
        LogBuffer->Append(InternalLog, formatbuff);
}


extern void RegisterInternalPlugin(const BetterAPI* api) {
        if (!InternalLog) {
                InternalLog = LogBuffer->Create("(internal)", nullptr);
        }

        SimpleDraw = api->SimpleDraw;
        static ModInfo callbacks{};
        callbacks.Name = "(internal)";
        callbacks.PluginLog = InternalLog;
        api->Callback->RegisterModInfo(callbacks);
}