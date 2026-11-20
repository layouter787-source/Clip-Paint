// Selecao: laco/retangulo/elipse com anti-aliasing, inverter, suavizar.
#include "../layers/Document.h"

namespace ClipPaint {

namespace {

// Rasteriza um poligono (regra par-impar) com 4 sub-linhas por pixel e cobertura horizontal exata.
void rasterizePolygon(const std::vector<Vec2>& poly, int w, int h, std::vector<uint8_t>& shape) {
    shape.assign((size_t)w * h, 0);
    const size_t n = poly.size();
    if (n < 3) return;
    float minY = poly[0].y, maxY = poly[0].y;
    for (const Vec2& p : poly) { minY = std::min(minY, p.y); maxY = std::max(maxY, p.y); }
    const int y0 = std::max(0, (int)std::floor(minY));
    const int y1 = std::min(h - 1, (int)std::ceil(maxY));
    std::vector<float> row(w + 1);
    std::vector<float> xs;
    for (int y = y0; y <= y1; ++y) {
        std::fill(row.begin(), row.end(), 0.0f);
        bool any = false;
        for (int s = 0; s < 4; ++s) {
            const float sy = y + (s + 0.5f) * 0.25f;
            xs.clear();
            for (size_t i = 0; i < n; ++i) {
                const Vec2 a = poly[i], b = poly[(i + 1) % n];
                if (a.y == b.y) continue;
                const float lo = std::min(a.y, b.y), hi = std::max(a.y, b.y);
                if (sy < lo || sy >= hi) continue;
                xs.push_back(a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x));
            }
            std::sort(xs.begin(), xs.end());
            for (size_t k = 0; k + 1 < xs.size(); k += 2) {
                const float xl = clampf(xs[k], 0.0f, (float)w);
                const float xr = clampf(xs[k + 1], 0.0f, (float)w);
                if (xr <= xl) continue;
                any = true;
                const int i0 = (int)std::floor(xl), i1 = std::min(w - 1, (int)std::floor(xr));
                if (i0 == i1) {
                    row[i0] += (xr - xl) * 0.25f;
                } else {
                    row[i0] += (i0 + 1 - xl) * 0.25f;
                    for (int i = i0 + 1; i < i1; ++i) row[i] += 0.25f;
                    row[i1] += (xr - i1) * 0.25f;
                }
            }
        }
        if (!any) continue;
        for (int x = 0; x < w; ++x) {
            shape[(size_t)y * w + x] = (uint8_t)clampi((int)(row[x] * 255.0f + 0.5f), 0, 255);
        }
    }
}

}  // namespace

void Document::selectPolygon(const std::vector<Vec2>& poly, SelOp op) {
    std::vector<uint8_t> shape;
    rasterizePolygon(poly, w_, h_, shape);
    auto next = std::make_shared<Mask>((size_t)w_ * h_, 0);
    const Mask* cur = state.selection.get();
    bool any = false;
    for (size_t i = 0; i < next->size(); ++i) {
        const int s = shape[i];
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

void Document::selectRect(float x0, float y0, float x1, float y1, SelOp op) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    selectPolygon({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, op);
}

void Document::selectEllipse(float x0, float y0, float x1, float y1, SelOp op) {
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const float rx = std::fabs(x1 - x0) * 0.5f, ry = std::fabs(y1 - y0) * 0.5f;
    std::vector<Vec2> poly;
    const int n = 96;
    for (int i = 0; i < n; ++i) {
        const float a = 2.0f * kPi * i / n;
        poly.emplace_back(cx + std::cos(a) * rx, cy + std::sin(a) * ry);
    }
    selectPolygon(poly, op);
}

void Document::selectAll() {
    state.selection = std::make_shared<Mask>((size_t)w_ * h_, 255);
    touch();
}

void Document::selectNone() {
    state.selection = nullptr;
    touch();
}

void Document::invertSelection() {
    auto next = std::make_shared<Mask>((size_t)w_ * h_, 255);
    bool any = false;
    if (state.selection) {
        const Mask& m = *state.selection;
        for (size_t i = 0; i < next->size(); ++i) {
            (*next)[i] = (uint8_t)(255 - m[i]);
            if ((*next)[i]) any = true;
        }
    } else {
        any = true;
    }
    state.selection = any ? std::shared_ptr<const Mask>(next) : nullptr;
    touch();
}

void Document::featherSelection(int radius) {
    if (!state.selection || radius < 1) return;
    auto cur = std::make_shared<Mask>(*state.selection);
    std::vector<uint8_t> tmp((size_t)w_ * h_);
    const int win = radius * 2 + 1;
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < h_; ++y) {  // horizontal
            int sum = 0;
            for (int x = -radius; x <= radius; ++x) sum += (*cur)[(size_t)y * w_ + clampi(x, 0, w_ - 1)];
            for (int x = 0; x < w_; ++x) {
                tmp[(size_t)y * w_ + x] = (uint8_t)(sum / win);
                sum += (*cur)[(size_t)y * w_ + clampi(x + radius + 1, 0, w_ - 1)];
                sum -= (*cur)[(size_t)y * w_ + clampi(x - radius, 0, w_ - 1)];
            }
        }
        for (int x = 0; x < w_; ++x) {  // vertical
            int sum = 0;
            for (int y = -radius; y <= radius; ++y) sum += tmp[(size_t)clampi(y, 0, h_ - 1) * w_ + x];
            for (int y = 0; y < h_; ++y) {
                (*cur)[(size_t)y * w_ + x] = (uint8_t)(sum / win);
                sum += tmp[(size_t)clampi(y + radius + 1, 0, h_ - 1) * w_ + x];
                sum -= tmp[(size_t)clampi(y - radius, 0, h_ - 1) * w_ + x];
            }
        }
    }
    state.selection = cur;
    touch();
}

}  // namespace ClipPaint
