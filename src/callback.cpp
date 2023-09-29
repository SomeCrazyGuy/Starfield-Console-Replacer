#include "main.h"

#include "callback.h"

#include <vector>

static DrawCallbacks* DrawHead = nullptr;
static DrawCallbacks* DrawTail = nullptr;

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

static constexpr struct callback_api_t CallbackAPI{
        &RegisterDrawCallbacks
};

extern constexpr const struct callback_api_t* GetCallbackAPI() {
        return &CallbackAPI;
}

extern const DrawCallbacks* GetCallbackHead() {
        return DrawHead;
}