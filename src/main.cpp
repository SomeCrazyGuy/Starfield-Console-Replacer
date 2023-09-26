#include "main.h"

#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

#include <dxgi1_6.h>
#include <d3d12.h>

#include "callback.h"
#include "console.h"
#include "hook_api.h"
#include "simpledraw.h"
#include "configfile.h"
#include "log_buffer.h"


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

//#define FATAL_ERROR(X) do{MessageBoxA(NULL, X, "Debug", 0); ExitProcess(1);}while(0)
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


static int ConsoleHotkey = VK_F1;
static int FontScalePercent = 100;


static HRESULT FAKE_CreateCommandQueue(ID3D12Device * This, D3D12_COMMAND_QUEUE_DESC * pDesc, REFIID riid, void** ppCommandQueue) {
        auto ret = OLD_CreateCommandQueue(This, pDesc, riid, ppCommandQueue);
        static unsigned ccc = 0;
        ++ccc;
        char buff[128];
        snprintf(buff, 128, "FAKE_CreateCommandQueue: %u, queue: %p", ccc, *(ID3D12CommandQueue**)ppCommandQueue);
        //MessageBoxA(NULL, buff, "Debug", 0);
        if(!d3d12CommandQueue) d3d12CommandQueue = *(ID3D12CommandQueue**)ppCommandQueue;
        return ret;
}


extern "C" __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface * sfse) {
        ASSERT(OLD_Present == NULL);

        ReadConfigFile();
        CONFIG_INT(ConsoleHotkey);
        CONFIG_INT(FontScalePercent);
        SaveConfigFile();

        static PluginHandle MyPluginHandle;
        static SFSEMessagingInterface* MessageInterface;

        static BetterAPI API;
        API.Hook = GetHookAPI();
        API.Callback = GetCallbackAPI();
        API.LogBuffer = GetLogBufferAPI();
        API.SimpleDraw = GetSimpleDrawAPI();

        //broadcast to all listeners of "BetterConsole" during sfse postpostload
        static auto CALLBACK_sfse = [](SFSEMessage* msg) -> void {
                if (msg->type == MessageType_SFSE_PostPostLoad) {
                        MessageInterface->Dispatch(MyPluginHandle, BetterAPIMessageType, &API, sizeof(API), NULL);
                }
        };

        MyPluginHandle = sfse->GetPluginHandle();
        MessageInterface = (SFSEMessagingInterface*) sfse->QueryInterface(InterfaceID_Messaging);
        MessageInterface->RegisterListener(MyPluginHandle, "SFSE", CALLBACK_sfse);

        //We need to find IDXGISwapChain3::Present() to hook it, we are going to take the long way....
        static const auto temp_wndproc = [](HWND, UINT, WPARAM, LPARAM)-> LRESULT {
                return true;
        };

        WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, temp_wndproc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "temp", NULL };
        ::RegisterClassExA(&wc);
        HWND hwnd = ::CreateWindowA(wc.lpszClassName, "temp window", WS_OVERLAPPEDWINDOW, 100, 100, 600, 800, NULL, NULL, wc.hInstance, NULL);

        // Setup swap chain
        DXGI_SWAP_CHAIN_DESC1 sd;
        {
                ZeroMemory(&sd, sizeof(sd));
                sd.BufferCount = 3;
                sd.Width = 0;
                sd.Height = 0;
                sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
                sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                sd.SampleDesc.Count = 1;
                sd.SampleDesc.Quality = 0;
                sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
                sd.Scaling = DXGI_SCALING_STRETCH;
                sd.Stereo = FALSE;
        }

        static ID3D12Device* g_pd3dDevice = NULL;
        static IDXGISwapChain3* g_pSwapChain = NULL;
        static ID3D12CommandQueue* g_pd3dCommandQueue = NULL;

        // Create device
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
        ASSERT(D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) == S_OK);
        
        IDXGIFactory4* dxgiFactory = NULL;
        ASSERT(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) == S_OK);

        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        ASSERT(g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) == S_OK);

        //hack commandqueue vtable
        enum class DeviceFunctionIndex : unsigned {
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
                        g_pd3dDevice,
                        (unsigned) DeviceFunctionIndex::CreateCommandQueue,
                        (FUNC_PTR) FAKE_CreateCommandQueue
                );

        IDXGISwapChain1* swapChain1 = NULL;
        ASSERT(dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hwnd, &sd, NULL, NULL, &swapChain1) == S_OK);
        ASSERT(swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) == S_OK);

        swapChain1->Release();
        dxgiFactory->Release();

        enum class SwapchainFunctionIndex : unsigned {
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
                        g_pSwapChain,
                        (unsigned) SwapchainFunctionIndex::Present,
                        (FUNC_PTR) FAKE_Present
                );

        g_pSwapChain->Release();
        g_pd3dCommandQueue->Release();
        g_pd3dDevice->Release();

        ::DestroyWindow(hwnd);
        ::UnregisterClassA(wc.lpszClassName, wc.hInstance);
        
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
                input->header.dwType = RIM_TYPEHID;
                return ret;
        };
        OLD_GetRawInputData = (decltype(OLD_GetRawInputData))API.Hook->HookFunction(fun_get_rawinput, (FUNC_PTR) FAKE_GetRawInputData);

        //i would prefer not hooking multiple win32 apis but its more update-proof than engaging with the game's wndproc
        auto fun_clip_cursor = (FUNC_PTR)GetProcAddress(huser32, "ClipCursor");
        static BOOL(*OLD_ClipCursor)(const RECT*) = nullptr;
        static decltype(OLD_ClipCursor) FAKE_ClipCursor = [](const RECT* rect) -> BOOL {
                // When the imgui window is open only pass through clipcursor(NULL);
                if (should_show_ui) {
                        if (rect == NULL) {
                                return OLD_ClipCursor(NULL);
                        }
                        else {
                                //dont waste cpu calling real clipcursor when should_show_ui
                                return TRUE;
                        }
                }
                else {
                        return OLD_ClipCursor(rect);
                }
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

                buffersCounts = sdesc.BufferCount;
                frameContext = (FrameContext*)calloc(buffersCounts, sizeof(FrameContext));

                D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
                descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                descriptorImGuiRender.NumDescriptors = buffersCounts;
                descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

                d3d12Device->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&d3d12DescriptorHeapImGuiRender));

                ID3D12CommandAllocator* allocator;
                d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
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

                static bool initpos = false;
                if (!initpos) {
                        initpos = true;
                        RECT rect;
                        GetClientRect(window_handle, &rect);
                        float width = (float)(rect.right - rect.left) / 2;
                        float height = (float)(rect.bottom - rect.top) / 2;
                        assert(width > 0.f);
                        assert(height > 0.f);
                        ImGui::SetNextWindowPos(ImVec2{ width / 2, height / 2 });
                        ImGui::SetNextWindowSize(ImVec2{ width, height });
                }

                ImGui::Begin("BetterConsole");
                ImGui::SetWindowFontScale(FontScalePercent / 100.f);
                ImGui::BeginTabBar("tab items");
                if (ImGui::BeginTabItem("Console")) {
                        draw_console_window();
                        ImGui::EndTabItem();
                }
                draw_simpledraw_tabs();
                ImGui::EndTabBar();
                ImGui::End();
                draw_imguidraw_callbacks();
                

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

        return OLD_Present(This, SyncInterval, PresentFlags);
}


static LRESULT FAKE_Wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if ((wParam == ConsoleHotkey) && (uMsg == WM_KEYDOWN)) {
                should_show_ui = !should_show_ui;
        }
        if (should_show_ui) {
                ClipCursor(NULL);
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        }
        return CallWindowProc(OLD_Wndproc, hWnd, uMsg, wParam, lParam);
}