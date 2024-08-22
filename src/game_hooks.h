#pragma once
#include "main.h"

//public api
BC_EXPORT const struct gamehook_api_t* GetGameHookAPI();

// when game launches:
extern void GameHook_Init();

//internal api
extern LogBufferHandle GameHook_GetConsoleInputHandle();