#include <cstdint>
#include <hilog/log.h>
#include "common/plugin_common.h"
#include "manager/plugin_manager.h"
#include "native_common.h"
#include "render/plugin_render.h"

std::unordered_map<std::string, PluginRender *> PluginRender::instance_;
OH_NativeXComponent_Callback PluginRender::callback_;

static napi_threadsafe_function g_gameOverTsfn = nullptr;

struct GameOverPayload {
    int score;
};

static void CallGameOverJS(napi_env env, napi_value js_callback, void *context, void *data) {
    std::unique_ptr<GameOverPayload> payload(static_cast<GameOverPayload *>(data));

    if (env == nullptr || js_callback == nullptr) {
        LOGE("CallGameOverJS: env or js_callback is null");
        return;
    }

    napi_value scoreArg;
    napi_status status = napi_create_int32(env, payload->score, &scoreArg);
    if (status != napi_ok) {
        LOGE("Failed to create score argument");
        return;
    }

    napi_value undefined;
    napi_get_undefined(env, &undefined);

    napi_value result;
    status = napi_call_function(env, undefined, js_callback, 1, &scoreArg, &result);

    if (status == napi_ok) {
        LOGI("✅ Game over callback called successfully with score: %{public}d", payload->score);
    } else {
        LOGE("❌ Failed to call game over callback, status: %{public}d", status);
    }
}

void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    LOGD("OnSurfaceCreatedCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        LOGE("GetXComponentId failed");
        return;
    }

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceCreated(component, window);
}

void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window) {
    LOGD("OnSurfaceChangedCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS)
        return;

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceChanged(component, window);
}

void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {
    LOGD("OnSurfaceDestroyedCB");
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS)
        return;

    std::string id(idStr);
    auto render = PluginRender::GetInstance(id);
    render->OnSurfaceDestroyed(component, window);
}

PluginRender::PluginRender(std::string &id) : id_(id) {
    eglCore_ = new EGLCore(id);
    auto renderCallback = PluginRender::GetNXComponentCallback();
    renderCallback->OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback->OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback->OnSurfaceDestroyed = OnSurfaceDestroyedCB;
}

PluginRender *PluginRender::GetInstance(std::string &id) {
    if (instance_.find(id) == instance_.end()) {
        PluginRender *instance = new PluginRender(id);
        instance_[id] = instance;
        return instance;
    } else {
        return instance_[id];
    }
}

OH_NativeXComponent_Callback *PluginRender::GetNXComponentCallback() { return &PluginRender::callback_; }

void PluginRender::SetNativeXComponent(OH_NativeXComponent *component) {
    OH_NativeXComponent_RegisterCallback(component, &PluginRender::callback_);
}

void PluginRender::OnSurfaceCreated(OH_NativeXComponent *component, void *window) {
    LOGD("PluginRender::OnSurfaceCreated");
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width_, &height_);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS && eglCore_) {
        eglCore_->OnSurfaceCreated(window, width_, height_);
    }
}

void PluginRender::OnSurfaceChanged(OH_NativeXComponent *component, void *window) {
    LOGD("PluginRender::OnSurfaceChanged");
    int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width_, &height_);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS && eglCore_) {
        eglCore_->OnSurfaceChanged(window, width_, height_);
    }
}

void PluginRender::OnSurfaceDestroyed(OH_NativeXComponent *component, void *window) {
    LOGD("PluginRender::OnSurfaceDestroyed");
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    int32_t ret = OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    if (ret == OH_NATIVEXCOMPONENT_RESULT_SUCCESS && eglCore_) {
        eglCore_->OnSurfaceDestroyed();
    }
    if (eglCore_) {
        delete eglCore_;
        eglCore_ = nullptr;
    }

    // TSFN temizle
    if (g_gameOverTsfn != nullptr) {
        napi_release_threadsafe_function(g_gameOverTsfn, napi_tsfn_release);
        g_gameOverTsfn = nullptr;
    }

    if (instance_[id_]) {
        delete instance_[id_];
        instance_[id_] = nullptr;
        instance_.erase(id_);
    }
}

napi_value PluginRender::Export(napi_env env, napi_value exports) {
    LOGI("PluginRender::Export");

    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("moveLeft", PluginRender::NapiMoveLeft),
        DECLARE_NAPI_FUNCTION("moveRight", PluginRender::NapiMoveRight),
        DECLARE_NAPI_FUNCTION("restartGame", PluginRender::NapiRestartGame),
        DECLARE_NAPI_FUNCTION("setGameOverCallback", PluginRender::NapiSetGameOverCallback),
        DECLARE_NAPI_FUNCTION("switchAmbient", PluginRender::NapiSwitchAmbient),
        DECLARE_NAPI_FUNCTION("switchDiffuse", PluginRender::NapiSwitchDiffuse),
        DECLARE_NAPI_FUNCTION("switchSpecular", PluginRender::NapiSwitchSpecular),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}

napi_value PluginRender::NapiSetGameOverCallback(napi_env env, napi_callback_info info) {
    LOGI("NapiSetGameOverCallback called");

    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok || argc < 2) {
        LOGE("Failed to get callback info or wrong argument count");
        return nullptr;
    }

    if (g_gameOverTsfn != nullptr) {
        napi_release_threadsafe_function(g_gameOverTsfn, napi_tsfn_release);
        g_gameOverTsfn = nullptr;
    }

    napi_value resourceName;
    napi_create_string_utf8(env, "GameOverCallbackTSFN", NAPI_AUTO_LENGTH, &resourceName);

    status = napi_create_threadsafe_function(env, args[1], nullptr, resourceName, 0, 1, nullptr, nullptr, nullptr,
                                             CallGameOverJS, &g_gameOverTsfn);

    if (status != napi_ok) {
        LOGE("Failed to create threadsafe function");
        return nullptr;
    }

    LOGI("Threadsafe function created successfully");

    std::string id("A");
    PluginRender *instance = PluginRender::GetInstance(id);

    if (instance && instance->eglCore_) {
        instance->eglCore_->SetGameOverCallback([](int finalScore) {
            LOGI("Game over! Triggering callback with score: %{public}d", finalScore);

            if (g_gameOverTsfn == nullptr) {
                LOGE("TSFN is null, cannot call callback");
                return;
            }

            auto *payload = new GameOverPayload{finalScore};

            napi_status callStatus = napi_call_threadsafe_function(g_gameOverTsfn, payload, napi_tsfn_nonblocking);

            if (callStatus != napi_ok) {
                LOGE("Failed to call threadsafe function, status: %{public}d", callStatus);
                delete payload;
            } else {
                LOGI("Threadsafe function call queued successfully");
            }
        });

        LOGI("Game over callback registered in EGLCore");
    }

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

napi_value PluginRender::NapiMoveLeft(napi_env env, napi_callback_info info) {
    LOGD("NapiMoveLeft called");

    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok || argc < 1) {
        LOGE("NapiMoveLeft: Failed to get callback info");
        return nullptr;
    }

    napi_value exportInstance = args[0];
    OH_NativeXComponent *nativeXComponent = nullptr;

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        LOGE("NapiMoveLeft: unwrap failed");
        return nullptr;
    }

    std::string id("A");
    PluginRender *instance = PluginRender::GetInstance(id);
    if (instance && instance->eglCore_) {
        instance->eglCore_->MovePlayerLeft();
        LOGI("Player moved left");
    }
    return nullptr;
}

napi_value PluginRender::NapiMoveRight(napi_env env, napi_callback_info info) {
    LOGD("NapiMoveRight called");

    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok || argc < 1) {
        LOGE("NapiMoveRight: Failed to get callback info");
        return nullptr;
    }

    napi_value exportInstance = args[0];
    OH_NativeXComponent *nativeXComponent = nullptr;

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        LOGE("NapiMoveRight: unwrap failed");
        return nullptr;
    }

    std::string id("A");
    PluginRender *instance = PluginRender::GetInstance(id);
    if (instance && instance->eglCore_) {
        instance->eglCore_->MovePlayerRight();
        LOGI("Player moved right");
    }
    return nullptr;
}

napi_value PluginRender::NapiRestartGame(napi_env env, napi_callback_info info) {
    LOGD("NapiRestartGame called");

    size_t argc = 1;
    napi_value args[1] = {nullptr};

    napi_status status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (status != napi_ok || argc < 1) {
        LOGE("NapiRestartGame: Failed to get callback info");
        return nullptr;
    }

    napi_value exportInstance = args[0];
    OH_NativeXComponent *nativeXComponent = nullptr;

    status = napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    if (status != napi_ok) {
        LOGE("NapiRestartGame: unwrap failed");
        return nullptr;
    }

    std::string id("A");
    PluginRender *instance = PluginRender::GetInstance(id);
    if (instance && instance->eglCore_) {
        instance->eglCore_->RestartGame();
        LOGI("Game restarted");
    }
    return nullptr;
}

napi_value PluginRender::NapiSwitchAmbient(napi_env env, napi_callback_info info) {
    LOGD("NapiSwitchAmbient - Deprecated");
    return nullptr;
}

napi_value PluginRender::NapiSwitchDiffuse(napi_env env, napi_callback_info info) {
    LOGD("NapiSwitchDiffuse - Deprecated");
    return nullptr;
}

napi_value PluginRender::NapiSwitchSpecular(napi_env env, napi_callback_info info) {
    LOGD("NapiSwitchSpecular - Deprecated");
    return nullptr;
}