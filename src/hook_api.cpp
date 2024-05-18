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


static void* AOBScanBuffer(const unsigned char* buffer, unsigned buffer_size, const char* signature) {
        constexpr uint16_t sig_end = 0xFFFF;
        const auto siglen = strlen(signature);

        //compile signature
        uint16_t* sig = (uint16_t*)malloc((siglen + 1) * 2);
        ASSERT(sig != NULL);

        static const auto unhex = [](char upper, char lower) -> uint16_t {
                uint16_t ret = 0xFF00;

                upper = (char)::toupper(upper);
                lower = (char)::toupper(lower);

                if (upper == '?') {
                        ret &= 0x0FFF;
                }
                else if ((upper >= '0') && (upper <= '9')) {
                        ret |= (upper - '0') << 4;
                }
                else if ((upper >= 'A') && (upper <= 'F')) {
                        ret |= (10 + (upper - 'A')) << 4;
                }
                else {
                        ASSERT(false && "invalid character in signature");
                }

                if (lower == '?') {
                        ret &= 0xF0FF;
                }
                else if ((lower >= '0') && (lower <= '9')) {
                        ret |= (lower - '0');
                }
                else if ((lower >= 'A') && (lower <= 'F')) {
                        ret |= (10 + (lower - 'A'));
                }
                else {
                        ASSERT(false && "invalid character in signature");
                }

                return ret;
                };

        auto matchcount = 0;
        for (auto i = 0; i < siglen; ) {
                char upper = signature[i++];
                char lower = signature[i++];
                char test = signature[i++];

                ASSERT(matchcount < siglen);
                sig[matchcount++] = unhex(upper, lower);

                if ((test != ' ') && (test != '\0')) {
                        DEBUG("signature-bad format: %s", &signature[i - 3]);
                        free(sig);
                        return NULL;
                }
        }

        sig[matchcount] = sig_end;

        for (unsigned i = 0; i < buffer_size; ++i) {
                auto match = 0;

                while ((buffer[i] & (sig[match] >> 8)) == (sig[match] & 0xFF)) {
                        ++i;
                        ++match;
                        if (sig[match] == sig_end) {
                                free(sig);
                                return Relocate(i - match);
                        }
                }
        }

        free(sig);
        return NULL;
}


static void* AOBScanEXE(const char* signature) {
        const auto hdr1 = (const IMAGE_DOS_HEADER*)Relocate(0);
        const auto hdr2 = (const IMAGE_NT_HEADERS64*)Relocate(hdr1->e_lfanew);
        const unsigned char* haystack = (const unsigned char*)hdr1;
        const unsigned count = hdr2->OptionalHeader.SizeOfImage;
        return AOBScanBuffer(haystack, count, signature);
}


static unsigned GetExeVersion() {
        const auto hdr1 = (const IMAGE_DOS_HEADER*)Relocate(0);
        const auto hdr2 = (const IMAGE_NT_HEADERS64*)Relocate(hdr1->e_lfanew);
        const auto sections = (const IMAGE_SECTION_HEADER*)Relocate(hdr1->e_lfanew + sizeof(IMAGE_NT_HEADERS64));
        const auto num_sections = hdr2->FileHeader.NumberOfSections;

        const unsigned char* rsrc = nullptr;
        uint32_t rsrc_size = 0;
        for (auto i = 0; i < num_sections; ++i) {
                if (*(const uint32_t*)sections[i].Name == *(const uint32_t*)".rsrc") {
                        rsrc = (const unsigned char*)Relocate(sections[i].VirtualAddress);
                        rsrc_size = sections[i].SizeOfRawData;
                }
        }

        if (!rsrc_size) {
                return 0;
        }
        
        auto ver = (const unsigned char*)AOBScanBuffer(rsrc, rsrc_size, "50 00 72 00 6f 00 64 00 75 00 63 00 74 00 56 00 65 00 72 00 73 00 69 00 6f 00 6e 00");
        
        if (!ver) {
                return 0;
        }

        ver += 30;

        unsigned major;
        unsigned minor;
        unsigned patch;
        unsigned revision;
        swscanf_s((const wchar_t*)ver, L"%u.%u.%u.%u", &major, &minor, &patch, &revision);

        return MAKE_VERSION(major, minor, patch);
}       


static constexpr struct hook_api_t HookAPI {
        &HookFunction,
        &HookVirtualTable,
        &Relocate,
        &SafeWriteMemory,
        &GetProcAddressFromIAT,
        &HookFunctionIAT,
        &AOBScanEXE,
        &GetExeVersion
};

extern constexpr const struct hook_api_t* GetHookAPI() {
        return &HookAPI;
}