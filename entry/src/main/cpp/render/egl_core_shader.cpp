#include <hilog/log.h>
#include <random>
#include <cmath>
#include <functional>
#include "egl_core_shader.h"
#include "plugin_common.h"

const char *GAME_SYNC_NAME = "openglVSync";

#ifndef EGL_GL_COLORSPACE_KHR
#define EGL_GL_COLORSPACE_KHR 0x309D
#endif

#ifndef EGL_GL_COLORSPACE_SRGB_KHR
#define EGL_GL_COLORSPACE_SRGB_KHR 0x3089
#endif

char vertexShader[] = "#version 300 es\n"
                      "layout(location = 0) in vec4 a_position;\n"
                      "layout(location = 1) in vec4 a_color;\n"
                      "out vec4 v_color;\n"
                      "void main()\n"
                      "{\n"
                      "   gl_Position = a_position;\n"
                      "   v_color = a_color;\n"
                      "}\n";

char fragmentShader[] = "#version 300 es\n"
                        "precision mediump float;\n"
                        "in vec4 v_color;\n"
                        "out vec4 fragColor;\n"
                        "void main()\n"
                        "{\n"
                        "   fragColor = v_color;\n"
                        "}\n";

struct GameObject {
    float x, y;
    float width, height;
    float speedX, speedY;
    bool active;
};

GameObject player;
GameObject obstacles[25];
int score = 0;
bool gameOver = false;
int frameCount = 0;
std::mt19937 rng;

static std::function<void(int)> g_gameOverCallback = nullptr;

void InitGame() {
    player.x = 0.0f;
    player.y = -0.8f;
    player.width = 0.15f;
    player.height = 0.15f;
    player.speedX = 0.04f;
    player.active = true;

    rng.seed(std::random_device{}());
    for (int i = 0; i < 35; i++) {
        obstacles[i].active = false;
    }

    score = 0;
    gameOver = false;
    frameCount = 0;
    LOGI("Game initialized");
}

void SpawnObstacle() {
    std::uniform_real_distribution<float> dist(-0.75f, 0.75f);

    for (int i = 0; i < 25; i++) {
        if (!obstacles[i].active) {
            obstacles[i].x = dist(rng);
            obstacles[i].y = 1.2f;
            obstacles[i].width = 0.12f;
            obstacles[i].height = 0.12f;
            obstacles[i].speedY = -0.025f - (score / 1000.0f * 0.008f);
            obstacles[i].active = true;
            break;
        }
    }
}

bool CheckCollision(GameObject &a, GameObject &b) {
    return (a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y);
}

void UpdateGame() {
    if (gameOver)
        return;

    frameCount++;

    bool allInactive = true;
    for (int i = 0; i < 25; i++) {
        if (obstacles[i].active) {
            allInactive = false;
            obstacles[i].y += obstacles[i].speedY;

            if (CheckCollision(player, obstacles[i])) {
                gameOver = true;
                LOGI("COLLISION! GAME OVER! Final Score: %{public}d", score);

                if (g_gameOverCallback) {
                    LOGI("Callback exists, calling it now...");
                    g_gameOverCallback(score);
                    LOGI("Callback called");
                } else {
                    LOGE("Callback is NULL!");
                }
                return;
            }

            if (obstacles[i].y < -1.5f) {
                obstacles[i].active = false;
                score += 10;
            }
        }
    }

    if (frameCount % 25 == 0 || allInactive) {
        SpawnObstacle();
    }

    std::uniform_int_distribution<int> randomSpawn(0, 100);
    if (frameCount % 10 == 0 && randomSpawn(rng) < 30) { // %30 şans
        SpawnObstacle();
    }
}

void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    GLfloat vertices[] = {x - w / 2, y - h / 2, x + w / 2, y - h / 2, x - w / 2, y + h / 2, x + w / 2, y + h / 2};

    GLfloat colors[] = {r, g, b, a, r, g, b, a, r, g, b, a, r, g, b, a};

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

struct SyncParam {
    EGLCore *eglCore = nullptr;
    void *window = nullptr;
};

static EGLConfig getConfig(EGLDisplay eglDisplay) {
    int attribList[] = {EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        8,
                        EGL_BLUE_SIZE,
                        8,
                        EGL_ALPHA_SIZE,
                        8,
                        EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES2_BIT,
                        EGL_NONE};
    EGLConfig configs = NULL;
    int configsNum;
    if (!eglChooseConfig(eglDisplay, attribList, &configs, 1, &configsNum)) {
        LOGE("eglChooseConfig ERROR");
        return NULL;
    }
    return configs;
}

void EGLCore::SetGameOverCallback(std::function<void(int)> callback) {
    g_gameOverCallback = callback;
    LOGI("Game over callback SET in EGLCore");
}

void EGLCore::OnSurfaceCreated(void *window, int w, int h) {
    LOGD("EGLCore::OnSurfaceCreated w=%{public}d, h=%{public}d", w, h);
    width_ = w;
    height_ = h;

    InitGame();

    SyncParam *param = new SyncParam();
    param->eglCore = this;
    param->window = window;
    mVsync = OH_NativeVSync_Create(GAME_SYNC_NAME, 3);

    if (!mVsync) {
        LOGE("Create mVsync failed");
        return;
    }

    OH_NativeVSync_RequestFrame(
        mVsync,
        [](long long timestamp, void *data) {
            SyncParam *syncParam = (SyncParam *)data;
            if (!syncParam)
                return;

            EGLCore *eglCore = syncParam->eglCore;
            void *window = syncParam->window;
            if (!eglCore || !window)
                return;

            eglCore->mEglWindow = static_cast<EGLNativeWindowType>(window);
            eglCore->mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

            if (eglCore->mEGLDisplay == EGL_NO_DISPLAY) {
                LOGE("Unable to get EGL display");
                return;
            }

            EGLint eglMajVers, eglMinVers;
            if (!eglInitialize(eglCore->mEGLDisplay, &eglMajVers, &eglMinVers)) {
                LOGE("Unable to initialize display");
                return;
            }

            eglCore->mEGLConfig = getConfig(eglCore->mEGLDisplay);
            if (!eglCore->mEGLConfig) {
                LOGE("Config ERROR");
                return;
            }

            EGLint winAttribs[] = {EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE};
            eglCore->mEGLSurface =
                eglCreateWindowSurface(eglCore->mEGLDisplay, eglCore->mEGLConfig, eglCore->mEglWindow, winAttribs);

            if (!eglCore->mEGLSurface) {
                LOGE("eglSurface is null");
                return;
            }

            int attrib3_list[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
            eglCore->mEGLContext =
                eglCreateContext(eglCore->mEGLDisplay, eglCore->mEGLConfig, eglCore->mSharedEGLContext, attrib3_list);

            if (!eglMakeCurrent(eglCore->mEGLDisplay, eglCore->mEGLSurface, eglCore->mEGLSurface,
                                eglCore->mEGLContext)) {
                LOGE("eglMakeCurrent error = %{public}d", eglGetError());
                return;
            }

            eglCore->mProgramHandle = eglCore->CreateProgram(vertexShader, fragmentShader);
            if (!eglCore->mProgramHandle) {
                LOGE("Could not create program");
                return;
            }

            LOGI("EGL initialized successfully, starting game loop");
            eglCore->GameLoop();
        },
        param);
}

void EGLCore::GameLoop() {
    if (!eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext)) {
        LOGE("GameLoop: eglMakeCurrent error = %{public}d", eglGetError());
        return;
    }

    UpdateGame();

    glViewport(0, 0, width_, height_);
    glClearColor(0.04f, 0.04f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(mProgramHandle);

    if (player.active) {
        DrawRect(player.x, player.y, player.width, player.height, 0.0f, 1.0f, 1.0f, 1.0f);
    }

    for (int i = 0; i < 25; i++) {
        if (obstacles[i].active) {
            DrawRect(obstacles[i].x, obstacles[i].y, obstacles[i].width, obstacles[i].height, 1.0f, 0.2f, 0.2f, 1.0f);
        }
    }

    glFlush();
    glFinish();
    eglSwapBuffers(mEGLDisplay, mEGLSurface);

    if (!gameOver) {
        OH_NativeVSync_RequestFrame(
            mVsync, [](long long timestamp, void *data) { (reinterpret_cast<EGLCore *>(data))->GameLoop(); },
            (void *)this);
    } else {
        LOGI("Game loop stopped - Game Over");
    }
}

void EGLCore::MovePlayerLeft() {
    if (gameOver || !player.active)
        return;
    player.x -= player.speedX;
    if (player.x - player.width / 2 < -1.0f) {
        player.x = -1.0f + player.width / 5;
    }
}

void EGLCore::MovePlayerRight() {
    if (gameOver || !player.active)
        return;

    player.x += player.speedX;
    if (player.x + player.width / 2 > 1.0f) {
        player.x = 1.0f - player.width / 5;
    }
}

void EGLCore::RestartGame() {
    LOGI("Restarting game...");
    InitGame();
    if (mVsync) {
        GameLoop();
    }
}

void EGLCore::Update() { eglSwapBuffers(mEGLDisplay, mEGLSurface); }

GLuint EGLCore::LoadShader(GLenum type, const char *shaderSrc) {
    GLuint shader = glCreateShader(type);
    if (shader == 0)
        return 0;

    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char *infoLog = (char *)malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
            LOGE("Shader compile error: %{public}s", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint EGLCore::CreateProgram(const char *vertexShader, const char *fragShader) {
    GLuint vertex = LoadShader(GL_VERTEX_SHADER, vertexShader);
    GLuint fragment = LoadShader(GL_FRAGMENT_SHADER, fragShader);
    GLuint program = glCreateProgram();

    if (vertex == 0 || fragment == 0 || program == 0)
        return 0;

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char *infoLog = (char *)malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(program, infoLen, nullptr, infoLog);
            LOGE("Program link error: %{public}s", infoLog);
            free(infoLog);
        }
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

GLuint EGLCore::CreateProgramError(const char *vertexShader, const char *fragShader) { return 0; }

void EGLCore::OnSurfaceDestroyed() {
    LOGI("EGLCore::OnSurfaceDestroyed");
    if (mVsync) {
        OH_NativeVSync_Destroy(mVsync);
        mVsync = nullptr;
    }
    if (mEGLContext != EGL_NO_CONTEXT) {
        eglDestroyContext(mEGLDisplay, mEGLContext);
        mEGLContext = EGL_NO_CONTEXT;
    }
    if (mEGLSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mEGLDisplay, mEGLSurface);
        mEGLSurface = EGL_NO_SURFACE;
    }
}

void EGLCore::OnSurfaceChanged(void *window, int32_t w, int32_t h) {
    width_ = w;
    height_ = h;
}

void EGLCore::DrawSquare() {}
void EGLCore::switchDiffuse() {}
void EGLCore::switchAmbient() {}
void EGLCore::switchSpecular() {}