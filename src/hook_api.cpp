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


template<typename T>
static inline void VolatileWrite(void* const dest, const void* const src) noexcept {
        ASSERT((sizeof(T) <= 8) && "x86_64 limits atomic operations to 1, 2, 4, or 8 bytes");
        ASSERT((0 == (((*(uintptr_t*)dest)) & ((uintptr_t)(sizeof(T) | (sizeof(T) - 1))))) && "Unaligned write to atomic memory is not atomic!!!");
        volatile T* const Dest = (volatile T* const)dest;
        const T Src = *(const T* const)src;
        *Dest = Src;
}


static boolean SafeWriteMemory(void* const dest, const void* const src, const unsigned length) {
        DWORD oldprot, unusedprot;
        if (VirtualProtect(dest, length, PAGE_EXECUTE_READWRITE, &oldprot) == FALSE) {
                return 0;
        }

        //perform atomic operations for small writes up to sizeof(void*)
        //x86_64 can have strong atomic guarantees for these sizes
        switch (length) {
        case 1:
                VolatileWrite<uint8_t>(dest, src);
                break;
        case 2:
                VolatileWrite<uint16_t>(dest, src);
                break;
        case 4:
                VolatileWrite<uint32_t>(dest, src);
                break;
        case 8:
                VolatileWrite<uint64_t>(dest, src);
                break;
        default:
                memcpy(dest, src, length);
                break;
        }
        //lol at compiler warning coverting between BOOL and boolean, 2 interfaces unnecessary because bool exists
        //now i feel like im programming in javascript with the dumb "!!" trick
        return !!VirtualProtect(dest, length, oldprot, &unusedprot);
}


static FUNC_PTR HookVirtualTable(void* class_instance, unsigned method_index, FUNC_PTR new_func) {
        struct class_instance_2 { FUNC_PTR* vtable; };
        auto vtable = ((class_instance_2*)(class_instance))->vtable;
        auto ret = vtable[method_index];
        SafeWriteMemory(&vtable[method_index], &new_func, 8);
        return ret;
}


static void* Relocate(unsigned offset) {
        static uintptr_t imagebase = 0;
        if (!imagebase) {
                imagebase = (uintptr_t)GetModuleHandleW(NULL);
        }
        return (void*)(imagebase + offset);
}

template<typename T>
static inline T RVA(uintptr_t offset) {
	return (T)Relocate((unsigned int)offset);
}


typedef FUNC_PTR* IATEntry;

/// <summary>
/// Search the Import Address Table of the exe (starfield) for the matching function.
/// dll_name can be null to search all dlls in the iat
/// returns null if not found or the address of the iat entry on success
/// </summary>
/// <param name="dll_name">the dll name (not case sensitive) or null to search all dlls</param>
/// <param name="func_name">the name of the dll function (same as getprocaddress would use)</param>
/// <returns>iatentry* on success, null on failure</returns>
static IATEntry SearchIAT(const char* dll_name, const char* func_name) {
	auto dosHeaders = RVA<IMAGE_DOS_HEADER*>(0);
	auto ntHeaders = RVA<const IMAGE_NT_HEADERS64*>(dosHeaders->e_lfanew);
	auto importsDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	auto imports = RVA<const IMAGE_IMPORT_DESCRIPTOR*>(importsDirectory.VirtualAddress);
        IATEntry ret = nullptr;
	
        for (uint64_t i = 0; imports[i].Characteristics; ++i) {
		auto dname = RVA<const char*>(imports[i].Name);
		if (dll_name && _stricmp(dname, dll_name) != 0) continue;

		auto names = RVA<const IMAGE_THUNK_DATA64*>(imports[i].OriginalFirstThunk);
		auto thunks = RVA<IMAGE_THUNK_DATA64*>(imports[i].FirstThunk);
		for (uint64_t j = 0; thunks[j].u1.AddressOfData; ++j) {
			auto fname = RVA<const IMAGE_IMPORT_BY_NAME*>(names[j].u1.AddressOfData)->Name;

			if (_stricmp(fname, func_name) == 0) {
				ret = (IATEntry)&thunks[j].u1.AddressOfData;
				goto END;
			}
		}
	}

END:
	return ret;
}


static FUNC_PTR GetProcAddressFromIAT(const char* dll_name, const char* func_name) {
	auto entry = SearchIAT(dll_name, func_name);
	if (entry) return *entry;
	return NULL;
}


static FUNC_PTR HookFunctionIAT(const char* dll_name, const char* func_name, const FUNC_PTR new_function) {
	auto entry = SearchIAT(dll_name, func_name);
	FUNC_PTR ret = nullptr;
	if (entry) {
                ret = *entry;
                SafeWriteMemory(entry, &new_function, 8);
	}
	return ret;
}

static constexpr struct hook_api_t HookAPI {
        &HookFunction,
        &HookVirtualTable,
        &Relocate,
        &SafeWriteMemory,
        &GetProcAddressFromIAT,
        &HookFunctionIAT
};

extern constexpr const struct hook_api_t* GetHookAPI() {
        return &HookAPI;
}