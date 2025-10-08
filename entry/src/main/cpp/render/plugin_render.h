#ifndef PLUGIN_RENDER_H
#define PLUGIN_RENDER_H

#include <unordered_map>
#include <string>
#include <napi/native_api.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include "render/egl_core_shader.h"  // Forward declaration yerine tam include

class PluginRender {
public:
    explicit PluginRender(std::string &id);
    ~PluginRender() = default;
    
    static PluginRender* GetInstance(std::string &id);
    static OH_NativeXComponent_Callback* GetNXComponentCallback();
    
    void SetNativeXComponent(OH_NativeXComponent* component);
    void OnSurfaceCreated(OH_NativeXComponent* component, void* window);
    void OnSurfaceChanged(OH_NativeXComponent* component, void* window);
    void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window);
    
    napi_value Export(napi_env env, napi_value exports);
    
    // NAPI functions
    static napi_value NapiMoveLeft(napi_env env, napi_callback_info info);
    static napi_value NapiMoveRight(napi_env env, napi_callback_info info);
    static napi_value NapiRestartGame(napi_env env, napi_callback_info info);
    static napi_value NapiSetGameOverCallback(napi_env env, napi_callback_info info);
    static napi_value NapiSwitchAmbient(napi_env env, napi_callback_info info);
    static napi_value NapiSwitchDiffuse(napi_env env, napi_callback_info info);
    static napi_value NapiSwitchSpecular(napi_env env, napi_callback_info info);

    EGLCore* eglCore_;

private:
    static std::unordered_map<std::string, PluginRender*> instance_;
    static OH_NativeXComponent_Callback callback_;
    std::string id_;
    uint64_t width_;
    uint64_t height_;
};

#endif // PLUGIN_RENDER_H