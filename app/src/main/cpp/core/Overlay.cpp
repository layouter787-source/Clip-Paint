#include "Overlay.h"

namespace ClipPaint {

namespace {
const Color kWhite(1, 1, 1, 1);
const Color kAccent(0.231f, 0.510f, 0.965f, 1.0f);
}  // namespace

void drawOverlays(UiRenderer& g, Editor& e, int W, int H, double now) {
    const float dp = e.dp();
    const ViewState& v = e.view;
    const float dw = (float)e.doc.width(), dh = (float)e.doc.height();
    auto S = [&](float x, float y) { return v.toScreen(Vec2(x, y)); };
    g.begin(W, H);

    if (e.showGrid) {
        const Color gc(0.3f, 0.55f, 1.0f, 0.35f);
        for (float x = 128; x < dw; x += 128) { const Vec2 a = S(x, 0), b = S(x, dh); g.line(a.x, a.y, b.x, b.y, 1.0f, gc); }
        for (float y = 128; y < dh; y += 128) { const Vec2 a = S(0, y), b = S(dw, y); g.line(a.x, a.y, b.x, b.y, 1.0f, gc); }
    }
    if (e.symmetry) {
        const Vec2 a = S(dw * 0.5f, 0), b = S(dw * 0.5f, dh);
        g.line(a.x, a.y, b.x, b.y, std::max(1.5f, dp * 1.2f), Color(0.2f, 0.85f, 1.0f, 0.75f));
    }
    if (e.selDragging) {
        std::vector<Vec2> pts;
        if (e.selMethod == SelMethod::Freehand) {
            pts = e.selPoints;
        } else if (e.selMethod == SelMethod::Rect) {
            pts = {e.selStart, Vec2(e.selCur.x, e.selStart.y), e.selCur, Vec2(e.selStart.x, e.selCur.y)};
        } else {
            const Vec2 c = (e.selStart + e.selCur) * 0.5f;
            const float rx = std::fabs(e.selCur.x - e.selStart.x) * 0.5f, ry = std::fabs(e.selCur.y - e.selStart.y) * 0.5f;
            for (int i = 0; i < 48; ++i) {
                const float a = 2 * kPi * i / 48;
                pts.emplace_back(c.x + std::cos(a) * rx, c.y + std::sin(a) * ry);
            }
        }
        for (size_t i = 0; i < pts.size(); ++i) {
            if (i + 1 >= pts.size() && e.selMethod == SelMethod::Freehand) break;
            const Vec2 a = v.toScreen(pts[i]), b = v.toScreen(pts[(i + 1) % pts.size()]);
            g.line(a.x, a.y, b.x, b.y, dp * 2.6f, Color(0, 0, 0, 0.6f));
            g.line(a.x, a.y, b.x, b.y, dp * 1.3f, kWhite);
        }
    }
    if (e.transforming) {
        Vec2 p[4];
        for (int i = 0; i < 4; ++i) p[i] = v.toScreen(e.quad[i]);
        for (int i = 0; i < 4; ++i) {
            const Vec2 a = p[i], b = p[(i + 1) % 4];
            g.line(a.x, a.y, b.x, b.y, dp * 3.0f, Color(0, 0, 0, 0.45f));
            g.line(a.x, a.y, b.x, b.y, dp * 1.5f, kWhite);
        }
        const Vec2 tm = v.toScreen(e.sideMid(0)), rn = v.toScreen(e.rotateNode());
        g.line(tm.x, tm.y, rn.x, rn.y, dp * 1.5f, kWhite);
        g.circle(rn.x, rn.y, dp * 11, kWhite);
        g.circle(rn.x, rn.y, dp * 9, kAccent);
        for (int i = 0; i < 4; ++i) {
            g.circle(p[i].x, p[i].y, dp * 12, kWhite);
            g.circle(p[i].x, p[i].y, dp * 10, kAccent);
            const Vec2 m = v.toScreen(e.sideMid(i));
            g.circle(m.x, m.y, dp * 8, kWhite);
            g.circle(m.x, m.y, dp * 6, Color(0.45f, 0.68f, 1.0f, 1));
        }
    }
    if (e.pickActive) {
        const Vec2 c(e.pickScreen.x, e.pickScreen.y - dp * 72);
        g.circle(c.x, c.y, dp * 32, kWhite);
        g.circle(c.x, c.y, dp * 29, e.color);
        g.ring(e.pickScreen.x, e.pickScreen.y, dp * 10, dp * 2, kWhite);
    }
    if (now < e.ringUntil) {
        const float d = clampf(e.currentBrush().diameter * v.scale, 2.0f, (float)std::min(W, H) * 0.9f);
        const float cx = (float)W * 0.5f, cy = (float)H * 0.5f;
        g.ring(cx, cy, d * 0.5f + 1.5f, 2.0f, Color(0, 0, 0, 0.6f));
        g.ring(cx, cy, d * 0.5f, 1.5f, kWhite);
    }
    g.end();
}

}  // namespace ClipPaint
