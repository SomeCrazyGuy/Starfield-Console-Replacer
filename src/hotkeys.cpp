#include "main.h"
#include "hotkeys.h"
#include "callback.h"
#include "simpledraw.h"
#include "settings.h"

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <string>

#include <Windows.h>


#define HOTKEY_FILE_NAME "BetterConsoleHotkeys.txt"

static const auto Config = GetConfigAPI();


struct HotKey {
        unsigned owner; // handle to mod that owns hotkey
        unsigned set_key; //the key combo that this hotkey is set to
        uintptr_t userdata;
        char name[32];
};
static constexpr bool operator < (const HotKey& A, const HotKey& B) { return A.owner < B.owner; }
static constexpr bool operator < (const unsigned A, const HotKey& B) { return A < B.owner; }
static constexpr bool operator < (const HotKey& A, const unsigned B) { return A.owner < B; }


static std::unordered_map<unsigned, unsigned> ActiveHotkeys{}; //mapping <input, hotkey index>
static std::vector<HotKey> AllHotkeys{};
static bool cache_rebuild_needed = true;


// when setting a hotkey in the UI, this will be the index of the hotkey the user wants to set
static unsigned index_waiting_for_input = UINT32_MAX;


// my simpledraw wrapper for imgui has some nice features that make this part particularly easy
const static auto SimpleDraw = GetSimpleDrawAPI();


// this is how hotkeys are put together
static uint16_t make_hotkey(unsigned vk_key, bool control, bool alt, bool shift) {
        return (vk_key & 0xFF) | ((control & 1) << 10) | ((alt & 1) << 9) | ((shift & 1) << 8);
}


//get the human readable text of a virtual key code
static const char* GetHotkeyText(char* tmp_buffer, unsigned tmp_size, uint16_t hotkey) {
        const auto bytes = snprintf(tmp_buffer,
                tmp_size,
                "%s%s%s",
                (hotkey & 0x400) ? "Control + " : "",
                (hotkey & 0x200) ? "Alt + " : "",
                (hotkey & 0x100) ? "Shift + " : ""
                
        );
        hotkey &= 0xFF;

        if (bytes < 0) return "(error)";

        // Get the scan code for the virtual key code
        UINT scanCode = MapVirtualKeyW(hotkey, MAPVK_VK_TO_VSC);

        // Get the name of the key
        LONG lParam = (scanCode << 16);
        int result = GetKeyNameTextA(lParam, tmp_buffer + bytes, tmp_size - bytes);

        if (result == 0) {
                // If GetKeyNameTextA fails, provide a default fallback
                snprintf(tmp_buffer + bytes, tmp_size - bytes, "0x%X", hotkey & 0xFF);
        }

        return tmp_buffer;
}


// its possible that several mods will all add several hotkeys at once,
// so we add them to the array now and set a dirty flag to rebuild the cache
extern void HotkeyRequestNewHotkey(RegistrationHandle owner, const char* name, uintptr_t userdata, unsigned forced_hotkey) {
        HotKey hotkey{};
        hotkey.owner = owner;
        hotkey.set_key = forced_hotkey;
        snprintf(hotkey.name, sizeof(hotkey.name), "%s", name);
        hotkey.userdata = userdata;
        AllHotkeys.push_back(hotkey);
        cache_rebuild_needed = true;
}



static void rebuild_hotkey_cache() {
        cache_rebuild_needed = false;
        
        char tmp_buffer[128];

        ActiveHotkeys.clear();
        std::sort(AllHotkeys.begin(), AllHotkeys.end());

        ConfigSetMod("(hotkeys)");
        for (unsigned i = 0; i < AllHotkeys.size(); ++i) {
                const auto h = &AllHotkeys[i];
                snprintf(tmp_buffer, sizeof(tmp_buffer), "%s-%s", CallbackGetName(h->owner), h->name);
                Config->ConfigU32(ConfigAction_Read, tmp_buffer, &h->set_key);
                if (h->set_key) {
                        ActiveHotkeys[h->set_key] = i;
                }
        }
}


// wndproc calls this for every WM_KEYDOWN message
extern bool HotkeyReceiveKeypress(unsigned vk_key) {
        if(vk_key == VK_SHIFT || vk_key == VK_CONTROL || vk_key == VK_MENU) return false;

        const bool control = GetAsyncKeyState(VK_CONTROL) < 0;
        const bool alt = GetAsyncKeyState(VK_MENU) < 0;
        const bool shift = GetAsyncKeyState(VK_SHIFT) < 0;
        const auto key = make_hotkey(vk_key, control, alt, shift);

        // first check if we are waiting for a keypress to set a hotkey
        // this takes priority over rebuilding the cache
        if (index_waiting_for_input != UINT32_MAX) {
                AllHotkeys[index_waiting_for_input].set_key = key;
                ActiveHotkeys[key] = index_waiting_for_input;
                index_waiting_for_input = UINT32_MAX;
                return true;
        }

        //check to see if the cache needs to be rebuilt
        if (cache_rebuild_needed) {
                rebuild_hotkey_cache();
        }
        
        //otherwise check if we are trying to call a hotkey
        const auto indexref = ActiveHotkeys.find(key);
        if (indexref != ActiveHotkeys.end()) {
                const auto hotkey = &AllHotkeys[indexref->second];
                DEBUG("Activating hotkey: %s", hotkey->name);
                const auto callback = CallbackGetCallback(CALLBACKTYPE_HOTKEY, hotkey->owner);
                callback.hotkey_callback(hotkey->userdata);
                return true;
        }

        return false;
}

// just for use in gui.cpp, it would be too cumbersome to try to split the hotkey code from the hotkey UI
extern void draw_hotkeys_tab() {
        // no hotkeys? nothing to do
        if (AllHotkeys.empty()) {
                SimpleDraw->Text("No hotkeys requested!");
                return;
        }

        // hotkeys change since last time? rebuild the cache
        if (cache_rebuild_needed) {
                rebuild_hotkey_cache();
        }

        // mods can only register one callback for hotkeys, so the callbacks list
        // has only unique owners
        uint32_t num_handles;
        const auto hot_handles = CallbackGetHandles(CALLBACKTYPE_HOTKEY, &num_handles);

        const auto to_string = [](const void* handles, uint32_t index, char*, uint32_t) noexcept -> const char* {
                const auto u = (const RegistrationHandle*)handles;
                return CallbackGetName(u[index]);
                };

        
        static uint32_t selected = UINT32_MAX;
        static uint32_t infos_index = 0;
        static uint32_t index_count = 0;

        SimpleDraw->HBoxLeft(0.f, 12.f);
        if (SimpleDraw->SelectionList(&selected, hot_handles, num_handles, to_string)) {
                const auto range = std::equal_range(AllHotkeys.begin(), AllHotkeys.end(), (uint32_t)hot_handles[selected]);
                infos_index = std::distance(AllHotkeys.begin(), range.first);
                index_count = std::distance(range.first, range.second);
        }
        SimpleDraw->HBoxRight();
        enum table_columns { TC_HOTKEY_NAME, TC_HOTKEY_COMBO, TC_BUTTON, TC_COUNT };
        
        const char* table_headers[TC_COUNT]{};
        table_headers[TC_HOTKEY_NAME] = "Hotkey Name";
        table_headers[TC_HOTKEY_COMBO] = "Hotkey Combo";

        static const auto table_cell_renderer = [](uintptr_t table_userdata, int cur_row, int cur_col) {
                ASSERT(cur_col < TC_COUNT);
                const auto base_index = (unsigned)table_userdata;
                const auto hotkey_index = base_index + cur_row;
                const auto hotkey = &AllHotkeys[hotkey_index];
                switch (cur_col) {
                case TC_HOTKEY_NAME:
                        SimpleDraw->Text(hotkey->name);
                        break;
                case TC_HOTKEY_COMBO:
                        if (hotkey->set_key) {
                                char combo[128];
                                SimpleDraw->Text(GetHotkeyText(combo, sizeof(combo), hotkey->set_key));
                        }
                        else {
                                SimpleDraw->Text("no key combo set");
                        }
                        break;
                case TC_BUTTON:
                        if (hotkey->set_key) {
                                if (SimpleDraw->Button("Unset Key Combo")) {
                                        ActiveHotkeys.erase(hotkey->set_key);
                                        hotkey->set_key = 0;
                                }
                        } else {
                                if (index_waiting_for_input == hotkey_index) {
                                        SimpleDraw->Text("Press A key combo");
                                } else if (SimpleDraw->Button("Set key Combo")) {
                                        index_waiting_for_input = hotkey_index;
                                }
                        }
                        break;
                }
                };
        SimpleDraw->Table(table_headers, TC_COUNT, infos_index, index_count, table_cell_renderer);
        SimpleDraw->HBoxEnd();
}


extern void HotkeySaveSettings() {
        if (cache_rebuild_needed) {
                rebuild_hotkey_cache();
        }

        ConfigSetMod("(hotkeys)");
        char tmp_buffer[128];
        for (auto& info : ActiveHotkeys) {
                const auto hotkey = &AllHotkeys[info.second];
                snprintf(tmp_buffer, sizeof(tmp_buffer), "%s-%s", CallbackGetName(hotkey->owner), hotkey->name);
                uint32_t tmp = info.first;
                Config->ConfigU32(ConfigAction_Write, tmp_buffer, &tmp);
        }
}