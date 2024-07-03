#pragma once
#include "main.h"

typedef struct GameHookData {
        void (*ConsoleRun)(const char* command);
        void* (*GetFormByID)(const char* form_identifier);

        //Using this scheme: https://falloutck.uesp.net/wiki/Template:INI:Papyrus:sTraceStatusOfQuest
        const char* (*GetFormName)(void* form);
        
        bool* (*GetGamePausedFlag)();

        bool* ConsoleReadyFlag;
        LogBufferHandle ConsoleInput;
        LogBufferHandle ConsoleOutput;
        bool ConsoleOutputHooked;
} GameHookData;




extern const GameHookData* GameHook_GetData(void);

// when game launches:
extern void GameHook_Init();