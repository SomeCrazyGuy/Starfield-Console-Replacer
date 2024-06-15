#pragma once

#include "../betterapi.h"

// New strategy, the mod menu and console replacer are separating
// need to have only the most minimal binding between the two
extern void setup_console(const BetterAPI* api);

// but I still need to send out the console api to other components
extern const struct console_api_t* GetConsoleAPI();