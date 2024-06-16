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
#include "std_api.h"

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

static HRESULT FAKE_ResizeBuffers(IDXGISwapChain3* This, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags);
static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static decltype(FAKE_ResizeBuffers)* OLD_ResizeBuffers = nullptr;
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
                        DEBUG("REPLACE [%u]: CommandQueue: '%p', SwapChain: '%p'", i, Queues[i].Queue, Queues[i].SwapChain);
                        Queues[i].Age = 0;
                        swapchain_inserted = true;
                        break;
                } else if (Queues[i].Queue == nullptr) {
                        //otherwise fill empty queue
                        Queues[i].Queue = (ID3D12CommandQueue*)Device;
                        Queues[i].SwapChain = *ppSwapChain;
                        Queues[i].Age = 0;
                        DEBUG("ADD [%u]: CommandQueue: '%p', SwapChain: '%p'", i, Queues[i].Queue, Queues[i].SwapChain);
                        swapchain_inserted = true;
                        break;
                }
                else {
                        DEBUG("UNUSED [%u]: CommandQueue: '%p', SwapChain: '%p', Age: %llu", i, Queues[i].Queue, Queues[i].SwapChain, Queues[i].Age);
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
                DEBUG("Overwrite[%u]: CommandQueue: '%p', SwapChain: '%p'", highest, Queues[highest].Queue, Queues[highest].SwapChain);
        }

        for(unsigned i = 0; i < MAX_QUEUES; ++i) {
                DEBUG("FINAL STATE [%u]: CommandQueue: '%p', SwapChain: '%p', Age: %llu", i, Queues[i].Queue, Queues[i].SwapChain, Queues[i].Age);
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
                GetBuffer,
                SetFullscreenState,
                GetFullscreenState,
                GetDesc,
                ResizeBuffers,
        };
        
        static void* vtable[128]; // 128 should be enough :)
        
        static bool once = false;
        if (!once) {
                once = true;

                // I wish it didnt have to come to this, but it does.
                // Steam overwrites the start of the IDXGISwapChain::Present
                // function with a JMP to GameOverlayRenderer.dll so that it can
                // render the steam overlay. RivaTuner aggressively replaces
                // the same start of IDXGISwapChain::Present with a JMP to
                // RTSS64.dll every frame to reassert dominance. then chainloads
                // the original steam overlay hook.
                // I need to hook IDXGISwapChain::Present too, so I hooked the
                // virtual function table at the offset of Present.
                // It should end there, but the Steam Overlay occasionally
                // reasserts that its hooks are in place and the way it does
                // that is to lookup the address of IDXGISwapChain::Present
                // in the vtable - and it finds my hook.
                // So Steam ends up overwriting my hook with a JMP to
                // GameOverlayRenderer.dll that then redirects back to my Present
                // that calls OLD_Present that was already hooked by steam or
                // rivatuner, then BOOM! infinite loop.
                // So now I need a hook that when the game calls present()
                // its my hook, then rivatuner's aggressive hook, then steam
                // overlay, then nvidia stramline native, then maybe eventually
                // the original IDXGISwapChain::Present in dxgi.dll
                // 
                // This is the solution I came up with. Im just going to copy all
                // the vtable functions from whatever IDXGISwapChain that this
                // function just created, then replace the vtable pointer of
                // every instance of IDXGISwapChain that comes down t   he line
                // with the new vtable. Its silly but it works.

                memcpy(vtable, **(void***)ppSwapChain, sizeof(vtable));

                OLD_Present = (decltype(OLD_Present))vtable[Present];
                vtable[Present] = (void*)FAKE_Present;

                OLD_ResizeBuffers = (decltype(OLD_ResizeBuffers))vtable[ResizeBuffers];
                vtable[ResizeBuffers] = (void*)FAKE_ResizeBuffers;

                DEBUG("Hooked Present, OLD_Present: %p, NEW_Present: %p", OLD_Present, FAKE_Present);
                DEBUG("Hooked ResizeBuffers, OLD_ResizeBuffers: %p, NEW_ResizeBuffers: %p", OLD_ResizeBuffers, FAKE_ResizeBuffers);

                /*
                OLD_Present = (decltype(OLD_Present))API.Hook->HookVirtualTable(
                        *ppSwapChain,
                        Present,
                        (FUNC_PTR)FAKE_Present
                );
                DEBUG("Hooked Present, OLD_Present: %p, NEW_Present: %p", OLD_Present, FAKE_Present);
                
                OLD_ResizeBuffers = (decltype(OLD_ResizeBuffers))API.Hook->HookVirtualTable(
                        *ppSwapChain,
                        ResizeBuffers,
                        (FUNC_PTR)FAKE_ResizeBuffers
                );
                */
        }

        DEBUG("Hooked Swapchain: %p, replacing vtable: %p with: %p", *ppSwapChain, **(void***)ppSwapChain, vtable);
        **(void***)ppSwapChain = vtable;

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
        c->ConfigU32(action, "FontScaleOverride", &s->FontScaleOverride);
 
        //do this last until i have this working with the official api
        if (action == ConfigAction_Write) {
                HotkeySaveSettings();
        }
}


static void SetupModMenu() {
        // use the directory of the betterconsole dll as the place to put other files
        // NOTE: this needs to be done before any other file (logfile/config/console history) is opened
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
                if (data == NULL) return ret;
                 
                if (cmd == RID_INPUT) {
                        auto input = (RAWINPUT*)data;

                        if (input->header.dwType == RIM_TYPEKEYBOARD) {
                                auto keydata = input->data.keyboard;
                                if (keydata.Message == WM_KEYDOWN || keydata.Message == WM_SYSKEYDOWN) {
                                        if (HotkeyReceiveKeypress(keydata.VKey)) {
                                                goto HIDE_INPUT_FROM_GAME;
                                        }
                                }
                        }

                        if (should_show_ui == false) return ret;

                        HIDE_INPUT_FROM_GAME:
                        //hide input from the game when shouldshowui is true and data is not null
                        input->header.dwType = RIM_TYPEHID; //game ignores typehid messages
                }
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

        // Gather all my friends!
        BroadcastBetterAPIMessage(&API);
        ASSERT(betterapi_load_selftest == true);

        // Load any settings from the config file and call any config callbacks
        LoadSettingsRegistry();
}

extern "C" __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface*) {}


static int OnBetterConsoleLoad(const struct better_api_t* api) {
        ASSERT(api == &API && "Betterconsole already loaded?? Do you have multiple versions of BetterConsole installed?");
        
        // The console part of better console is now minimally coupled to the mod menu
        DEBUG("Console setup - crashing here is AOB issue");
        setup_console(api);

        //should the hotkeys code be an internal plugin too?

        const auto handle = api->Callback->RegisterMod("(internal)");
        api->Callback->RegisterConfigCallback(handle, Callback_Config);
        api->Callback->RegisterHotkeyCallback(handle, OnHotheyActivate);
        
        //force hotkey for betterconsole default action using internal api
        HotkeyRequestNewHotkey(handle, "BetterConsole", 0, VK_F1);
        
        betterapi_load_selftest = true;
        DEBUG("Self Test Complete");
        return 0;
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


static HRESULT FAKE_ResizeBuffers(IDXGISwapChain3* This, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
        if (should_show_ui) {
                DX11_ReleaseIfInitialized();
        }
        DEBUG("ResizeBuffers: %p, BufferCount: %u, Width: %u, Height: %u, NewFormat: %u", This, BufferCount, Width, Height, NewFormat);
        return OLD_ResizeBuffers(This, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}


static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags) {
        if (EveryNFrames(240)) {
                DEBUG("render heartbeat, showing ui: %s", (should_show_ui)? "true" : "false");
        }

        static IDXGISwapChain3* last_swapchain = nullptr;
        static ID3D12CommandQueue* command_queue = nullptr;

        SwapChainQueue* best_match = nullptr;
        for (uint32_t i = 0; i < MAX_QUEUES; ++i) {
                //DEBUG("SEARCH [%u]: CommandQueue: '%p', SwapChain: '%p', Age: %u, Match: %s", i, Queues[i].Queue, Queues[i].SwapChain, Queues[i].Age, (Queues[i].SwapChain == This) ? "True" : "False");
                if (Queues[i].SwapChain == This) {
                        if (best_match == nullptr || Queues[i].Age < best_match->Age) {
                                best_match = &Queues[i];
                        }
                }
        }
        ASSERT(best_match != nullptr);
        //DEBUG("BEST MATCH: CommandQueue: '%p', SwapChain: '%p', Age: %u", best_match->Queue, best_match->SwapChain, best_match->Age);
        
        if (command_queue != best_match->Queue) {
                command_queue = best_match->Queue;
                DX11_ReleaseIfInitialized();
        }


        if (last_swapchain != This) {
                last_swapchain = This;
                DX11_ReleaseIfInitialized();
        }
        

        if (should_show_ui) {
                DX11_InitializeOrRender(This, command_queue);
        }
        else {
                DX11_ReleaseIfInitialized();
        }


        // keep this detection code in place to detect breaking changes to the hook causing recursive nightmare
        static unsigned loop_check = 0;
        ASSERT((loop_check == 0) && "recursive hook detected");
        ++loop_check;
        auto ret = OLD_Present(This, SyncInterval, PresentFlags);
        if (ret == DXGI_ERROR_DEVICE_REMOVED || ret == DXGI_ERROR_DEVICE_RESET) {
                DEBUG("DXGI_ERROR_DEVICE_REMOVED || DXGI_ERROR_DEVICE_RESET");
                //DX11_ReleaseIfInitialized();
        } else if (ret != S_OK) {
                DEBUG("Swapchain::Present returned error: %u", ret);
        }
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

        if (uMsg == WM_KEYDOWN && wParam == VK_F1) {
                //OnHotheyActivate(0); // this is for debugging only
        }

        if (uMsg == WM_SIZE || uMsg == WM_CLOSE) {
                DX11_ReleaseIfInitialized();
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