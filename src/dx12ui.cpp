#include "main.h"
#include "../imgui/imgui_impl_dx12.h"
#include "../imgui/imgui_impl_win32.h"
#include "gui.h"
#include <dxgi1_6.h>
#include <d3d12.h>

struct FrameContext {
        ID3D12CommandAllocator* commandAllocator{ nullptr };
        ID3D12Resource* mainRenderTargetResource{ nullptr };
        D3D12_CPU_DESCRIPTOR_HANDLE mainRenderTargetDescriptor;
        UINT64 fenceValue{ 0 }; //TODO: use this
};

struct RenderState {
        ID3D12Device* device{ nullptr };
        IDXGISwapChain3* swapchain{ nullptr };
        ID3D12CommandQueue* commandQueue{ nullptr };
        ID3D12DescriptorHeap* descriptorHeapBackBuffers{ nullptr };
        ID3D12DescriptorHeap* descriptorHeapImGuiRender{ nullptr };
        ID3D12GraphicsCommandList* commandList{ nullptr };
        FrameContext* frameContext{ nullptr };
        UINT bufferCount{ 0 };
};

RenderState state;

extern void DX12_Initialize(void* dx12_swapchain, void* dx12_commandqueue) {
        const auto Swapchain = (IDXGISwapChain*)dx12_swapchain;

        state.commandQueue = (ID3D12CommandQueue*)dx12_commandqueue;

        Swapchain->QueryInterface(IID_PPV_ARGS(&state.swapchain));
        Swapchain->GetDevice(IID_PPV_ARGS(&state.device));
        ASSERT(state.device != NULL);

        DXGI_SWAP_CHAIN_DESC sdesc;
        Swapchain->GetDesc(&sdesc);

        state.bufferCount = sdesc.BufferCount;

        free(state.frameContext);
        state.frameContext = (FrameContext*)calloc(state.bufferCount, sizeof(FrameContext));

        D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
        descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorImGuiRender.NumDescriptors = state.bufferCount;
        descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        state.device->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&state.descriptorHeapImGuiRender));

        ID3D12CommandAllocator* allocator = nullptr;
        state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        ASSERT(allocator != NULL);
        for (size_t i = 0; i < state.bufferCount; i++) {
                state.frameContext[i].commandAllocator = allocator;
        }

        state.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&state.commandList));

        D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers{};
        descriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descriptorBackBuffers.NumDescriptors = state.bufferCount;
        descriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descriptorBackBuffers.NodeMask = 1;

        state.device->CreateDescriptorHeap(&descriptorBackBuffers, IID_PPV_ARGS(&state.descriptorHeapBackBuffers));

        const auto rtvDescriptorSize = state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = state.descriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < state.bufferCount; i++) {
                ID3D12Resource* pBackBuffer = nullptr;
                state.frameContext[i].mainRenderTargetDescriptor = rtvHandle;
                Swapchain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
                state.device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
                state.frameContext[i].mainRenderTargetResource = pBackBuffer;
                rtvHandle.ptr += rtvDescriptorSize;
        }

        ImGui_ImplDX12_Init(state.device, state.bufferCount,
                DXGI_FORMAT_R8G8B8A8_UNORM, state.descriptorHeapImGuiRender,
                state.descriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
                state.descriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart());

        ImGui_ImplDX12_CreateDeviceObjects();
}


extern void DX12_Render() {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        draw_gui();

        ImGui::Render();
        FrameContext& currentFrameContext = state.frameContext[state.swapchain->GetCurrentBackBufferIndex()];
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

void DX12_Release() {
        //lol
}