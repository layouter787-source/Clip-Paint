// Balde de tinta (flood fill por scanline) e operacoes de pixel sobre a selecao.
#include "../layers/Document.h"

#include <cstdlib>
#include <utility>

namespace ClipPaint {

namespace {

// Mistura uma cor solida (pre-multiplicada, 0..255) com cobertura `cover` (0..1).
inline void paintPixel(uint8_t* p, float sr, float sg, float sb, float sa, float cover,
                       bool alphaLock) {
    const float da = p[3];
    const float k = 1.0f - sa * cover;
    if (alphaLock) {
        const float lock = da / 255.0f;
        p[0] = (uint8_t)std::min(255.0f, p[0] * k + sr * cover * lock + 0.5f);
        p[1] = (uint8_t)std::min(255.0f, p[1] * k + sg * cover * lock + 0.5f);
        p[2] = (uint8_t)std::min(255.0f, p[2] * k + sb * cover * lock + 0.5f);
        return;
    }
    const uint8_t na = (uint8_t)std::min(255.0f, da * k + 255.0f * sa * cover + 0.5f);
    p[3] = na;
    p[0] = std::min<uint8_t>(na, (uint8_t)std::min(255.0f, p[0] * k + sr * cover + 0.5f));
    p[1] = std::min<uint8_t>(na, (uint8_t)std::min(255.0f, p[1] * k + sg * cover + 0.5f));
    p[2] = std::min<uint8_t>(na, (uint8_t)std::min(255.0f, p[2] * k + sb * cover + 0.5f));
}

}  // namespace

std::vector<uint8_t> Document::floodMask(int li, int sx, int sy, float tol, bool all, int grow) const {
    if (li < 0 || li >= layerCount() || sx < 0 || sy < 0 || sx >= w_ || sy >= h_) return {};
    const Layer& l = state.layers[li];

    // Imagem de referencia (alfa reto).
    std::vector<uint8_t> ref;
    if (all) {
        compositeAll(ref, paperVisible);
    } else {
        ref.assign((size_t)w_ * h_ * 4, 0);
        for (int ty = 0; ty < ty_; ++ty) {
            for (int tx = 0; tx < tx_; ++tx) {
                const Tile* t = readTile(l, tx, ty);
                if (!t) continue;
                for (int y = 0; y < TILE && ty * TILE + y < h_; ++y) {
                    for (int x = 0; x < TILE && tx * TILE + x < w_; ++x) {
                        const uint8_t* p = t->px + (y * TILE + x) * 4;
                        uint8_t* o = &ref[((size_t)(ty * TILE + y) * w_ + tx * TILE + x) * 4];
                        if (p[3] == 0) continue;
                        const float inv = 255.0f / p[3];
                        o[0] = (uint8_t)std::min(255.0f, p[0] * inv + 0.5f);
                        o[1] = (uint8_t)std::min(255.0f, p[1] * inv + 0.5f);
                        o[2] = (uint8_t)std::min(255.0f, p[2] * inv + 0.5f);
                        o[3] = p[3];
                    }
                }
            }
        }
    }

    uint8_t seed[4];
    std::memcpy(seed, &ref[((size_t)sy * w_ + sx) * 4], 4);
    const int t = (int)(clampf(tol, 0.0f, 1.0f) * 255.0f);
    auto match = [&](int x, int y) {
        const uint8_t* p = &ref[((size_t)y * w_ + x) * 4];
        return std::abs((int)p[0] - seed[0]) <= t && std::abs((int)p[1] - seed[1]) <= t &&
               std::abs((int)p[2] - seed[2]) <= t && std::abs((int)p[3] - seed[3]) <= t;
    };

    std::vector<uint8_t> mask((size_t)w_ * h_, 0);
    std::vector<std::pair<int, int>> stack;
    stack.emplace_back(sx, sy);
    int minX = sx, maxX = sx, minY = sy, maxY = sy;
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        if (mask[(size_t)y * w_ + x] || !match(x, y)) continue;
        int xl = x, xr = x;
        while (xl > 0 && !mask[(size_t)y * w_ + xl - 1] && match(xl - 1, y)) --xl;
        while (xr < w_ - 1 && !mask[(size_t)y * w_ + xr + 1] && match(xr + 1, y)) ++xr;
        for (int i = xl; i <= xr; ++i) mask[(size_t)y * w_ + i] = 1;
        minX = std::min(minX, xl); maxX = std::max(maxX, xr);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        for (int dy = -1; dy <= 1; dy += 2) {
            const int ny = y + dy;
            if (ny < 0 || ny >= h_) continue;
            bool inSpan = false;
            for (int i = xl; i <= xr; ++i) {
                if (!mask[(size_t)ny * w_ + i] && match(i, ny)) {
                    if (!inSpan) { stack.emplace_back(i, ny); inSpan = true; }
                } else {
                    inSpan = false;
                }
            }
        }
    }

    // Expande a regiao (cobre o halo dos tracos suavizados).
    for (int g = 0; g < grow; ++g) {
        std::vector<uint8_t> next = mask;
        const int x0 = std::max(0, minX - 1), x1 = std::min(w_ - 1, maxX + 1);
        const int y0 = std::max(0, minY - 1), y1 = std::min(h_ - 1, maxY + 1);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                if (mask[(size_t)y * w_ + x]) continue;
                if ((x > 0 && mask[(size_t)y * w_ + x - 1]) || (x < w_ - 1 && mask[(size_t)y * w_ + x + 1]) ||
                    (y > 0 && mask[(size_t)(y - 1) * w_ + x]) || (y < h_ - 1 && mask[(size_t)(y + 1) * w_ + x])) {
                    next[(size_t)y * w_ + x] = 1;
                }
            }
        }
        mask.swap(next);
        minX = x0; maxX = x1; minY = y0; maxY = y1;
    }

    return mask;
}

bool Document::floodFill(int li, int sx, int sy, const Color& c, float tol, bool all, int grow) {
    if (li < 0 || li >= layerCount() || sx < 0 || sy < 0 || sx >= w_ || sy >= h_) return false;
    Layer& l = state.layers[li];
    if (l.locked) return false;
    const std::vector<uint8_t> mask = floodMask(li, sx, sy, tol, all, grow);
    if (mask.empty()) return false;
    int minX = w_, maxX = -1, minY = h_, maxY = -1;
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x)
            if (mask[(size_t)y * w_ + x]) { minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y); }
    const Mask* sel = selectionMask();
    const float sr = c.r * c.a * 255.0f, sg = c.g * c.a * 255.0f, sb = c.b * c.a * 255.0f;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (!mask[(size_t)y * w_ + x]) continue;
            float cover = 1.0f;
            if (sel) cover = (*sel)[(size_t)y * w_ + x] / 255.0f;
            if (cover <= 0.0f) continue;
            Tile* tile = writableTile(l, x / TILE, y / TILE);
            paintPixel(tile->px + ((y % TILE) * TILE + (x % TILE)) * 4, sr, sg, sb, c.a, cover, l.alphaLock);
        }
    }
    return true;
}

void Document::fillSelection(int li, const Color& c) {
    if (li < 0 || li >= layerCount() || !state.selection) return;
    Layer& l = state.layers[li];
    if (l.locked) return;
    const Mask& m = *state.selection;
    const float sr = c.r * c.a * 255.0f, sg = c.g * c.a * 255.0f, sb = c.b * c.a * 255.0f;
    for (int ty = 0; ty < ty_; ++ty) {
        for (int tx = 0; tx < tx_; ++tx) {
            Tile* tile = nullptr;
            for (int y = 0; y < TILE && ty * TILE + y < h_; ++y) {
                for (int x = 0; x < TILE && tx * TILE + x < w_; ++x) {
                    const int mv = m[(size_t)(ty * TILE + y) * w_ + tx * TILE + x];
                    if (!mv) continue;
                    if (!tile) tile = writableTile(l, tx, ty);
                    paintPixel(tile->px + (y * TILE + x) * 4, sr, sg, sb, c.a, mv / 255.0f, l.alphaLock);
                }
            }
        }
    }
}

void Document::clearSelectionPixels(int li) {
    if (li < 0 || li >= layerCount() || !state.selection) return;
    Layer& l = state.layers[li];
    if (l.locked || l.alphaLock) return;
    const Mask& m = *state.selection;
    for (int ty = 0; ty < ty_; ++ty) {
        for (int tx = 0; tx < tx_; ++tx) {
            if (!readTile(l, tx, ty)) continue;
            Tile* tile = nullptr;
            for (int y = 0; y < TILE && ty * TILE + y < h_; ++y) {
                for (int x = 0; x < TILE && tx * TILE + x < w_; ++x) {
                    const int mv = m[(size_t)(ty * TILE + y) * w_ + tx * TILE + x];
                    if (!mv) continue;
                    if (!tile) tile = writableTile(l, tx, ty);
                    uint8_t* p = tile->px + (y * TILE + x) * 4;
                    const float k = 1.0f - mv / 255.0f;
                    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(p[i] * k + 0.5f);
                }
            }
        }
    }
}

int Document::layerViaCopy() {
    if (!state.selection) return -1;
    const int src = state.active;
    const int idx = duplicateLayer(src);
    if (idx < 0) return -1;
    Layer& l = state.layers[idx];
    l.name = "Selecao";
    const Mask& m = *state.selection;
    for (int ty = 0; ty < ty_; ++ty) {
        for (int tx = 0; tx < tx_; ++tx) {
            if (!readTile(l, tx, ty)) continue;
            Tile* tile = writableTile(l, tx, ty);
            for (int y = 0; y < TILE; ++y) {
                for (int x = 0; x < TILE; ++x) {
                    const int gx = tx * TILE + x, gy = ty * TILE + y;
                    uint8_t* p = tile->px + (y * TILE + x) * 4;
                    const float k = (gx < w_ && gy < h_) ? m[(size_t)gy * w_ + gx] / 255.0f : 0.0f;
                    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(p[i] * k + 0.5f);
                }
            }
        }
    }
    touch();
    return idx;
}

}  // namespace ClipPaint
