#include "main.h"

//public api
extern const struct config_api_t* GetConfigAPI();

//private api
extern void LoadSettingsRegistry();
extern void SaveSettingsRegistry();

extern void ConfigSetMod(const char* mod_name);
extern void draw_settings_tab();