#pragma once

extern void DX12_Initialize(void* dx12_swapchain, void* dx12_commandqueue);
extern void DX12_Render();
void DX12_Release();