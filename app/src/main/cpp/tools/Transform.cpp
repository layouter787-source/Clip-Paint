// Transformacao por homografia (mover, escalar, girar, perspectiva) com reamostragem bilinear,
// conteudo flutuante da selecao e selecao por cor.
#include "../layers/Document.h"

namespace ClipPaint {

namespace {

bool invert3(const float m[9], float o[9]) {
    const float a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    const float det = a * A + b * B + c * C;
    if (std::fabs(det) < 1e-12f) return false;
    const float id = 1.0f / det;
    o[0] = A * id; o[1] = -(b * i - c * h) * id; o[2] = (b * f - c * e) * id;
    o[3] = B * id; o[4] = (a * i - c * g) * id; o[5] = -(a * f - c * d) * id;
    o[6] = C * id; o[7] = -(a * h - b * g) * id; o[8] = (a * e - b * d) * id;
    return true;
}

inline bool mapPoint(const float m[9], float x, float y, float& ox, float& oy) {
    const float w = m[6] * x + m[7] * y + m[8];
    if (w < 1e-6f) return false;
    ox = (m[0] * x + m[1] * y + m[2]) / w;
    oy = (m[3] * x + m[4] * y + m[5]) / w;
    return true;
}

// Caixa de destino da caixa de origem; false se algum canto passa do plano (w <= 0).
bool dstBounds(const float H[9], const RectI& b, int W, int Hh, RectI& out) {
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    const float cx[4] = {(float)b.x0, (float)b.x1, (float)b.x1, (float)b.x0};
    const float cy[4] = {(float)b.y0, (float)b.y0, (float)b.y1, (float)b.y1};
    for (int i = 0; i < 4; ++i) {
        float X, Y;
        if (!mapPoint(H, cx[i], cy[i], X, Y)) return false;
        minX = std::min(minX, X); maxX = std::max(maxX, X);
        minY = std::min(minY, Y); maxY = std::max(maxY, Y);
    }
    out = RectI((int)std::floor(minX) - 1, (int)std::floor(minY) - 1, (int)std::ceil(maxX) + 1, (int)std::ceil(maxY) + 1)
              .intersect(RectI(0, 0, W, Hh));
    return true;
}

}  // namespace

bool Document::layerBounds(const Layer& l, RectI& out) const {
    out = RectI();
    bool any = false;
    for (int ty = 0; ty < ty_; ++ty) {
        for (int tx = 0; tx < tx_; ++tx) {
            if (!l.tiles[(size_t)ty * tx_ + tx]) continue;
            out.unite(RectI(tx * TILE, ty * TILE, std::min(w_, tx * TILE + TILE), std::min(h_, ty * TILE + TILE)));
            any = true;
        }
    }
    return any;
}

bool Document::transformLayerH(int idx, const float H[9]) {
    if (idx < 0 || idx >= layerCount()) return false;
    Layer& l = state.layers[idx];
    if (l.locked) return false;
    RectI b;
    if (!layerBounds(l, b)) return true;
    float inv[9];
    RectI dst;
    if (!invert3(H, inv) || !dstBounds(H, b, w_, h_, dst)) return false;

    std::vector<TilePtr> out((size_t)tx_ * ty_, nullptr);
    auto fetch = [&](int x, int y, float px[4]) {
        px[0] = px[1] = px[2] = px[3] = 0.0f;
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        const Tile* t = l.tiles[(size_t)(y / TILE) * tx_ + (x / TILE)].get();
        if (!t) return;
        const uint8_t* p = t->px + ((y % TILE) * TILE + (x % TILE)) * 4;
        px[0] = p[0]; px[1] = p[1]; px[2] = p[2]; px[3] = p[3];
    };
    for (int y = dst.y0; y < dst.y1; ++y) {
        for (int x = dst.x0; x < dst.x1; ++x) {
            float sx, sy;
            if (!mapPoint(inv, x + 0.5f, y + 0.5f, sx, sy)) continue;
            sx -= 0.5f;
            sy -= 0.5f;
            if (sx < b.x0 - 1 || sx > b.x1 || sy < b.y0 - 1 || sy > b.y1) continue;
            const int x0 = (int)std::floor(sx), y0 = (int)std::floor(sy);
            const float fx = sx - x0, fy = sy - y0;
            float p00[4], p10[4], p01[4], p11[4], r[4];
            fetch(x0, y0, p00); fetch(x0 + 1, y0, p10); fetch(x0, y0 + 1, p01); fetch(x0 + 1, y0 + 1, p11);
            for (int i = 0; i < 4; ++i) {
                const float top = p00[i] + (p10[i] - p00[i]) * fx;
                const float bot = p01[i] + (p11[i] - p01[i]) * fx;
                r[i] = top + (bot - top) * fy;
            }
            if (r[3] < 0.5f) continue;
            TilePtr& tp = out[(size_t)(y / TILE) * tx_ + (x / TILE)];
            if (!tp) {
                tp = std::make_shared<Tile>();
                std::memset(tp->px, 0, sizeof(tp->px));
                tp->version = nextTileVersion();
            }
            uint8_t* o = tp->px + ((y % TILE) * TILE + (x % TILE)) * 4;
            const uint8_t na = (uint8_t)clampi((int)(r[3] + 0.5f), 0, 255);
            o[3] = na;
            o[0] = std::min<uint8_t>(na, (uint8_t)clampi((int)(r[0] + 0.5f), 0, 255));
            o[1] = std::min<uint8_t>(na, (uint8_t)clampi((int)(r[1] + 0.5f), 0, 255));
            o[2] = std::min<uint8_t>(na, (uint8_t)clampi((int)(r[2] + 0.5f), 0, 255));
        }
    }
    l.tiles.swap(out);
    touch();
    return true;
}

bool Document::transformLayer(int idx, const float m[6]) {
    const float H[9] = {m[0], m[2], m[4], m[1], m[3], m[5], 0.0f, 0.0f, 1.0f};
    return transformLayerH(idx, H);
}

// ------------------------------------------------------------ flutuante

bool Document::beginFloating(bool copy) {
    if (floatingId) return false;
    const int a = state.active;
    Layer& src = state.layers[a];
    if (src.locked) return false;
    const Mask* m = selectionMask();

    RectI bb;
    if (m) {
        int minX = w_, maxX = -1, minY = h_, maxY = -1;
        for (int y = 0; y < h_; ++y) {
            const uint8_t* row = &(*m)[(size_t)y * w_];
            for (int x = 0; x < w_; ++x) {
                if (row[x]) { minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y); }
            }
        }
        if (maxX < 0) return false;
        bb = RectI(minX, minY, maxX + 1, maxY + 1);
    } else if (!layerBounds(src, bb)) {
        return false;
    }

    Layer F = makeLayer("Flutuante");
    F.opacity = src.opacity;
    F.blend = src.blend;
    for (int ty = bb.y0 / TILE; ty <= (bb.y1 - 1) / TILE; ++ty) {
        for (int tx = bb.x0 / TILE; tx <= (bb.x1 - 1) / TILE; ++tx) {
            const size_t idx = (size_t)ty * tx_ + tx;
            if (!src.tiles[idx]) continue;
            if (!m) {  // camada inteira: so troca de dono
                F.tiles[idx] = src.tiles[idx];
                if (!copy) src.tiles[idx] = nullptr;
                continue;
            }
            auto nt = std::make_shared<Tile>();
            std::memset(nt->px, 0, sizeof(nt->px));
            nt->version = nextTileVersion();
            Tile* st = nullptr;
            bool any = false;
            for (int y = 0; y < TILE; ++y) {
                const int gy = ty * TILE + y;
                if (gy >= h_) break;
                for (int x = 0; x < TILE; ++x) {
                    const int gx = tx * TILE + x;
                    if (gx >= w_) break;
                    const int mv = (*m)[(size_t)gy * w_ + gx];
                    if (!mv) continue;
                    const uint8_t* sp = src.tiles[idx]->px + (y * TILE + x) * 4;
                    if (!sp[3]) continue;
                    const float k = mv / 255.0f;
                    uint8_t* np = nt->px + (y * TILE + x) * 4;
                    for (int i = 0; i < 4; ++i) np[i] = (uint8_t)(sp[i] * k + 0.5f);
                    any = true;
                    if (!copy) {
                        if (!st) st = writableTile(src, tx, ty);
                        uint8_t* dp = st->px + (y * TILE + x) * 4;
                        for (int i = 0; i < 4; ++i) dp[i] = (uint8_t)(dp[i] * (1.0f - k) + 0.5f);
                    }
                }
            }
            if (any) F.tiles[idx] = nt;
        }
    }
    floatBounds = bb;
    floatingId = F.id;
    state.layers.insert(state.layers.begin() + a + 1, std::move(F));
    state.active = a + 1;
    touch();
    return true;
}

void Document::commitFloating(const float H[9]) {
    int fi = -1;
    for (int i = 0; i < layerCount(); ++i) if (state.layers[i].id == floatingId) fi = i;
    floatingId = 0;
    if (fi <= 0) return;
    const int ti = fi - 1;
    float inv[9];
    RectI dst;
    const bool ok = invert3(H, inv) && dstBounds(H, floatBounds, w_, h_, dst);

    if (ok) {
        transformLayerH(fi, H);
        const Layer& baked = state.layers[fi];
        Layer& T = state.layers[ti];
        for (int ty = 0; ty < ty_; ++ty) {
            for (int tx = 0; tx < tx_; ++tx) {
                const Tile* st = readTile(baked, tx, ty);
                if (!st) continue;
                Tile* dt = writableTile(T, tx, ty);
                for (int k = 0; k < TILE * TILE; ++k) {
                    const uint8_t* s = st->px + k * 4;
                    if (!s[3]) continue;
                    uint8_t* d = dt->px + k * 4;
                    const float inva = 1.0f - s[3] / 255.0f;
                    for (int c = 0; c < 4; ++c) d[c] = (uint8_t)std::min(255.0f, s[c] + d[c] * inva + 0.5f);
                    d[0] = std::min(d[0], d[3]); d[1] = std::min(d[1], d[3]); d[2] = std::min(d[2], d[3]);
                }
            }
        }
        if (state.selection) {  // a selecao acompanha o conteudo
            auto nm = std::make_shared<Mask>((size_t)w_ * h_, 0);
            const Mask& om = *state.selection;
            bool any = false;
            for (int y = dst.y0; y < dst.y1; ++y) {
                for (int x = dst.x0; x < dst.x1; ++x) {
                    float sx, sy;
                    if (!mapPoint(inv, x + 0.5f, y + 0.5f, sx, sy)) continue;
                    sx -= 0.5f; sy -= 0.5f;
                    const int x0 = (int)std::floor(sx), y0 = (int)std::floor(sy);
                    const float fx = sx - x0, fy = sy - y0;
                    auto g = [&](int px, int py) -> float {
                        return (px < 0 || py < 0 || px >= w_ || py >= h_) ? 0.0f : (float)om[(size_t)py * w_ + px];
                    };
                    const float top = g(x0, y0) + (g(x0 + 1, y0) - g(x0, y0)) * fx;
                    const float bot = g(x0, y0 + 1) + (g(x0 + 1, y0 + 1) - g(x0, y0 + 1)) * fx;
                    const int v = (int)(top + (bot - top) * fy + 0.5f);
                    if (v > 0) { (*nm)[(size_t)y * w_ + x] = (uint8_t)std::min(255, v); any = true; }
                }
            }
            state.selection = any ? std::shared_ptr<const Mask>(nm) : nullptr;
        }
    }
    state.layers.erase(state.layers.begin() + fi);
    state.active = ti;
    touch();
}

void Document::selectByColor(int sx, int sy, float tol, bool allLayers, SelOp op) {
    const std::vector<uint8_t> region = floodMask(state.active, sx, sy, tol, allLayers, 0);
    if (region.empty()) return;
    auto next = std::make_shared<Mask>((size_t)w_ * h_, 0);
    const Mask* cur = state.selection.get();
    bool any = false;
    for (size_t i = 0; i < next->size(); ++i) {
        const int s = region[i] ? 255 : 0;
        const int o = cur ? (*cur)[i] : 0;
        int v = s;
        if (op == SelOp::Add) v = std::max(o, s);
        else if (op == SelOp::Subtract) v = o * (255 - s) / 255;
        (*next)[i] = (uint8_t)v;
        if (v) any = true;
    }
    state.selection = any ? std::shared_ptr<const Mask>(next) : nullptr;
    touch();
}

}  // namespace ClipPaint
