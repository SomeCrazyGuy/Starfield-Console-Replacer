#pragma once
#include "main.h"

typedef struct GameHookData {
        void (*ConsoleRun)(const char* command);
        void* (*GetFormByID)(const char* form_identifier);
        const char* (*GetFormName)(void* form);
        bool* (*GetGamePausedFlag)();
        bool* ConsoleReadyFlag;
        LogBufferHandle ConsoleInput;
        LogBufferHandle ConsoleOutput;
        bool ConsoleOutputHooked;
} GameHookData;


BC_EXPORT const struct gamehook_api_t* GetGameHookAPI();

extern const GameHookData* GameHook_GetData(void);

// when game launches:
extern void GameHook_Init();