#pragma once

#include "main.h"

// public api
extern constexpr const struct callback_api_t* GetCallbackAPI();

//internal api
struct CallbackInfo {
        const char* name;
        FUNC_PTR callback;
};

extern const DrawCallbacks* GetCallbackHead();

extern uint32_t GetHotkeyCount();
extern HOTKEY_FUNC GetHotkeyFunc(uint32_t index);

extern uint32_t GetEveryFrameCallbackCount();
extern FUNC_PTR GetEveryFrameCallback(uint32_t index);