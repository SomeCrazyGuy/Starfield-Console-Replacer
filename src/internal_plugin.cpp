#include "internal_plugin.h"

#include "log_buffer.h"

#include <stdarg.h>
#include <stdio.h>


static const struct simple_draw_t* SimpleDraw = nullptr;
static LogBufferHandle InternalLog = 0;



extern void Log(const char* file, const char* func, const int line, const char* fmt, ...) {
        static const struct log_buffer_api_t* LogBuffer = nullptr;
        
        static char formatbuff[4096];
        
        if (LogBuffer == nullptr) {
                LogBuffer = GetLogBufferAPI();
                InternalLog = LogBuffer->Create("(internal)", NULL);
        }
        
        auto size = snprintf(formatbuff, sizeof(formatbuff), "%s:%s:%d> ", file, func, line);

        va_list args;
        va_start(args, fmt);
        vsnprintf(&formatbuff[size], sizeof(formatbuff) - size, fmt, args);
        va_end(args);

        LogBuffer->Append(InternalLog, formatbuff);
}


static void DrawLogTab(void* imgui_context) {
        (void)imgui_context;

        SimpleDraw->ShowLogBuffer(InternalLog, false);
}



extern void RegisterInternalPlugin(const BetterAPI* api) {
        (void)api;
        static DrawCallbacks callbacks{};
        callbacks.Name = "(internal)";
        callbacks.DrawLog = DrawLogTab;
        api->Callback->RegisterDrawCallbacks(&callbacks);
        SimpleDraw = api->SimpleDraw;
}