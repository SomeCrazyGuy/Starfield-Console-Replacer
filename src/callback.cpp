#include "main.h"

#include "callback.h"

#include <vector>


static std::vector<ModInfo> Infos{};

extern const ModInfo* GetModInfo(size_t* out_count) {
        *out_count = Infos.size();
        return (*out_count > 0)? &Infos[0] : nullptr;
}

static void RegisterModInfo(const ModInfo info) {
        ASSERT(info.Name != nullptr);
        Infos.push_back(info);
}

static constexpr struct callback_api_t CallbackAPI {
        RegisterModInfo
};

extern constexpr const struct callback_api_t* GetCallbackAPI() {
        return &CallbackAPI;
}


// TODO: reimplement the callback infastructure once settings and log tabs are generated automatically
// ie RegisterPluginLog(LogBufferHandle, filename)
// and the bindsettings interface

/*

callback types:
        tab
        window,
        overlay

        everyframe -- rename to periodic? does it need to be every frame?
        onkeydown -- aka hotkey callback, mabye the lookup should be an unordered_map, handle conflicts?

allow plugins to register all 3 draw callbacks?

//TODO: how would i get names from this? need to return a struct with the name* too...
extern FUNC_PTR* GetCallbackArray(callback_type, &callback_count)

maybe also rewrite the public api part ??:
SetCallback(callback_type, func_ptr);


*/
