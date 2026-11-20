// Clip Paint - tipos e utilitarios comuns (sem dependencia de Android/GL).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#define CP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ClipPaint", __VA_ARGS__)
#define CP_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ClipPaint", __VA_ARGS__)
#else
#define CP_LOGI(...) (std::fprintf(stderr, __VA_ARGS__), std::fprintf(stderr, "\n"))
#define CP_LOGE(...) (std::fprintf(stderr, __VA_ARGS__), std::fprintf(stderr, "\n"))
#endif

namespace ClipPaint {

constexpr float kPi = 3.14159265358979f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    Vec2() = default;
    Vec2(float px, float py) : x(px), y(py) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
};

// Cor em float. Os canais NAO sao pre-multiplicados, salvo indicacao.
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    Color() = default;
    Color(float pr, float pg, float pb, float pa = 1.0f) : r(pr), g(pg), b(pb), a(pa) {}
};

struct RectI {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // [x0,x1) x [y0,y1)
    RectI() = default;
    RectI(int ax0, int ay0, int ax1, int ay1) : x0(ax0), y0(ay0), x1(ax1), y1(ay1) {}
    bool empty() const { return x1 <= x0 || y1 <= y0; }
    int width() const { return x1 - x0; }
    int height() const { return y1 - y0; }
    void unite(const RectI& o) {
        if (o.empty()) return;
        if (empty()) { *this = o; return; }
        x0 = std::min(x0, o.x0); y0 = std::min(y0, o.y0);
        x1 = std::max(x1, o.x1); y1 = std::max(y1, o.y1);
    }
    RectI intersect(const RectI& o) const {
        RectI r(std::max(x0, o.x0), std::max(y0, o.y0), std::min(x1, o.x1), std::min(y1, o.y1));
        if (r.empty()) return RectI();
        return r;
    }
};

inline void rgbToHsv(float r, float g, float b, float& h, float& s, float& v) {
    const float mx = std::max(r, std::max(g, b));
    const float mn = std::min(r, std::min(g, b));
    const float d = mx - mn;
    v = mx;
    s = mx <= 0.0f ? 0.0f : d / mx;
    if (d <= 1e-6f) { h = 0.0f; return; }
    if (mx == r) h = std::fmod((g - b) / d, 6.0f);
    else if (mx == g) h = (b - r) / d + 2.0f;
    else h = (r - g) / d + 4.0f;
    h /= 6.0f;
    if (h < 0.0f) h += 1.0f;
}

inline void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    h = h - std::floor(h);
    const float c = v * s;
    const float hp = h * 6.0f;
    const float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float rr = 0, gg = 0, bb = 0;
    if (hp < 1) { rr = c; gg = x; }
    else if (hp < 2) { rr = x; gg = c; }
    else if (hp < 3) { gg = c; bb = x; }
    else if (hp < 4) { gg = x; bb = c; }
    else if (hp < 5) { rr = x; bb = c; }
    else { rr = c; bb = x; }
    const float m = v - c;
    r = rr + m; g = gg + m; b = bb + m;
}

double nowSeconds();

}  // namespace ClipPaint
