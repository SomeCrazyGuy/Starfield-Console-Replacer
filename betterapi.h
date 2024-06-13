#ifndef BETTERAPI_API_H
#define BETTERAPI_API_H

// Always use the latest betterapi.h file from github in your build if the BETTERAPI_VERSION
// number is compatible with the published version of betterconsole on nexusmods.
// sometimes small hacks or fixes are put here to fix issues in the published mod between releases.
// https://raw.githubusercontent.com/SomeCrazyGuy/Starfield-Console-Replacer/master/betterapi.h


// Version number for betterapi, when a new version of betterconsole changes the public API
// this number is incremented. Usually only when bigger feature changes are added.
// I will not break the API in a bugfix release of betterconsole and feature releases
// are not expected to happen more often than starfield updates.
// Usually porting to a new version of the api is as easy as dropping in the new betterapi.h
// into your build and fixing the compiler errors (if any).
#define BETTERAPI_VERSION 1


///////////////////////////////////////////////////////////////////////////////
//         Gain BetterConsole features for your mod in 2 easy steps:         //
///////////////////////////////////////////////////////////////////////////////

// Step 1) Include "betterapi.h" anywhere you want to use the api
// 
// 
// Step 2) In only one translation unit (.c or .cpp file) define 
//         "BETTERAPI_IMPLEMENTATION" before including betterapi.h
//         this will generate magic glue code that makes it all work,
//         then you can write the "OnBetterConsoleLoad" function.
//         You will probably just copy/paste this into a file:
/*

// BETTERAPI_IMPLEMENTATION can only be defined in one file
// and must be defined before including betterapi.h in that one file
#define BETTERAPI_IMPLEMENTATION

// but you can include betterapi.h in any number of files
#include "betterapi.h"

// If betterconsole is installed and your mod is compatible then
// this function will get called *magically* even though its static
static int OnBetterConsoleLoad(const struct better_api_t* BetterAPI) {
        
        // First register your mod with betterconsole by providing a mod name
        // you could save the handle for later use, but most of the time you can
        // just register any callbacks you want upfront then forget about the handle
        RegistrationHandle my_mod_handle = BetterAPI->Callback->RegisterMod("My Mod Name");

        // Registering any callback is optional, but you will probably want
        // to at least register the draw callback to show a mod menu
        //BetterAPI->Callback->RegisterDrawCallback(my_mod_handle, &MyDrawCallback);

        // If you have a set of global config options, you could make betterconsole
        // take care of saving, loading, and editing these options with a gui
        //BetterAPI->Callback->RegisterConfigCallback(my_mod_handle, &MySaveLoadCallback);

        // Hotkey Registration happens in two steps: setting the callback and setting the hotkey
        // your mod can only have one hotkey callback, but you can request multiple hotkey
        // entries and tell them apart in your callback by the "userdata" value
        //BetterAPI->Callback->RegisterHotkeyCallback(my_mod_handle, &MyHotkeyCallback); 
        //BetterAPI->Callback->RequestHotkey(my_mod_handle, "Activate Mod Feature", 0); 
        //BetterAPI->Callback->RequestHotkey(my_mod_handle, "Activate Other Feature", 1); 
        //BetterAPI->Callback->RequestHotkey(my_mod_handle, "Activate Third Feature", 2); 

        // Maybe save the betterapi pointer to a global variable use elsewhere
        //API = BetterAPI;

        // I often find it convenient to assign individual api pointers to global variables
        // UI = BetterAPI->SimpleDraw

        return 0; // return 0 for success or any positive number to indicate a failure
}

*/
// Sit back and relax! The callbacks registered will be called automatically when
// necessary. For example, lets implement that MyDrawCallback function from the
// example code above and assume we also decided to set that UI global variable
// to the SimpleDraw api and the API global variable to BetterAPI:
/*

static void MyDrawCallback(void*) {
        static int counter = 0;

        UI->Text("Hello World!");

        if (UI->Button("Click Me!")) {
                counter++;
        }

        // Text also supports format specifiers
        UI->Text("You Clicked the button %d times", counter);

        UI->Text(label);
}

*/
// Seems pretty easy, but while we are here may as well show a config callback
// and assume the same API variable is still available:
/*

//any arbitrary data your mod wants to configure
static uint32_t VendorRestockSeconds = 3600;

// action is either read (config file loaded), write (config save event), or edit (show config editor)
// the Config api only supports certain types, but automatically handles all 3 actions
void MySaveLoadCallback(ConfigAction action) {
        API->Config->ConfigU32(action, "Vendor Restock Seconds", &VendorRestockSeconds);
}

*/
// Betterapi integration is a soft dependency - if a user doesn't have
// or want betterconsole installed, or betterconsole updates and your mod is no
// longer compatible, then your mod just works the way it would without the
// betterconsole features and api. If your mod can function without betterconsole
// you dont need to do any additional work - the OnBetterConsoleLoad function
// just wont be called.
//
//
// Bonus Step:
// If you are writing an asi mod or trying to port a cheat engine table,
// you may want to Control+F for SFSE_MINIMAL. You can utilize the sfse
// minimal api to make your mod into a combo asi and sfse plugin without
// having to include any other headers or libraries not even any sfse code.
// I have written and included an example of how to use this near the
// bottom of the file.
//
// 
// You can always reach out to me if you have any questions or need help integrating betterapi
// I'm almost always available in the v2 discord or nexusmods.


// Betterconsole is written in C++ but the public API is plain C99 and only includes
// headers that are from the C standard library no C library functions are even 
// called by betterapi. This makes it easier to integrate betterconsole into any
// project because the whole API is one header file with no other dependencies. On a
// philisophical level, if you cannot describe an interface in C then the interface
// is not simple enough. Since almost every programming language has some type of C
// compatible foreign function interface, this allows you to write mods in almost
// any programming language albeit with some challenges.
#include <stdint.h> 
#include <stdbool.h>


// Sometimes betterconsole updates and supports new features and APIs without
// breaking backwards compatibility with older versions. By default betterapi
// uses the most backwards compatible options which are only the features that
// all versions of BETTERAPI_VERSION support. When new features are added, a new
// feature level is defined below. if you need a more up-to-date feature set
// and are willing to break compatibility with older versions of betterconsole
// then you can define BETTERAPI_FEATURE_LEVEL before including betterapi.h
// BETTERAPI_FEATURE_LEVEL is defined as an integer constant that represents a
// release date for betterconsole on nexusmods. usually you would want to support
// at least one previous version of betterconsole in case a mod collection that
// includes betterconsole has not yet been updated.
#if 0
// these are provided for easy copy-paste
#define BETTERAPI_FEATURE_LEVEL_1_3_1 0
#endif
#ifndef BETTERAPI_FEATURE_LEVEL
#define BETTERAPI_FEATURE_LEVEL 0
#endif



// You might see the use of BETTERAPI_DEVELOPMENT_FEATURES in the code below.
// these are features that will be in the next version of betterconsole but
// not part of the current release on nexusmods.
// If you are developing a mod using the betterconsole api you MUST NOT define
// BETTERAPI_DEVELOPMENT_FEATURES, as the resulting plugin will not be compatible
// with the version of betterconsole that everyone is using.
#ifdef BETTERAPI_DEVELOPMENT_FEATURES
#pragma message ("BETTERAPI_DEVELOPMENT_FEATURES is defined, any plugin using the api will not be compatible the nexusmods release of betterconsole.")
#define BETTERAPI_FEATURE_LEVEL 99999999
#endif // BETTERAPI_DEVELOPMENT_FEATURES






//           A note about thread safety
// There is no thread safety! All of the betterapi functions are operating within
// the game's idxgiswapchain::present call as the game is trying submit a 
// frame to the gpu. Do not use any of these apis from a separate thread unless
// it is explicitly marked as safe to do so and please keep it quick - we don't
// want to slow down the game. I will add an api for a separate task thread to
// do sig scanning or other slow tasks in a future update.


//            A note about "userdata"
// Several of the APIs in BetterConsole take a "userdata" argument
// since we are exposing all these capabilities in plain C99 we
// need a way to express a generic type or piece of data that you
// want to work with. Often this will show up in the SimpleDraw functions.
// Lets look at an example of why and how you would do this...
// 
// say you have an array of pointers to some struct:
// struct GameType {
//      ...
//      uint32_t formid;
//      ...
//      const char* name;
//      ...
// } *data_array[100] = some_pointer;
// 
// how would you make a selectable list of these in a gui that knows nothing
// about GameType? The answer is userdata and a callback!
// 
// Create a to_string function for GameType:
// 
// const char* gametype_to_string(const void* userdata, uint32_t index, char* out_tmp, uint32_t tmp_size) {
//      const GameType** pGT = (const GameType**)userdata;
//      const GameType* GT = pGT[index];
//      snprintf(out_tmp, tmp_size, "FormID: %X, Name: %s", GT->formid, GT->name);
//      return out_tmp;
// }
// 
// Then make it a selectable list:
// 
// static uint32_t selected_item = 0;
// bool was_clicked = API->SimpleDraw->SelectionList(&selected_item, (const void*)data_array, 100, gametype_to_string);
//
// Now the user can scroll through and click on an entry in the list without
//              the UI code ever needing to know what the heck a GameType is.
// Note: selections lists dont always start at the first index, in our example above
//       there were 100 items in the list, but selectionlist will only call gametype_to_string
//       on the items that are visible, if the selection list can only show 20 items
//       your gametype_to_string function may only be called for say index 53-73.
//       So make sure your data is random access iterable.


// Opaque handle for the log buffer system
typedef uint32_t LogBufferHandle;


// When you register a mod you receive a handle to your mod
// this handle can be used to register additional functionality
// like hotkeys or draw callbacks
typedef uint32_t RegistrationHandle;


// The settings callback tells you what type of action to perform from these options:
// ConfigAction_Read - betterconsole finished reading the settings file
//                     and now you can try loading any settings you saved
// ConfigAction_Write - betterconsole is about to save all settings to
//                      the settings file
// ConfigAction_Edit - the user is in the settings menu and wants to adjust a setting
//                     show a UI to the user to edit the setting
typedef enum ConfigAction {
        ConfigAction_Read,
        ConfigAction_Write,
        ConfigAction_Edit,
} ConfigAction;


// while many compilers let you cast a function pointer to a void*
// the C standard only lets you cast a function pointer to another
// function pointer. This does help document the API better though.
typedef void (*FUNC_PTR)(void);


// To show a GUI you must register a draw callback with this signature.
//
// `imgui_context` can be ignored if using the SimpleDraw API, but if you link with
//    imgui directly this can be used to create more complex user interfaces but you
//    must call ImGui::SetCurrentContext(imgui_context) first in your draw callback
//    and match the version of imgui used by betterconsole exactly.
// 
// Note: this callback will be called every frame when betterconsole is open and
//       the tab for your mod is selected.
typedef void (*DRAW_CALLBACK)(void* imgui_context);


// if your application would like to take advantage of the built-in settings 
// registry you will need to provide a callback function that is called when
// the settings file is loaded, saved, or when the user is in the settings menu
// and selects your mod. The Settings API provides automatic read/write/edit 
// functionality if the built-in types work for you.
// `read_write_or_edit` is one of: ConfigAction_Read, ConfigAction_Write, or
//                      ConfigAction_Edit
typedef void (*CONFIG_CALLBACK)(ConfigAction read_write_or_edit);


// This is a function called when the user presses a hotkey combination that
// matches the hotkey set for your plugin. Setting a hotkey is done by the user
// in the hotkeys menu. Hotkeys are saved automatically in the settings registry
// so a set hotkey combo is preserved across sessions. Only one hotkey callback
// per mod can be registered, but you can request hotkeys multiple times with
// different user data.
// 
// `userdata` is any arbitrary data that you want to pass to the callback usually
//            to help identify the hotkey if multiple hotkeys are requested
typedef void (*HOTKEY_CALLBACK)(uintptr_t userdata);


// For the simpledraw selectionlist, this is a function called to provide text for
// a single entry in a selectable list of items. This function will be called every
// frame for every *visible* item in the list. The return value should be a null
// terminated string that will be shown in the UI.
// 
// `userdata` is any arbitrary data that you want to pass to the callback
// 
// `index` is the index of the item in the list to "stringify"
// 
// `out_buffer` is provided as a temporary buffer that can be used
//              to store the result of snprintf() or similar since
//              freeing a heap allocation made within the callback
//              is a very difficult task.
typedef const char* (*CALLBACK_SELECTIONLIST)(const void* userdata, uint32_t index, char* out_buffer, uint32_t out_buffer_size);


// For the simpledraw table renderer, used to build the UI shown in tables.
// This callback will be called for each *visible* cell in the table (unseen cells
// will not be rendered and thus will not call this function). Tables are drawn
// first left to right for each column, then from top to bottom for each row.
// 
// `table_userdata` is any arbitrary data that you want to pass to the callback and
//                  should be used for the data you wish to draw in the table
// 
// `current_row` is the index of the current row in the table that is being drawn
// 
// `current_column` is the index of the current column in the table that is being drawn
// TODO: next version update this to use unsigned integers and possibly use uintptr_t 
//       instead of void* for table_userdata
typedef void (*CALLBACK_TABLE)(void* table_userdata, int current_row, int current_column);


// This is the main api you will use to add betterconsole integration to your mod
struct callback_api_t {
        // Register your mod with betterconsole.
        // The returned handle can be used to register additional functionality
        // such as hotkeys or draw callbacks.
        // 
        // `mod_name` should uniquely identify your mod and conform to the following:
        //                 - must be less than 32 characters
        //                 - must have a length of >3 characters
        //                 - must not begin or end with whitespace
        //
        // Almost all mods will need to use this to interact with betterconsole. 
        RegistrationHandle(*RegisterMod)(const char* mod_name);
        

        // Register a function to show your mod's user interface.
        // 
        // `handle` is your registration handle from a previous call to RegisterMod
        // 
        // `draw_callback` will be called every frame when the following conditions are met:
        //                 - the BetterConsole UI is open
        //                 - your mod is the one in focus
        //
        // Usually, you would utilize the SimpleDraw API to create your UI, advanced
        // users may link with imgui directly and utilize the imgui context provided
        // as the first parameter to the draw callback. If using imgui directly, you
        // must call ImGui::SetCurrentContext() in the draw callback before using 
        // any imgui functions AND you must match the version of imgui with the 
        // version used by betterconsole.
        void (*RegisterDrawCallback)(RegistrationHandle handle, DRAW_CALLBACK draw_callback);


        // Register a configuration callback
        // 
        // `handle` is your registration handle from a previous call to RegisterMod
        // 
        // `config_callback` will be called when the settings file is loaded (read event)
        //                   saved (write event) or when the user is in the setting menu
        //                   and selects your mod in the UI (edit event)
        void (*RegisterConfigCallback)(RegistrationHandle handle, CONFIG_CALLBACK config_callback);


        // Register a hotkey callback
        // 
        // `handle` is your registration handle from a previous call to RegisterMod
        //
        //  `hotkey_callback` will be called when the user presses the hotkey combo
        //                    set for your plugin. to have an effect, you must also
        //                    call RequestHotkey. This is a two-step process because
        //                    your plugin can have only one hotkey callback but you
        //                    request any number of hotkeys with different userdata.
        void (*RegisterHotkeyCallback)(RegistrationHandle handle, HOTKEY_CALLBACK hotkey_callback);


        // Request a hotkey handler, the user configures the hotkey in the hotkeys menu
        // 
        // `handle` is the handle you created when registering your mod
        // 
        // `hotkey_name` is the name of the hotkey in the ui, use a descriptive name
        //               less than the 32 character internal buffer. this string is 
        //               copied to an internal buffer so feel free to use a generated
        //               name via snprintf() or similar on stack allocated memory.
        // 
        // `userdata` is optional extra data that is sent to the hotkey callback.
        //            if you want to request multiple hotkeys this parameter is
        //            how you would differentiate them.
        void (*RequestHotkey)(RegistrationHandle handle, const char* hotkey_name, uintptr_t userdata);

        
};


/// This API is used for saving and loading data to a configuration file.
struct config_api_t {
        // this handles read / write / edit on 32-bit unsigned integers
        void (*ConfigU32)(ConfigAction action, const char* key_name, uint32_t* value);

        // get the unparsed string value of a key if it exists or null
        //const char* (*ConfigRead)(const char* key_name);

        // more types will be added here in the future
};



// This api deals with hooking functions and vtables
struct hook_api_t {
        // Hook old_func so it is redirected to new_func
        // returns a function pointer to call old_func after the hook
        // uses the minhook library internally
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
        bool(*WriteMemory)(void* dest, const void* src, unsigned size);

        // get the address of a function through starfield's import address table
        FUNC_PTR(*GetProcAddressFromIAT)(const char* dll_name, const char* func_name);

        // hook a function in the Import Address Table of starfield
        FUNC_PTR(*HookFunctionIAT)(const char* dll_name, const char* func_name, const FUNC_PTR new_function);

        // !EXPERIMENTAL API! AOB scan the exe memory and return the first match 
        void* (*AOBScanEXE)(const char* signature);
};

// This API allows you to create a basic mod menu without linking to imgui.
// if you play your cards right, you can write an entire mod without any other
// headers or libraries... not even the standard library!
struct simple_draw_t {
        // Draw a separator line that fills the horizontal space
        void (*Separator)(void);


        // Draw some text, supports format specifiers
        void (*Text)(const char *fmt, ...);


        // Draw a button, returns true if the button was clicked
        // Example:
        // if (SimpleDraw->Button("Hello")) { /* do something on click */ }
        bool (*Button)(const char* name);


        // Draw a checkbox, this widget needs to store persistent state
        // usually you would pass a static bool as a pointer
        // returns true if the state was changed this frame
        // Example:
        // static bool state = false;
        // if (SimpleDraw->Checkbox("Hello", &state)) { /* do something on state change */ }
        bool (*Checkbox)(const char* name, bool* state);


        // Draw a text input widget for a single line of text
        // `buffer` will be used to store the text the user types
        // `true_on_enter` determines if the widget returns true on every character
        //                 typed or only when enter is pressed
        // Example:
        // static char buffer[32];
        // if (SimpleDraw->InputText("Hello", buffer, sizeof(buffer), true)) { /* do something on enter */ }
        bool (*InputText)(const char* name, char* buffer, uint32_t buffer_size, bool true_on_enter);


        // Split the remainaing window space into a left and right region.
        // the left region occupies a `left_size` (eg: .5) fraction of the screen
        // the remaining space is reserved for the HBoxRight() side
        // `min_size_em` is a safety minimum number of charactes wide that the
        // HBoxLeft region can be and overrides the `left_size` if it is smaller.
        // If you call HBoxLeft, you must also call HBoxRight and HBoxEnd
        // HBox and VBox regions can be nested for more complex layouts but
        // are more performance intensive than most other SimpleDraw functions.
        void (*HBoxLeft)(float left_size, float min_size_em);


        // Move to the right side region after a call to HBoxLeft
        // no argument needed, the size of the region is the remaining
        // space not taken up by HBoxLeft
        void (*HBoxRight)();


        // Finish the HBoxLeft and HBoxRight regions
        // no argument needed, this signals the end of the region
        // and resumes normal layout
        void (*HBoxEnd)();


        // This is the same as hbox, but for vertically separated spaces
        void (*VboxTop)(float top_size, float min_size_em);
        void (*VBoxBottom)();
        void (*VBoxEnd)();


        // Display an int number editor as a draggable slider that
        // can be double-clicked to edit the value directly.
        // `min` and `max` define the range of the *value
        // the return value is true if the value was changed this frame
        bool (*DragInt)(const char* name, int* value, int min, int max);


        // Follows the same rules as the sliderint above
        bool (*DragFloat)(const char* name, float* value, float min, float max);


        // Use the remaining vertical space to render the logbuffer
        // `handle` is the logbuffer handle to render
        // `scroll_to_bottom` if true, force the logbuffer region to scroll
        //                    from its current position to the end (newest data)
        void (*ShowLogBuffer)(LogBufferHandle handle, bool scroll_to_bottom);


        // Use the remaining vertical space to render a filtered logbuffer
        // `handle` is the logbuffer handle to render
        // `lines` is an array of line numbers to display
        // `line_count` is the number of lines to display from the `lines` array
        // `scroll_to_bottom` if true, force the logbuffer region to scroll
        //                    from its current position to the end (newest data)
        void (*ShowFilteredLogBuffer)(LogBufferHandle handle, const uint32_t* lines, uint32_t line_count, bool scroll_to_bottom);


        // Show a selectable list of text items.
        // returns true when the user click on an item and the selection changes.
        // `selection` is the index of the selected item and should be a static variable
        // `userdata` is any arbitrary data that you want to pass to the callback
        // `count` is the number of items in the list
        // `to_string` is a function that converts an index and userdata into a string
        //             an is called for each *visible* item in the list every frame.
        // Proper clipping of non-visible items is done automatically so large lists
        // can be drawn in a performant manner.
        // Usually you would want to contain this in an HBox or VBox otherwise
        // it will take up all remaining horizontal and vertical space.
        bool(*SelectionList)(uint32_t *selection, const void* userdata, uint32_t count, CALLBACK_SELECTIONLIST to_string);


        // Draw a table one cell at a time through the `draw_cell` callback
        // `header_labels` is an array of strings that are the column headers
        // `header_count` is the number of strings in the `header_labels` array
        //                this is assumed to be the same as the count of columns
        // `table_userdata` is any arbitrary data that you want to pass to the callback
        // `row_count` is the number of rows in the table, the total number of cells in
        //             the table is row_count * header_count
        // `draw_cell` is a callback that is called for each *visible* cell in the table
        // Proper vertical and horizontal clipping is done to accelerate even very large tables
        void (*Table)(const char * const * const header_labels, uint32_t header_count, void* table_userdata, uint32_t row_count, CALLBACK_TABLE draw_cell);


        // draw tabs that can be switched between
        void (*TabBar)(const char* const* const headers, uint32_t header_count, int* state);


        // draw buttons in a row, similar to a toolbar
        // returns -1 if no button was pressed this frame (common case)
        // otherwise returns the index of the button pressed this frame
        // `labels` is an array of strings that are the button labels
        // `label_count` is the number of strings in the `labels` array
        //               this is assumed to be the count of buttons
        // if there is not enough horizontal space available the buttons
        // out of bounds will wrap to the next row.
        int (*ButtonBar)(const char* const* const labels, uint32_t label_count);


        // show a tooltip marker that looks like "(?)"
        // when the user hovers over the marker `text` will be shown
        // in a little popup
        void (*Tip)(const char* text);


        // place multiple simpledraw elements on the same line
        void (*SameLine)();

#ifdef BETTERAPI_DEVELOPMENT_FEATURES
        // Draw a radiobutton, returns true if the button was selected this frame
        // 
        // `selection_group` is the group this radiobutton belongs to.
        //                   use the address of a static variable here.
        //                   the value is the currently selected radiobutton.
        // 
        // `current_id`      the id of the current button in a group.
        //                   set this to 0 every frame before drawing the 
        //                   first button in a group. no need for a static variable
        bool (*RadioButton)(const char* text, uint32_t* selection_group, uint32_t* current_id);
#endif
};


// the logbuffer API is deprecated and will be changed in a future release
struct log_buffer_api_t {
	// Create a handle, needed to identify the log buffer
        // name is the name of the logbuffer as it shows up in the UI
        // path is an optional path to a logfile to append with logs
        LogBufferHandle (*Create)(const char* name, const char* path);

        // Get the number of lines stored in the buffer
        uint32_t (*GetLineCount)(LogBufferHandle);

        // Retrieve a specific line for storage
        const char* (*GetLine)(LogBufferHandle, uint32_t line);

        // Log a line of text to the buffer
        void (*Append)(LogBufferHandle, const char* log_line);

        // Restore the log buffer from a file, the file is then used for appending
        LogBufferHandle (*Restore)(const char* name, const char* filename);
};


// this api is just a small selection of the most commonly needed standard library functions
// using this api means that your plugin might not need to link with the standard library at all
struct std_api_t {
        // allocate `bytes` of ram, return NULL on error
        void* (*malloc)(size_t bytes);

        // free a previously allocated block of ram
        void (*free)(void* malloc_buffer);

        // printf style formatting into `buffer` of `buffer_size` bytes, returns <0 on error or bytes used
        int (*snprintf)(char* buffer, size_t buffer_size, const char* fmt, ...);

        // copy `bytes` of memory from `src` to `dest`, return dest
        void* (*memcpy)(void* dest, const void* src, size_t bytes);

        // set `bytes` of memory in `dest` to `value`, return dest, value must be <256
        void* (*memset)(void* dest, int value, size_t bytes);
};

struct console_api_t {
        // run `command` on the in-game console
        // max length of `command` is limited to 512 bytes by the game
        // NOTE: the game wont run console commands until the main menu
        //       has finished loading otherwise it freezes waiting
        //       for the console to be ready (if your compiling shaders
        //       the game could seem frozen for several minutes)
        // there should be a way to know when the console is ready, but
        // as i dont link with sfse, addresslibrary, commonlibsf or anything
        // i have to figure it out myself.
        void (*RunCommand)(char* command);
};


// This is all the above structs wrapped up in one place
// why pointers to apis instead of the api itself? so that
// I may extend any api without changing the size of BetterAPI
// this helps with forwards compatibility and i wont need to
// update the BETTERAPI_VERSION as often.
typedef struct better_api_t {
#if BETTERAPI_VERSION > 1
        //I don't know why I didnt think of this before
        const uint64_t version_info;
        //maybe add some more fields before official v2
#endif
        const struct hook_api_t* Hook;
        const struct log_buffer_api_t* LogBuffer;
        const struct simple_draw_t* SimpleDraw;
        const struct callback_api_t* Callback;
        const struct config_api_t* Config;
        const struct std_api_t* Stdlib;
        const struct console_api_t* Console;
} BetterAPI;
#endif // !BETTERAPI_API_H



#ifdef BETTERAPI_ENABLE_SFSE_MINIMAL
#ifndef BETTERAPI_SFSE_MINIMAL
#define BETTERAPI_SFSE_MINIMAL

// For people that want to port Cheat Engine or ASI mods to sfse without including
// sfse code into the project, define BETTERAPI_ENABLE_SFSE_MINIMAL before 
// including betterapi.h, this should get you 90% of the way to a
// plug and play sfse plugin with no brain required:
// 
// #define BETTERAPI_ENABLE_SFSE_MINIMAL
// #define BETTERAPI_IMPLEMENTATION
// #include "betterapi.h"

// if you want your dll to be loadable with sfse you need to export two symbols:
// `SFSEPlugin_Version` and `SFSEPlugin_Load` paste this in one translation unit
// and modify to suit your needs:

/*
// Step 1) Export this struct so sfse knows your DLL is compatible
DLLEXPORT SFSEPluginVersionData SFSEPlugin_Version = {
        1,     // SFSE api version, 1 is current

        1,     // Plugin api version, 1 is current

        "BetterConsole",   // Mod/Plugin Name (limit: 255 characters)

        "Linuxversion",    // Mod Author(s)   (limit: 255 characters)

               // Address Independance:
        1,     // 0 - hardcoded offsets (game version specific)
               // 1 - signature scanning (not version specific)

               // Structure Independance:
        1,     // 0 - relies on specific game structs
               // 1 - mod does not care if game structs change

                                          // Compatible Game Versions:
        {                                 // A list of up to 15 game versions
                MAKE_VERSION(1, 11, 36),  // This means compatible with 1.11.36
                0                         // The list must be terminated with 0
        },                                // if address & structure independent
                                          // then this is minimum version required

        0,      // 0 = does not rely on any specific sfse version

        0, 0               // reserved fields, must be 0
};

// Step 2) Export this function so sfse knows to load your dll.
//         Doing anything inside the function is optional.
DLLEXPORT void SFSEPlugin_Load(const SFSEInterface * sfse) {}
*/


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
        const char*     name;
        uint32_t	version;
} SFSEPluginInfo;

typedef struct SFSEInterface_t {
        uint32_t	sfseVersion;
        uint32_t	runtimeVersion;
        uint32_t	interfaceVersion;
        void*           (*QueryInterface)(uint32_t id);
        PluginHandle    (*GetPluginHandle)(void);
        SFSEPluginInfo* (*GetPluginInfo)(const char* name);
} SFSEInterface;

typedef struct SFSEMessage_t {
        const char*     sender;
        uint32_t        type;
        uint32_t        dataLen;
        void*           data;
} SFSEMessage;

typedef void (*SFSEMessageEventCallback)(SFSEMessage* msg);

typedef struct SFSEMessagingInterface_t {
        uint32_t        interfaceVersion;
        bool            (*RegisterListener)(PluginHandle listener, const char* sender, SFSEMessageEventCallback handler);
        bool	        (*Dispatch)(PluginHandle sender, uint32_t messageType, void* data, uint32_t dataLen, const char* receiver);
}SFSEMessagingInterface;

#endif // !BETTERAPI_SFSE_MINIMAL
#endif

#ifdef BETTERAPI_IMPLEMENTATION
#ifdef __cplusplus
#define DLLEXPORT extern "C" __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllexport)
#endif // __cplusplus
static int OnBetterConsoleLoad(const struct better_api_t* betterapi);
#if BETTERAPI_VERSION == 1
DLLEXPORT void BetterConsoleReceiver(const struct better_api_t* api) {
        OnBetterConsoleLoad(api);
}
#else
#define MAKE_NAME(NAME) NAME##BETTERCONSOLE_VERSION
DLLEXPORT int MAKE_NAME(BetterConsoleReceiver) {
        if (api->betterapi_version == BETTERAPI_VERSION) {
                return OnBetterConsoleLoad(api);
        }
        return -1; //error, plugin not compatible
}
#endif // BETTERAPI_VERSION == 1
#endif // BETTERAPI_IMPLEMENTATION
