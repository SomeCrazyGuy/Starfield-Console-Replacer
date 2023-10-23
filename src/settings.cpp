#include "main.h"

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <string>

#include "simpledraw.h"
#include "internal_plugin.h"

#define SETTINGS_REGISTRY_PATH ".\\Data\\SFSE\\Plugins\\MiniModMenuRegistry.txt"


// Opaque handle for the settings save/load system
typedef uint32_t SettingsHandle;

enum class SettingType : uint32_t {
        Undefined,
        Integer,
        Boolean,
        Float,
        String,
        Data
};


union SettingValue {
        void* as_data;
        int* as_int;
        boolean* as_boolean;
        float* as_float;
        char* as_string;
};

union MinMax {
        int as_int;
        float as_float;
};


struct BoundSetting {
        const char* name;
        const char* description;
        SettingValue value;
        SettingsHandle handle;
        SettingType type;
        MinMax min;
        MinMax max;
        uint32_t value_size; // used for string and data type settings
        char padding[4];
};

constexpr SettingsHandle InvalidSettingsHandle = 0xFFFFFFFF;

static std::unordered_map<std::string, std::string> Registry{};
static ItemArray* Handles{nullptr};
static std::vector<BoundSetting> Settings{};
static SettingsHandle CurrentValidSettingsHandle = InvalidSettingsHandle;


#define BIND_CHECK(BIND_NAME, BIND_VALUE) do {\
                ASSERT(CurrentValidSettingsHandle != InvalidSettingsHandle && "A mod forgot to call OpenSettings() before calling any Bind* function!"); \
                ASSERT(BIND_NAME != NULL); \
                ASSERT(BIND_NAME[0] != '\0'); \
                ASSERT(BIND_VALUE != NULL); \
        } while(0)

//foward declare the settings registry initialization function
static void ParseSettingsRegistry();


// note: the settingshandle is invalidated by the next call to OpenSettings() from any code!
//       you should always OpenSettings(), then bind all your settings within the same function
//       then ignore the settingshandle.
//       the same mod should not call OpenSettings() twice, you only get one valid handle
//
// note2: the argument passed to OpenSettings will be the name that shows up in the settings menu
static void OpenSettings(const char* mod_name) {
        ASSERT(mod_name != NULL);
        ASSERT(mod_name[0] && "mod_name must not be an empty string!");
        ASSERT(CurrentValidSettingsHandle == -1 && "A mod did not call CloseSettings() after binding settings!");

        //best place to put this
        
        ParseSettingsRegistry();

        if (!Handles) {
                Handles = ItemArray_Create(sizeof(const char*));
        }

        //TODO: a debug build should iterate all handles and string compare to assert no two handles share the same name
        auto ret = (SettingsHandle) Handles->count;
        //Handles.push_back(mod_name);
        ItemArray_PushBack(Handles, ITEMARRAY_TO_ITEM(mod_name));
        CurrentValidSettingsHandle = ret;
}


static void CloseSettings() {
        CurrentValidSettingsHandle = InvalidSettingsHandle;
}


/// <summary>
/// Bind the int pointed to by `value` to the string `name` in the settings registry.
/// If the settings registry contains an entry for `name`, then its value is parsed and *value is modified.
/// 
/// </summary>
/// <param name="handle"> -- The valid handle returned by opensettings </param>
/// <param name="name"> -- The name of the setting.
///                     Use only ascii characters, numbers, and underscore '_' in the setting name.
///                     Must be unique across all settings in the setting registry!
///                     Consider namespacing all your setting names with the name of your mod like so:
///                     "SettingName:ModName", the UI will remove the ":ModName" part in the settings menu.
/// </param>
/// <param name="value"> -- A pointer to a static, global, or heap allocated int</param>
/// <param name="min_value"> -- The minimum allowed value for `value`</param>
/// <param name="max_value"> -- The maximum allowed value for `value`</param>
/// <param name="description"> -- Optional description text for the setting shown to the user in the setting menu</param>
static void BindSettingInt(const char* name, int* value, int min_value, int max_value, const char* description) {
        BIND_CHECK(name, value);
        
        BoundSetting s{ name, description, SettingValue{value}, CurrentValidSettingsHandle };
        s.type = SettingType::Integer;
        s.min.as_int = min_value;
        s.max.as_int = max_value;
        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += ITEMARRAY_FROM_ITEM(const char*, ItemArray_At(Handles, s.handle));

        auto find = Registry.find(key);
        if (find == Registry.end()) {
                LOG("Key (%s) not found in setting registry", key.c_str());
                return;
        }
        *value = (int)strtol(find->second.c_str(), NULL, 0);
        LOG("Key (%s) found in registry with value (%d)", key.c_str(), *value);
}


static void BindSettingFloat(const char* name, float* value, float min_value, float max_value, const char* description) {
        BIND_CHECK(name, value);

        BoundSetting s{ name, description, SettingValue{value}, CurrentValidSettingsHandle};
        s.type = SettingType::Float;
        s.min.as_float = min_value;
        s.max.as_float = max_value;
        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += ITEMARRAY_FROM_ITEM(const char*, ItemArray_At(Handles, s.handle));

        auto find = Registry.find(key);
        if (find == Registry.end()) {
                LOG("Key (%s) not found in setting registry", key.c_str());
                return;
        }
        *value = strtof(find->second.c_str(), NULL);
        LOG("Key (%s) found in registry with value (%f)", key.c_str(), *value);
}


static void BindSettingBoolean(const char* name, boolean* value, const char* description) {
        BIND_CHECK(name, value);

        BoundSetting s{ name, description, SettingValue{value}, CurrentValidSettingsHandle };
        s.type = SettingType::Boolean;
        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += ITEMARRAY_FROM_ITEM(const char*, ItemArray_At(Handles, s.handle));

        auto find = Registry.find(key);
        if (find == Registry.end()) {
                LOG("Key (%s) not found in setting registry", key.c_str());
                return;
        }
        *value = (0 == strcmp(find->second.c_str(), "true"));
        LOG("Key (%s) found in registry with value (%s)", key.c_str(), (*value)? "true" : "false");
}


static void BindSettingString(const char* name, char* value, uint32_t value_size, const char* description) {
        BIND_CHECK(name, value);

        BoundSetting s{ name, description, SettingValue{value}, CurrentValidSettingsHandle };
        s.type = SettingType::String;
        s.value_size = value_size;

        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += ITEMARRAY_FROM_ITEM(const char*, ItemArray_At(Handles, s.handle));

        auto find = Registry.find(key);
        if (find == Registry.end()) {
                LOG("Key (%s) not found in setting registry", key.c_str());
                return;
        }
        const char* str = find->second.c_str();

        if (*str == '"') {
                ++str;
                for (uint32_t i = 0; i < value_size; ++i) {
                        if (*str == '"') {
                                value[i] = '\0';
                                return;
                        }
                        value[i] = *str;
                }
                memset(value, 0, value_size);
        }

        LOG("Key (%s) found in registry with value (%s)", key.c_str(), value);
}



static void BindSettingData(const char* name, void* value, uint32_t value_size, const char* description) {
        BIND_CHECK(name, value);

        BoundSetting s{ name, description, SettingValue{value}, CurrentValidSettingsHandle };
        s.type = SettingType::Data;
        s.value_size = value_size;

        Settings.push_back(s);

        //TODO: lookup
        ASSERT(false && "Not Implemented!");
}


static void ParseSettingsRegistry() {
        static bool registry_parsed = false;
        if (registry_parsed) return;
        registry_parsed = true;

        FILE* f = NULL;
        fopen_s(&f, SETTINGS_REGISTRY_PATH, "rb");
        if (f == NULL) return;

        char max_line[1024]; //maybe note this limitation somewhere

        std::string key;
        std::string value;

        // Ultra minimal "key = value" parser, can skip blank (whitespace only) lines, comments are not supported
        // Because key-value pairs are one per line, newlines '\n' are not allowed in the key or value
        // The key may not end with whitespace characters (they will be trimmed), and must not contain null or '='
        // The value may not end with whitespace characters (they will be trimmed), and must not contain null
        // Only 1023 characters are read from each line (tweakable)
        while (fgets(max_line, 1024, f)) {                             //max characters per line is 1024 - 1 for null terminator
                char* s = max_line;                                    //alias the line buffer for terse-ness
                key.clear();                                           //make sure key is cleared from previous iteration
                value.clear();                                         //make sure value is cleared from previous iteration
                while (*s && isspace(*s)) ++s;                         //skip whitespace before the key
                if (!*s) continue;                                     //skip blank lines
                while (*s && (*s != '=')) key += *s++;                 //consume key token
                while (isspace(key.back())) key.pop_back();            //strip trailing whitespace from key
                ASSERT(*s++ == '=');                                   //make sure we found the '='
                while (*s && isspace(*s)) ++s;                         //skip whitespace between '=' and the start of value
                while (*s) value += *s++;                              //consume the rest of the line
                while (isspace(value.back())) value.pop_back();        //trim whitespace from the value
                Registry[key] = value;                                 //register the key/value pair
                LOG("Adding key (%s) to settings registry with value (%s)", key.c_str(), value.c_str());
        }
        fclose(f);
}


extern void SaveSettingsRegistry() {
        FILE* f = NULL;
        fopen_s(&f, SETTINGS_REGISTRY_PATH, "wb");
        if (f == NULL) return;

        for (const auto& s : Settings) {
                std::string key = s.name;
                key += ':';
                key += ITEMARRAY_FROM_ITEM(const char*, ItemArray_At(Handles, s.handle));;


                switch (s.type) {
                case SettingType::Undefined:
                        ASSERT(false && "Unreachable!");
                        break;
                case SettingType::Integer:
                        fprintf(f, "%s=%d\n", key.c_str(), *s.value.as_int);
                        break;
                case SettingType::Float:
                        fprintf(f, "%s=%f\n", key.c_str(), *s.value.as_float);
                        break;
                case SettingType::Boolean:
                        fprintf(f, "%s=%s\n", key.c_str(), (*s.value.as_boolean) ? "true" : "false");
                        break;
                case SettingType::String:
                        fprintf(f, "%s=\"%s\"\n", key.c_str(), s.value.as_string);
                        break;
                case SettingType::Data:
                        ASSERT(false && "Unimplemented!");
                        break;
                }
        }

        fclose(f);
}


static constexpr const struct config_api_t Config = {
        OpenSettings,
        CloseSettings,
        BindSettingInt,
        BindSettingFloat,
        BindSettingBoolean,
        BindSettingString,
        BindSettingData
};


extern const struct config_api_t* GetConfigAPI() {
        return &Config;
}




// Only the settings.cpp file knows about the internal configuration of a setting.
// It is less offensive to have this file also render the UI for settings as well.
static void gui_edit_setting(BoundSetting& S) {
        ImGui::Separator();
        if (S.description) ImGui::Text(S.description);

        switch (S.type) {
        case SettingType::Undefined:
                ASSERT(false && "Undefined Setting!");
                break;

        case SettingType::Integer:
                // TODO: min / max value
                ImGui::DragInt(S.name, S.value.as_int);
                break;

        case SettingType::Float:
                // TODO: min / max value
                ImGui::DragFloat(S.name, S.value.as_float);
                break;

        case SettingType::Boolean:
                ImGui::Checkbox(S.name, (bool*)S.value.as_boolean);
                break;

        case SettingType::String:
                ImGui::InputText(S.name, S.value.as_string, S.value_size);
                break;

        case SettingType::Data:
                ASSERT(false && "Not Implemented!");
                break;

        default:
                ASSERT(false && "Setting Edit Not Implemented!");
                break;
        }
}


extern void draw_settings_tab() {
        static const struct simple_draw_t* const SimpleDraw{ GetSimpleDrawAPI() };

        static int selection = -1;

        SimpleDraw->HboxLeft(0.f, 12.f);
        SimpleDraw->SelectionList(
                &selection,
                Handles,
                (int)Handles->count,
                [](const void* items, uint32_t index, char* buffer, uint32_t buffer_size) -> const char* {
                        (void)buffer;
                        (void)buffer_size;
                        return ITEMARRAY_FROM_ITEM(const char*, ItemArray_At((const ItemArray*)items, index));
                });
        SimpleDraw->HBoxRight();
        if (selection != -1) {
                struct CompareSelection {
                        bool operator()(const BoundSetting& setting, const int i) {
                                return setting.handle < i;
                        }
                        bool operator()(const int i, const BoundSetting& setting) {
                                return i < setting.handle;
                        }
                };

                const auto range = std::equal_range(
                        Settings.begin(),
                        Settings.end(),
                        selection,
                        CompareSelection{}
                 );

                // TODO: clipping? or will it be uncommon to have lots of settings in the registry
                for (auto it = range.first; it != range.second; ++it) {
                        ImGui::PushID(it->name);
                        gui_edit_setting(*it);
                        ImGui::PopID();
                }

                
        }
        SimpleDraw->HBoxEnd();
}

