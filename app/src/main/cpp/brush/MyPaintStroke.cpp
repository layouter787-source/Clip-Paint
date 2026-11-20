// Adaptador libmypaint -> Document: implementa MyPaintSurface sobre as camadas em tiles.
#include "StrokeEngine.h"

extern "C" {
#include "../third_party/libmypaint/mypaint-brush.h"
#include "../third_party/libmypaint/mypaint-surface.h"
}

namespace ClipPaint {

namespace {

struct MPSurface {
    MyPaintSurface base;  // deve ser o primeiro membro
    Document* doc = nullptr;
    int layer = 0;
    const Mask* sel = nullptr;
    bool mirror = false;
};

int mpDrawDab(MyPaintSurface* s, float x, float y, float radius, float r, float g, float b, float opaque,
              float hardness, float alphaEraser, float aspect, float angle, float lockAlpha, float colorize) {
    (void)colorize;
    MPSurface* S = reinterpret_cast<MPSurface*>(s);
    if (S->layer < 0 || S->layer >= S->doc->layerCount()) return 0;
    DabParams d;
    d.x = x; d.y = y; d.radius = radius; d.hardness = hardness; d.aspect = aspect;
    d.angleDeg = S->mirror ? -angle : angle;
    d.opaque = opaque;
    d.r = r; d.g = g; d.b = b;
    d.colorA = alphaEraser;
    d.lockAlpha = lockAlpha;
    return S->doc->drawDab(S->doc->state.layers[S->layer], d, S->sel).empty() ? 0 : 1;
}

void mpGetColor(MyPaintSurface* s, float x, float y, float radius, float* r, float* g, float* b, float* a) {
    MPSurface* S = reinterpret_cast<MPSurface*>(s);
    float p[4] = {0, 0, 0, 0};
    if (S->layer >= 0 && S->layer < S->doc->layerCount())
        S->doc->sampleLayer(S->doc->state.layers[S->layer], x, y, std::max(1.0f, radius), p);
    if (p[3] > 1e-5f) {
        *r = clampf(p[0] / p[3], 0, 1); *g = clampf(p[1] / p[3], 0, 1); *b = clampf(p[2] / p[3], 0, 1);
    } else {
        *r = *g = *b = 0.0f;
    }
    *a = clampf(p[3], 0, 1);
}

void mpBegin(MyPaintSurface*) {}
void mpEnd(MyPaintSurface*, MyPaintRectangle*) {}
void mpDestroy(MyPaintSurface*) {}

class MyPaintStroke : public StrokeEngine {
public:
    ~MyPaintStroke() override { release(); }

    void begin(Document& doc, int layer, const StrokeSetup& s, float x, float y, float pressure, double t) override {
        release();
        brush_ = mypaint_brush_new();
        std::string json;
        if (!s.entry || !BrushLibrary::get().readMyb(*s.entry, json) || !mypaint_brush_from_string(brush_, json.c_str()))
            mypaint_brush_from_defaults(brush_);

        mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                                     std::log(std::max(0.25f, s.diameter * 0.5f)));
        mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_OPAQUE,
                                     clampf(mypaint_brush_get_base_value(brush_, MYPAINT_BRUSH_SETTING_OPAQUE) * s.opacity, 0.0f, 1.0f));
        if (s.hardness >= 0.0f) mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_HARDNESS, s.hardness);
        if (s.smoothing > 0.0f) {
            mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_SLOW_TRACKING,
                                         mypaint_brush_get_base_value(brush_, MYPAINT_BRUSH_SETTING_SLOW_TRACKING) + s.smoothing * 8.0f);
        }
        float h, sat, v;
        rgbToHsv(s.color.r, s.color.g, s.color.b, h, sat, v);
        mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_COLOR_H, h);
        mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_COLOR_S, sat);
        mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_COLOR_V, v);
        if (s.eraseTool) mypaint_brush_set_base_value(brush_, MYPAINT_BRUSH_SETTING_ERASER, 1.0f);

        mypaint_surface_init(&surf_.base);
        surf_.base.draw_dab = mpDrawDab;
        surf_.base.get_color = mpGetColor;
        surf_.base.begin_atomic = mpBegin;
        surf_.base.end_atomic = mpEnd;
        surf_.base.destroy = mpDestroy;
        surf_.doc = &doc;
        surf_.layer = layer;
        surf_.sel = doc.selectionMask();
        surf_.mirror = s.mirror;
        mirror_ = s.mirror;
        axis_ = s.axis;
        if (mirror_) x = 2.0f * axis_ - x;

        mypaint_brush_reset(brush_);
        mypaint_brush_new_stroke(brush_);
        mypaint_brush_stroke_to(brush_, &surf_.base, x, y, 0.0f, 0.0f, 0.0f, 10.0);  // posiciona
        mypaint_brush_stroke_to(brush_, &surf_.base, x, y, pressure, 0.0f, 0.0f, 0.005);
        lastX_ = x; lastY_ = y; lastP_ = pressure; lastT_ = t;
    }

    void move(float x, float y, float pressure, double t) override {
        if (!brush_) return;
        if (mirror_) x = 2.0f * axis_ - x;
        const double dt = std::max(0.0005, t - lastT_);
        mypaint_brush_stroke_to(brush_, &surf_.base, x, y, pressure, 0.0f, 0.0f, dt);
        lastX_ = x; lastY_ = y; lastP_ = pressure; lastT_ = t;
    }

    void hold(double t) override {
        if (!brush_) return;
        const double dt = std::max(0.0005, t - lastT_);
        mypaint_brush_stroke_to(brush_, &surf_.base, lastX_, lastY_, lastP_, 0.0f, 0.0f, dt);
        lastT_ = t;
    }

    void end(float x, float y, double t) override {
        if (!brush_) return;
        if (mirror_) x = 2.0f * axis_ - x;
        const double dt = std::max(0.0005, t - lastT_);
        mypaint_brush_stroke_to(brush_, &surf_.base, x, y, lastP_, 0.0f, 0.0f, dt);
        mypaint_brush_stroke_to(brush_, &surf_.base, x, y, 0.0f, 0.0f, 0.0f, 0.01);
        release();
    }

    bool active() const override { return brush_ != nullptr; }

private:
    void release() {
        if (brush_) { mypaint_brush_unref(brush_); brush_ = nullptr; }
    }
    MyPaintBrush* brush_ = nullptr;
    MPSurface surf_;
    bool mirror_ = false;
    float axis_ = 0, lastX_ = 0, lastY_ = 0, lastP_ = 1;
    double lastT_ = 0;
};

}  // namespace

std::unique_ptr<StrokeEngine> makeMyPaintStroke() { return std::make_unique<MyPaintStroke>(); }

}  // namespace ClipPaint
