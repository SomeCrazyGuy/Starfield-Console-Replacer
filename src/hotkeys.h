#pragma once

extern void draw_hotkeys_tab();
extern void HotkeyReceiveKeypress(unsigned vk_key, bool control, bool alt, bool shift);
extern void HotkeySaveSettings(); // call when normal settings are saved
extern void HotkeyRequestNewHotkey(RegistrationHandle owner, const char* name, uintptr_t userdata, unsigned forced_hotkey);