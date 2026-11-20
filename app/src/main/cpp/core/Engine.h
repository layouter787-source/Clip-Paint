// Clip Paint - motor nativo: documento + editor + thread de renderizacao (EGL/GLES3) ligados ao Kotlin via JNI.
#pragma once

#include <android/asset_manager.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Editor.h"
#include "InputManager.h"

namespace ClipPaint {

class Engine {
public:
    static Engine& get();

    std::mutex mtx;   // protege `ed` (documento, ferramentas); segurar em toda chamada que o toque/UI faz
    Editor ed;

    void init(AAssetManager* mgr, const std::string& internalDir, const std::string& externalDir, float dp);
    bool inited() const { return inited_; }

    bool openProject(const std::string& path, int w, int h, bool transparent, int dpi);
    bool saveProject(const std::string& path);
    bool exportPng(const std::string& path);
    bool saveThumbnail(const std::string& path, int maxSize);

    void surfaceCreated(ANativeWindow* window);  // assume a posse (ja adquirida)
    void surfaceDestroyed();
    void setMargins(float top, float bottom);
    void touch(const TouchEvent& ev);
    void requestRender(int frames = 8);

private:
    void renderLoop();
    void frame();

    InputManager input_;
    Renderer rd_;
    ANativeWindow* window_ = nullptr;
    std::thread th_;
    std::mutex m_;
    std::condition_variable cv_;
    bool running_ = false;
    int activity_ = 0;
    bool inited_ = false;
    float dp_ = 2.75f;
    int lastW_ = 0, lastH_ = 0;
    bool needFit_ = true;
};

}  // namespace ClipPaint
