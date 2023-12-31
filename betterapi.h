#ifndef BETTERAPI_API_H
#define BETTERAPI_API_H

#include <stdint.h>

/******************************************************************************
        W A R N I N G !

        Because no other published mods *currenty* rely on this API,
        I'm actively working on it and as such breaking it all the time.
        I'm trying to refine the API to best suit plugin development.

******************************************************************************/


// Note: this API is not thread safe,
// all of the below functions are operating within idxgiswapchain::present
// as the game is trying submit a frame to the gpu
// do not use any of these apis from a separate thread
// and please keep it quick, don't slow down the game
// I will add an api for a separate task thread to do sig scanning or whatever


//these are some defines to use to interface with BetterConsole through the message passing
//api of sfse, see the complete example in the commented out region at the bottom

// when receiving the broadcast message use this as the message->type
#define BetterAPIMessageType 1

// in your sfse RegisterListener() use this as the sender name
#define BetterAPIName "ImGuiInjector"


// This file no dependencies and is plain C (C89 maybe? a 34 year old language)
// so we need a couple convenience typedefs
typedef unsigned char boolean;


// Opaque handle for the log buffer system
typedef uint32_t LogBufferHandle;


//used to implment an std::vector knockoff for C
typedef struct item_array_t {
        char* data;
        size_t count;
        size_t capacity;
        size_t element_size;
} ItemArray;


/// <summary>
/// for the itemarray API, used to sort the elements via user supplied function
/// similar to qsort()
/// returns 0 if A == B
/// returns 1 if A > B
/// return -1 if A < B
/// return value is similar to how strcmp() works
/// </summary>
typedef int (*ItemSortFunc)(const void* A, const void* B);


// while many compilers let you cast a function pointer to a void*
// the C standard only lets you cast a function pointer to another
// function pointer. Whatever, but this does help document the API better
typedef void (*FUNC_PTR)(void);

// For draw callbacks
typedef void (*CALLBACK_DRAW_TAB)(void* imgui_context);

//used to draw an overlay or separate imgui window, requires linking with imgui (no simpledraw)
typedef void (*CALLBACK_DRAW_WINDOW)(void* imgui_context, boolean menu_is_open);

// For hotkeys callback
typedef boolean(*CALLBACK_HOTKEY)(uint32_t vk_keycode, boolean shift, boolean ctrl);

// for the simpledraw selectionlist, used to turn an entry into a string
typedef const char* (*CALLBACK_SELECTIONLIST_TEXT)(const void* item, uint32_t index, char* out, uint32_t num_chars);

// for the simpledraw table renderer, used to draw the controls for a specific cell in the table
// this callback will be called for each *rendered* cell in the table (the table clips unseen cells)
typedef void (*CALLBACK_TABLE_DRAWCELL)(void* userdata, int current_row, int column_id);


typedef struct mod_info_t {
        // how you want you plugin to show up in the UI
        // i dont restrict how many modinfo structures you can register, but i should restrict to 1 right?
        const char* Name;

        // the optional callbacks for drawing
        CALLBACK_DRAW_TAB    DrawTab; // called to draw the contents of your mods tab
        CALLBACK_DRAW_WINDOW DrawWindow; // called to draw your mods imgui window
        CALLBACK_HOTKEY      HotkeyCallback; //called on key down, return true if you handled the input. TODO: add hashmap for hotkey filter
        FUNC_PTR             PeriodicCallback; // called every "1 / number of ModInfo" frames
                                                // for example, if 5 plugins are loaded, this is called every 5 frames
        LogBufferHandle PluginLog; // show this log in the "modmenu tab/logs tab/mod name" log
} ModInfo;


// for some of the more complex UI elements this structure can be used to provide the UI system a list of items to render
// see the simpledraw selectionlist, table, tabbar, and buttonbar
typedef struct ui_data_list_t {
        // some data that your mod uses
        const void* UserData; 
        // some callback that can convert that data into a string to be shown in the UI
        // note that you are provided a temp buffer to use as well if you need to snprintf()
        const char* (*ToString)(const void* userdata, uint32_t index, char* temp, uint32_t temp_size);
        // The count of elements to draw
        uint32_t Count;
} UIDataList;


struct callback_api_t {
        void (*RegisterModInfo)(const ModInfo info);
};

/// <summary>
/// This API is used for saving and loading data to a configuration file.
/// </summary>
struct config_api_t {
        /// <summary>
        /// Call this function once to begin binding your settings to the settings registry
        /// `mod_name` should be unique to your mod because settings are namespaced to your mod name
        ///     and all settings are stored in the same configuration file (known as the config "registry")
        /// </summary>
        void (*Open)(const char* mod_name);

        /// <summary>
        /// After you have bound all your settings, call close to mark the settings object ready for the next
        /// mod to call Open(). It is a fatal error for your mod to let another mod call Open() before you call Close().
        /// </summary>
        void (*Close)();

        /// <summary>
        /// Bind an int* to the settings system and register it to the string `name`
        /// `value` must be static, global, or heap allocated so that the settings system can read/write or save/load at any point.
        /// The min and max values clamp the settings UI - set these both to 0 to disable clamping.
        /// The description is optional but provides the user a hint when editing the setting in the UI.
        /// </summary>
        void (*BindInt)(const char* name, int* value, int min_value, int max_value, const char* description);

        /// <summary>
        /// Bind a float* to the settings system and register it to the string `name`
        /// `value` must be static, global, or heap allocated so that the settings system can read/write or save/load at any point.
        /// The min and max values clamp the settings UI - set these both to 0 to disable clamping.
        /// The description is optional but provides the user a hint when editing the setting in the UI.
        /// </summary>
        void (*BindFloat)(const char* name, float* value, float min_value, float max_value, const char* description);

        /// <summary>
        /// Bind a boolean* to the settings system and register it to the string `name`
        /// `value` must be static, global, or heap allocated so that the settings system can read/write or save/load at any point.
        /// The description is optional but provides the user a hint when editing the setting in the UI.
        /// </summary>
        void (*BindBoolean)(const char* name, boolean* value, const char* description);

        /// <summary>
        /// Bind a char* to the settings system and register it to the string `name`
        /// `value` must be static, global, or heap allocated so that the settings system can read/write or save/load at any point.
        /// `value_size` must be supplied so the settings system knows the maximum size string that `value` will hold.
        /// The description is optional but provides the user a hint when editing the setting in the UI.
        /// </summary>
        void (*BindSettingString)(const char* name, char* value, uint32_t value_size, const char* description);

        /*
                Note: there is no Save function, or any way to directly save values to the config file, saving is performed automatically
                        and is one of the reasons why all of the bind functions want a global/static/heap pointer - periodically all settings
                        from all mods will be iterated and stored into the configuration registry, usually when the UI is opened.
        */
};



// This api deals with hooking functions and vtables
struct hook_api_t {
        // Hook old_func so it is redirected to new_func
        // returns a function pointer to call old_func after the hook
        FUNC_PTR (*HookFunction)(FUNC_PTR old_func, FUNC_PTR new_func);

        // Hook a vtable function for all instances of a class
        // returns the old function pointer
        FUNC_PTR(*HookVirtualTable)(void* class_instance, unsigned method_index, FUNC_PTR new_func);

        // Relocate an offset from imagebase
        // sfse offers a better version of this, but if you are using the minimal api (see below)
        // then you might want to use this
        void* (*Relocate)(unsigned imagebase_offset);

        // Write memory
        // same as Relocate, use the sfse version if you can
        boolean(*WriteMemory)(void* dest, const void* src, unsigned size);

        // get the address of a function through starfield's import address table
        FUNC_PTR(*GetProcAddressFromIAT)(const char* dll_name, const char* func_name);

        // hook a function in the Import Address Table of starfield
        FUNC_PTR(*HookFunctionIAT)(const char* dll_name, const char* func_name, const FUNC_PTR new_function);
};

// This API allows you to create a basic mod menu without linking to imgui
// if you play your cards right, you can write an entire mod without including any other
// headers... not even the standard library!
struct simple_draw_t {
        // draw a separator line
        void (*Separator)(void);

        // draw some text, supports format specifiers
        void (*Text)(const char *fmt, ...);

        // draw a button, returns true if the button was clicked
        boolean(*Button)(const char* name);

        // draw a checkbox, needs to store persistent state so pass a static boolean pointer
        // returns the value of *state for convenience in an 'if' statement
        boolean (*Checkbox)(const char* name, boolean* state);

        // draw a text input widget for a single line of text
        // buffer is where the text is stored
        // true_on_enter determines if the widget returns true on every character typed or only when enter is pressed
        boolean(*InputText)(const char* name, char* buffer, uint32_t buffer_size, boolean true_on_enter);


        // split the remainaing space into a left and right region
        // the left region occupies a left_size (eg: .5) fraction of the screen
        // the remaining space is reserved for the HBoxRight() side
        // min_size is a safety minimum number of pixels that hboxleft can be
        void (*HboxLeft)(float left_size, float min_size_em);

        // the counterpart of HBoxLeft 
        // no argument needed, the size is the remaining space not taken up by hbox left
        void (*HBoxRight)();

        // needed to end hbox calculation
        void (*HBoxEnd)();

        //this is the same as hbox, but for vertically separated spaces
        void (*VboxTop)(float top_size, float min_size_em);
        void (*VBoxBottom)();
        void (*VBoxEnd)();


        // display an int number editor as a draggable slider that is aldo editable
        // min and max define the range of the *value
        // step defines the granilarity of the movement
        // the return value is true if the value was changed this frame
        boolean (*DragInt)(const char* name, int* value, int min, int max);

        // follows the same rules as the sliderint above
        boolean(*DragFloat)(const char* name, float* value, float min, float max);


        // use the remaining vertical space to render the logbuffer referenced by handle
        // if scroll_to_bottom is true, force the logbuffer region to scroll from its current position
        // to the bottom (useful when you add lines)
        void (*ShowLogBuffer)(LogBufferHandle handle, boolean scroll_to_bottom);

        // use the remaining vertical space to render the logbuffer referenced by handle
        // if scroll_to_bottom is true, force the logbuffer region to scroll from its current position
        // takes a pointer to an array of line numbers and only displays those lines in the widget
        void (*ShowFilteredLogBuffer)(LogBufferHandle handle, const uint32_t* lines, uint32_t line_count, boolean scroll_to_bottom);

        //if tostring returns null, the entry is skipped
        boolean(*SelectionList)(const UIDataList* datalist, int* selected);


        void (*Table)(const char * const * const headers, uint32_t header_count, void* rows_userdata, uint32_t row_count, CALLBACK_TABLE_DRAWCELL draw_cell);


        void (*TabBar)(const char* const* const headers, uint32_t header_count, int* state);

        int (*ButtonBar)(const char* const* const labels, uint32_t label_count);

        void (*Tip)(const char* text);
};


// A more rugged system for storing the log buffer
// This is used 
struct log_buffer_api_t {
	// Create a handle, needed to identify the log buffer
        // name is the name of the logbuffer as it shows up in the UI
        // path is an optional path to a logfile to append with logs
        LogBufferHandle (*Create)(const char* name, const char* path);

        // Self documenting use
        const char* (*GetName)(const LogBufferHandle);

        // Get the total number of bytes stored in the log buffer
        uint32_t (*GetSize)(LogBufferHandle);

        // Get the number of lines stored in the buffer
        uint32_t (*GetLineCount)(LogBufferHandle);

        // Retrieve a specific line for storage
        const char* (*GetLine)(LogBufferHandle, uint32_t line);

        // Clear a line buffer
        void (*Clear)(LogBufferHandle);

        // Log a line of text to the buffer
        void (*Append)(LogBufferHandle, const char* log_line);

        // Save the log buffer to a file, all at once rather than on append
        void (*Save)(LogBufferHandle handle, const char* filename);

        // Restore the log buffer from a file, the file is then used for appending
        LogBufferHandle (*Restore)(const char* name, const char* filename);

        // if a logbuffer has a file it is writing, close it
        void (*CloseFile)(LogBufferHandle handle);
};




// This is all the above struct wrapped up in one place
// why pointers to apis instead of the api itself? so that
// I may extend any api without changing the size of BetterAPI
// this helps with forwards compatibility
//
// when you receive the message from betterconsole in your sfse
// MessageEventCallback function, the data* will be a const BetterAPI*
//
// the BetterAPI* that you receive will never be NULL - its statically allocated
// HOWEVER, you need to check (just once) if any of the api pointers that you use are NULL!
// there might be updates to the game that temporarily limit the functionality of betterconsole
// and as a stop-gap measure, updates might be released that have non-working APIs
typedef struct better_api_t {
        const struct hook_api_t* Hook;
        const struct log_buffer_api_t* LogBuffer;
        const struct simple_draw_t* SimpleDraw;
        const struct callback_api_t* Callback;
        const struct config_api_t* Config;
} BetterAPI;
#endif // !BETTERAPI_API_H


#ifdef BETTERAPI_ENABLE_STD
#ifndef BETTERAPI_STD
#define BETTERAPI_STD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline ItemArray* ItemArray_Create(size_t element_size) {
        ItemArray* ret = (ItemArray*)malloc(sizeof(*ret));
        ASSERT(element_size > 0);
        ASSERT(ret != NULL);

        memset(ret, 0, sizeof(*ret));
        ret->element_size = element_size;
        return ret;
}


inline void ItemArray_Clear(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        
        items->count = 0;
}


inline size_t ItemArray_Count(const ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        
        return items->count;
}


inline void ItemArray_Free(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        
        free(items->data);
        memset(items, 0, sizeof(*items));
}


inline void* ItemArray_At(const ItemArray* items, uint32_t index) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items->data != NULL);
        ASSERT(items->count > index);
        
        return (void*)(items->data + (items->element_size * index));
}
#define ITEMARRAY_AT(ITEMS, INDEX, TYPE) *(TYPE*) ItemArray_At((ITEMS),(INDEX))


inline void* ItemArray_Append(ItemArray* items, const void* items_array, uint32_t items_count) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items_count > 0);

        size_t elim_size = items->element_size;
        size_t capacity_needed = items_count + items->count;

        if (capacity_needed > items->capacity) {
                size_t n = capacity_needed;

                /* find the power of 2 larger than n for a 64-bit n via bit-twiddling hacks */
                n--;
                n |= n >> 1;
                n |= n >> 2;
                n |= n >> 4;
                n |= n >> 8;
                n |= n >> 16;
                n |= n >> 32;
                n++;

                ASSERT(n > items->capacity);
                items->data = (char*)realloc(items->data, n * elim_size);
                ASSERT(items->data != NULL && "Buy more RAM lol");

                items->capacity = n;
        }

        char* end = (items->data + (elim_size * items->count));
        items->count += items_count;
        if (items_array) {
                memcpy(end, items_array, (elim_size * items_count));
        }
        return end;
}


inline void* ItemArray_PushBack(ItemArray* items, const void* item) {
        return ItemArray_Append(items, item, 1);
}
#define ITEMARRAY_PUSHBACK(ITEMS, ITEM) ItemArray_PushBack((ITEMS),(void*)&(ITEM))



//this one is interesting, you cant placement new in C, but you can avoid copying large array items
//by just extending the itemarray and returning the pointer to where you want the item to go.
//basically using the itemarray as a kind of arena allocator or object pool.
inline void* ItemArray_EmplaceBack(ItemArray* items, uint32_t items_count) {
        return ItemArray_Append(items, NULL, items_count);
}


inline void ItemArray_PopBack(ItemArray* items, uint32_t count) {
        ASSERT(items != NULL);
        if (items->count > count) {
                items->count -= count;
        }
        else {
                items->count = 0;
        }
}


inline void ItemArray_Reserve(ItemArray* items, uint32_t count) {
        ItemArray_EmplaceBack(items, count);
        ItemArray_PopBack(items, count);
}


inline void ItemArray_Sort(ItemArray* items, ItemSortFunc fn) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items->data != NULL);

        auto count = items->count;
        auto size = items->element_size;
        void* T = ItemArray_EmplaceBack(items, 1);
        auto base = (unsigned char*)items->data;
        unsigned char swaps = 1;
        while (swaps && count--) {
                swaps = 0;
                for (size_t i = 0; i < count; ++i) {
                        void* A = base + (size * i);
                        void* B = base + (size * (i + 1));
                        auto dir = fn(A, B);
                        if (dir > 0) {
                                memcpy(T, A, size);
                                memcpy(A, B, size);
                                memcpy(B, T, size);
                                swaps = 1;
                        }
                }
        }
        ItemArray_PopBack(items, 1);
}

// 1 2 3 4 5 6 7 8 - lower_bound(7)
//       ^ - start at halfway and test, 4 < 7 is true advance index forward [count = 8]
//           ^ - iteration 2, 6 < 7 is true, advance iterater               [count = 4]
//             ^ - iteration 3, 7 < 7 is false, step is 0, return           [count = 2]



#endif // !BETTERAPI_STD
#endif //BETTERAPI_ENABLE_STD




// For people that want to port Cheat Engine or ASI mods to sfse without including sfse code into the project,
// define BETTERAPI_ENABLE_SFSE_MINIMAL before including betterapi.h, this should get you 90% of the way to a
// plug and play sfse plugin with no brain required

#ifdef BETTERAPI_ENABLE_SFSE_MINIMAL
#ifndef BETTERAPI_SFSE_MINIMAL
#define BETTERAPI_SFSE_MINIMAL

#define MAKE_VERSION(major, minor, build) ((((major)&0xFF)<<24)|(((minor)&0xFF)<<16)|(((build)&0xFFF)<<4))

typedef uint32_t PluginHandle;

typedef enum SFSEEnumeration_t {
        InterfaceID_Invalid = 0,
        InterfaceID_Messaging,
        InterfaceID_Trampoline,

        MessageType_SFSE_PostLoad = 0,
        MessageType_SFSE_PostPostLoad,
} InterfaceID;

typedef struct SFSEPluginVersionData_t {
        uint32_t        dataVersion;
        uint32_t        pluginVersion;
        char            name[256];
        char            author[256];
        uint32_t        addressIndependence;
        uint32_t        structureIndependence;
        uint32_t        compatibleVersions[16];
        uint32_t        seVersionRequired;
        uint32_t        reservedNonBreaking;
        uint32_t        reservedBreaking;
} SFSEPluginVersionData;

typedef struct SFSEPluginInfo_t {
        uint32_t	infoVersion;
        const char* name;
        uint32_t	version;
} SFSEPluginInfo;

typedef struct SFSEInterface_t {
        uint32_t	sfseVersion;
        uint32_t	runtimeVersion;
        uint32_t	interfaceVersion;
        void* (*QueryInterface)(uint32_t id);
        PluginHandle(*GetPluginHandle)(void);
        SFSEPluginInfo* (*GetPluginInfo)(const char* name);
} SFSEInterface;

typedef struct SFSEMessage_t {
        const char* sender;
        uint32_t        type;
        uint32_t        dataLen;
        void* data;
} SFSEMessage;

typedef void (*SFSEMessageEventCallback)(SFSEMessage* msg);

typedef struct SFSEMessagingInterface_t {
        uint32_t        interfaceVersion;
        bool            (*RegisterListener)(PluginHandle listener, const char* sender, SFSEMessageEventCallback handler);
        bool	        (*Dispatch)(PluginHandle sender, uint32_t messageType, void* data, uint32_t dataLen, const char* receiver);
}SFSEMessagingInterface;

/* In your main C/C++ file copy-paste the code below and edit it to suit your project 

static void SimpleDrawCallback();
static void CALLBACK_betterconsole(SFSEMessage* msg);
static void CALLBACK_sfse(SFSEMessage* msg);


const BetterAPI* API = NULL;
static PluginHandle my_handle;
static SFSEMessagingInterface* message_interface = nullptr;


extern "C" __declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version = {
        1, // SFSE api version
        1, // Plugin version
        "ExamplePlugin",
        "PluginAuthor",
        1, // AddressIndependence::Signatures
        1, // StructureIndependence::NoStructs
        {MAKE_VERSION(1, 7, 29), 0}, //game version 1.7.29
        0, //does not rely on any sfse version
        0, 0 //reserved fields
};


extern "C" __declspec(dllexport) void SFSEPlugin_Load(const SFSEInterface * sfse) {
        my_handle = sfse->GetPluginHandle();
        message_interface = (SFSEMessagingInterface*)sfse->QueryInterface(InterfaceID_Messaging);
        message_interface->RegisterListener(my_handle, "SFSE", CALLBACK_sfse);
}


static void CALLBACK_sfse(SFSEMessage* msg) {
        if (msg->type == MessageType_SFSE_PostLoad) {
                message_interface->RegisterListener(my_handle, "BetterConsole", CALLBACK_betterconsole);
        }
}


static void CALLBACK_betterconsole(SFSEMessage* msg) {
        if (msg->type == BetterAPIMessageType) {
                API = (const BetterAPI*)msg->data;
                API->Callback->SimpleDrawCallback("Example", &SimpleDrawCallback);
        }
}


static void SimpleDrawCallback() {
        API->SimpleDraw->Text("Hello World!");
}

*/
#endif // !BETTERAPI_SFSE_MINIMAL
#endif
