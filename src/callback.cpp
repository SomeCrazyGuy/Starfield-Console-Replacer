#include "main.h"
#include "callback.h"
#include "hotkeys.h"

#include <vector>


struct ModInfo {
        const char* mod_name;
        DRAW_CALLBACK draw_callback;
        CONFIG_CALLBACK config_callback;
        HOTKEY_CALLBACK hotkey_callback;
};


static std::vector<ModInfo> RegisteredMods{};
static std::vector<RegistrationHandle> DrawHandles{};
static std::vector<RegistrationHandle> ConfigHandles{};
static std::vector<RegistrationHandle> HotkeyHandles{};


extern const RegistrationHandle* CallbackGetHandles(CallbackType type, uint32_t* out_count) {
        uint32_t count = 0;
        RegistrationHandle *ret = nullptr;

        switch (type)
        {
        case CALLBACKTYPE_DRAW:
                count = (uint32_t)DrawHandles.size();
                ret = &DrawHandles[0];
                break;
        case CALLBACKTYPE_CONFIG:
                count = (uint32_t)ConfigHandles.size();
                ret = &ConfigHandles[0];
                break;
        case CALLBACKTYPE_HOTKEY:
                count = (uint32_t)HotkeyHandles.size();
                ret = &HotkeyHandles[0];
                break;
        default:
                DEBUG("Invalid callback type %d", type);
                ASSERT(false && "Invalid callback type");
                break;
        }

        if (out_count) {
                *out_count = count;
        }

        return ret;
}


extern const char* CallbackGetName(RegistrationHandle handle) {
        ASSERT(handle < RegisteredMods.size());
        return RegisteredMods[handle].mod_name;
}


extern CallbackFunction CallbackGetCallback(CallbackType type, RegistrationHandle handle) {
        ASSERT(handle < RegisteredMods.size());
        CallbackFunction ret{};

        switch (type)
        {
        case CALLBACKTYPE_DRAW:
                ret.draw_callback = RegisteredMods[handle].draw_callback;
                break;
        case CALLBACKTYPE_CONFIG:
                ret.config_callback = RegisteredMods[handle].config_callback;
                break;
        case CALLBACKTYPE_HOTKEY:
                ret.hotkey_callback = RegisteredMods[handle].hotkey_callback;
                break;
        default:
                DEBUG("Invalid callback type %d", type);
                ASSERT(false && "Invalid callback type");
                break;
        }

        return ret;
}


static RegistrationHandle RegisterMod(const char* mod_name) {
        ASSERT(mod_name != NULL);
        ASSERT(*mod_name && "mod_name cannot be empty");
        ASSERT(strlen(mod_name) < 32 && "mod_name too long >= 32 characters");
        ASSERT(strlen(mod_name) >= 3 && "mod_name too short < 3 characters");
        //todo check for whitespace and other invalid characters inthe mod name

        const auto ret = (uint32_t) RegisteredMods.size();
        RegisteredMods.push_back({ mod_name });
        return ret;
}


//TODO: allow changing the draw callback without pushing a new handle
static void RegisterDrawCallback(RegistrationHandle owner, DRAW_CALLBACK callback) {
        ASSERT(owner < RegisteredMods.size());
        RegisteredMods[owner].draw_callback = callback;
        DrawHandles.push_back(owner);
}


static void RegisterConfigCallback(RegistrationHandle owner, CONFIG_CALLBACK callback) {
        ASSERT(owner < RegisteredMods.size());
        RegisteredMods[owner].config_callback = callback;
        ConfigHandles.push_back(owner);
}


static void RegisterHotkeyCallback(RegistrationHandle owner, HOTKEY_CALLBACK callback) {
        ASSERT(owner < RegisteredMods.size());
        RegisteredMods[owner].hotkey_callback = callback;
        HotkeyHandles.push_back(owner);
}


// This function is best fit next to the hotkey callback registration, but all of the data
// is in the hotkeys.cpp file, so we forward the call to that function instead of trying to
// share data and state between the two
static void RequestHotkey(RegistrationHandle owner, const char* name, uintptr_t userdata) {
        ASSERT(owner < RegisteredMods.size());
        ASSERT(name != NULL);
        ASSERT(*name && "name cannot be empty");
        ASSERT(strlen(name) < 32 && "name too long >= 32 characters");
        //todo check for whitespace and other invalid characters in the hotkey name

        HotkeyRequestNewHotkey(owner, name, userdata, 0);
}


static constexpr struct callback_api_t CallbackAPI {
        RegisterMod,
        RegisterDrawCallback,
        RegisterConfigCallback,
        RegisterHotkeyCallback,
        RequestHotkey
};


extern constexpr const struct callback_api_t* GetCallbackAPI() {
        return &CallbackAPI;
}
