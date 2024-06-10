#include "main.h"

#include "../imgui/imgui_impl_win32.h"

#include <dxgi1_6.h>
#include <d3d12.h>
#include <intrin.h>

#include "gui.h"
#include "console.h"
#include "broadcast_api.h"
#include "std_api.h"
#include "hotkeys.h"

#include "d3d11on12ui.h"

#define BETTERAPI_IMPLEMENTATION
#include "../betterapi.h"


extern "C" __declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
        1, // SFSE api version
        1, // Plugin version
        "BetterConsole",
        "Linuxversion",
        1, // AddressIndependence::Signatures
        1, // StructureIndependence::NoStructs
        {GAME_VERSION, 0}, 
        0, //does not rely on any sfse version
        0, 0 //reserved fields
};

static void OnHotheyActivate(uintptr_t);
static void SetupModMenu();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags);
static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static decltype(FAKE_Present)* OLD_Present = nullptr;
static decltype(FAKE_Wndproc)* OLD_Wndproc = nullptr;


static HINSTANCE self_module_handle = nullptr;
static bool should_show_ui = false;
static bool betterapi_load_selftest = false;

#define EveryNFrames(N) []()->bool{static unsigned count=0;if(++count==(N)){count=0;}return !count;}()


static char DLL_DIR[MAX_PATH]{};

extern char* GetPathInDllDir(char* path_max_buffer, const char* filename) {
        ASSERT(DLL_DIR[0] != '\0' && "a file was opened before dll dir was setup");

        char* p = path_max_buffer;
        
        unsigned i, j;

        for (i = 0; DLL_DIR[i]; ++i) {
                *p++ = DLL_DIR[i];
        }

        for (j = 0; filename[j]; ++j) {
                *p++ = filename[j];
        }

        *p = 0;

        return path_max_buffer;
}


const char* const filename_only(const char* path) {
        const char* p = path;
        while (*path) {
                if (*path == '\\') p = path;
                ++path;
        }
        return ++p;
}


#ifdef MODMENU_DEBUG
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
        auto bytes = snprintf(format_buffer, buffer_size, "%s:%s:%d>", filename_only(filename), func, line);
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
                filename_only(filename),
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
        const auto bytes = snprintf(tracebuff, sizeof(tracebuff), "%s:%s:%d> ", filename_only(filename), func, line);
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
#endif


//this is where the api* that all clients use comes from
static const BetterAPI API {
        GetHookAPI(),
        GetLogBufferAPI(),
        GetSimpleDrawAPI(),
        GetCallbackAPI(),
        GetConfigAPI(),
        GetStdAPI(),
        GetConsoleAPI()
};


extern "C" __declspec(dllexport) const BetterAPI * GetBetterAPI() {
        return &API;
}


extern ModMenuSettings* GetSettingsMutable() {
        static ModMenuSettings Settings{};
        return &Settings;
}


extern const ModMenuSettings* GetSettings() {
        return GetSettingsMutable();
}


#define MAX_QUEUES 4
struct SwapChainQueue {
        ID3D12CommandQueue* Queue;
        IDXGISwapChain* SwapChain;
        uint64_t Age;
} static Queues[MAX_QUEUES];


static HRESULT(*OLD_CreateSwapChainForHwnd)(
        IDXGIFactory2* This,
        ID3D12Device* Device,
        HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1* pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
        IDXGIOutput* pRestrictToOutput,
        IDXGISwapChain1** ppSwapChain
        ) = nullptr;

static HRESULT FAKE_CreateSwapChainForHwnd(
        IDXGIFactory2* This,
        ID3D12Device* Device,
        HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1* pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
        IDXGIOutput* pRestrictToOutput,
        IDXGISwapChain1** ppSwapChain
) {
        auto ret = OLD_CreateSwapChainForHwnd(This, Device, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
        DEBUG("Factory: %p, Device: %p, HWND: %p, SwapChain: %p, Owner: %p, Ret: %X", This, Device, hWnd, *ppSwapChain, ppSwapChain, ret);

        if (ret != S_OK) return ret;

        ASSERT(ppSwapChain != NULL);

        bool swapchain_inserted = false;
        for (unsigned i = 0; i < MAX_QUEUES; ++i) {
                //the device parameter of this function is actually a commandqueue in dx12
                if (Queues[i].Queue == (ID3D12CommandQueue*)Device) {
                        //if the device is already in the queue, just update the swapchain parameter
                        Queues[i].SwapChain = *ppSwapChain;
                        DEBUG("Reuse Queues[%u] '%p' Age: %llu, for new swapchain '%p'", i, Device, Queues[i].Age, *ppSwapChain);
                        Queues[i].Age = 0;
                        swapchain_inserted = true;
                        break;
                } else if (Queues[i].Queue == nullptr) {
                        //otherwise fill empty queue
                        Queues[i].Queue = (ID3D12CommandQueue*)Device;
                        Queues[i].SwapChain = *ppSwapChain;
                        Queues[i].Age = 0;
                        DEBUG("Add Device '%p' to Queues[%u] with swapchain '%p'", Device, i, *ppSwapChain);
                        swapchain_inserted = true;
                        break;
                }
                else {
                        DEBUG("Queues[%u] possibly unused commandqueue: '%p'", i, Queues[i].Queue);
                        Queues[i].Age++;
                }
        }

        //ok, try removing the oldest one
        if (!swapchain_inserted) {
                unsigned highest = 0;

                for (unsigned i = 0; i < MAX_QUEUES; ++i) {
                        if (Queues[i].Age > Queues[highest].Age) {
                                highest = i;
                        }
                }

                Queues[highest].Age = 0;
                Queues[highest].Queue = (ID3D12CommandQueue*)Device;
                Queues[highest].SwapChain = *ppSwapChain;
                DEBUG("Queues[%u] was overwritten", highest);
        }

        
        auto proc = (decltype(OLD_Wndproc))GetWindowLongPtrW(hWnd, GWLP_WNDPROC);
        if ((uint64_t)FAKE_Wndproc != (uint64_t)proc) {
                OLD_Wndproc = proc;
                SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)FAKE_Wndproc);
                DEBUG("Input Hook: OLD_Wndproc: %p, Current_Wndproc: %p, NEW_Wndproc: %p", OLD_Wndproc, proc, FAKE_Wndproc);
        }
        else {
                DEBUG("WndProc already hooked");
        }

        enum : unsigned {
                QueryInterface,
                AddRef,
                Release,
                GetPrivateData,
                SetPrivateData,
                SetPrivateDataInterface,
                GetParent,
                GetDevice,
                Present,
        };
        
        
        static bool once = false;
        if (!once) {
                OLD_Present = (decltype(OLD_Present))API.Hook->HookVirtualTable(
                        *ppSwapChain,
                        Present,
                        (FUNC_PTR)FAKE_Present
                );
                DEBUG("Hooked Present, OLD_Present: %p, NEW_Present: %p", OLD_Present, FAKE_Present);
                once = true;
        }
        
        return ret;
}


static HRESULT(*OLD_CreateDXGIFactory2)(UINT, REFIID, void**) = nullptr;
static HRESULT FAKE_CreateDXGIFactory2(UINT Flags, REFIID RefID, void **ppFactory) {
        auto ret = OLD_CreateDXGIFactory2(Flags, RefID, ppFactory);

        static bool once = 1;
        if (once) {
                once = 0;

                // now that we are out of dllmain, complete the initialization of betterconsole
                SetupModMenu();

                enum {
                        QueryInterface,
                        AddRef,
                        Release,
                        SetPrivateData,
                        SetPrivateDataInterface,
                        GetPrivateData,
                        GetParent,
                        EnumAdapters,
                        MakeWindowAssociation,
                        GetWindowAssociation,
                        CreateSwapChain,
                        CreateSoftwareAdapter,
                        EnumAdapters1,
                        IsCurrent,
                        IsWindowedStereoEnabled,
                        CreateSwapChainForHwnd
                };

                // Using a stronger hook here because otherwise the steam overlay will not function
                // not sure why a vmt hook doesnt work here, steam checks and rejects?
                FUNC_PTR* fp = *(FUNC_PTR**)*ppFactory;
                DEBUG("HookFunction: CreateSwapChainForHwnd");
                OLD_CreateSwapChainForHwnd = (decltype(OLD_CreateSwapChainForHwnd))API.Hook->HookFunction(fp[CreateSwapChainForHwnd], (FUNC_PTR)FAKE_CreateSwapChainForHwnd);
        }

        return ret;
}


static void Callback_Config(ConfigAction action) {
        auto c = GetConfigAPI();
        auto s = GetSettingsMutable();
        ConfigSetMod("(internal)");
        c->ConfigU32(action, "FontScaleOverride", &s->FontScaleOverride);
        if (action == ConfigAction_Write) {
                HotkeySaveSettings();
        }
}


static void SetupModMenu() {
        // use the directory of the betterconsole dll as the place to put other files
        // NOTE: this needs to be dome before any other file (logfile/config/conaole history) is opened
        GetModuleFileNameA(self_module_handle, DLL_DIR, MAX_PATH);
        char* n = DLL_DIR;
        while (*n) ++n;
        while ((n != DLL_DIR) && (*n != '\\')) --n;
        ++n;
        *n = 0;

        DEBUG("Initializing BetterConsole...");
        DEBUG("BetterConsole Version: " BETTERCONSOLE_VERSION);
        static UINT(*OLD_GetRawInputData)(HRAWINPUT hri, UINT cmd, LPVOID data, PUINT data_size, UINT hsize) = nullptr;
        static decltype(OLD_GetRawInputData) FAKE_GetRawInputData = [](HRAWINPUT hri, UINT cmd, LPVOID data, PUINT data_size, UINT hsize) -> UINT {
                auto ret = OLD_GetRawInputData(hri, cmd, data, data_size, hsize);

                if ((should_show_ui == false) || (data == NULL)) return ret;

                //hide input from the game when shouldshowui is true and data is not null
                auto input = (RAWINPUT*)data;
                input->header.dwType = RIM_TYPEHID; //game ignores typehid messages
                return ret;
        };
        OLD_GetRawInputData = (decltype(OLD_GetRawInputData)) API.Hook->HookFunctionIAT("user32.dll", "GetRawInputData", (FUNC_PTR)FAKE_GetRawInputData);
        DEBUG("Hook GetRawInputData: %p", OLD_GetRawInputData);

        //i would prefer not hooking multiple win32 apis but its more update-proof than engaging with the game's wndproc
        static BOOL(*OLD_ClipCursor)(const RECT*) = nullptr;
        static decltype(OLD_ClipCursor) FAKE_ClipCursor = [](const RECT* rect) -> BOOL {
                // When the imgui window is open only pass through clipcursor(NULL);
                return OLD_ClipCursor((should_show_ui) ? NULL : rect);
        };
        OLD_ClipCursor = (decltype(OLD_ClipCursor)) API.Hook->HookFunctionIAT("user32.dll", "ClipCursor", (FUNC_PTR)FAKE_ClipCursor);
        DEBUG("Hook ClipCursor: %p", OLD_ClipCursor);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = true;
        ImGui::StyleColorsDark();
        DEBUG("ImGui one time init completed!");

        // The console part of better console is now minimally coupled to the mod menu
        DEBUG("Console setup - crashing here is AOB issue");
        setup_console(&API);

        // Gather all my friends!
        BroadcastBetterAPIMessage(&API);
        ASSERT(betterapi_load_selftest == true);

        // Load any settings from the config file and call any config callbacks
        LoadSettingsRegistry();
}

extern "C" __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface*) {}

extern "C" __declspec(dllexport) void BetterConsoleReceiver(const struct better_api_t* api) {
        BETTERAPI_INIT(api);
        const auto handle = api->Callback->RegisterMod("(internal)", BETTERAPI_VERSION);
        api->Callback->RegisterConfigCallback(handle, Callback_Config);
        api->Callback->RegisterHotkeyCallback(handle, OnHotheyActivate);
        
        //force hotkey for betterconsole default action using internal api
        HotkeyRequestNewHotkey(handle, "BetterConsole", 0, VK_F1);
        
        betterapi_load_selftest = true;
        DEBUG("Self Test Complete");
}

extern "C" BOOL WINAPI DllMain(HINSTANCE self, DWORD fdwReason, LPVOID) {
        if (fdwReason == DLL_PROCESS_ATTACH) {
                /* lock the linker/dll loader until hooks are installed, TODO: make sure this code path is fast */
                static bool RunHooksOnlyOnce = true;
                ASSERT(RunHooksOnlyOnce == true); //i want to know if this assert ever gets triggered

#ifdef _DEBUG
                //while (!IsDebuggerPresent()) Sleep(100);
#endif // _DEBUG

                self_module_handle = self;

                // just hook this one function the game needs to display graphics, then lazy hook the rest when it's called later
                OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2))API.Hook->HookFunctionIAT("sl.interposer.dll", "CreateDXGIFactory2", (FUNC_PTR)FAKE_CreateDXGIFactory2);
                if (!OLD_CreateDXGIFactory2) {
                        OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2))API.Hook->HookFunctionIAT("dxgi.dll", "CreateDXGIFactory2", (FUNC_PTR)FAKE_CreateDXGIFactory2);
                }
                ASSERT(OLD_CreateDXGIFactory2 != NULL);

                RunHooksOnlyOnce = false;
        }
        return TRUE;
}


static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags) {
        if (EveryNFrames(240)) {
                DEBUG("render heartbeat, showing ui: %s", (should_show_ui)? "true" : "false");
        }

        static bool hooked = false;
        static IDXGISwapChain3* last_swapchain = nullptr;
        static ID3D12CommandQueue* comand_queue = nullptr;

        if (last_swapchain != This) {
                last_swapchain = This;
                
                for (auto i : Queues) {
                        DEBUG("Searching for CommandQueue for swapchain %p: [ chain: %p, queue: %p, age: %u ]", This, i.SwapChain, i.Queue, i.Age);
                        if (i.SwapChain == This) {
                                comand_queue = i.Queue;
                                DEBUG("Commandqueue found!");
                                break;
                        }
                }
        }
        
        if (should_show_ui) {
                if (!hooked) {
                        UI_Initialize(This, comand_queue);
                        hooked = true;
                }
                UI_Render();
        }
        else {
                UI_Release();
                hooked = false;
        }

        // keep this detection code in place to detect breaking changes to the hook causing recursive nightmare
        static unsigned loop_check = 0;
        ASSERT((loop_check == 0) && "recursive hook detected");
        ++loop_check;
        auto ret = OLD_Present(This, SyncInterval, PresentFlags);
        --loop_check;

        return ret;
}


static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        static HWND last_window_handle = nullptr;

        if (EveryNFrames(600)) {
                DEBUG("input heartbeat");
        }

        if (last_window_handle != hWnd) {
                DEBUG("Window handle changed: %p -> %p", last_window_handle, hWnd);
                last_window_handle = hWnd;

                ImGuiIO& io = ImGui::GetIO();

                if (io.BackendPlatformUserData) {
                        ImGui_ImplWin32_Shutdown();
                }
                ImGui_ImplWin32_Init(hWnd);
        }

        static bool shift = false;
        static bool ctrl = false;
        static bool alt = false;

        if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
                if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
                        shift = true;
                } else if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL) {
                        ctrl = true;
                } else if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU) {
                        alt = true;
                } else {
                        unsigned vk_key = wParam & 0xFF;
                        HotkeyReceiveKeypress(vk_key, ctrl, alt, shift);
                }
        } else if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) {
                if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
                        shift = false;
                } else if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL) {
                        ctrl = false;
                } else if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU) {
                        alt = false;
                }
        }

        if (should_show_ui) {
                ClipCursor(NULL);
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        }

        return CallWindowProcW(OLD_Wndproc, hWnd, uMsg, wParam, lParam);
}

static void OnHotheyActivate(uintptr_t) {
        should_show_ui = !should_show_ui;
        DEBUG("ui toggled");

        if (!should_show_ui) {
                //when you close the UI, settings are saved
                SaveSettingsRegistry();
        }
}