#define BETTERAPI_ENABLE_ASSERTIONS
#define BETTERAPI_ENABLE_STD



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
#define BetterAPIName "BetterConsole"


// This file no dependencies and is plain C (C89 maybe? a 34 year old language)
// so we need a couple convenience typedefs
typedef unsigned char boolean;


// Opaque handle for the log buffer system
typedef uint32_t LogBufferHandle;


//opaque handle for the itemarray api
typedef uint32_t ItemArrayHandle;


// while visual studio (MSVC) lets you cast a function pointer to a void*
// the C standard only lets you cast a function pointer to another
// function pointer. Whatever, but this does help document the API better
typedef void (*FUNC_PTR)(void);


// For draw callbacks
typedef void (*DRAW_FUNC)(void* imgui_context);


// For hotkeys callback
typedef boolean(*HOTKEY_FUNC)(uint32_t vk_keycode, boolean shift, boolean ctrl);


typedef struct draw_callbacks_t {
        // intrusive double-linked list, do not touch
        struct draw_callbacks_t* ll_prev;
        struct draw_callbacks_t* ll_next;

        // how you want you plugin to show up in the UI
        const char* Name; 
        
        // the optional callbacks for drawing
        DRAW_FUNC DrawTab; // called to draw the contents of your mods tab
        DRAW_FUNC DrawSettings; // called to draw your mod settings page
        DRAW_FUNC DrawLog; // called to draw your mods log page
        DRAW_FUNC DrawWindow; // called to draw your mods imgui window
} DrawCallbacks;

struct callback_api_t {
        void (*RegisterDrawCallbacks)(DrawCallbacks* draw_callbacks);
        void (*RegisterHotkey)(const char* name, HOTKEY_FUNC func);
        void (*RegisterEveryFrameCallabck)(FUNC_PTR func);
};


struct config_api_t {
        // load the "key=value" pairs from file specified by filepath
        void (*Load)(const char* filepath);

        // Save all "key=value" pairs previously loaded
        void (*Save)(const char* filepath);

        // bind the int to the keyname "name"
        // int must be heap allocated or statically allocated because the pointer
        // to value is stored for a later call to Save() to save the current
        // value without needing to explicity save_int(name, value)
        void (*BindInt)(const char* name, int* value);
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
        void (*HboxLeft)(float left_size, float min_size);

        // the counterpart of HBoxLeft 
        // no argument needed, the size is the remaining space not taken up by hbox left
        void (*HBoxRight)();

        // needed to end hbox calculation
        void (*HBoxEnd)();

        //this is the same as hbox, but for vertically separated spaces
        void (*VboxTop)(float top_size, float min_size);
        void (*VBoxBottom)();
        void (*VBoxEnd)();


        // retrieve the font size, useful for min_size calculation for the h/v box above
        float (*CurrentFontSize)();


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

#ifdef BETTERAPI_ENABLE_ASSERTIONS
#include <stdio.h>
#include <Windows.h>
#define ASSERT(X) do { if((X)) break; char msg[1024]; snprintf(msg, sizeof(msg), "FILE: %s\nFUNC: %s:%u\n%s", __FILE__, __func__, __LINE__, #X); MessageBoxA(NULL, msg, "Assertion Failure", 0); exit(EXIT_FAILURE); }while(0)
#else
#define ASSERT(X) do {} while(0)
#endif // BETTERAPI_ENABLE_ASSERTIONS


#endif // !BETTERAPI_API_H


#ifdef BETTERAPI_ENABLE_STD
#ifndef BETTERAPI_STD
#define BETTERAPI_STD

#include <stdio.h>
#include <stdlib.h>

//TODO: port this over to size_t again

typedef struct item_array_t {
        char* data;
        uint32_t count;
        uint32_t capacity;
        uint32_t element_size;
} ItemArray;


inline static ItemArray ItemArray_Create(uint32_t element_size) {
        ASSERT(element_size > 0);
        ItemArray ret = {};
        ret.element_size = element_size;
        return ret;
}


inline static void ItemArray_Clear(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        items->count = 0;
}


inline static uint32_t ItemArray_Count(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        return items->count;
}


inline static void ItemArray_Free(ItemArray* items) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        free(items->data);
        items->count = 0;
        items->data = NULL;
}


inline static void* ItemArray_At(ItemArray* items, uint32_t index) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items->data != NULL);
        ASSERT(items->count > index);
        return (void*)(items->data + (items->element_size * index));
}


inline static void ItemArray_Append(ItemArray* items, void* items_array, uint32_t items_count) {
        ASSERT(items != NULL);
        ASSERT(items->element_size != 0);
        ASSERT(items->count > 0);

        uint32_t capacity_needed = items_count + items->count;

        if (capacity_needed > items->capacity) {
                uint32_t n = capacity_needed;

                /* find the power of 2 larger than n for a 32-bit n */
                n--;
                n |= n >> 1;
                n |= n >> 2;
                n |= n >> 4;
                n |= n >> 8;
                n |= n >> 16;
                n++;

                ASSERT(n > items->capacity);
                items->data = (char*)realloc(items->data, n);
                ASSERT(items->data != NULL);

                items->capacity = n;
        }

        char* end = (items->data + (items->element_size * items->count));
        items->count += items_count;
        memcpy(end, items_array, (items->element_size * items_count));
}


inline static void ItemArray_PushBack(ItemArray* items, void* item) {
        ItemArray_Append(items, item, 1);
}
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
