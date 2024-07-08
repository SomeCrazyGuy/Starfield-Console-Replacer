#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#define NOGDICAPMASKS //- CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES //- VK_ *
#define NOWINMESSAGES //- WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES //- WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS //- SM_ *
#define NOMENUS //- MF_ *
#define NOICONS //- IDI_ *
#define NOKEYSTATES //- MK_ *
#define NOSYSCOMMANDS //- SC_ *
#define NORASTEROPS //- Binary and Tertiary raster ops
//#define NOSHOWWINDOW //- SW_ * (shellexecute, etc)
#define OEMRESOURCE //- OEM Resource values
#define NOATOM //- Atom Manager routines
#define NOCLIPBOARD //- Clipboard routines
#define NOCOLOR //- Screen colors
#define NOCTLMGR //- Control and Dialog routines
#define NODRAWTEXT //- DrawText() and DT_ *
#define NOGDI //- All GDI defines and routines
#define NOKERNEL //- All KERNEL defines and routines
//#define NOUSER //- All USER defines and routines (shellexecute, etc)
//#define NONLS //- All NLS defines and routines (wideCharToMultibyte & friends)
//#define NOMB //- MB_ * and MessageBox()
#define NOMEMMGR //- GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE //- typedef METAFILEPICT
#define NOMINMAX //- Macros min(a, b) and max(a, b)
#define NOMSG //- typedef MSG and associated routines
#define NOOPENFILE //- OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL //- SB_ * and scrolling routines
#define NOSERVICE //- All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND //- Sound driver routines
#define NOTEXTMETRIC //- typedef TEXTMETRIC and associated routines
#define NOWH //- SetWindowsHook and WH_ *
#define NOWINOFFSETS //- GWL_*, GCL_*, associated routines
#define NOCOMM //- COMM driver routines
#define NOKANJI //- Kanji support stuff.
#define NOHELP //- Help engine interface.
#define NOPROFILER //- Profiler interface.
#define NODEFERWINDOWPOS //- DeferWindowPos routines
#define NOMCX //- Modem Configuration Extensions

#include <Windows.h>
#include <shellapi.h>

#include "main.h"

#include "winapi.h"

// -This file wraps the functions from the Windows API and also
//      adjusts the types to match those from the c standard library.
//      static asserts verify that the types match.
// -extra assertions are added to the wrapped functions
//      to help with debugging.
// -functions taking strings (like paths) are assumed to be utf-8
//      and are converted to wide strings.
// -function that fail will log using DEBUG with a
//      message from formatting getlasterror().
// -usage of the functions is traced with DEBUG as well.


static_assert(sizeof(uintptr_t) == sizeof(void*), "uintptr_t != void*");
static_assert(sizeof(DWORD) == sizeof(uint32_t), "DWORD != uint32_t");
static_assert(sizeof(SIZE_T) == sizeof(size_t), "SIZE_T != size_t");
static_assert(sizeof(HMODULE) == sizeof(void*), "HMODULE != void*");


// --ChatGPT--
extern char* WideStringToUTF8(const wchar_t* wideStr) {
        // Check if input wide string is NULL
        if (wideStr == NULL) {
                return NULL;
        }

        // Calculate the length of the UTF-8 string
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, NULL, 0, NULL, NULL);
        if (utf8Len == 0) {
                return NULL; // Conversion failed
        }

        // Allocate memory for the UTF-8 string
        char* utf8Str = (char*)malloc(utf8Len);
        if (utf8Str == NULL) {
                return NULL; // Memory allocation failed
        }

        // Perform the conversion
        int result = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Str, utf8Len, NULL, NULL);
        if (result == 0) {
                free(utf8Str); // Free allocated memory on failure
                return NULL; // Conversion failed
        }

        return utf8Str; // Success
}


// --CHATGPT--
extern wchar_t* UTF8ToWideString(const char* utf8Str) {
        // Check if input UTF-8 string is NULL
        if (utf8Str == NULL) {
                return NULL;
        }

        // Calculate the length of the wide string
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
        if (wideLen == 0) {
                return NULL; // Conversion failed
        }

        // Allocate memory for the wide string
        wchar_t* wideStr = (wchar_t*)malloc(wideLen * sizeof(wchar_t));
        if (wideStr == NULL) {
                return NULL; // Memory allocation failed
        }

        // Perform the conversion
        int result = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wideStr, wideLen);
        if (result == 0) {
                free(wideStr); // Free allocated memory on failure
                return NULL; // Conversion failed
        }

        return wideStr; // Success
}


// --CHATGPT--
extern char* GetLastErrorString() {
        // Retrieve the last error code
        DWORD errorMessageID = GetLastError();
        if (errorMessageID == 0) {
                return NULL; // No error message has been recorded
        }

        // Allocate buffer to hold the error message
        LPWSTR messageBuffer = NULL;

        // Format the message string
        size_t size = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                errorMessageID,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&messageBuffer,
                0,
                NULL
        );

        if (size == 0) {
                return NULL; // Failed to format message
        }

        char* ret = WideStringToUTF8(messageBuffer);
 
        // Clean up
        LocalFree(messageBuffer);

        return ret;
}


static bool wrapped_VirtualProtect(void* address, size_t size, uint32_t new_protect, uint32_t* old_protect) {
        ASSERT(address != NULL);
        ASSERT(size > 0);
        ASSERT(new_protect != 0);
        ASSERT(old_protect != NULL);
        DEBUG("lpAddress: %p, dwSize: %u, flNewProtect: %u, lpflOldProtect = %p", address, size, new_protect, old_protect);
        const bool result = (TRUE == VirtualProtect((LPVOID)address, (SIZE_T)size, (DWORD)new_protect, (PDWORD)old_protect));
        if (!result) {
                const auto message = GetLastErrorString();
                DEBUG("Error: %s", message);
                free(message);
        }
        return result;
}

static void* wrapped_GetModuleHandle(const char* module_name) {
        wchar_t *wide_name = NULL;
        if (module_name) {
                wide_name = UTF8ToWideString(module_name);
                ASSERT(wide_name != NULL);
        }
        DEBUG("lpModuleName: %s", module_name);
        const auto result = GetModuleHandleW(wide_name);
        if (result == NULL) {
                const auto message = GetLastErrorString();
                DEBUG("Error: %s", message);
                free(message);
        }
        free(wide_name);
        return (void*)result;
}


static bool wrapped_ShellExecute(const char* type, const char* resource_path) {
        const auto wide_type = UTF8ToWideString(type);
        const auto wide_path = UTF8ToWideString(resource_path);
        ASSERT(wide_type != NULL);
        ASSERT(wide_path != NULL);
        
        DEBUG("HWND: NULL, lpOperation: %s, lpFile: %s, lpParameters: NULL, lpDirectory: NULL, nShowCmd: SW_SHOWNORMAL", type, resource_path);
        uint32_t result = (uint32_t)ShellExecuteW(NULL, wide_type, wide_path, NULL, NULL, SW_SHOWNORMAL);
        free(wide_type);
        free(wide_path);

        if (result <= 32) {
                const auto message = GetLastErrorString();
                DEBUG("Error: %s", message);
                free(message);
                return false;
        }

        return true;
}


static void wrapped_MessageBox(const char* message, const char* title) {
        const auto wide_title = UTF8ToWideString(title);
        const auto wide_message = UTF8ToWideString(message);
        ASSERT(wide_title != NULL);
        ASSERT(wide_message != NULL);
        DEBUG("hWnd: NULL, lpText: %s, lpCaption: %s, uType: MB_OK", message, title);
        MessageBoxW(NULL, wide_message, wide_title, MB_OK);
        free(wide_title);
        free(wide_message);
}


static bool WaitForDebugger(uint32_t milliseconds) {
        DEBUG("ms: %u", milliseconds);
        bool ret = true;
        while (!IsDebuggerPresent()) {
                Sleep(100);
                if (milliseconds < 100) {
                        ret = false;
                        break;
                }
                milliseconds -= 100;
        }
        DEBUG("Debugger Present: %s", ret ? "true" : "false");
        return ret;
}


static void* wrapped_GetProcAddress(void* module_name, const char* name) {
        ASSERT(module_name != NULL);
        ASSERT(name != NULL);
        DEBUG("hModule: %p, lpProcName: %s", module_name, name);
        const auto result = GetProcAddress((HMODULE)module_name, name);
        if (result == NULL) {
                const auto message = GetLastErrorString();
                DEBUG("Error: %s", message);
                free(message);
        }
        return result;
}


extern const struct windows_api_t* GetWindowsAPI() {
        static const struct windows_api_t WindowsAPI = {
                wrapped_VirtualProtect,
                wrapped_GetModuleHandle,
                wrapped_ShellExecute,
                wrapped_MessageBox,
                wrapped_GetProcAddress,
                WaitForDebugger
        };
        return &WindowsAPI;
}