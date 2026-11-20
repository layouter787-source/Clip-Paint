#include "BrushPreview.h"

namespace ClipPaint {

bool renderBrushPreview(const BrushEntry& e, int w, int h, std::vector<uint8_t>& out) {
    Document d;
    d.create(w, h);
    d.paperVisible = false;
    if (e.eraser) {  // apagadores: pinta uma faixa e apaga um traco por cima
        d.selectAll();
        d.fillSelection(0, Color(0.55f, 0.62f, 0.78f, 1.0f));
        d.selectNone();
    }
    std::unique_ptr<StrokeEngine> eng = makeStrokeEngine(e);
    if (!eng) return false;

    StrokeSetup s;
    s.entry = &e;
    Color col(0.93f, 0.94f, 0.96f, 1.0f);
    if (e.source == BrushSource::Anim && (e.suggested.r + e.suggested.g + e.suggested.b) > 0.45f) col = e.suggested;
    s.color = col;
    s.diameter = clampf(e.defaultDiameter * 0.55f, 3.0f, (float)h * 0.55f);
    s.opacity = 1.0f;
    s.eraseTool = e.eraser;
    s.seed = 11;

    const float margin = (float)h * 0.45f;
    std::vector<Vec2> path;
    const int n = 24;
    for (int i = 0; i <= n; ++i) {
        const float t = (float)i / n;
        path.emplace_back(margin + (w - 2 * margin) * t, h * 0.5f + std::sin(t * 2.0f * kPi) * h * 0.2f);
    }
    double t = 0;
    eng->begin(d, 0, s, path[0].x, path[0].y, 0.15f, t);
    float total = 0;
    for (size_t i = 1; i < path.size(); ++i) total += (path[i] - path[i - 1]).length();
    float acc = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        const Vec2 a = path[i - 1], b = path[i];
        const float len = (b - a).length();
        const int steps = std::max(1, (int)(len / 3.0f));
        for (int k = 1; k <= steps; ++k) {
            const float u = (float)k / steps;
            const Vec2 p = a + (b - a) * u;
            const float sp = (acc + len * u) / std::max(1.0f, total);
            t += 0.008;
            eng->move(p.x, p.y, 0.25f + 0.7f * std::sin(sp * kPi), t);
        }
        acc += len;
    }
    const int holds = (int)(std::min(0.25f, e.demoHold) * 30.0f);
    for (int i = 0; i < holds; ++i) { t += 1.0 / 30.0; eng->hold(t); }
    eng->end(path.back().x, path.back().y, t + 0.01);

    std::vector<uint8_t> straight;
    d.compositeAll(straight, false);
    out.assign(straight.size(), 0);
    for (size_t i = 0; i < straight.size(); i += 4) {
        const float a = straight[i + 3] / 255.0f;
        out[i] = (uint8_t)(straight[i] * a + 0.5f);
        out[i + 1] = (uint8_t)(straight[i + 1] * a + 0.5f);
        out[i + 2] = (uint8_t)(straight[i + 2] * a + 0.5f);
        out[i + 3] = straight[i + 3];
    }
    return true;
}

}  // namespace ClipPaint
