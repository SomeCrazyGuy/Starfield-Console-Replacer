#include "main.h"

#include "../minhook/MinHook.h"


static FUNC_PTR HookFunction(FUNC_PTR old, FUNC_PTR new_func) {
        //todo: check retval x3, ret!=NULL
        static bool init = false;
        if (!init) {
                MH_Initialize(); 
                init = true;
        }
        FUNC_PTR ret = nullptr;
        MH_CreateHook(old, new_func, (LPVOID*) &ret);
        MH_EnableHook(old);
        return ret;
}


static FUNC_PTR HookVirtualTable(void* class_instance, unsigned method_index, FUNC_PTR new_func) {
        struct class_instance_2 { FUNC_PTR* vtable; };
        auto vtable = ((class_instance_2*)(class_instance))->vtable;
        auto ret = vtable[method_index];
        DWORD perm;
        uintptr_t fun = (uintptr_t)&vtable[method_index];
        VirtualProtect((void*)fun, sizeof(void*), PAGE_EXECUTE_READWRITE, &perm);
        vtable[method_index] = new_func;
        DWORD perm2;
        VirtualProtect((void*)fun, sizeof(void*), perm, &perm2);
        return ret;
}


static void* Relocate(unsigned offset) {
        static uintptr_t imagebase = 0;
        if (!imagebase) {
                imagebase = (uintptr_t)GetModuleHandleW(NULL);
        }
        return (void*)(imagebase + offset);
}


static boolean WriteMemory(void* dest, const void* src, unsigned length) {
        static HANDLE this_process = nullptr;
        if (!this_process) {
                this_process = GetCurrentProcess();
        }
        SIZE_T bytes_written = 0;
        auto ret = WriteProcessMemory(this_process, dest, src, length, &bytes_written);
        return (ret && (bytes_written == length));
}

static constexpr struct hook_api_t HookAPI {
        &HookFunction,
        &HookVirtualTable,
        &Relocate,
        &WriteMemory
};

extern constexpr const struct hook_api_t* GetHookAPI() {
        return &HookAPI;
}