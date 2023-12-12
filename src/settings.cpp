#include "main.h"

#include <algorithm>
#include <vector>

#include <inttypes.h>

#include "simpledraw.h"
#include "internal_plugin.h"

#define SETTINGS_REGISTRY_PATH ".\\Data\\SFSE\\Plugins\\MiniModMenuRegistry.txt"

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

struct BoundSetting;

struct SettingMethods {
        void (*Save)(const BoundSetting* var, FILE* f);
        void (*Load)(BoundSetting* var, const char* value);
        void (*Edit)(BoundSetting* var);
};


struct BoundSetting {
        const char* name;
        const char* mod_name;
        const char* description;
        const SettingMethods* methods;
        SettingValue value;
        uint32_t mod_id;
        //these below mugh be a more generic union
        MinMax min;
        MinMax max;
        uint32_t value_size; // used for string and data type settings
};


struct ConfigVar {
        uint64_t hash;
        const char* key;
        const char* value;
        void* padding;
};


static std::vector<const char*> ModNames{};
static std::vector<BoundSetting> Registry{};
static std::vector<ConfigVar> ConfigFile{};
const char* CurrentModName = nullptr;
uint32_t CurrentModId = 0;


#define BIND_CHECK(BIND_NAME, BIND_VALUE) do {\
                ASSERT(CurrentModName != nullptr && "A mod forgot to call OpenSettings() before calling any Bind* function!"); \
                ASSERT(BIND_NAME != NULL); \
                ASSERT(BIND_NAME[0] != '\0'); \
                ASSERT(BIND_VALUE != NULL); \
        } while(0)


static uint64_t hash_fnv1a(const char* key, const char* mod_name) {
        uint64_t ret = 0xcbf29ce484222325;

        unsigned char c;
        while (c = *key++) {
                ret ^= c;
                ret *= 0x00000100000001B3;
        }

        if (!mod_name) return ret;

        ret ^= ':';
        ret *= 0x00000100000001B3;

        while (c = *mod_name++) {
                ret ^= c;
                ret *= 0x00000100000001B3;
        }

        return ret;
}

static bool TurboSettingsParser();


// note: the settingshandle is invalidated by the next call to OpenSettings() from any code!
//      you should always OpenSettings(), then bind all your settings, then CloseSettings()
//      preferably all within the same function
//      the same mod should not call OpenSettings() twice
//
// note2: the argument passed to OpenSettings will be the name that shows up in the settings menu
static void OpenSettings(const char* mod_name) {
        ASSERT(mod_name != NULL);
        ASSERT(mod_name[0] && "mod_name must not be an empty string!");
        ASSERT(CurrentModName == NULL && "A mod did not call CloseSettings() after binding settings!");

        //best place to put this ??
        static bool once = true;
        if (once) TurboSettingsParser();
        once = false;

        CurrentModName = mod_name;
        CurrentModId = (uint32_t)ModNames.size();
        ModNames.push_back(mod_name);
}


static void CloseSettings() {
        // TODO: perform all the lookup here actually?
        CurrentModName = nullptr;
}


static const char* LookupVar(const char* name, const char* mod_name) {
        const auto hash = hash_fnv1a(name, mod_name);

        struct Compare {
                bool operator()(const ConfigVar& var, const uint64_t hash) {
                        return var.hash < hash;
                }
                bool operator()(const uint64_t hash, const ConfigVar& var) {
                        return hash < var.hash;
                }
        };

        auto range = std::equal_range(ConfigFile.begin(), ConfigFile.end(), hash, Compare{});

        for (auto it = range.first; it != range.second; ++it) {
                const char* s = name;
                const char* k = it->key;
                while (*s && (*s == *k++)) ++s;
                if (*s) continue;
                if (*k++ != ':') continue;
                const char* m = mod_name;
                while (*m && (*m == *k++)) ++m;
                if (*m) continue;
                if (*k) continue;
                return it->value;
        }

        return NULL;
}


static void BindSettingData(BoundSetting& s, const char* name, void* value, uint32_t value_size, const SettingMethods* methods, const char* description) {
        s.name = name;
        s.mod_name = CurrentModName;
        s.description = description;
        s.value.as_data = value;
        s.methods = methods;
        s.mod_id = CurrentModId;
        s.value_size = value_size;

        const char* val = LookupVar(name, CurrentModName);
        if (val) {
                s.methods->Load(&s, val);
        }

        Registry.push_back(s);
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
       
        const static SettingMethods int_methods = {
                [](const BoundSetting* var, FILE* f){
                        fprintf(f, "%s:%s=%d\n", var->name, var->mod_name, *var->value.as_int);
                },
                [](BoundSetting* var, const char* value){
                        *var->value.as_int = strtol(value, NULL, 0);
                },
                [](BoundSetting* var){
                        ImGui::DragInt(var->name, var->value.as_int, 1.f, var->min.as_int, var->max.as_int);
                },
        };

        if ((min_value | max_value) == 0) {
                min_value = INT32_MIN;
                max_value = INT32_MAX;
        }

        BoundSetting s{};
        s.min.as_int = min_value;
        s.max.as_int = max_value;

        BindSettingData(s, name, value, sizeof(*value), &int_methods, description);
}


static void BindSettingFloat(const char* name, float* value, float min_value, float max_value, const char* description) {
        BIND_CHECK(name, value);

        const static SettingMethods float_methods = {
                [](const BoundSetting* var, FILE* f) {
                        fprintf(f, "%s:%s=%f\n", var->name, var->mod_name, *var->value.as_float);
                },
                [](BoundSetting* var, const char* value) {
                        *var->value.as_float = strtof(value, NULL);
                },
                [](BoundSetting* var) {
                        ImGui::DragFloat(var->name, var->value.as_float, 1.f, var->min.as_float, var->max.as_float);
                },
        };

        if ((min_value == 0.f) && (max_value == 0.f)) {
                min_value = FLT_MIN;
                max_value = FLT_MAX;
        }

        BoundSetting s{};
        s.min.as_float = min_value;
        s.max.as_float = max_value;

        BindSettingData(s, name, value, sizeof(*value), &float_methods, description);
}


static void BindSettingBoolean(const char* name, boolean* value, const char* description) {
        BIND_CHECK(name, value);

        const static SettingMethods boolean_methods = {
                [](const BoundSetting* var, FILE* f) {
                        fprintf(f, "%s:%s=%s\n", var->name, var->mod_name, (*var->value.as_boolean)? "true" : "false");
                },
                [](BoundSetting* var, const char* value) {
                        *var->value.as_float = (*value == 't');
                },
                [](BoundSetting* var) {
                        ImGui::Checkbox(var->name, (bool*)var->value.as_boolean);
                },
        };

        BoundSetting s{};
        BindSettingData(s, name, value, sizeof(*value), &boolean_methods, description);
}


static void BindSettingString(const char* name, char* value, uint32_t value_size, const char* description) {
        BIND_CHECK(name, value);

        const static SettingMethods string_methods = {
                [](const BoundSetting* var, FILE* f) {
                        fprintf(f, "%s:%s=\"%s\"\n", var->name, var->mod_name, var->value.as_string);
                },
                [](BoundSetting* var, const char* value) {
                        if (*value != '"') return;
                        ++value;
                        for (uint32_t i = 0; i < var->value_size; ++i) {
                                if (value[i] == '"') {
                                        var->value.as_string[i] = '\0';
                                        break;
                                }
                                var->value.as_string[i] = value[i];
                        }
                        var->value.as_string[var->value_size - 1] = '\0';
                },
                [](BoundSetting* var) {
                        ImGui::InputText(var->name, var->value.as_string, var->value_size);
                },
        };

        BoundSetting s{};
        BindSettingData(s, name, value, value_size, &string_methods, description);
}


#include <Windows.h>

static bool TurboSettingsParser() {
        bool ret = false;

        //TODO: should use the 'W' variant to handle unsual paths?
        auto hFile = CreateFileA(SETTINGS_REGISTRY_PATH, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
                DEBUG("could not open settings file: %s", SETTINGS_REGISTRY_PATH);
                return false; //file cannot be opened
        }

        LARGE_INTEGER sizeFile;
        if (!GetFileSizeEx(hFile, &sizeFile)) {
                DEBUG("could not get size of settings file: %s", SETTINGS_REGISTRY_PATH);
                CloseHandle(hFile);
                return false;
        }
        uint64_t m = sizeFile.QuadPart;

        unsigned char* f = (unsigned char*) VirtualAlloc(NULL, m, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (f == NULL) {
                DEBUG("could not allocate " PRIu64 " bytes for settings file", m);
                goto E_FILE;
        }

        DWORD readbytes;
        if (!ReadFile(hFile, f, (DWORD)m, &readbytes, NULL) || (readbytes != m)) {
                DEBUG("could not read from settings registry");
                VirtualFree(f, 0, MEM_RELEASE);
                goto E_FILE;
        }

        for(uint64_t p = 0; p < m; ++p) {
                unsigned char* key{ nullptr };
                unsigned char* key_end{ nullptr };
                unsigned char* value{ nullptr };
                unsigned char* value_end{ nullptr };

                //Parse and extract the start of the key
                for (uint64_t i = p; i < m; ++i) {
                        if (f[i] == '\n') continue;
                        if (!isspace(f[i])) {
                                key = &f[i];
                                p = i;
                                break;
                        }
                }

                //find the '=' character (and the end of the key)
                for (uint64_t i = p; i < m; ++i) {
                        if (f[i] == '\n') continue;
                        if (f[i] == '=') {
                                key_end = &f[i - 1];
                                //trim the key end of whitespace
                                while (::isspace(*key_end)) --key_end;
                                *++key_end = 0; //NULL terminate the key component
                                p = ++i; //move past the '=' character
                                break;
                        }
                }

                //find the start of the value
                for (uint64_t i = p; i < m; ++i) {
                        if (f[i] == '\n') continue;
                        if (!isspace(f[i])) {
                                value = &f[i];
                                p = i;
                                break;
                        }
                }

                //find the end of the value
                for (uint64_t i = p; i < m; ++i) {
                        if (f[i] == '\n') {
                                value_end = &f[i];
                                //trim the key end of whitespace
                                while (::isspace(*value_end)) --value_end;
                                *++value_end = 0; //NULL terminate the value component
                                p = i;
                                break;
                        }
                }

                uint64_t hash = hash_fnv1a((const char*)key, NULL);

                LOG("key(%s), hash(%" PRIx64 ") = value(%s)", key, hash, value);
                ConfigFile.push_back(ConfigVar{ hash, (const char*)key, (const char*)value, NULL });
        }

        ret = true;

        std::sort(
                ConfigFile.begin(),
                ConfigFile.end(),
                [](const ConfigVar& A, const ConfigVar& B) -> bool {
                        return A.hash < B.hash;
                }
        );

E_FILE:
        if (!CloseHandle(hFile)) {
                DEBUG("error while closing settings registry");
        }

        return ret;
}




extern void SaveSettingsRegistry() {
        FILE* f = NULL;
        fopen_s(&f, SETTINGS_REGISTRY_PATH, "wb");
        if (f == NULL) return;

        for (const auto& s : Registry) {
                s.methods->Save(&s, f);
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
};


extern const struct config_api_t* GetConfigAPI() {
        return &Config;
}


extern void draw_settings_tab() {
        static const struct simple_draw_t* const SimpleDraw{ GetSimpleDrawAPI() };

        static int selection = -1;
        static UIDataList datalist;
        static uint32_t first_setting;
        static uint32_t last_setting;

        datalist.UserData = ModNames.data();
        datalist.Count = ModNames.size();
        datalist.ToString = [](const void* userdata, uint32_t index, char* fmt, uint32_t fmt_size) -> const char* {
                (void)fmt;
                (void)fmt_size;
                auto names = (const char* const* const)userdata;
                return names[index];
        };

        SimpleDraw->HboxLeft(0.f, 12.f);
        if (SimpleDraw->SelectionList(&datalist, &selection)) {
                struct CompareSelection {
                        bool operator()(const BoundSetting& setting, const int i) {
                                return setting.mod_id < i;
                        }
                        bool operator()(const int i, const BoundSetting& setting) {
                                return i < setting.mod_id;
                        }
                };

                const auto range = std::equal_range(
                        Registry.begin(),
                        Registry.end(),
                        selection,
                        CompareSelection{}
                );

                first_setting = std::distance(Registry.begin(), range.first);
                last_setting = std::distance(Registry.begin(), range.second);
        }
        SimpleDraw->HBoxRight();
        if (selection != -1) {
                // TODO: clipping? or will it be uncommon to have lots of settings in the registry
                for (auto i = first_setting; i != last_setting; ++i) {
                        ImGui::PushID(i);
                        ImGui::Separator();
                        auto& setting = Registry[i];
                        if (setting.description) ImGui::TextUnformatted(setting.description);
                        setting.methods->Edit(&setting);
                        ImGui::PopID();
                }
        }
        SimpleDraw->HBoxEnd();
}

