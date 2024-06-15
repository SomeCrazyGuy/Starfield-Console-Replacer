#pragma once

extern void DX11_InitializeOrRender(void *dx12_swapchain, void *dx12_commandqueue);
extern void DX11_ReleaseIfInitialized();