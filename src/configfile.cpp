#include "configfile.h"

//for logapi
#include "console.h"

#include <string>
#include <vector>
#include <unordered_map>


#define CONFIG_FILE_PATH ".\\Data\\SFSE\\Plugins\\BetterConsoleConfig.txt"


//TODO: refactor config api and promote to betterapi.h
enum class ValueType : uint32_t {
        VT_INT,
        VT_STRING
};

struct Value {
        char* FileData; // Pointer to the character after the '=' in the config file
        void* Data;     // Pointer to the bound datatype
        ValueType Type; // Type of Data
        uint32_t Size;  // Size of Data
};

// A mapping of string keys to values
std::unordered_map<std::string, Value> Settings;


static void LoadConfig(const char* filename) {
        FILE* f = NULL;
        fopen_s(&f, filename, "rb");
        if (!f) return;

        fseek(f, 0, SEEK_END);
        auto size = ftell(f);
        if (size < 0) return;
        fseek(f, 0, SEEK_SET);

        char* s = (char*)malloc(size + 16);
        if (s == NULL) return;
        fread(s, size, 1, f);
        memset(&s[size], 0, 16);
        fclose(f);

        //parse "key=value" pairs
        for (uint32_t i = 0; i < size; ++i) {
                //skip any whitespace at the beginning of the line
                while (isspace(s[i])) continue;

                
                if (isalnum(s[i])) {
                        //read identifier
                        auto start = i;
                        while (isalnum(s[i])) ++i;
                        auto end = i;

                        //move identifier into string
                        std::string key{&s[start], (end - start)};

                        //skip until '='
                        while (isspace(s[i])) ++i;
                        ASSERT(s[i] == '=');

                        //init the value
                        Value v{};
                        v.FileData = &s[i];

                        //add setting to map
                        Settings[key] = v;

                        //skip until newline
                        while (s[i] && (s[i] != '\n')) ++i;
                        s[i] = '\0'; //add null terminator
                }
        }
}


void BindConfigInt(const char* name, int* value) {
        std::string key{name};

        auto& v = Settings[key];
        ASSERT(v.Data == NULL); //cant bind data twice

        v.Data = (void*)value;
        v.Type = ValueType::VT_INT;
        
        // if "name:category" was found, parse the value:
        if (v.FileData) {
                auto val = strtol(v.FileData, NULL, 0);
                *value = (int)val;
        }
}


void SaveConfig(const char* filename) {
        FILE* f = nullptr;
        fopen_s(&f, filename, "wb");
        ASSERT(f != nullptr);

        for (const auto& x : Settings) {
                const char* key = x.first.c_str();
                const void* val = x.second.Data;

                if (!val) {
                        fprintf(f, "%s=%s\n", key, x.second.FileData);
                        continue;
                }

                switch (x.second.Type) {
                case ValueType::VT_INT:
                        fprintf(f, "%s=%d\n", key, *(int*)val);
                        break;
                case ValueType::VT_STRING:
                        fprintf(f, "%s=%s\n", key, (char*)val);
                        break;
                }
        }
        fclose(f);
}


static constexpr struct config_api_t Config {
        LoadConfig,
        SaveConfig,
        BindConfigInt,
};

extern const struct config_api_t* GetConfigAPI() {
        return &Config;
}