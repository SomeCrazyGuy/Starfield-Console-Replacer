#include "main.h"

#include <Windows.h>

#include <mutex>

std::mutex logging_mutex;
static thread_local char format_buffer[4096];
static constexpr auto buffer_size = sizeof(format_buffer);


static void write_log(const char* const str) noexcept {
        static HANDLE debugfile = INVALID_HANDLE_VALUE;
        logging_mutex.lock();
        if (debugfile == INVALID_HANDLE_VALUE) {
                char path[MAX_PATH];
                debugfile = CreateFileA(GetPathInDllDir(path, "BetterConsoleLog.txt"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (debugfile != INVALID_HANDLE_VALUE) {
                WriteFile(debugfile, str, (DWORD)strnlen(str, 4096), NULL, NULL);
                FlushFileBuffers(debugfile);
        }
        else {
                MessageBoxA(NULL, "Could not write to 'BetterConsoleLog.txt'", "ASSERTION FAILURE", 0);
                abort();
        }
        logging_mutex.unlock();
}


extern void DebugImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept {
        auto bytes = snprintf(format_buffer, buffer_size, "%s:%s:%d>", filename, func, line);
        ASSERT(bytes > 0);
        ASSERT(bytes < buffer_size);

        va_list args;
        va_start(args, fmt);
        bytes += vsnprintf(&format_buffer[bytes], buffer_size - bytes, fmt, args);
        va_end(args);
        ASSERT(bytes > 0);
        ASSERT(bytes < buffer_size);

        format_buffer[bytes++] = '\n';
        format_buffer[bytes] = '\0';
        ASSERT(bytes < buffer_size);

        write_log(format_buffer);
}


extern void AssertImpl [[noreturn]] (const char* const filename, const char* const func, int line, const char* const text) noexcept {
        snprintf(
                format_buffer,
                buffer_size,
                "In file     '%s'\n"
                "In function '%s'\n"
                "On line     '%d'\n"
                "Message:    '%s'",
                filename,
                func,
                line,
                text
        );
        write_log("!!! ASSERTION FAILURE !!!\n");
        write_log(format_buffer);
        MessageBoxA(NULL, format_buffer, "BetterConsole Crashed!", 0);
        abort();
}


extern void TraceImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept {
        static bool init = false;
        static char tracebuff[2048];
        const auto bytes = snprintf(tracebuff, sizeof(tracebuff), "%s:%s:%d> ", filename, func, line);
        ASSERT(bytes > 0 && "trace buffer too small ?");
        va_list args;
        va_start(args, fmt);
        const auto bytes2 = vsnprintf(tracebuff + bytes, sizeof(tracebuff) - bytes, fmt, args);
        ASSERT(bytes2 > 0 && "trace buffer too small ?");
        tracebuff[bytes + bytes2] = '\r';
        tracebuff[bytes + bytes2 + 1] = '\n';
        tracebuff[bytes + bytes2 + 2] = '\0';
        va_end(args);
        if (!init) {
                AllocConsole();
                FILE* file = nullptr;
                freopen_s(&file, "CONIN$", "rb", stdin);
                freopen_s(&file, "CONOUT$", "wb", stdout);
                freopen_s(&file, "CONOUT$", "wb", stderr);
        }
        fputs(tracebuff, stdout);
}
