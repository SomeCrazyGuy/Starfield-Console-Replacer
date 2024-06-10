#pragma once
#include "main.h"


// public api
extern constexpr const struct callback_api_t* GetCallbackAPI();


typedef enum CallbackType {
        CALLBACKTYPE_DRAW,
        CALLBACKTYPE_CONFIG,
        CALLBACKTYPE_HOTKEY,
} CallbackType;


typedef union CallbackFunction {
        DRAW_CALLBACK draw_callback;
        CONFIG_CALLBACK config_callback;
        HOTKEY_CALLBACK hotkey_callback;
} CallbackFunction;


extern const RegistrationHandle* CallbackGetHandles(CallbackType type, uint32_t* out_count);
extern const char* CallbackGetName(RegistrationHandle handle);
extern CallbackFunction CallbackGetCallback(CallbackType type, RegistrationHandle handle);
