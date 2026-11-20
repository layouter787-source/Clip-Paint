// Adaptador AnimBrush -> Document: ISurface sobre camadas em tiles (RGBA8 pre-multiplicado).
// A logica de cobertura/mistura de drawDab segue a RasterSurface de referencia da AnimBrush.
#include "StrokeEngine.h"

#include "../third_party/animbrush/include/animbrush/animbrush.hpp"

namespace ClipPaint {

namespace {

uint32_t g_seed = 0x1234567u;
inline float frand() {
    g_seed ^= g_seed << 13; g_seed ^= g_seed >> 17; g_seed ^= g_seed << 5;
    return (g_seed & 0xFFFFFF) * (1.0f / 16777216.0f);
}
inline uint8_t q8(float v) {
    const float q = v * 255.0f + frand();
    return (uint8_t)(q < 0.0f ? 0.0f : (q > 255.0f ? 255.0f : q));
}

class LayerSurface : public ab::ISurface {
public:
    LayerSurface() = default;
    void bind(Document* d, int layer, bool mirror) {
        doc_ = d; layer_ = layer; mirror_ = mirror; sel_ = d->selectionMask();
    }
    int width() const override { return doc_->width(); }
    int height() const override { return doc_->height(); }

    void load(int x, int y, float out[4]) const {
        uint8_t c[4];
        doc_->getPixel(doc_->state.layers[layer_], x, y, c);
        for (int i = 0; i < 4; ++i) out[i] = c[i] * (1.0f / 255.0f);
    }
    void store(Tile* t, int x, int y, const float v[4]) {
        uint8_t* p = t->px + ((y % TILE) * TILE + (x % TILE)) * 4;
        const uint8_t a = q8(v[3]);
        p[3] = a;
        p[0] = std::min(q8(v[0]), a); p[1] = std::min(q8(v[1]), a); p[2] = std::min(q8(v[2]), a);
    }

    void drawDab(const ab::Dab& Din) override {
        Layer& L = doc_->state.layers[layer_];
        if (L.locked || Din.radius <= 0.f || Din.opacity <= 0.f) return;
        ab::Dab D = Din;
        if (mirror_) { D.angle = -D.angle; D.mirrorX = !D.mirrorX; }
        if (L.alphaLock) D.lockAlpha = true;
        const int W = doc_->width(), H = doc_->height();
        const float b = std::max(0.05f, D.radius);
        const float a = b * std::max(0.05f, D.aspect);
        const float ca = std::cos(D.angle), sa = std::sin(D.angle);
        const float ex = std::fabs(a * ca) + std::fabs(b * sa) + 1.5f;
        const float ey = std::fabs(a * sa) + std::fabs(b * ca) + 1.5f;
        const int x0 = std::max(0, (int)std::floor(D.x - ex)), x1 = std::min(W, (int)std::ceil(D.x + ex));
        const int y0 = std::max(0, (int)std::floor(D.y - ey)), y1 = std::min(H, (int)std::ceil(D.y + ey));
        if (x1 <= x0 || y1 <= y0) return;

        const bool eraseLike = D.blend == ab::Blend::Erase || D.blend == ab::Blend::Dissolve ||
                               D.blend == ab::Blend::EraseSimilar || D.blend == ab::Blend::EraseDark ||
                               D.blend == ab::Blend::EraseLight || D.blend == ab::Blend::EraseEdge ||
                               D.blend == ab::Blend::Soften;
        ab::Patch tmp;
        if (D.blend == ab::Blend::Soften) tmp = readPatch({x0 - 2, y0 - 2, x1 + 2, y1 + 2});

        const float rmin = std::min(a, b);
        const float hard = ab::clampf(D.hardness, 0.f, 0.999f);
        const float invHard = 1.f / (1.f - hard);
        const bool useGrain = D.grain && D.grainDepth > 0.f && !D.grain->empty();
        const float gs = D.grainScale > 0 ? D.grainScale : 1.f;
        const bool procedural = D.shape == nullptr || D.shape->empty();

        for (int ty = y0 / TILE; ty <= (y1 - 1) / TILE; ++ty) {
            for (int tx = x0 / TILE; tx <= (x1 - 1) / TILE; ++tx) {
                if (eraseLike && !L.tiles[(size_t)ty * doc_->tilesX() + tx]) continue;
                Tile* tile = nullptr;
                const int ax0 = std::max(x0, tx * TILE), ax1 = std::min(x1, tx * TILE + TILE);
                const int ay0 = std::max(y0, ty * TILE), ay1 = std::min(y1, ty * TILE + TILE);
                for (int py = ay0; py < ay1; ++py) {
                    for (int px = ax0; px < ax1; ++px) {
                        const float dx = px + 0.5f - D.x, dy = py + 0.5f - D.y;
                        const float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
                        const float nu = u / a, nv = v / b;
                        float cov;
                        if (procedural) {
                            const float d2 = nu * nu + nv * nv;
                            if (d2 >= 1.f) continue;
                            const float d = std::sqrt(d2);
                            const float fall = d <= hard ? 1.f : 1.f - ab::smoothstep(0.f, 1.f, (d - hard) * invHard);
                            cov = fall * ab::clampf((1.f - d) * rmin + 0.5f);
                        } else {
                            if (std::fabs(nu) >= 1.f || std::fabs(nv) >= 1.f) continue;
                            const float su = D.mirrorX ? -nu : nu;
                            cov = D.shape->sample(su * 0.5f + 0.5f, nv * 0.5f + 0.5f);
                            if (D.invertShape) cov = 1.f - cov;
                        }
                        if (cov <= 0.002f) continue;
                        float al = cov * D.opacity;
                        if (sel_) al *= (*sel_)[(size_t)py * W + px] * (1.0f / 255.0f);
                        if (al <= 0.0005f) continue;
                        if (useGrain) {
                            float g = D.grain->sampleTiled(px * gs / D.grain->w, py * gs / D.grain->h);
                            g = ab::clampf(g + D.grainBias);
                            al *= ab::lerpf(1.f, ab::smoothstep(0.32f, 0.68f, g), D.grainDepth);
                        }
                        if (D.dissolve > 0.f && D.blend != ab::Blend::Dissolve) {
                            if (ab::hashf(ab::hash2(px, py, D.seed)) < D.dissolve) continue;
                        }
                        float dst[4];
                        load(px, py, dst);
                        switch (D.blend) {
                            case ab::Blend::Smudge: {
                                const float k = 1.f - al, ca2 = D.colorAlpha;
                                dst[0] = dst[0] * k + D.color.r * ca2 * al; dst[1] = dst[1] * k + D.color.g * ca2 * al;
                                dst[2] = dst[2] * k + D.color.b * ca2 * al; dst[3] = dst[3] * k + ca2 * al;
                                break;
                            }
                            case ab::Blend::Dissolve: {
                                const float h = ab::hashf(ab::hash2(px, py, D.seed + 17));
                                if (h < al * (D.dissolve > 0 ? D.dissolve : 1.f)) dst[0] = dst[1] = dst[2] = dst[3] = 0.f;
                                break;
                            }
                            case ab::Blend::EraseSimilar: {
                                const float pa = dst[3];
                                if (pa <= 1e-4f) break;
                                const float dr = dst[0] / pa - D.refColor.r, dg = dst[1] / pa - D.refColor.g,
                                            db = dst[2] / pa - D.refColor.b;
                                const float dist = std::sqrt((dr * dr + dg * dg + db * db) * (1.f / 3.f)) * 1.7320508f * 0.57735f;
                                const float w = 1.f - ab::smoothstep(D.tolerance, D.tolerance * 1.6f + 0.02f, dist);
                                ab::blendPixel(dst, D.color, al * w, ab::Blend::Erase, false);
                                break;
                            }
                            case ab::Blend::EraseDark:
                            case ab::Blend::EraseLight: {
                                const float pa = dst[3];
                                if (pa <= 1e-4f) break;
                                const float lum = 0.2126f * dst[0] / pa + 0.7152f * dst[1] / pa + 0.0722f * dst[2] / pa;
                                const float w = D.blend == ab::Blend::EraseDark
                                                    ? 1.f - ab::smoothstep(D.tolerance, D.tolerance + 0.15f, lum)
                                                    : ab::smoothstep(1.f - D.tolerance - 0.15f, 1.f - D.tolerance, lum);
                                ab::blendPixel(dst, D.color, al * w, ab::Blend::Erase, false);
                                break;
                            }
                            case ab::Blend::EraseEdge: {
                                const float pa = dst[3];
                                if (pa <= 1e-4f) break;
                                const float w = 1.f - ab::smoothstep(D.edgeLimit * 0.6f, D.edgeLimit, pa);
                                ab::blendPixel(dst, D.color, al * w, ab::Blend::Erase, false);
                                break;
                            }
                            case ab::Blend::Soften: {
                                float acc[4] = {0, 0, 0, 0};
                                int n = 0;
                                for (int oy = -2; oy <= 2; ++oy)
                                    for (int ox = -2; ox <= 2; ++ox) {
                                        const int sx = px + ox, sy = py + oy;
                                        if (!tmp.contains(sx, sy)) continue;
                                        const float* qv = tmp.at(sx, sy);
                                        for (int c = 0; c < 4; ++c) acc[c] += qv[c];
                                        ++n;
                                    }
                                if (n) for (int c = 0; c < 4; ++c) dst[c] = ab::lerpf(dst[c], acc[c] / n, al);
                                break;
                            }
                            default:
                                ab::blendPixel(dst, D.color, al, D.blend, D.lockAlpha);
                        }
                        if (!tile) tile = doc_->writableTile(L, tx, ty);
                        store(tile, px, py, dst);
                    }
                }
            }
        }
    }

    ab::Color getColor(float x, float y, float radius) const override {
        radius = std::max(0.5f, radius);
        const int n = (int)ab::clampf(std::ceil(radius * 2.f), 1.f, 16.f);
        double acc[4] = {0, 0, 0, 0};
        double wsum = 0;
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i) {
                const float fx = (i + 0.5f) / n * 2.f - 1.f, fy = (j + 0.5f) / n * 2.f - 1.f;
                const float r2 = fx * fx + fy * fy;
                if (r2 > 1.f) continue;
                const int sx = (int)std::floor(x + fx * radius), sy = (int)std::floor(y + fy * radius);
                const float w = 1.f - r2 * 0.6f;
                wsum += w;
                if (sx < 0 || sy < 0 || sx >= doc_->width() || sy >= doc_->height()) continue;
                float p[4];
                load(sx, sy, p);
                for (int c = 0; c < 4; ++c) acc[c] += p[c] * w;
            }
        if (wsum <= 0) return {0, 0, 0, 0};
        const float a = (float)(acc[3] / wsum);
        if (a <= 1e-5f) return {0, 0, 0, 0};
        return {(float)(acc[0] / wsum) / a, (float)(acc[1] / wsum) / a, (float)(acc[2] / wsum) / a, a};
    }

    ab::Patch readPatch(ab::IRect r) const override {
        r.x0 = std::max(0, r.x0); r.y0 = std::max(0, r.y0);
        r.x1 = std::min(doc_->width(), r.x1); r.y1 = std::min(doc_->height(), r.y1);
        if (r.empty()) return ab::Patch();
        ab::Patch p(r.x0, r.y0, r.w(), r.h());
        for (int y = r.y0; y < r.y1; ++y)
            for (int x = r.x0; x < r.x1; ++x) load(x, y, p.at(x, y));
        return p;
    }

    void compositePatch(const ab::Patch& p, ab::Blend mode, float opacity) override {
        Layer& L = doc_->state.layers[layer_];
        if (L.locked) return;
        for (int y = std::max(0, p.y0); y < std::min(doc_->height(), p.y0 + p.h); ++y) {
            for (int x = std::max(0, p.x0); x < std::min(doc_->width(), p.x0 + p.w); ++x) {
                const float* s = p.at(x, y);
                float sel = 1.0f;
                if (sel_) sel = (*sel_)[(size_t)y * doc_->width() + x] * (1.0f / 255.0f);
                if (sel <= 0.0f) continue;
                const float op = opacity * sel;
                float d[4];
                load(x, y, d);
                if (mode == ab::Blend::Smudge) {
                    for (int c = 0; c < 4; ++c) d[c] = ab::lerpf(d[c], s[c], op);
                } else {
                    const float sa = s[3] * op;
                    if (sa <= 1e-6f && mode != ab::Blend::Add) continue;
                    if (mode == ab::Blend::Normal) {
                        const float k = 1.f - sa;
                        for (int c = 0; c < 3; ++c) d[c] = d[c] * k + s[c] * op;
                        d[3] = d[3] * k + sa;
                    } else if (mode == ab::Blend::Add) {
                        for (int c = 0; c < 3; ++c) d[c] = std::min(1.f, d[c] + s[c] * op);
                        d[3] = d[3] + sa * (1.f - d[3]);
                    } else {
                        if (s[3] <= 1e-6f) continue;
                        ab::Color cs{s[0] / s[3], s[1] / s[3], s[2] / s[3], 1.f};
                        ab::blendPixel(d, cs, sa, mode, L.alphaLock);
                    }
                }
                Tile* t = doc_->writableTile(L, x / TILE, y / TILE);
                store(t, x, y, d);
            }
        }
    }

private:
    Document* doc_ = nullptr;
    int layer_ = 0;
    bool mirror_ = false;
    const Mask* sel_ = nullptr;
};

class AnimStroke : public StrokeEngine {
public:
    void begin(Document& doc, int layer, const StrokeSetup& s, float x, float y, float pressure, double t) override {
        brush_ = ab::BrushLibrary::instance().create(s.entry ? s.entry->id : "");
        if (!brush_) return;
        if (s.params) for (const auto& kv : *s.params) brush_->setParam(kv.first, kv.second);
        surf_.bind(&doc, layer, s.mirror);
        mirror_ = s.mirror;
        axis_ = s.axis;
        ab::StrokeParams sp;
        sp.color = ab::Color(s.color.r, s.color.g, s.color.b, 1.0f);
        sp.color2 = ab::Color(1 - s.color.r, 1 - s.color.g, 1 - s.color.b, 1.0f);
        sp.size = std::max(0.5f, s.diameter * 0.5f);
        sp.opacity = clampf(s.opacity, 0.0f, 1.0f);
        sp.seed = s.seed;
        sp.lockAlpha = doc.state.layers[layer].alphaLock;
        t0_ = t;
        lastT_ = t;
        lastX_ = mirror_ ? 2.0f * axis_ - x : x;
        lastY_ = y;
        lastP_ = pressure;
        brush_->begin(surf_, sp, sample(lastX_, lastY_, pressure, 1.0f / 120.0f));
        active_ = true;
    }

    void move(float x, float y, float pressure, double t) override {
        if (!brush_ || !active_) return;
        if (mirror_) x = 2.0f * axis_ - x;
        const float dt = (float)std::max(0.0005, t - lastT_);
        brush_->move(surf_, sample(x, y, pressure, dt));
        lastX_ = x; lastY_ = y; lastP_ = pressure; lastT_ = t;
    }

    void hold(double t) override {
        if (!brush_ || !active_) return;
        const float dt = (float)std::max(0.0005, t - lastT_);
        brush_->hold(surf_, dt);
        lastT_ = t;
    }

    void end(float x, float y, double t) override {
        if (!brush_ || !active_) return;
        move(x, y, lastP_, t);
        brush_->end(surf_);
        active_ = false;
        brush_.reset();
    }

    bool active() const override { return active_; }

private:
    ab::InputSample sample(float x, float y, float p, float dt) const {
        ab::InputSample s;
        s.x = x; s.y = y; s.pressure = p; s.dt = dt;
        return s;
    }
    std::unique_ptr<ab::Brush> brush_;
    LayerSurface surf_;
    bool active_ = false, mirror_ = false;
    float axis_ = 0, lastX_ = 0, lastY_ = 0, lastP_ = 1;
    double t0_ = 0, lastT_ = 0;
};

}  // namespace

std::unique_ptr<StrokeEngine> makeMyPaintStroke();

std::unique_ptr<StrokeEngine> makeStrokeEngine(const BrushEntry& e) {
    if (e.source == BrushSource::Anim) return std::make_unique<AnimStroke>();
    return makeMyPaintStroke();
}

}  // namespace ClipPaint
