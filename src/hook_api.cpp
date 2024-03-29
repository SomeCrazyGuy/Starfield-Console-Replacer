#include "main.h"

#include "../minhook/MinHook.h"

static FUNC_PTR HookFunction(FUNC_PTR old, FUNC_PTR new_func) {
        ASSERT(old != NULL);
        ASSERT(new_func != NULL);
        DEBUG("OLD: '%p', NEW: '%p'", old, new_func);
        static bool init = false;
        if (!init) {
                if (MH_Initialize() != MH_OK) {
                        ASSERT(false && "minhook failed to initialize");
                }
                init = true;
        }
        FUNC_PTR ret = nullptr;
        if (MH_CreateHook(old, new_func, (LPVOID*)&ret) != MH_OK) {
                ASSERT(false && "minhook failed to hook function");
                return NULL;
        }
        if (MH_EnableHook(old) != MH_OK) {
                ASSERT(false && "minhook failed to enable hook");
                return NULL;
        };
        ASSERT(ret != NULL);
        return ret;
}


template<typename T>
static inline void VolatileWrite(void* const dest, const void* const src) noexcept {
        ASSERT(dest != NULL);
        ASSERT(src != NULL);
        ASSERT((sizeof(T) <= 8) && "x86_64 limits atomic operations to 1, 2, 4, or 8 bytes");
        const auto is_write_unaligned = ((uintptr_t)dest) & (sizeof(T) - 1);
        if (is_write_unaligned) {
                DEBUG("ERROR: write %u bytes to address %p is not atomic", sizeof(T), dest);
                ASSERT(false && "Unaligned write to atomic memory is not atomic!!!");
        }
        volatile T* const Dest = (volatile T* const)dest;
        T Src;
        memcpy(&Src, src, sizeof(Src));
        *Dest = Src;
}


static boolean SafeWriteMemory(void* const dest, const void* const src, const unsigned length) {
        ASSERT(dest != NULL);
        ASSERT(src != NULL);
        ASSERT(length > 0);
        DWORD oldprot, unusedprot;
        if (VirtualProtect(dest, length, PAGE_EXECUTE_READWRITE, &oldprot) == FALSE) {
                ASSERT(false && "virtualprotect failed");
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
        //now i feel like im programming in javascript with the dumb "!!" trick but i brought this on myself
        return !!VirtualProtect(dest, length, oldprot, &unusedprot);
}


static FUNC_PTR HookVirtualTable(void* class_instance, unsigned method_index, FUNC_PTR new_func) {
        ASSERT(class_instance != NULL);
        struct class_instance_2 { FUNC_PTR* vtable; };
        auto vtable = ((class_instance_2*)(class_instance))->vtable;
        ASSERT(vtable != NULL);
        auto ret = vtable[method_index];
        ASSERT(ret != NULL);
        if (!SafeWriteMemory(&vtable[method_index], &new_func, 8)) {
                ASSERT(false && "safewrite failed");
        }
        return ret;
}


static void* Relocate(unsigned offset) {
        static uintptr_t imagebase = 0;
        if (!imagebase) {
                imagebase = (uintptr_t)GetModuleHandleW(NULL);
        }
        ASSERT(imagebase != 0);
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
        ASSERT(func_name != NULL);
        ASSERT(*func_name != '\0');
	auto dosHeaders = RVA<IMAGE_DOS_HEADER*>(0);
	auto ntHeaders = RVA<const IMAGE_NT_HEADERS64*>(dosHeaders->e_lfanew);
	auto importsDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	auto imports = RVA<const IMAGE_IMPORT_DESCRIPTOR*>(importsDirectory.VirtualAddress);
	
        for (uint64_t i = 0; imports[i].Characteristics; ++i) {
		auto dname = RVA<const char*>(imports[i].Name);
		if (dll_name && _stricmp(dname, dll_name) != 0) continue;

		auto names = RVA<const IMAGE_THUNK_DATA64*>(imports[i].OriginalFirstThunk);
		auto thunks = RVA<IMAGE_THUNK_DATA64*>(imports[i].FirstThunk);
		for (uint64_t j = 0; thunks[j].u1.AddressOfData; ++j) {
			auto fname = RVA<const IMAGE_IMPORT_BY_NAME*>(names[j].u1.AddressOfData)->Name;

			if (_stricmp(fname, func_name) == 0) {
				return (IATEntry)&thunks[j].u1.AddressOfData;
			}
		}
	}

	return nullptr;
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