// Clip Paint - entrada: caneta/dedo, gestos de 2 dedos (zoom, giro, deslocar),
// toque de 2 dedos = desfazer, 3 dedos = refazer. Recebe eventos ja convertidos pelo Kotlin.
#pragma once

#include "Editor.h"

namespace ClipPaint {

struct TouchHist { float x, y, pressure; double time; };
struct TouchPointer {
    int id = 0;
    int toolType = 1;  // 1 dedo, 2 caneta, 3 mouse, 4 borracha da caneta (valores do MotionEvent)
    float x = 0, y = 0, pressure = 1;
    std::vector<TouchHist> history;
};
struct TouchEvent {
    int action = 0;        // MotionEvent.ACTION_* (ja mascarado)
    int actionIndex = 0;
    double time = 0;       // segundos (uptime)
    std::vector<TouchPointer> pointers;
};

class InputManager {
public:
    void handle(const TouchEvent& ev, Editor& ed);
    bool busy() const { return mode_ != Mode::None; }

private:
    struct Ptr { int id; Vec2 pos; };
    enum class Mode { None, Draw, Pan, Gesture };

    int findCanvas(int id) const;
    void startGesture(Editor& ed, double t);
    void updateGesture(Editor& ed);
    void endAll(Editor& ed);

    Mode mode_ = Mode::None;
    std::vector<Ptr> canvas_;
    bool eraserSwap_ = false;
    ToolId prevTool_ = ToolId::Brush;
    double downTime_ = 0;
    Vec2 lastPan_;

    float g_d0_ = 1, g_a0_ = 0, g_prevD_ = 1, g_prevA_ = 0;
    Vec2 g_mid0_, g_prevMid_;
    ViewState g_view0_;
    Vec2 g_doc0_;
    double g_start_ = 0;
    int g_maxCount_ = 2;
    bool g_moved_ = false, g_fired_ = false;
    Vec2 g_p0_[2];
};

}  // namespace ClipPaint
