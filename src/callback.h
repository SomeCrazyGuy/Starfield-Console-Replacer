#pragma once

#include "main.h"

// public api
extern constexpr const struct callback_api_t* GetCallbackAPI();

//internal api
extern const DrawCallbacks* GetCallbackHead();
extern uint32_t GetHotkeyCount();
extern HOTKEY_FUNC GetHotkeyFunc(uint32_t index);