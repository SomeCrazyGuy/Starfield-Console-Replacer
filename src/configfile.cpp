#include "configfile.h"

//for logapi
#include "console.h"

#include <vector>

//TODO: refactor config api and promote to betterapi.h


#define CONFIG_FILE ".\\Data\\SFSE\\Plugins\\BetterConsoleConfig.txt"

struct Config {
        const char* key;
        const char* value;
        int* data;
};

static std::vector<Config> ConfigFile{};
static const auto Log = GetLogAPI()->Log;

//TODO: copy over assert code from other project
extern void ReadConfigFile() {
        FILE* f = NULL;
        fopen_s(&f, CONFIG_FILE, "rb");
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


        //parse simple "key=value" config files
        //TODO: not a real parser, cant handle lots of common formatting issues
        Config c;
        while (*s) {
                c.key = NULL;
                c.value = NULL;
                c.data = 0;

                while (*s == '\n') ++s;
                if (!*s) break;

                //key
                c.key = s;

                // = 
                while (*s && (*s != '=')) ++s;
                *s++ = '\0'; //overwrite '=' and null terminate key

                //value
                c.value = s;
                while (*s && (*s != '\n')) ++s;
                *s++ = '\0'; //overwrite '\n' and null terminate value

                Log("key(%s) = value(%s)", c.key, c.value);
                ConfigFile.push_back(c);
        }
}



//int* must be heap or statically allocated, will read it back for savefile
extern void BindConfigInt(const char* name, int* value) {

        //do dumb search, good enough for the couple values i have now
        for (auto& x : ConfigFile) {
                if (_strcmpi(name, x.key) == 0) {
                        x.data = value; //bind
                        *value = strtol(x.value, NULL, 0);
                        Log("Bind var(%s) to memory (%p)", name, value);
                        return;
                }
        }

        //create key if not found
        Config c;
        c.key = name;
        c.data = value;
        c.value = NULL;
        ConfigFile.push_back(c);
        Log("Adding config var %s = %d", c.key, *c.data);
}

extern void SaveConfigFile() {
        FILE* f = NULL;
        fopen_s(&f, CONFIG_FILE, "wb");
        if (f == NULL) return;

        for (const auto& x : ConfigFile) {
                if (!x.key) { //no key means comment line
                        Log("Save comment %s", x.value);
                        fprintf(f, "%s\n", x.value);
                } else if (x.data) { //only save lines that were bound to a value??
                        Log("Save var %s", x.key);
                        fprintf(f, "%s=%d\n", x.key, *x.data);
                } else {
                        Log("Did not save Value");
                }
        }
        fflush(f);
        fclose(f);
}