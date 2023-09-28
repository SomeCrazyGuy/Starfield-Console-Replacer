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


// while visual studio (MSVC) lets you cast a function pointer to a void*
// the C standard only lets you cast a function pointer to another
// function pointer. Whatever, but this does help document the API better
typedef void (*FUNC_PTR)(void);

// This is used in the callback api below to register a function
// to be called every frame for the purpose of drawing using imgui
typedef void(*ImGuiDrawCallback)(void* imgui_context);


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

        // use the remaining vertical space to render the logbuffer referenced by handle
        // if scroll_to_bottom is true, force the logbuffer region to scroll from its current position
        // to the bottom (useful when you add lines)
        void (*ShowLogBuffer)(LogBufferHandle handle, boolean scroll_to_bottom);
};

// This allows you to register a callback function for various functions like drawing to the screen
// currently there is no way to remove a callback, nor a way to prevent callbacks from being added more than once
// these callbacks are called every frame only while the imgui interface is shown
struct callback_api_t {
        // your mod will show up as "name" in the mod menu
        // if your entry in the mod menu is selected then callback will be called every frame
        // use the simple_draw api to build your mod menu
        void (*SimpleDrawCallback)(const char* name, FUNC_PTR callback);

        // register "callback" to be called every frame, you will be given the pointer to
        // the imgui context, make sure to call ImGui::SetCurrentContext(imgui_context)
        // before using any imgui drawing functions
        //
        // Please make sure you are using the same version of imgui that betterconsole is using!!!
        // imgui versions are not forward compatible and drawing across dll boundaries is not easy
        // Never try to draw from a separate thread!
        //
        // Note: do not compile any imgui backends (files with "impl" in the name) betterconsole already
        // has the backends setup, it should just work
        //
        // Note2: technically you can use the simple_draw api in here as well, but why would you?
        void (*ImGuiDrawCallback)(ImGuiDrawCallback callback);
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
} BetterAPI;



#endif // !BETTERAPI_API_H


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
