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
