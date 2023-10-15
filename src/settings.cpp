#include "main.h"

#include <vector>
#include <unordered_map>
#include <string>


#define SETTINGS_REGISTRY_PATH ".\\Data\\SFSE\\Plugins\\MiniModMenuRegistry.txt"

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


static std::unordered_map<std::string, std::string> Registry{};
static std::vector<const char*> Handles{};
static std::vector<BoundSetting> Settings{};
static SettingsHandle CurrentValidSettingsHandle = (SettingsHandle) -1;


//foward declare the settings registry initialization function
static void ParseSettingsRegistry();


// note: the settingshandle is invalidated by the next call to OpenSettings() from any code!
//       you should always OpenSettings(), then bind all your settings within the same function
//       then ignore the settingshandle.
//       the same mod should not call OpenSettings() twice, you only get one valid handle
//
// note2: the argument passed to OpenSettings will be the name that shows up in the settings menu
//
// note3: why OpenSettings? there is no CloseSettings... maybe rename to GetSettingHandle or similar?
static SettingsHandle OpenSettings(const char* mod_name) {
        ASSERT(mod_name != NULL);
        ASSERT(mod_name[0] && "mod_name must not be an empty string!");

        //best place to put this
        ParseSettingsRegistry();

        //TODO: a debug build should iterate all handles and string compare to assert no two handles share the same name
        auto ret = (SettingsHandle) Handles.size();
        Handles.push_back(mod_name);
        CurrentValidSettingsHandle = ret;
        return ret;
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
static void BindSettingInt(SettingsHandle handle, const char* name, int* value, int min_value, int max_value, const char* description) {
        ASSERT(handle == CurrentValidSettingsHandle);
        ASSERT(name != NULL);
        ASSERT(name[0]);
        ASSERT(value != NULL);

        BoundSetting s{};
        s.handle = handle;
        s.description = description;
        s.name = name;
        s.value.as_int = value;
        s.type = SettingType::Integer;
        s.min.as_int = min_value;
        s.max.as_int = max_value;
        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += Handles[handle];

        auto find = Registry.find(key);
        if (find == Registry.end()) return;
        *value = (int)strtol(find->second.c_str(), NULL, 0);
}


static void BindSettingFloat(SettingsHandle handle, const char* name, float* value, float min_value, float max_value, const char* description) {
        ASSERT(handle == CurrentValidSettingsHandle);
        ASSERT(name != NULL);
        ASSERT(name[0]);
        ASSERT(value != NULL);

        BoundSetting s{};
        s.handle = handle;
        s.description = description;
        s.name = name;
        s.value.as_float = value;
        s.type = SettingType::Float;
        s.min.as_float = min_value;
        s.max.as_float = max_value;
        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += Handles[handle];

        auto find = Registry.find(key);
        if (find == Registry.end()) return;
        *value = strtof(find->second.c_str(), NULL);
}


static void BindSettingBoolean(SettingsHandle handle, const char* name, boolean* value, const char* description) {
        ASSERT(handle == CurrentValidSettingsHandle);
        ASSERT(name != NULL);
        ASSERT(name[0]);
        ASSERT(value != NULL);

        BoundSetting s{};
        s.handle = handle;
        s.description = description;
        s.name = name;
        s.value.as_boolean = value;
        s.type = SettingType::Boolean;
        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += Handles[handle];

        auto find = Registry.find(key);
        if (find == Registry.end()) return;
        *value = (0 == strcmp(find->second.c_str(), "true"));
}


static void BindSettingString(SettingsHandle handle, const char* name, char* value, uint32_t value_size, const char* description) {
        ASSERT(handle == CurrentValidSettingsHandle);
        ASSERT(name != NULL);
        ASSERT(name[0]);
        ASSERT(value != NULL);

        BoundSetting s{};
        s.handle = handle;
        s.description = description;
        s.name = name;
        s.value.as_string = value;
        s.type = SettingType::String;
        s.value_size = value_size;

        Settings.push_back(s);

        std::string key = name;
        key += ':';
        key += Handles[handle];

        auto find = Registry.find(key);
        if (find == Registry.end()) return;
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
}



static void BindSettingData(SettingsHandle handle, const char* name, void* value, uint32_t value_size, const char* description) {
        ASSERT(handle == CurrentValidSettingsHandle);
        ASSERT(name != NULL);
        ASSERT(name[0]);
        ASSERT(value != NULL);

        BoundSetting s{};
        s.handle = handle;
        s.description = description;
        s.name = name;
        s.value.as_data = value;
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

        while (fgets(max_line, 1024, f)) {
                max_line[1023] = '\0';
                char* s = max_line;

                key.clear();
                value.clear();

                while (*s && isspace(*s)) ++s;
                while (*s && (*s != '=') && !isspace(*s)) key += *s++;
                while (*s && (*s != '=')) ++s;
                ASSERT(*s++ == '=');
                while (*s && isspace(*s)) ++s;
                while (*s) value += *s++;
                while (isspace(value.back())) value.pop_back();

                Registry[key] = value;
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
                key += Handles[s.handle];


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
        BindSettingInt,
        BindSettingFloat,
        BindSettingBoolean,
        BindSettingString,
        BindSettingData
};


extern const struct config_api_t* GetConfigAPI() {
        return &Config;
}


//TODO: implement the following functions for the internal api, or draw using simpledraw?
// gethandlecount()
// gethandlename(handle)
// getsettingscount(handle)
// getboundsetting(handle, index)