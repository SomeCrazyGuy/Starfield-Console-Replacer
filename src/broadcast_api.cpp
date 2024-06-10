#include "main.h"

#include <Windows.h>
#include <Psapi.h>

extern void BroadcastBetterAPIMessage(const struct better_api_t *API) {
        DWORD needed = 0;
        EnumProcessModules(GetCurrentProcess(), NULL, 0, &needed);

        DWORD size = (needed | 127) + 1;
        HMODULE* handles = (HMODULE*)malloc(size);
        ASSERT(handles != NULL);

        EnumProcessModules(GetCurrentProcess(), handles, size, &needed);

        auto count = needed / sizeof(*handles);

        for (unsigned i = 0; i < count; ++i) {
                if (!handles[i]) {
                        continue;
                }
                
                char path[MAX_PATH];
                GetModuleFileNameA(handles[i], path, MAX_PATH);

                //skip all modules in the windows directory
                if (_strnicmp(path, "C:\\WINDOWS\\", sizeof("C:\\WINDOWS\\") - 1) == 0) {
                        continue;
                }

                const auto BetterAPICallback = (void(*)(const struct better_api_t*)) GetProcAddress(handles[i], "BetterConsoleReceiver");
                
                if (BetterAPICallback) {
                        DEBUG("Sending API pointer to module: %s", path);
                        BetterAPICallback(API);
                }
        }

        free(handles);
}