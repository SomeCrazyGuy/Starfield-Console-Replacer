#pragma once

#include "main.h"

// public api
extern constexpr const struct callback_api_t* GetCallbackAPI();

//internal API
extern const ModInfo* GetModInfo(size_t* out_count);