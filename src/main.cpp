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

static decltype(FAKE_Present)* OLD_Present = nullptr;
static decltype(FAKE_Wndproc)* OLD_Wndproc = nullptr;

//static ID3D12CommandQueue* d3d12CommandQueue = nullptr;
static bool should_show_ui = false;
static HWND window_handle = nullptr;

#ifdef MODMENU_DEBUG
static char format_buffer[4096];
static constexpr auto buffer_size = sizeof(format_buffer);
extern void DebugImpl(const char* const filename, const char* const func, int line, const char* const fmt, ...) noexcept {
        static HANDLE debugfile = INVALID_HANDLE_VALUE;

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

        if (debugfile == INVALID_HANDLE_VALUE) {
                debugfile = CreateFileW(L"debuglog.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }

        ASSERT(debugfile != INVALID_HANDLE_VALUE);
        WriteFile(debugfile, format_buffer, bytes, NULL, NULL);
        FlushFileBuffers(debugfile);
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
        MessageBoxA(NULL, format_buffer, "ASSERTION FAILURE", 0);
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
} static Queues[MAX_QUEUES];

static unsigned QueueIndex = 0;



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
        DEBUG("Factory: %p, Device: %p, HWND: %p, SwapChain: %p", This, Device, hWnd, *ppSwapChain);
        //TODO: the device parameter of this function is actually a commandqueue, I need that!
        ASSERT(QueueIndex < MAX_QUEUES);
        Queues[QueueIndex++] = { (ID3D12CommandQueue*)Device, *ppSwapChain };

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


                // need to use a stronger hook here, the steam overlay uses vmt hooks to draw
                // if the window is resized and this function is called again, we end up in a loop
                // with the steam overlay
                FUNC_PTR* fp = *(FUNC_PTR**)*ppSwapChain;
                OLD_Present = (decltype(OLD_Present))API.Hook->HookFunction(fp[Present], (FUNC_PTR)FAKE_Present);
                
                /*
                OLD_Present =
                        (decltype(OLD_Present))API.Hook->HookVirtualTable(
                        *ppSwapChain,
                        Present,
                        (FUNC_PTR)FAKE_Present
                );
                */
                
                DEBUG("Hooked present!");
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
        auto s = GetSettingsMutable();
        API.Config->Open(BetterAPIName);
        API.Config->BindInt(BIND_INT_DEFAULT(s->ConsoleHotkey));
        API.Config->BindInt(BIND_VALUE(s->FontScaleOverride), 50, 300, NULL);
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
                if (should_show_ui && (rect != NULL)) {
                        return true;
                }
                return OLD_ClipCursor(rect);
        };
        OLD_ClipCursor = (decltype(OLD_ClipCursor)) API.Hook->HookFunctionIAT("user32.dll", "ClipCursor", (FUNC_PTR)FAKE_ClipCursor);
        DEBUG("Hook ClipCursor: %p", OLD_ClipCursor);

        OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2)) API.Hook->HookFunctionIAT("sl.interposer.dll", "CreateDXGIFactory2", (FUNC_PTR)FAKE_CreateDXGIFactory2);
        if (!OLD_CreateDXGIFactory2) {
                OLD_CreateDXGIFactory2 = (decltype(OLD_CreateDXGIFactory2))API.Hook->HookFunctionIAT("dxgi.dll", "CreateDXGIFactory2", (FUNC_PTR)FAKE_CreateDXGIFactory2);
        }
        DEBUG("Hook CreateDXGIFactory2: %p", OLD_CreateDXGIFactory2);
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
                        DEBUG("SFSE PostPostLoad callback");

                        // the modmenu UI is internally imlpemented using the plugin api, it gets coupled here
                        RegisterInternalPlugin(&API);
                        DEBUG("RegisterInternalPlugin");

                        MessageInterface->Dispatch(MyPluginHandle, BetterAPIMessageType, (void*)&API, sizeof(API), NULL);
                        DEBUG("Dispatch SFSE message");

                        // The console part of better console is now minimally coupled to the mod menu
                        setup_console(&API);
                        DEBUG("Console setup");

                        //RegisterRandomizer(&API);
                        //DEBUG("Randomizer registered");
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
extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
        if (fdwReason == DLL_PROCESS_ATTACH) {
                CreateThread(NULL, 0, [](void*)->DWORD{ SetupModMenu(); return 0; }, NULL, 0, NULL);
        }
        return TRUE;
}


static HRESULT FAKE_Present(IDXGISwapChain3* This, UINT SyncInterval, UINT PresentFlags) {
        struct FrameContext {
                ID3D12CommandAllocator* commandAllocator = nullptr;
                ID3D12Resource* main_render_target_resource = nullptr;
                D3D12_CPU_DESCRIPTOR_HANDLE main_render_target_descriptor;
        };

        static ID3D12Device* d3d12Device = nullptr;
        static ID3D12CommandQueue* d3d12CommandQueue = nullptr;
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
                DEBUG("RenderPresent init Swapchain: %p, Sync %u, Flags: %u", This, SyncInterval, PresentFlags);
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

                ID3D12CommandAllocator* allocator{};
                d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
                ASSERT(allocator != NULL);
                for (size_t i = 0; i < buffersCounts; i++) {
                        frameContext[i].commandAllocator = allocator;
                }

                d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&d3d12CommandList));

                D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers{};
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
                DEBUG("present init complete");
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

                D3D12_RESOURCE_BARRIER barrier{};
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
                

                if (!d3d12CommandQueue) {
                        for (const auto& x : Queues) {
                                DEBUG("Queue %p, Swapchain: %p", x.Queue, x.SwapChain);
                                if (x.SwapChain == This) {
                                        d3d12CommandQueue = x.Queue;
                                        break;
                                }
                        }
                        DEBUG("Selecting command queue: %p", d3d12CommandQueue);
                }

                
                d3d12CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&d3d12CommandList));
        }

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

        return OLD_Present(This, SyncInterval, PresentFlags);
}


static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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