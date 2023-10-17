#include "main.h"

//public api
extern const struct config_api_t* GetConfigAPI();

//internal api
extern void SaveSettingsRegistry();
extern void draw_settings_tab();