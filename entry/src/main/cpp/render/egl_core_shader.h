#ifndef EGL_CORE_SHADER_H
#define EGL_CORE_SHADER_H

#include <functional>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <native_vsync/native_vsync.h>
#include <string>

class EGLCore {
public:
    EGLCore(std::string &id) : mId(id) {}
    
    void OnSurfaceCreated(void *window, int w, int h);
    void OnSurfaceChanged(void *window, int32_t w, int32_t h);
    void OnSurfaceDestroyed();
    void GameLoop();
    void Update();
    
    void MovePlayerLeft();
    void MovePlayerRight();
    void RestartGame();
    void SetGameOverCallback(std::function<void(int)> callback);
    
    GLuint LoadShader(GLenum type, const char *shaderSrc);
    GLuint CreateProgram(const char *vertexShader, const char *fragShader);
    GLuint CreateProgramError(const char *vertexShader, const char *fragShader);
    
    void DrawSquare();
    void switchDiffuse();
    void switchAmbient();
    void switchSpecular();

private:
    std::string mId;
    EGLNativeWindowType mEglWindow;
    EGLDisplay mEGLDisplay = EGL_NO_DISPLAY;
    EGLConfig mEGLConfig = nullptr;
    EGLContext mEGLContext = EGL_NO_CONTEXT;
    EGLContext mSharedEGLContext = EGL_NO_CONTEXT;
    EGLSurface mEGLSurface = nullptr;
    GLuint mProgramHandle;
    OH_NativeVSync *mVsync = nullptr;
    int width_ = 0;
    int height_ = 0;
};

#endif