#include "callback.h"

#include <vector>

struct Callback {
        const char* name;
        FUNC_PTR callback;
};

static std::vector<Callback> SimpleCallbacks{};
static std::vector<ImGuiDrawCallback> DrawCallbacks{};


static void RegisterSimpleDrawCallback(const char* name, FUNC_PTR callback) {
        SimpleCallbacks.push_back(Callback{ name, callback });
}

static void RegisterImGuiDrawCallback(ImGuiDrawCallback callback) {
        DrawCallbacks.push_back(callback);
}

extern const struct callback_api_t* GetCallbackAPI() {
        static bool init = false;
        static callback_api_t ret;
        if (!init) {
                init = true;
                ret.SimpleDrawCallback = &RegisterSimpleDrawCallback;
                ret.ImGuiDrawCallback = &RegisterImGuiDrawCallback;
        }
        return &ret;
}


//i dont like mixing different domains here, but that is a job for the next refactor
extern void draw_simpledraw_tabs() {
        for (const auto cb : SimpleCallbacks) {
                if (ImGui::BeginTabItem(cb.name)) {
                        ImGui::PushID(cb.name);
                        cb.callback();
                        ImGui::PopID();
                        ImGui::EndTabItem();
                }
        }
}

extern void draw_imguidraw_callbacks() {
        auto ctx = ImGui::GetCurrentContext();
        for (const auto cb : DrawCallbacks) {
                cb(ctx);
        }
}