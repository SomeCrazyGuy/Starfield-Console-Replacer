#include "main.h"

#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

#include <dxgi1_6.h>
#include <d3d12.h>
#include <intrin.h>

#include "gui.h"
#include "console.h"
#include "settings.h"

#include "internal_plugin.h"


extern "C" __declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
        1, // SFSE api version
        1, // Plugin version
        BetterAPIName,
        "Linuxversion",
        1, // AddressIndependence::Signatures
        1, // StructureIndependence::NoStructs
        {GAME_VERSION, 0}, 
        0, //does not rely on any sfse version
        0, 0 //reserved fields
};

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags);
static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static decltype(FAKE_Present)* OLD_Present = nullptr;
static decltype(FAKE_Wndproc)* OLD_Wndproc = nullptr;

static bool should_show_ui = false;

#define EveryNFrames(N) []()->bool{static unsigned count=0;if(++count==(N)){count=0;}return !count;}()


static char DLL_DIR[MAX_PATH];

extern char* GetPathInDllDir(char* path_max_buffer, const char* filename) {

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
#endif


//this is where the api* that all clients use comes from
static const BetterAPI API {
        GetHookAPI(),
        GetLogBufferAPI(),
        GetSimpleDrawAPI(),
        GetCallbackAPI(),
        GetConfigAPI()
};


extern ModMenuSettings* GetSettingsMutable() {
        static ModMenuSettings Settings;
        return &Settings;
}


extern const ModMenuSettings* GetSettings() {
        return GetSettingsMutable();
}


#define MAX_QUEUES 16
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

        static bool once = 1;
        if (once) {
                once = 0;
                // needed to use a stronger hook here, the steam overlay uses vmt hooks to draw
                // if the window is resized and this function is called again, we end up in a
                // recursive loop with the steam overlay
                FUNC_PTR* fp = *(FUNC_PTR**)*ppSwapChain;
                DEBUG("HookFunction: IDXGISwapChain::Present");
                OLD_Present = (decltype(OLD_Present))API.Hook->HookFunction(fp[Present], (FUNC_PTR)FAKE_Present);
        }

        return ret;
}


static HRESULT(*OLD_CreateDXGIFactory2)(UINT, REFIID, void**) = nullptr;
static HRESULT FAKE_CreateDXGIFactory2(UINT Flags, REFIID RefID, void **ppFactory) {
        auto ret = OLD_CreateDXGIFactory2(Flags, RefID, ppFactory);
        DEBUG("Factory: %p", *ppFactory);

        static bool once = 1;
        if (once) {
                once = 0;

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

                OLD_CreateSwapChainForHwnd = (decltype(OLD_CreateSwapChainForHwnd))API.Hook->HookVirtualTable(
                        *ppFactory,
                        CreateSwapChainForHwnd,
                        (FUNC_PTR) FAKE_CreateSwapChainForHwnd
                );
                DEBUG("Hooked CreateSwapChainForHwnd");
        }

        return ret;
}

// quick hack for structname.membername or struct->member passed to BIND_INT
inline constexpr const char* member_name_only(const char* in) {
        while (*in) ++in;
        --in;
        while (
                ((*in >= 'a') && (*in <= 'z')) ||
                ((*in >= '0') && (*in <= '9')) ||
                ((*in >= 'A') && (*in <= 'Z'))
                ) --in;
        ++in;
        return in;
}

#define BIND_INT_DEFAULT(INT_PTR) member_name_only(#INT_PTR), &INT_PTR, 0, 0, NULL
#define BIND_VALUE(VALUE) member_name_only(#VALUE), &VALUE

static void SetupModMenu() {
        DEBUG(BetterAPIName " Version: " BETTERCONSOLE_VERSION);
        auto s = GetSettingsMutable();
        API.Config->Open(BetterAPIName);
        API.Config->BindInt(BIND_INT_DEFAULT(s->ConsoleHotkey));
        API.Config->BindInt(BIND_INT_DEFAULT(s->FontScaleOverride));
        API.Config->BindInt(BIND_INT_DEFAULT(s->HotkeyModifier));
        API.Config->Close();
        DEBUG("Settings Loaded");

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

        OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2)) API.Hook->HookFunctionIAT("sl.interposer.dll", "CreateDXGIFactory2", (FUNC_PTR)FAKE_CreateDXGIFactory2);
        if (!OLD_CreateDXGIFactory2) {
                OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2))API.Hook->HookFunctionIAT("dxgi.dll", "CreateDXGIFactory2", (FUNC_PTR)FAKE_CreateDXGIFactory2);
        }
        DEBUG("Hook CreateDXGIFactory2: %p", OLD_CreateDXGIFactory2);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = true;
        ImGui::StyleColorsDark();
        DEBUG("ImGui one time init completed!");

        // the modmenu UI is internally implemented using the plugin api, it gets coupled here
        DEBUG("RegisterInternalPlugin");
        RegisterInternalPlugin(&API);


        // The console part of better console is now minimally coupled to the mod menu
        DEBUG("Console setup - crashing here is AOB or offset issue");
        setup_console(&API);
} 

extern "C" __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface * sfse) {
#ifdef _DEBUG
        //while (!IsDebuggerPresent()) Sleep(100);
#endif // _DEBUG

        static PluginHandle MyPluginHandle;
        static SFSEMessagingInterface* MessageInterface;

        //broadcast to all listeners of "BetterConsole" during sfse postpostload
        static auto CALLBACK_sfse = [](SFSEMessage* msg) -> void {
                if (msg->type == MessageType_SFSE_PostPostLoad) {
                        MessageInterface->Dispatch(MyPluginHandle, BetterAPIMessageType, (void*)&API, sizeof(API), NULL);
                        DEBUG("SFSE PostPostLoad callback message dispatched");
                }
        };

        MyPluginHandle = sfse->GetPluginHandle();
        MessageInterface = (SFSEMessagingInterface*)sfse->QueryInterface(InterfaceID_Messaging);
        MessageInterface->RegisterListener(MyPluginHandle, "SFSE", CALLBACK_sfse);
}

// we need to figure out what type of loader was used here
// could check if we are named vcruntime
// could check if path is sfse plugin dir
// could fallback to asi loader called us
extern "C" BOOL WINAPI DllMain(HINSTANCE self, DWORD fdwReason, LPVOID) {
        if (fdwReason == DLL_PROCESS_ATTACH) {
                /* lock the linker/dll loader until hooks are installed, TODO: make sure this code path is fast */
                static bool RunHooksOnlyOnce = true;
                ASSERT(RunHooksOnlyOnce == true); //i want to know if this assert ever gets triggered
                
                //use the directory of the betterconsole dll as the place to put other files
                GetModuleFileNameA(self, DLL_DIR, MAX_PATH);
                char* n = DLL_DIR;
                while (*n) ++n;
                while ((n != DLL_DIR) && (*n != '\\')) --n;
                ++n;
                *n = 0;

                SetupModMenu();
                RunHooksOnlyOnce = false;
        }
        return TRUE;
}

struct FrameContext {
        ID3D12CommandAllocator* commandAllocator{ nullptr };
        ID3D12Resource* mainRenderTargetResource{ nullptr };
        D3D12_CPU_DESCRIPTOR_HANDLE mainRenderTargetDescriptor;
        UINT64 fenceValue{ 0 }; //TODO: use this
};

struct RenderState {
        ID3D12Device* device{ nullptr };
        //IDXGISwapChain* swapchain{ nullptr };
        ID3D12CommandQueue* commandQueue{ nullptr };
        ID3D12DescriptorHeap* descriptorHeapBackBuffers{ nullptr };
        ID3D12DescriptorHeap* descriptorHeapImGuiRender{ nullptr };
        ID3D12GraphicsCommandList* commandList{ nullptr };
        FrameContext* frameContext{ nullptr };
        UINT bufferCount{ 0 };
};


static void SetupRenderState(RenderState* state, IDXGISwapChain3* Swapchain) {
        Swapchain->GetDevice(IID_PPV_ARGS(&state->device));
        ASSERT(state->device != NULL);

        DXGI_SWAP_CHAIN_DESC sdesc;
        Swapchain->GetDesc(&sdesc);

        state->bufferCount = sdesc.BufferCount;

        free(state->frameContext);
        state->frameContext = (FrameContext*)calloc(state->bufferCount, sizeof(FrameContext));

        D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
        descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorImGuiRender.NumDescriptors = state->bufferCount;
        descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        state->device->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&state->descriptorHeapImGuiRender));

        ID3D12CommandAllocator* allocator = nullptr;
        state->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        ASSERT(allocator != NULL);
        for (size_t i = 0; i < state->bufferCount; i++) {
                state->frameContext[i].commandAllocator = allocator;
        }

        state->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&state->commandList));

        D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers{};
        descriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descriptorBackBuffers.NumDescriptors = state->bufferCount;
        descriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descriptorBackBuffers.NodeMask = 1;

        state->device->CreateDescriptorHeap(&descriptorBackBuffers, IID_PPV_ARGS(&state->descriptorHeapBackBuffers));

        const auto rtvDescriptorSize = state->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = state->descriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < state->bufferCount; i++) {
                ID3D12Resource* pBackBuffer = nullptr;
                state->frameContext[i].mainRenderTargetDescriptor = rtvHandle;
                Swapchain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
                state->device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
                state->frameContext[i].mainRenderTargetResource = pBackBuffer;
                rtvHandle.ptr += rtvDescriptorSize;
        }

        ImGui_ImplDX12_Init(state->device, state->bufferCount,
                DXGI_FORMAT_R8G8B8A8_UNORM, state->descriptorHeapImGuiRender,
                state->descriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
                state->descriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart());

        ImGui_ImplDX12_CreateDeviceObjects();
}


static void InjectImGui(IDXGISwapChain3* Swapchain) {
        static RenderState state;

        static IDXGISwapChain* last_swapchain = nullptr;
        if (last_swapchain != Swapchain) {
                last_swapchain = Swapchain;

                DEBUG("Injecting imgui renderer into swapchain: %p", Swapchain);

                //find the command queue for this state
                ID3D12CommandQueue* tmp = nullptr;
                for (const auto& q : Queues) {
                        if (!state.commandQueue) {
                                DEBUG("Queue %p, Swapchain: %p", q.Queue, q.SwapChain);
                        }
                        if (q.SwapChain == Swapchain) {
                                tmp = q.Queue;
                                break;
                        }
                }
                if (tmp != state.commandQueue) {
                        DEBUG("Selecting command queue: %p", tmp);
                        state.commandQueue = tmp;
                }
                ASSERT(state.commandQueue != NULL);
                
                DEBUG("RenderPresent init Swapchain: %p", Swapchain);

                DXGI_SWAP_CHAIN_DESC sdesc;
                Swapchain->GetDesc(&sdesc);
                HWND window_handle = sdesc.OutputWindow;

                auto proc = (decltype(OLD_Wndproc))GetWindowLongPtrW(window_handle, GWLP_WNDPROC);
                DEBUG("Input Hook: OLD_OLD_Wndproc: %p, NEW_OLD_Wndproc: %p, FAKE_Wndproc: %p", OLD_Wndproc, proc, FAKE_Wndproc);
                if ((uint64_t)FAKE_Wndproc != (uint64_t)proc) {
                        OLD_Wndproc = proc;
                        SetWindowLongPtrW(window_handle, GWLP_WNDPROC, (LONG_PTR)FAKE_Wndproc);
                }
                else {
                        DEBUG("Wndproc already hooked");
                }

                //TODO: actually release and recapture graphics system
                state.device = nullptr;
        }

        if (should_show_ui) {
                if (!state.device) {
                        SetupRenderState(&state, Swapchain);
                        return;
                }

                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                draw_gui();

                ImGui::EndFrame();
                ImGui::Render();

                FrameContext& currentFrameContext = state.frameContext[Swapchain->GetCurrentBackBufferIndex()];
                currentFrameContext.commandAllocator->Reset();

                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = currentFrameContext.mainRenderTargetResource;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

                state.commandList->Reset(currentFrameContext.commandAllocator, nullptr);
                state.commandList->ResourceBarrier(1, &barrier);
                state.commandList->OMSetRenderTargets(1, &currentFrameContext.mainRenderTargetDescriptor, FALSE, nullptr);
                state.commandList->SetDescriptorHeaps(1, &state.descriptorHeapImGuiRender);

                ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), state.commandList);

                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

                state.commandList->ResourceBarrier(1, &barrier);
                state.commandList->Close();

                state.commandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&state.commandList));
        }
}


static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags) {
        // just when you though we were done loading down the main render thread...
        // now we have the periodic callback. these callbacks are spread out over seveal frames
        // each mod can register one callback and that callback will be called every "1 / mod count" frames

        size_t infos_count = 0;
        auto infos = GetModInfo(&infos_count);

        static uint32_t periodic_id = 0;
        if (infos_count) {
                ASSERT(infos != NULL);
                uint32_t this_frame_callback = periodic_id++ % infos_count;
                auto callback = infos[this_frame_callback].PeriodicCallback;
                if (callback) callback();
        }

        InjectImGui(This);

        // one common cause of crashes is steam overlay hooking ::Present() and getting us into a stack overflow
        // keep this detection code in place to detect breaking changes to the imgui hook
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
                DEBUG("Heartbeat");
        }

        if (hWnd && (last_window_handle != hWnd)) {
                DEBUG("Window handle changed: %p -> %p", last_window_handle, hWnd);
                last_window_handle = hWnd;

                ImGuiIO& io = ImGui::GetIO();

                if (io.BackendPlatformUserData) {
                        ImGui_ImplWin32_Shutdown();
                }
                ImGui_ImplWin32_Init(hWnd);
        }

        if (uMsg == WM_KEYDOWN) {
                // built-in hotkey takes priority
                if (wParam == GetSettings()->ConsoleHotkey) {
                        const auto modifier = GetSettings()->HotkeyModifier;
                        if ((modifier == 0) || (GetKeyState(modifier) < 0)) {
                                should_show_ui = !should_show_ui;
                                DEBUG("ui toggled");
                                //when you open or close the UI, settings are saved
                                SaveSettingsRegistry();
                        }
                }

                boolean shift = (GetKeyState(VK_SHIFT) < 0);
                boolean ctrl = (GetKeyState(VK_CONTROL) < 0);
          
                size_t infos_count = 0;
                auto infos = GetModInfo(&infos_count);
                for (auto i = 0; i < infos_count; ++i) {
                        if (infos[i].HotkeyCallback) {
                                if (infos[i].HotkeyCallback((uint32_t)wParam, shift, ctrl)) break;
                        }
                }
        }

        if (should_show_ui) {
                ClipCursor(NULL);
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        }

        return CallWindowProcW(OLD_Wndproc, hWnd, uMsg, wParam, lParam);
}