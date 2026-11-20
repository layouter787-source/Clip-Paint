#include "InputManager.h"

namespace ClipPaint {

namespace {
bool isDrawingTool(ToolId t) { return t == ToolId::Brush || t == ToolId::Eraser; }

float shapePressure(float p, bool stylus) { return stylus ? std::pow(clampf(p, 0.0f, 1.0f), 0.85f) : 1.0f; }
}  // namespace

int InputManager::findCanvas(int id) const {
    for (size_t i = 0; i < canvas_.size(); ++i) if (canvas_[i].id == id) return (int)i;
    return -1;
}

void InputManager::startGesture(Editor& ed, double t) {
    g_p0_[0] = canvas_[0].pos;
    g_p0_[1] = canvas_[1].pos;
    const Vec2 d = canvas_[1].pos - canvas_[0].pos;
    g_d0_ = std::max(1.0f, d.length());
    g_a0_ = std::atan2(d.y, d.x);
    g_mid0_ = (canvas_[0].pos + canvas_[1].pos) * 0.5f;
    g_view0_ = ed.view;
    g_doc0_ = ed.view.toDoc(g_mid0_);
    g_prevD_ = g_d0_;
    g_prevA_ = g_a0_;
    g_prevMid_ = g_mid0_;
    g_start_ = t;
    g_moved_ = false;
    g_fired_ = false;
    g_maxCount_ = 2;
}

void InputManager::updateGesture(Editor& ed) {
    if (canvas_.size() < 2) return;
    const Vec2 a = canvas_[0].pos, b = canvas_[1].pos;
    const Vec2 d = b - a;
    const float dist = std::max(1.0f, d.length());
    const float ang = std::atan2(d.y, d.x);
    const Vec2 mid = (a + b) * 0.5f;
    if (!g_moved_) {
        const float thr = 14.0f * ed.dp();
        if ((a - g_p0_[0]).length() > thr || (b - g_p0_[1]).length() > thr) g_moved_ = true;
    }
    if (ed.transforming) {
        const Vec2 pivot = ed.view.toDoc(g_prevMid_);
        float dr = ang - g_prevA_;
        while (dr > kPi) dr -= 2 * kPi;
        while (dr < -kPi) dr += 2 * kPi;
        ed.transformPinch(pivot, dist / g_prevD_, dr);
        ed.transformTranslate(ed.view.toDoc(mid) - ed.view.toDoc(g_prevMid_));
    } else {
        float dr = ang - g_a0_;
        while (dr > kPi) dr -= 2 * kPi;
        while (dr < -kPi) dr += 2 * kPi;
        ViewState v;
        v.scale = clampf(g_view0_.scale * dist / g_d0_, 0.03f, 48.0f);
        v.rot = g_view0_.rot + dr;
        float r = std::fmod(v.rot, 2 * kPi);
        if (r > kPi) r -= 2 * kPi;
        if (r < -kPi) r += 2 * kPi;
        if (std::fabs(r) < 0.06f) v.rot = 0.0f;  // "ima" para a posicao reta
        const float c = std::cos(v.rot), s = std::sin(v.rot);
        v.offset = Vec2(mid.x - (g_doc0_.x * c - g_doc0_.y * s) * v.scale,
                        mid.y - (g_doc0_.x * s + g_doc0_.y * c) * v.scale);
        ed.view = v;
    }
    g_prevD_ = dist;
    g_prevA_ = ang;
    g_prevMid_ = mid;
}

void InputManager::endAll(Editor& ed) {
    ed.cancelCanvasAction();
    canvas_.clear();
    mode_ = Mode::None;
    if (eraserSwap_) { ed.setTool(prevTool_); eraserSwap_ = false; }
}

void InputManager::handle(const TouchEvent& ev, Editor& ed) {
    const double now = ev.time;
    constexpr int kDown = 0, kUp = 1, kMove = 2, kCancel = 3, kPDown = 5, kPUp = 6;

    switch (ev.action) {
        case kDown:
        case kPDown: {
            const size_t i = ev.action == kDown ? 0 : (size_t)ev.actionIndex;
            if (i >= ev.pointers.size()) return;
            const TouchPointer& tp = ev.pointers[i];
            const Vec2 p(tp.x, tp.y);
            const bool stylus = tp.toolType == 2 || tp.toolType == 4;
            canvas_.push_back({tp.id, p});
            if (canvas_.size() == 1) {
                if (tp.toolType == 4 && ed.tool != ToolId::Eraser) {
                    prevTool_ = ed.tool;
                    ed.setTool(ToolId::Eraser);
                    eraserSwap_ = true;
                }
                downTime_ = now;
                if (isDrawingTool(ed.tool) && !stylus && !ed.fingerDraws) {
                    mode_ = Mode::Pan;
                    lastPan_ = p;
                } else {
                    mode_ = Mode::Draw;
                    ed.canvasDown(p, shapePressure(tp.pressure, stylus), now, stylus);
                }
            } else if (canvas_.size() == 2) {
                if (mode_ == Mode::Draw) {
                    if (now - downTime_ < 0.4 || !isDrawingTool(ed.tool)) ed.cancelCanvasAction();
                    else ed.canvasUp(canvas_[0].pos, now);
                }
                mode_ = Mode::Gesture;
                startGesture(ed, now);
            } else if (mode_ == Mode::Gesture) {
                g_maxCount_ = std::max(g_maxCount_, (int)canvas_.size());
            }
            return;
        }
        case kMove: {
            for (const TouchPointer& tp : ev.pointers) {
                const Vec2 p(tp.x, tp.y);
                const int ci = findCanvas(tp.id);
                if (ci < 0) continue;
                if (mode_ == Mode::Draw && canvas_.size() == 1) {
                    const bool stylus = tp.toolType == 2 || tp.toolType == 4;
                    for (const TouchHist& h : tp.history)
                        ed.canvasMove(Vec2(h.x, h.y), shapePressure(h.pressure, stylus), h.time);
                    ed.canvasMove(p, shapePressure(tp.pressure, stylus), now);
                } else if (mode_ == Mode::Pan && canvas_.size() == 1) {
                    ed.view.offset = ed.view.offset + (p - lastPan_);
                    lastPan_ = p;
                }
                canvas_[(size_t)ci].pos = p;
            }
            if (mode_ == Mode::Gesture) updateGesture(ed);
            return;
        }
        case kUp:
        case kPUp: {
            const size_t i = ev.action == kUp ? 0 : (size_t)ev.actionIndex;
            if (i >= ev.pointers.size()) { endAll(ed); return; }
            const TouchPointer& tp = ev.pointers[i];
            const Vec2 p(tp.x, tp.y);
            const int ci = findCanvas(tp.id);
            if (ci < 0) return;
            if (mode_ == Mode::Gesture && !g_fired_ && !g_moved_ && now - g_start_ < 0.32) {
                g_fired_ = true;
                if (g_maxCount_ == 2) ed.undo();
                else if (g_maxCount_ >= 3) ed.redo();
            }
            if (mode_ == Mode::Draw && canvas_.size() == 1) ed.canvasUp(p, now);
            canvas_.erase(canvas_.begin() + ci);
            if (canvas_.empty()) {
                mode_ = Mode::None;
                if (eraserSwap_) { ed.setTool(prevTool_); eraserSwap_ = false; }
            } else if (mode_ == Mode::Gesture && canvas_.size() < 2) {
                mode_ = Mode::None;  // exige levantar o dedo restante antes de desenhar
            }
            return;
        }
        case kCancel:
            endAll(ed);
            return;
        default:
            return;
    }
}

}  // namespace ClipPaint
