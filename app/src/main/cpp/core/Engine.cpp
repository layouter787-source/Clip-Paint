#include "Engine.h"

#include <android/log.h>

#include "../third_party/animbrush/include/animbrush/animbrush.hpp"
#include "Overlay.h"

#define ELOGI(...) __android_log_print(ANDROID_LOG_INFO, "ClipPaint", __VA_ARGS__)
#define ELOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ClipPaint", __VA_ARGS__)

namespace ClipPaint {

Engine& Engine::get() {
    static Engine inst;
    return inst;
}

void Engine::init(AAssetManager* mgr, const std::string& internalDir, const std::string& externalDir, float dp) {
    std::lock_guard<std::mutex> g(mtx);
    if (inited_) return;
    dp_ = dp;
    auto readAsset = [mgr](const std::string& path, std::string& out) -> bool {
        if (!mgr) return false;
        AAsset* as = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_BUFFER);
        if (!as) return false;
        const off_t len = AAsset_getLength(as);
        out.resize((size_t)len);
        const int n = len > 0 ? AAsset_read(as, &out[0], (size_t)len) : 0;
        AAsset_close(as);
        return n == len;
    };
    std::string pack;
    if (readAsset("animbrush/stamps.abpack", pack)) {
        const bool ok = ab::StampBank::instance().loadPackFromMemory(reinterpret_cast<const uint8_t*>(pack.data()), pack.size());
        ELOGI("AnimBrush stamps: %s", ok ? "ok" : "falhou");
    } else {
        ELOGE("stamps.abpack nao encontrado nos assets");
    }
    BrushLibrary::get().init(readAsset);
    ELOGI("Pincéis carregados: %zu", BrushLibrary::get().entries().size());
    ed.setPaths(internalDir, externalDir);
    ed.setScreen(1080, 1920, 0, 0, 0, 0, dp);
    ed.initBrushesAndPrefs();
    inited_ = true;
}

bool Engine::openProject(const std::string& path, int w, int h, bool transparent, int dpi) {
    {
        std::lock_guard<std::mutex> g(mtx);
        ed.cancelCanvasAction();
        if (ed.transforming) ed.cancelTransform();
        bool loaded = false;
        if (fileExists(path)) loaded = loadProject(ed.doc, path);
        if (!loaded) {
            ed.doc.create(w, h);
            ed.doc.paperVisible = !transparent;
        }
        ed.xform = LayerXform();
        ed.dpi = dpi;
        ed.fitView();
        needFit_ = true;
    }
    requestRender(20);
    return true;
}

bool Engine::saveProject(const std::string& path) {
    std::lock_guard<std::mutex> g(mtx);
    if (ed.transforming) ed.applyTransform();
    ed.savePrefs();
    return ClipPaint::saveProject(ed.doc, path);
}

bool Engine::exportPng(const std::string& path) {
    std::lock_guard<std::mutex> g(mtx);
    if (ed.transforming) ed.applyTransform();
    std::vector<uint8_t> px;
    ed.doc.compositeAll(px, ed.doc.paperVisible);
    return writePng(path, ed.doc.width(), ed.doc.height(), px.data(), ed.dpi);
}

bool Engine::saveThumbnail(const std::string& path, int maxSize) {
    std::lock_guard<std::mutex> g(mtx);
    const int W = ed.doc.width(), H = ed.doc.height();
    std::vector<uint8_t> px;
    ed.doc.compositeAll(px, ed.doc.paperVisible);
    const float s = (float)maxSize / (float)std::max(W, H);
    const int tw = std::max(1, (int)(W * std::min(1.0f, s))), th = std::max(1, (int)(H * std::min(1.0f, s)));
    std::vector<uint8_t> out((size_t)tw * th * 4);
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            const int x0 = x * W / tw, x1 = std::max(x0 + 1, (x + 1) * W / tw);
            const int y0 = y * H / th, y1 = std::max(y0 + 1, (y + 1) * H / th);
            float acc[4] = {0, 0, 0, 0};
            int n = 0;
            const int sx = std::max(1, (x1 - x0) / 3), sy = std::max(1, (y1 - y0) / 3);
            for (int yy = y0; yy < y1; yy += sy) {
                for (int xx = x0; xx < x1; xx += sx) {
                    const uint8_t* p = &px[((size_t)yy * W + xx) * 4];
                    const float a = p[3] / 255.0f;
                    acc[0] += p[0] * a; acc[1] += p[1] * a; acc[2] += p[2] * a; acc[3] += a;
                    ++n;
                }
            }
            // composicao sobre cinza claro (transparencia) para a miniatura
            const float a = acc[3] / n;
            const float bg = 235.0f;
            uint8_t* o = &out[((size_t)y * tw + x) * 4];
            for (int c = 0; c < 3; ++c) o[c] = (uint8_t)clampf(acc[c] / n + bg * (1.0f - a) + 0.5f, 0, 255);
            o[3] = 255;
        }
    }
    return writePng(path, tw, th, out.data());
}

void Engine::setMargins(float top, float bottom) {
    std::lock_guard<std::mutex> g(mtx);
    ed.setSafeMargins(top, bottom);
}

void Engine::touch(const TouchEvent& ev) {
    {
        std::lock_guard<std::mutex> g(mtx);
        input_.handle(ev, ed);
    }
    requestRender(6);
}

void Engine::requestRender(int frames) {
    {
        std::lock_guard<std::mutex> g(m_);
        activity_ = std::max(activity_, frames);
    }
    cv_.notify_one();
}

// -------------------------------------------------------------- renderizacao

void Engine::surfaceCreated(ANativeWindow* window) {
    surfaceDestroyed();
    window_ = window;
    {
        std::lock_guard<std::mutex> g(m_);
        running_ = true;
        activity_ = 30;
    }
    th_ = std::thread([this] { renderLoop(); });
}

void Engine::surfaceDestroyed() {
    {
        std::lock_guard<std::mutex> g(m_);
        running_ = false;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

void Engine::renderLoop() {
    if (!rd_.initialize(window_)) {
        ELOGE("Falha ao inicializar o renderizador");
        return;
    }
    lastW_ = lastH_ = 0;
    {
        std::lock_guard<std::mutex> g(mtx);
        ed.doc.touch();  // forca o reenvio das texturas ao novo contexto GL
    }
    while (true) {
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait_for(lk, std::chrono::milliseconds(400), [this] { return !running_ || activity_ > 0; });
            if (!running_) break;
            if (activity_ <= 0) continue;
        }
        frame();
    }
    rd_.destroy();
}

void Engine::frame() {
    std::unique_lock<std::mutex> lk(mtx);
    rd_.pollSize();
    const int w = rd_.width(), h = rd_.height();
    if (w != lastW_ || h != lastH_) {
        ed.setScreen(w, h, 0, 0, 0, 0, dp_);
        lastW_ = w;
        lastH_ = h;
    }
    if (needFit_ && w > 0) {
        ed.fitView();
        needFit_ = false;
    }
    const double now = nowSeconds();
    ed.update(now);
    const bool keep = ed.hasPendingWork() || ed.canvasBusy() || ed.transforming || ed.doc.hasSelection() ||
                      now < ed.ringUntil || now < ed.toastUntil || input_.busy();
    rd_.syncDocument(ed.doc, ed.xform);
    rd_.beginFrame();
    rd_.drawCanvas(ed.doc, ed.view, (float)now, true, dp_);
    drawOverlays(rd_.ui, ed, w, h, now);
    lk.unlock();
    rd_.endFrame();
    std::lock_guard<std::mutex> g(m_);
    if (keep) activity_ = std::max(activity_, 6);
    if (activity_ > 0) --activity_;
}

}  // namespace ClipPaint
