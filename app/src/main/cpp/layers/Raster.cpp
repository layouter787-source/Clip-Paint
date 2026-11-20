#include "Document.h"
#include "BlendMath.h"

namespace ClipPaint {

namespace {
uint32_t g_rng = 0x9E3779B9u;
inline float frand() {  // [0,1)
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return (g_rng & 0xFFFFFF) * (1.0f / 16777216.0f);
}
inline uint8_t quantize(float v) {  // arredondamento estocastico: evita "travar" em alfa baixo
    const float q = v + frand();
    return (uint8_t)(q < 0.0f ? 0.0f : (q > 255.0f ? 255.0f : q));
}
}  // namespace

RectI Document::drawDab(Layer& l, const DabParams& d, const Mask* sel) {
    if (l.locked || d.opaque <= 0.0f) return RectI();
    const float R = std::max(d.radius, 0.5f);
    const float aspect = std::max(d.aspect, 1.0f);
    const float h = clampf(d.hardness, 0.02f, 0.995f);
    const float seg1Slope = -(1.0f / h - 1.0f);
    const float seg2Off = h / (1.0f - h);
    const float seg2Slope = -h / (1.0f - h);
    const float invR2 = 1.0f / (R * R);
    const float ang = d.angleDeg * kPi / 180.0f;
    const float cs = std::cos(ang), sn = std::sin(ang);

    RectI box((int)std::floor(d.x - R - 1.0f), (int)std::floor(d.y - R - 1.0f),
              (int)std::ceil(d.x + R + 1.0f), (int)std::ceil(d.y + R + 1.0f));
    box = box.intersect(RectI(0, 0, w_, h_));
    if (box.empty()) return RectI();

    const float colorA = clampf(d.colorA, 0.0f, 1.0f);
    const bool erasing = colorA < 0.999f;
    const float lockK = clampf(d.lockAlpha, 0.0f, 1.0f);
    const bool lockAll = l.alphaLock || lockK > 0.999f;
    const bool needsData = !(erasing && colorA <= 0.001f) && !lockAll;
    const float cr = colorA <= 0.001f ? 0.0f : d.r * 255.0f;
    const float cg = colorA <= 0.001f ? 0.0f : d.g * 255.0f;
    const float cb = colorA <= 0.001f ? 0.0f : d.b * 255.0f;
    RectI touched;

    for (int ty = box.y0 / TILE; ty <= (box.y1 - 1) / TILE; ++ty) {
        for (int tx = box.x0 / TILE; tx <= (box.x1 - 1) / TILE; ++tx) {
            if (!l.tiles[(size_t)ty * tx_ + tx] && !needsData) continue;
            const RectI tr(tx * TILE, ty * TILE, tx * TILE + TILE, ty * TILE + TILE);
            const RectI r = box.intersect(tr);
            if (r.empty()) continue;
            Tile* tile = nullptr;
            for (int y = r.y0; y < r.y1; ++y) {
                const float dy = y + 0.5f - d.y;
                for (int x = r.x0; x < r.x1; ++x) {
                    const float dx = x + 0.5f - d.x;
                    const float xx = dx * cs + dy * sn;
                    const float yy = (dy * cs - dx * sn) * aspect;
                    const float rr = (xx * xx + yy * yy) * invR2;
                    if (rr >= 1.0f) continue;
                    const float opa = rr <= h ? 1.0f + rr * seg1Slope : seg2Off + rr * seg2Slope;
                    const float edge = clampf((1.0f - std::sqrt(rr)) * R + 0.5f, 0.0f, 1.0f);
                    float a = opa * edge * d.opaque;
                    if (sel) a *= (*sel)[(size_t)y * w_ + x] * (1.0f / 255.0f);
                    if (a <= 0.003f) continue;
                    if (!tile) tile = writableTile(l, tx, ty);
                    uint8_t* p = tile->px + ((y - tr.y0) * TILE + (x - tr.x0)) * 4;
                    float dr = p[0], dg = p[1], db = p[2], da = p[3];
                    if (lockAll || lockK > 0.0f) {
                        const float k = lockAll ? a : a * lockK;
                        const float lock = da * (1.0f / 255.0f);
                        dr = dr * (1.0f - k) + cr * lock * k;
                        dg = dg * (1.0f - k) + cg * lock * k;
                        db = db * (1.0f - k) + cb * lock * k;
                        if (lockAll) {
                            const uint8_t na = (uint8_t)da;
                            p[0] = std::min(quantize(dr), na); p[1] = std::min(quantize(dg), na);
                            p[2] = std::min(quantize(db), na);
                            continue;
                        }
                        a *= 1.0f - lockK;
                        if (a <= 0.003f) {
                            const uint8_t na = (uint8_t)da;
                            p[0] = std::min(quantize(dr), na); p[1] = std::min(quantize(dg), na);
                            p[2] = std::min(quantize(db), na);
                            continue;
                        }
                    }
                    const float k = 1.0f - a;
                    da = a * colorA * 255.0f + k * da;
                    dr = a * cr + k * dr;
                    dg = a * cg + k * dg;
                    db = a * cb + k * db;
                    const uint8_t na = quantize(da);
                    p[3] = na;
                    p[0] = std::min(quantize(dr), na);
                    p[1] = std::min(quantize(dg), na);
                    p[2] = std::min(quantize(db), na);
                }
            }
            if (tile) touched.unite(r);
        }
    }
    return touched;
}

void Document::getPixel(const Layer& l, int x, int y, uint8_t out[4]) const {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    const Tile* t = l.tiles[(size_t)(y / TILE) * tx_ + (x / TILE)].get();
    if (!t) return;
    const uint8_t* p = t->px + ((y % TILE) * TILE + (x % TILE)) * 4;
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; out[3] = p[3];
}

void Document::sampleLayer(const Layer& l, float cx, float cy, float radius, float out[4]) const {
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    const int n = 5;
    const float step = 2.0f * radius / n;
    int count = 0;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const float px = cx - radius + (i + 0.5f) * step;
            const float py = cy - radius + (j + 0.5f) * step;
            const float ddx = px - cx, ddy = py - cy;
            if (ddx * ddx + ddy * ddy > radius * radius) continue;
            uint8_t c[4];
            getPixel(l, (int)std::floor(px), (int)std::floor(py), c);
            for (int k = 0; k < 4; ++k) out[k] += c[k] * (1.0f / 255.0f);
            ++count;
        }
    }
    if (count > 0) for (int k = 0; k < 4; ++k) out[k] /= (float)count;
}

void Document::compositeAll(std::vector<uint8_t>& out, bool withPaper) const {
    out.assign((size_t)w_ * h_ * 4, 0);
    const float pr = paper.r * paper.a, pg = paper.g * paper.a, pb = paper.b * paper.a, pa = paper.a;
    std::vector<float> acc((size_t)TILE * TILE * 4);
    for (int ty = 0; ty < ty_; ++ty) {
        for (int tx = 0; tx < tx_; ++tx) {
            for (int k = 0; k < TILE * TILE; ++k) {
                acc[k * 4 + 0] = withPaper ? pr : 0.0f;
                acc[k * 4 + 1] = withPaper ? pg : 0.0f;
                acc[k * 4 + 2] = withPaper ? pb : 0.0f;
                acc[k * 4 + 3] = withPaper ? pa : 0.0f;
            }
            for (const Layer& l : state.layers) {
                if (!l.visible || l.opacity <= 0.0f) continue;
                const Tile* t = readTile(l, tx, ty);
                if (!t) continue;
                for (int k = 0; k < TILE * TILE; ++k) {
                    const uint8_t* p = t->px + k * 4;
                    if (p[3] == 0) continue;
                    float cs[4] = {p[0] * l.opacity / 255.0f, p[1] * l.opacity / 255.0f,
                                   p[2] * l.opacity / 255.0f, p[3] * l.opacity / 255.0f};
                    blendPremult(&acc[k * 4], cs, l.blend);
                }
            }
            const int x0 = tx * TILE, y0 = ty * TILE;
            for (int y = 0; y < TILE && y0 + y < h_; ++y) {
                for (int x = 0; x < TILE && x0 + x < w_; ++x) {
                    const float* a = &acc[(y * TILE + x) * 4];
                    uint8_t* o = &out[((size_t)(y0 + y) * w_ + (x0 + x)) * 4];
                    if (a[3] <= 0.0f) continue;
                    const float inv = 1.0f / a[3];
                    o[0] = (uint8_t)clampi((int)(a[0] * inv * 255.0f + 0.5f), 0, 255);
                    o[1] = (uint8_t)clampi((int)(a[1] * inv * 255.0f + 0.5f), 0, 255);
                    o[2] = (uint8_t)clampi((int)(a[2] * inv * 255.0f + 0.5f), 0, 255);
                    o[3] = (uint8_t)clampi((int)(a[3] * 255.0f + 0.5f), 0, 255);
                }
            }
        }
    }
}

Color Document::pickColor(int x, int y, bool allLayers) const {
    x = clampi(x, 0, w_ - 1);
    y = clampi(y, 0, h_ - 1);
    float acc[4] = {0, 0, 0, 0};
    if (allLayers) {
        if (paperVisible) {
            acc[0] = paper.r * paper.a; acc[1] = paper.g * paper.a;
            acc[2] = paper.b * paper.a; acc[3] = paper.a;
        }
        for (const Layer& l : state.layers) {
            if (!l.visible) continue;
            uint8_t c[4];
            getPixel(l, x, y, c);
            float cs[4] = {c[0] * l.opacity / 255.0f, c[1] * l.opacity / 255.0f,
                           c[2] * l.opacity / 255.0f, c[3] * l.opacity / 255.0f};
            blendPremult(acc, cs, l.blend);
        }
    } else {
        uint8_t c[4];
        getPixel(activeLayer(), x, y, c);
        for (int i = 0; i < 4; ++i) acc[i] = c[i] / 255.0f;
    }
    if (acc[3] <= 0.0f) return Color(1, 1, 1, 0);
    return Color(clampf(acc[0] / acc[3], 0, 1), clampf(acc[1] / acc[3], 0, 1),
                 clampf(acc[2] / acc[3], 0, 1), acc[3]);
}

}  // namespace ClipPaint
