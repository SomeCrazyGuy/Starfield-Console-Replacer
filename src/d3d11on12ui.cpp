#include <d3d11on12.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "main.h"
#include "gui.h"
#include "d3d11on12ui.h"

#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"

struct D3DBuffer {
        ID3D12Resource* d3d12RenderTarget;
        ID3D11Resource* d3d11WrappedBackBuffer;
        ID3D11RenderTargetView* d3d11RenderTargetView;
};

static D3DBuffer* Buffers{ nullptr };
static ID3D11DeviceContext* g_d3d11context{ nullptr };
static ID3D11On12Device* g_d3d11on12device{ nullptr };

static unsigned g_buffercount{ 0 };
static bool g_initialized = false;

static void DX11_Initialize(void* dx12_swapchain, void* dx12_commandqueue) {
        ASSERT(g_initialized == false);
        g_initialized = true;

        const auto swapchain = (IDXGISwapChain*)dx12_swapchain;
        const auto dx12queue = (ID3D12CommandQueue*)dx12_commandqueue;
        DEBUG("swapchain = %p, commandqueue = %p", dx12_swapchain, dx12_commandqueue);

        // These recources need to be released at the end
        ID3D12Device* d3d12device = nullptr;
        ID3D11Device* d3d11device = nullptr;
        ID3D12DescriptorHeap* d3d12decriptorheap = nullptr;

        // These resources do not need to be released in this function
        DXGI_SWAP_CHAIN_DESC desc{};
        const auto feature_level = D3D_FEATURE_LEVEL_11_0;
        D3D12_DESCRIPTOR_HEAP_DESC rtvdesc{};
        D3D12_CPU_DESCRIPTOR_HANDLE rtvhandle{};
        UINT rtvsize{};

        // Get the dx12 device from the swapchain
        HRESULT hr;
        hr = swapchain->GetDevice(IID_PPV_ARGS(&d3d12device));
        if(FAILED(hr)) {
                DEBUG("swapchain->GetDevice failed");
                goto FAILED;
        }

        // Get the info from the swapchain to get the number of frame buffers
        hr = swapchain->GetDesc(&desc);
        if(FAILED(hr)) {
                DEBUG("swapchain->GetDesc failed");
                goto FAILED;
        }

        g_buffercount = desc.BufferCount;

        // Create the dx11 device from the dx12 device and command queue
        hr = D3D11On12CreateDevice(d3d12device, 0, &feature_level, 1, (IUnknown* const*)&dx12queue, 1, 0, &d3d11device, &g_d3d11context, nullptr);
        if (FAILED(hr)) {
                DEBUG("D3D11On12CreateDevice failed");
                goto FAILED;
        }

        // verify the device
        hr = d3d11device->QueryInterface(IID_PPV_ARGS(&g_d3d11on12device));
        if (FAILED(hr)) {
                DEBUG("d3d11device->QueryInterface failed");
                goto FAILED;
        }

        // Create the dx12 buffers
        Buffers = (decltype(Buffers))calloc(g_buffercount, sizeof(*Buffers));
        ASSERT(Buffers != NULL);

        rtvdesc.NumDescriptors = g_buffercount;
        rtvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hr = d3d12device->CreateDescriptorHeap(&rtvdesc, IID_PPV_ARGS(&d3d12decriptorheap));
        if (FAILED(hr)) {
                DEBUG("d3d12device->CreateDescriptorHeap failed");
                goto FAILED;
        }

        rtvhandle = d3d12decriptorheap->GetCPUDescriptorHandleForHeapStart();
        rtvsize = d3d12device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (unsigned i = 0; i < g_buffercount; ++i) {
                hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&Buffers[i].d3d12RenderTarget));
                if (FAILED(hr)) {
                        DEBUG("swapchain->GetBuffer failed");
                        goto FAILED;
                }
                
                d3d12device->CreateRenderTargetView(Buffers[i].d3d12RenderTarget, nullptr, rtvhandle);

                D3D11_RESOURCE_FLAGS flags = { D3D11_BIND_RENDER_TARGET };
                hr = g_d3d11on12device->CreateWrappedResource(Buffers[i].d3d12RenderTarget, &flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&Buffers[i].d3d11WrappedBackBuffer));
                if (FAILED(hr)) {
                        DEBUG("g_d3d11on12device->CreateWrappedResource failed");
                        goto FAILED;
                }

                hr = d3d11device->CreateRenderTargetView(Buffers[i].d3d11WrappedBackBuffer, nullptr, &Buffers[i].d3d11RenderTargetView);
                if (FAILED(hr)) {
                        DEBUG("d3d11device->CreateRenderTargetView failed");
                        goto FAILED;
                }

                rtvhandle.ptr += rtvsize;
        }

        ImGui_ImplDX11_Init(d3d11device, g_d3d11context);
        DEBUG("DX11_Initialize succeeded");

        while (0) { 
        FAILED: 
                g_initialized = false;
                DEBUG("DX11_Initialize failed");
        } //weird

        if (d3d12decriptorheap) d3d12decriptorheap->Release();
        if (d3d11device) d3d11device->Release();
        if (d3d12device) d3d12device->Release();
        if (g_initialized == false) DX11_ReleaseIfInitialized();
}

extern void DX11_InitializeOrRender(void* dx12_swapchain, void* dx12_commandqueue) {
        if (g_initialized == false) {
                DX11_Initialize(dx12_swapchain, dx12_commandqueue);
                if (g_initialized == false) return;
        }

        const auto swapchain = (IDXGISwapChain3*)dx12_swapchain;

        const auto index = swapchain->GetCurrentBackBufferIndex();
        ASSERT(index < g_buffercount);
        const auto b = &Buffers[index];

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        draw_gui(); //from gui.h

        ImGui::Render();
        g_d3d11on12device->AcquireWrappedResources(&b->d3d11WrappedBackBuffer, 1);
        g_d3d11context->OMSetRenderTargets(1, &b->d3d11RenderTargetView, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_d3d11on12device->ReleaseWrappedResources(&b->d3d11WrappedBackBuffer, 1);
        g_d3d11context->Flush();
}

extern void DX11_ReleaseIfInitialized() {
        if (g_initialized == true) {
                ImGui_ImplDX11_Shutdown();
        }

        if (Buffers) {
                for (unsigned i = 0; i < g_buffercount; ++i) {
                        if (Buffers[i].d3d11RenderTargetView) {
                                Buffers[i].d3d11RenderTargetView->Release();
                                Buffers[i].d3d11RenderTargetView = nullptr;
                        }
                        if (Buffers[i].d3d11WrappedBackBuffer) {
                                Buffers[i].d3d11WrappedBackBuffer->Release();
                                Buffers[i].d3d11WrappedBackBuffer = nullptr;
                        }
                        if (Buffers[i].d3d12RenderTarget) {
                                Buffers[i].d3d12RenderTarget->Release();
                                Buffers[i].d3d12RenderTarget = nullptr;
                        }
                }
                free(Buffers);
                g_buffercount = 0;
                Buffers = nullptr;
        }

        if (g_d3d11on12device) {
                g_d3d11on12device->Release();
                g_d3d11on12device = nullptr;
        }
        
        if (g_d3d11context) {
                g_d3d11context->Flush();
                g_d3d11context->Release();
                g_d3d11context = nullptr;
        }

        g_initialized = false;
}