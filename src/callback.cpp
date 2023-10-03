#include "main.h"

#include "callback.h"

#include <vector>

static DrawCallbacks* DrawHead = nullptr;
static DrawCallbacks* DrawTail = nullptr;

struct HotkeyCallback {
        const char* name;
        HOTKEY_FUNC callback;
};

static std::vector<HotkeyCallback> HotKeys{};

static void RegisterDrawCallbacks(DrawCallbacks* callbacks) {
        callbacks->ll_next = nullptr;

        if (DrawHead == nullptr) {
                callbacks->ll_prev = nullptr;
                DrawHead = callbacks;
                DrawTail = DrawHead;
                return;
        }

        callbacks->ll_prev = DrawTail;
        DrawTail->ll_next = callbacks;
        DrawTail = callbacks;
}

static void RegisterHotkeyCallback(const char* name, HOTKEY_FUNC func) {
        HotKeys.push_back(HotkeyCallback{name, func});
}

static constexpr struct callback_api_t CallbackAPI{
        RegisterDrawCallbacks,
        RegisterHotkeyCallback
};

extern constexpr const struct callback_api_t* GetCallbackAPI() {
        return &CallbackAPI;
}

extern const DrawCallbacks* GetCallbackHead() {
        return DrawHead;
}

extern uint32_t GetHotkeyCount() {
        return (uint32_t)HotKeys.size();
}

extern HOTKEY_FUNC GetHotkeyFunc(uint32_t index) {
        ASSERT(index < HotKeys.size());
        return HotKeys[index].callback;
}