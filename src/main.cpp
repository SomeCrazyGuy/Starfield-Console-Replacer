#include "main.h"

#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

#include <dxgi1_6.h>
#include <d3d12.h>

#include "gui.h"
#include "console.h"
#include "settings.h"


#include "randomizer.h"

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
static HRESULT FAKE_CreateCommandQueue(ID3D12Device * This, D3D12_COMMAND_QUEUE_DESC * pDesc, REFIID riid, void** ppCommandQueue);

static decltype(FAKE_Present)* OLD_Present = nullptr;
static decltype(FAKE_Wndproc)* OLD_Wndproc = nullptr;
static decltype(FAKE_CreateCommandQueue)* OLD_CreateCommandQueue = nullptr;

static ID3D12CommandQueue* d3d12CommandQueue = nullptr;
static bool should_show_ui = false;
static HWND window_handle = nullptr;



static void DebugLog(const char* func, int line, const char* fmt, ...) {
        static char line_buffer[128];
        static char format_buffer[4096];
        static FILE* debugfile = nullptr;
        
        va_list args;
        va_start(args, fmt);
        vsnprintf(format_buffer, sizeof(format_buffer), fmt, args);
        va_end(args);

        snprintf(line_buffer, sizeof(line_buffer), "%s:%d>", func, line);

        if (debugfile == nullptr) {
                fopen_s(&debugfile, "debuglog.txt", "wb");
        }

        ASSERT(debugfile != NULL);
        fputs(line_buffer, debugfile);
        fputs(format_buffer, debugfile);
        fputc('\n', debugfile);
        fflush(debugfile);
}
#ifdef _DEBUG
#define Debug(...) do { DebugLog(__func__, __LINE__, " " __VA_ARGS__); } while(0)
#else
#define Debug(...) do {} while(0)
#endif // _DEBUG






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


static HRESULT FAKE_CreateCommandQueue(ID3D12Device * This, D3D12_COMMAND_QUEUE_DESC * pDesc, REFIID riid, void** ppCommandQueue) {
        auto ret = OLD_CreateCommandQueue(This, pDesc, riid, ppCommandQueue);
        if(!d3d12CommandQueue) d3d12CommandQueue = *(ID3D12CommandQueue**)ppCommandQueue;
        return ret;
}


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
        Debug();
        auto ret = OLD_CreateSwapChainForHwnd(This, Device, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
        Debug("On The other side!");

        static bool once = 1;
        if (once) {
                once = 0;

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

                OLD_Present =
                        (decltype(OLD_Present))API.Hook->HookVirtualTable(
                        *ppSwapChain,
                        Present,
                        (FUNC_PTR)FAKE_Present
                );

                Debug("Hooked present!");
        }

        return ret;
}


static HRESULT(*OLD_CreateDXGIFactory2)(UINT, REFIID, void**) = nullptr;
static HRESULT FAKE_CreateDXGIFactory2(UINT Flags, REFIID RefID, void **ppFactory) {
        auto ret = OLD_CreateDXGIFactory2(Flags, RefID, ppFactory);

        Debug();

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

                Debug();
                OLD_CreateSwapChainForHwnd = (decltype(OLD_CreateSwapChainForHwnd))API.Hook->HookVirtualTable(
                        *ppFactory,
                        CreateSwapChainForHwnd,
                        (FUNC_PTR) FAKE_CreateSwapChainForHwnd
                );
                Debug();
        }

        return ret;
}


static HRESULT (*OLD_D3D12CreateDevice)(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice) = nullptr;
static HRESULT FAKE_D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice) {
        Debug();
        auto ret = OLD_D3D12CreateDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);

        static bool once = 1;
        if (once) {
                once = 0;

                enum : unsigned {
                        QueryInterface,
                        AddRef,
                        Release,
                        GetPrivateData,
                        SetPrivateData,
                        SetPrivateDataInterface,
                        SetName,
                        GetNodeCount,
                        CreateCommandQueue
                };

                OLD_CreateCommandQueue =
                        (decltype(OLD_CreateCommandQueue))API.Hook->HookVirtualTable(
                                *ppDevice,
                                CreateCommandQueue,
                                (FUNC_PTR)FAKE_CreateCommandQueue
                        );

                Debug("Hooked CreateCommandQueue");
        }

        return ret;
}


static void HookDX12() {
        // Currently not compatible with dlss 3 frame generation, but the user wont know that unless we tell them
        if (GetModuleHandleA("nvngx_dlssg")) {
                MessageBoxA(NULL,
                        BetterAPIName " is currently not compatible with mods that load ngngx_dlssg.dll"
                        " which is used for the optional DLSS3 frame generation feature of some mods"
                        " in the future compatibility will improve but for now you will need to remove "
                        "ngngx_dlssg.dll in the nv-streamline folder. NOTE: ngngx_dlss.dll (without the 'g') IS "
                        "COMPATIBLE with " BetterAPIName " and will continue to be compatible going forward."
                        " Press OK to exit starfield",
                        BetterAPIName " Error",
                        0
                );

                ExitProcess(1);
        }


        FUNC_PTR FUN_CreateDXGIFactory2 = (FUNC_PTR)GetProcAddress(GetModuleHandleA("dxgi"), "CreateDXGIFactory2");
        ASSERT(FUN_CreateDXGIFactory2 != NULL);
        OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2)) API.Hook->HookFunction(
                (FUNC_PTR)FUN_CreateDXGIFactory2,
                (FUNC_PTR)FAKE_CreateDXGIFactory2
        );


        FUNC_PTR FUN_D3D12CreateDevice = (FUNC_PTR)GetProcAddress(GetModuleHandleA("d3d12"), "D3D12CreateDevice");
        ASSERT(FUN_D3D12CreateDevice != NULL);
        OLD_D3D12CreateDevice = (decltype(OLD_D3D12CreateDevice))API.Hook->HookFunction(
                (FUNC_PTR)FUN_D3D12CreateDevice,
                (FUNC_PTR)FAKE_D3D12CreateDevice
        );
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


extern "C" __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface * sfse) {
#ifdef _DEBUG
        while (!IsDebuggerPresent())Sleep(100);
#endif // DEBUG

        ASSERT(OLD_Present == NULL);

        auto s = GetSettingsMutable();
        API.Config->Open(BetterAPIName);
        API.Config->BindInt(BIND_INT_DEFAULT(s->ConsoleHotkey));
        API.Config->BindInt(BIND_INT_DEFAULT(s->FontScaleOverride));
        API.Config->BindInt(BIND_INT_DEFAULT(s->HotkeyModifier));
        API.Config->Close();

        static PluginHandle MyPluginHandle;
        static SFSEMessagingInterface* MessageInterface;

        //broadcast to all listeners of "BetterConsole" during sfse postpostload
        static auto CALLBACK_sfse = [](SFSEMessage* msg) -> void {
                if (msg->type == MessageType_SFSE_PostPostLoad) {
                        HookDX12();

                        // the modmenu UI is internally imlpemented using the plugin api, it gets coupled here
                        RegisterInternalPlugin(&API);

                        MessageInterface->Dispatch(MyPluginHandle, BetterAPIMessageType, (void*) &API, sizeof(API), NULL);

                        // The console part of better console is now minimally coupled to the mod menu
                        setup_console(&API);
                        RegisterRandomizer(&API);
                }
        };

        MyPluginHandle = sfse->GetPluginHandle();
        MessageInterface = (SFSEMessagingInterface*) sfse->QueryInterface(InterfaceID_Messaging);
        MessageInterface->RegisterListener(MyPluginHandle, "SFSE", CALLBACK_sfse);
        
        // the game uses the rawinput interface to read keyboard and mouse events
        // if we hook that function we can disable input to the game when the imgui interface is open
        auto huser32 = GetModuleHandleA("user32");
        ASSERT(huser32 != NULL);

        auto fun_get_rawinput = (FUNC_PTR) GetProcAddress(huser32, "GetRawInputData");
        static UINT(*OLD_GetRawInputData)(HRAWINPUT hri, UINT cmd, LPVOID data, PUINT data_size, UINT hsize) = nullptr;
        static decltype(OLD_GetRawInputData) FAKE_GetRawInputData = [](HRAWINPUT hri, UINT cmd, LPVOID data, PUINT data_size, UINT hsize) -> UINT {
                auto ret = OLD_GetRawInputData(hri, cmd, data, data_size, hsize);
                
                if ((should_show_ui == false) || (data == NULL)) return ret;
                
                //hide input from the game when shouldshowui is true and data is not null
                auto input = (RAWINPUT*)data;
                input->header.dwType = RIM_TYPEHID; //game ignores typehid messages
                return ret;
        };
        OLD_GetRawInputData = (decltype(OLD_GetRawInputData))API.Hook->HookFunction(fun_get_rawinput, (FUNC_PTR) FAKE_GetRawInputData);

        //i would prefer not hooking multiple win32 apis but its more update-proof than engaging with the game's wndproc
        auto fun_clip_cursor = (FUNC_PTR)GetProcAddress(huser32, "ClipCursor");
        static BOOL(*OLD_ClipCursor)(const RECT*) = nullptr;
        static decltype(OLD_ClipCursor) FAKE_ClipCursor = [](const RECT* rect) -> BOOL {
                // When the imgui window is open only pass through clipcursor(NULL);
                if (should_show_ui && (rect != NULL)) {
                        return true;
                }
                return OLD_ClipCursor(rect);
        };
        OLD_ClipCursor = (decltype(OLD_ClipCursor))API.Hook->HookFunction(fun_clip_cursor, (FUNC_PTR)FAKE_ClipCursor);
}


static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags) {
        struct FrameContext {
                ID3D12CommandAllocator* commandAllocator = nullptr;
                ID3D12Resource* main_render_target_resource = nullptr;
                D3D12_CPU_DESCRIPTOR_HANDLE main_render_target_descriptor;
        };

        static ID3D12Device* d3d12Device = nullptr;
        static ID3D12DescriptorHeap* d3d12DescriptorHeapBackBuffers = nullptr;
        static ID3D12DescriptorHeap* d3d12DescriptorHeapImGuiRender = nullptr;
        static ID3D12GraphicsCommandList* d3d12CommandList = nullptr;
        static UINT buffersCounts = 0;
        static FrameContext* frameContext = nullptr;

        //my state
        static ImGuiContext* imgui_context = nullptr;
        static unsigned once = 1;
        
        //once = 0;
        if (once) {
                once = 0;

                DXGI_SWAP_CHAIN_DESC sdesc;
                This->GetDesc(&sdesc);
                window_handle = sdesc.OutputWindow;

                IMGUI_CHECKVERSION();
                imgui_context = ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.MouseDrawCursor = true;
                ImGui::StyleColorsDark();
                ImGui_ImplWin32_Init(window_handle);
                OLD_Wndproc = (decltype(OLD_Wndproc))SetWindowLongPtrW(window_handle, GWLP_WNDPROC, (LONG_PTR)FAKE_Wndproc);

                This->GetDevice(IID_PPV_ARGS(&d3d12Device));
                ASSERT(d3d12Device != NULL);

                buffersCounts = sdesc.BufferCount;
                frameContext = (FrameContext*)calloc(buffersCounts, sizeof(FrameContext));

                D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
                descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                descriptorImGuiRender.NumDescriptors = buffersCounts;
                descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

                d3d12Device->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&d3d12DescriptorHeapImGuiRender));

                ID3D12CommandAllocator* allocator;
                d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
                ASSERT(allocator != NULL);
                for (size_t i = 0; i < buffersCounts; i++) {
                        frameContext[i].commandAllocator = allocator;
                }

                d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&d3d12CommandList));

                D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers;
                descriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                descriptorBackBuffers.NumDescriptors = buffersCounts;
                descriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                descriptorBackBuffers.NodeMask = 1;

                d3d12Device->CreateDescriptorHeap(&descriptorBackBuffers, IID_PPV_ARGS(&d3d12DescriptorHeapBackBuffers));

                const auto rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d12DescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

                for (UINT i = 0; i < buffersCounts; i++) {
                        ID3D12Resource* pBackBuffer = nullptr;
                        frameContext[i].main_render_target_descriptor = rtvHandle;
                        This->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
                        d3d12Device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
                        frameContext[i].main_render_target_resource = pBackBuffer;
                        rtvHandle.ptr += rtvDescriptorSize;
                }

                ImGui_ImplDX12_Init(d3d12Device, buffersCounts,
                        DXGI_FORMAT_R8G8B8A8_UNORM, d3d12DescriptorHeapImGuiRender,
                        d3d12DescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
                        d3d12DescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart());

                ImGui_ImplDX12_CreateDeviceObjects();
        }

        if (should_show_ui) {
                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                draw_gui();

                //ImGui::ShowDemoWindow();

                ImGui::EndFrame();
                ImGui::Render();

                FrameContext& currentFrameContext = frameContext[This->GetCurrentBackBufferIndex()];
                currentFrameContext.commandAllocator->Reset();

                D3D12_RESOURCE_BARRIER barrier;
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = currentFrameContext.main_render_target_resource;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

                d3d12CommandList->Reset(currentFrameContext.commandAllocator, nullptr);
                d3d12CommandList->ResourceBarrier(1, &barrier);
                d3d12CommandList->OMSetRenderTargets(1, &currentFrameContext.main_render_target_descriptor, FALSE, nullptr);
                d3d12CommandList->SetDescriptorHeaps(1, &d3d12DescriptorHeapImGuiRender);

                ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12CommandList);

                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

                d3d12CommandList->ResourceBarrier(1, &barrier);
                d3d12CommandList->Close();

                d3d12CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&d3d12CommandList));
        }

        // just when you though we were done loading down the main render thread...
        // now we have the periodic callback. these callbacks are spread out over seveal frames
        // each mod can register one callback and that callback will be called every "1 / mod count" frames
        
        size_t infos_count = 0;
        auto infos = GetModInfo(&infos_count);

        static uint32_t periodic_id = 0;
        uint32_t this_frame_callback = periodic_id++ % infos_count;
        auto callback = infos[this_frame_callback].PeriodicCallback;
        if (callback) callback();

        return OLD_Present(This, SyncInterval, PresentFlags);
}


static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_KEYDOWN) {
                // built-in hotkey takes priority
                if (wParam == GetSettings()->ConsoleHotkey) {
                        const auto modifier = GetSettings()->HotkeyModifier;
                        if ((modifier == 0) || (GetKeyState(modifier) < 0)) {
                                should_show_ui = !should_show_ui;
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
        return CallWindowProc(OLD_Wndproc, hWnd, uMsg, wParam, lParam);
}