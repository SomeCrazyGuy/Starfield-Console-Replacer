#include "../minhook/hde/hde64.c"
#include "../minhook/buffer.c"
#include "../minhook/trampoline.c"
#include "../minhook/hook.c"

#include "minhook_unity_build.h"

CEXPORT FUNC_PTR minhook_hook_function(FUNC_PTR old_func, FUNC_PTR new_func) {
        static unsigned init = 0;
        if (!init) {
                MH_Initialize();
                init = 1;
        }
        FUNC_PTR ret = NULL;
        if (MH_CreateHook((LPVOID)old_func, (LPVOID)new_func, (LPVOID*)&ret) != MH_OK) {
                return NULL;
        }
        if (MH_EnableHook((LPVOID)old_func) != MH_OK) {
                return NULL;
        };
        return ret;
}