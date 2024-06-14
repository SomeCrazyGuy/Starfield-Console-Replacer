#include <d3d11on12.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "main.h"
#include "gui.h"

#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"

//for _com_error
#include <comdef.h>

struct D3DBuffer {
        ID3D12Resource* d3d12RenderTarget;
        ID3D11Resource* d3d11WrappedBackBuffer;
        ID3D11RenderTargetView* d3d11RenderTargetView;
};

static D3DBuffer* Buffers{ nullptr };
static IDXGISwapChain3* g_d3d12swapchain3{ nullptr };


static ID3D11Device* g_d3d11device{ nullptr };
static ID3D11DeviceContext* g_d3d11context{ nullptr };
static ID3D11On12Device* g_d3d11on12device{ nullptr };


static unsigned g_buffercount{ 0 };
static bool g_initialized = false;

#define COM_ERROR(ERR) do { if ((ERR) != S_OK){ _com_error err(hr); DEBUG("ERROR: %s", err.ErrorMessage()); } ASSERT(hr == S_OK); } while (0)


// NOTE: the other code doesnt abort on hr != S_OK
extern void DX11_Initialize(void* dx12_swapchain, void* dx12_commandqueue) {
        ASSERT(g_initialized == false);
        const auto swapchain = (IDXGISwapChain*)dx12_swapchain;
        const auto dx12queue = (ID3D12CommandQueue*)dx12_commandqueue;
        DEBUG("swapchain = %p, commandqueue = %p", dx12_swapchain, dx12_commandqueue);

        // Get the dx12 device from the swapchain
        HRESULT hr;
        ID3D12Device* dx12device;
        hr = swapchain->GetDevice(IID_PPV_ARGS(&dx12device));
        COM_ERROR(hr);
        
        // Get the info from the swapchain to get the number of frame buffers
        DXGI_SWAP_CHAIN_DESC desc;
        hr = swapchain->GetDesc(&desc);
        COM_ERROR(hr);

        g_buffercount = desc.BufferCount;

        // Create the dx11 device from the dx12 device and command queue
        const auto feature_level = D3D_FEATURE_LEVEL_11_0;
        hr = D3D11On12CreateDevice(dx12device, 0, &feature_level, 1, (IUnknown* const*)&dx12queue, 1, 0, &g_d3d11device, &g_d3d11context, nullptr);
        COM_ERROR(hr);

        // verify the device
        hr = g_d3d11device->QueryInterface(IID_PPV_ARGS(&g_d3d11on12device));
        COM_ERROR(hr);

        // get the complete swapchain interface
        hr = swapchain->QueryInterface(IID_PPV_ARGS(&g_d3d12swapchain3));
        COM_ERROR(hr);

        // Create the dx12 buffers
        Buffers = (decltype(Buffers))calloc(g_buffercount, sizeof(*Buffers));
        ASSERT(Buffers != NULL);

        ID3D12DescriptorHeap* heap;
        D3D12_DESCRIPTOR_HEAP_DESC rtvdesc{};
        rtvdesc.NumDescriptors = g_buffercount;
        rtvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hr = dx12device->CreateDescriptorHeap(&rtvdesc, IID_PPV_ARGS(&heap));
        COM_ERROR(hr);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvhandle = heap->GetCPUDescriptorHandleForHeapStart();
        auto rtvsize = dx12device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (unsigned i = 0; i < g_buffercount; ++i) {
                hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&Buffers[i].d3d12RenderTarget));
                COM_ERROR(hr);

                dx12device->CreateRenderTargetView(Buffers[i].d3d12RenderTarget, nullptr, rtvhandle);

                D3D11_RESOURCE_FLAGS flags = { D3D11_BIND_RENDER_TARGET };
                hr = g_d3d11on12device->CreateWrappedResource(Buffers[i].d3d12RenderTarget, &flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, IID_PPV_ARGS(&Buffers[i].d3d11WrappedBackBuffer));
                COM_ERROR(hr);

                hr = g_d3d11device->CreateRenderTargetView(Buffers[i].d3d11WrappedBackBuffer, nullptr, &Buffers[i].d3d11RenderTargetView);
                COM_ERROR(hr);

                rtvhandle.ptr += rtvsize;
        }

        ImGui_ImplDX11_Init(g_d3d11device, g_d3d11context);
        g_initialized = true;
}


extern void DX11_Render() {
        if (g_initialized == false) return;
        const auto index = g_d3d12swapchain3->GetCurrentBackBufferIndex();
        ASSERT(index < g_buffercount);
        const auto b = &Buffers[index];
        
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        draw_gui();
        
        ImGui::Render();
        g_d3d11on12device->AcquireWrappedResources(&b->d3d11WrappedBackBuffer, 1);
        g_d3d11context->OMSetRenderTargets(1, &b->d3d11RenderTargetView, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_d3d11on12device->ReleaseWrappedResources(&b->d3d11WrappedBackBuffer, 1); //NOTE: this was array and size, but i made a struct of thses!
        g_d3d11context->Flush();
}


extern void DX11_Release() {
        if (g_initialized == false) return;

        for (unsigned i = 0; i < g_buffercount; ++i) {
                if (Buffers[i].d3d11RenderTargetView) Buffers[i].d3d11RenderTargetView->Release();
                if (Buffers[i].d3d11WrappedBackBuffer) Buffers[i].d3d11WrappedBackBuffer->Release();
                if (Buffers[i].d3d12RenderTarget) Buffers[i].d3d12RenderTarget->Release();
        }

        g_d3d11context->Flush(); //necessary??
        ImGui_ImplDX11_Shutdown();
        g_d3d11device->Release();
        g_d3d11context->Release();
        g_d3d11on12device->Release();
        g_initialized = false;
}