#include "main.h"

struct config_var_t;

/// <summary>
/// Use config methods to extend the config registry with user defined types.
/// You must implement at least save and load, edit is optional but recommended.
/// </summary>
struct config_methods_t {
        /// <summary>
        /// Serialize a config var and return a pointer to a null terminated buffer
        /// the buffer must not contain the newline character (breaks parser) and
        /// should consist of only printable ascii characters
        /// </summary>
        const char* (*Save)(const struct config_var_t* var);

        /// <summary>
        /// Deserialize a character buffer and write the value to the config var
        /// the buffer will be null terminated, and have whitespace trimmed.
        /// Load is only called if there is an entry to parse, otherwise it is skipped.
        /// </summary>
        void (*Load)(struct config_var_t* var, const char* buffer);

        /// <summary>
        /// Allow the user to edit a config var at runtime.
        /// this callback is optional and may be null to indicate that a var
        /// cannot be edited (it might contain generated data)
        /// </summary>
        void (*Edit)(struct config_var_t* var);
};


/// <summary>
/// this is a holder for the pointer to the variable to be serialized
/// for user defined types, you will likely be casting the void* into your own type 
/// </summary>
union config_value_t {
        void* as_data;
        char* as_string;
        float* as_float;
        int* as_int;
        boolean* as_bool;
};

struct config_var_t {
        const char* name;
        const char* mod_name;
        const char* description;
        union config_value_t data;
        const struct config_methods_t* methods;

        union config_userdata_t {
                char data[24];

                struct {
                        uint32_t data_size;
                } for_data;
                
                struct {
                        int min;
                        int max;
                } for_int;

                struct {
                        float min;
                        float max;
                } for_float;

                struct {
                        uint32_t string_size; 
                } for_string;
        } userdata;
};