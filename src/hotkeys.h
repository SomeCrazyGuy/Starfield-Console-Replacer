#pragma once


//TODO: register hotkeys as an internal plugin
//      then update the settings and gui api to allow
//      creating mod menu tabs
extern void draw_hotkeys_tab();
extern void HotkeyReceiveKeypress(unsigned vk_key);
extern void HotkeySaveSettings(); // call when normal settings are saved
extern void HotkeyRequestNewHotkey(RegistrationHandle owner, const char* name, uintptr_t userdata, unsigned forced_hotkey);