#pragma once

struct GUIInterface {
        void (*Initialize)(void* swapchain, void* commandqueue);
        void (*Render)(void);
        void (*Release)(void);
};

// In d3d11on12ui.cpp - draw the imgui interface using the dx11 to dx12 translation layer
const struct GUIInterface* Interface_dx11(void);

// in dx12ui.cpp - draw the imgui interface using native dx12
const struct GUIInterface* Interface_dx12(void);

// in windowui.cpp - draw the ingui interface in a completely different window
const struct GUIInterface* Interface_separate_window(void);