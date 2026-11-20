// AnimBrush - core math / color / random / noise utilities.
// Zero external dependencies. C++17.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ab {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 6.28318530717958647692f;
constexpr float kDeg2Rad = kPi / 180.0f;

inline float clampf(float v, float lo = 0.f, float hi = 1.f) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
  float t = clampf((x - e0) / (e1 - e0 == 0.f ? 1e-6f : (e1 - e0)));
  return t * t * (3.f - 2.f * t);
}
inline float sqr(float x) { return x * x; }

struct Vec2 {
  float x = 0, y = 0;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2 operator/(float s) const { return {x / s, y / s}; }
  Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
  float len() const { return std::sqrt(x * x + y * y); }
  float len2() const { return x * x + y * y; }
  Vec2 norm() const { float l = len(); return l > 1e-9f ? Vec2{x / l, y / l} : Vec2{1, 0}; }
  Vec2 perp() const { return {-y, x}; }  // rotated +90deg (screen coords: y down)
  float angle() const { return std::atan2(y, x); }
};
inline Vec2 operator*(float s, Vec2 v) { return v * s; }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float dist(Vec2 a, Vec2 b) { return (a - b).len(); }
inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
inline Vec2 fromAngle(float a, float len = 1.f) { return {std::cos(a) * len, std::sin(a) * len}; }
inline Vec2 rotate(Vec2 v, float a) {
  float c = std::cos(a), s = std::sin(a);
  return {v.x * c - v.y * s, v.x * s + v.y * c};
}

struct IRect {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // [x0,x1) x [y0,y1)
  int w() const { return x1 - x0; }
  int h() const { return y1 - y0; }
  bool empty() const { return x1 <= x0 || y1 <= y0; }
};

// Straight (non-premultiplied) RGBA, each 0..1.
struct Color {
  float r = 0, g = 0, b = 0, a = 1;
  Color() = default;
  Color(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_) {}
  static Color hex(uint32_t rgb, float a = 1.f) {
    return {((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, a};
  }
};
inline Color mix(Color a, Color b, float t) {
  return {lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t), lerpf(a.a, b.a, t)};
}
inline float luma(Color c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

void rgb2hsv(float r, float g, float b, float& h, float& s, float& v);  // h in 0..1
void hsv2rgb(float h, float s, float v, float& r, float& g, float& b);
Color shiftHSV(Color c, float dh, float ds, float dv);  // dh wraps (0..1), ds/dv additive & clamped
// Approximate black-body color for temperature in Kelvin (1000..12000), 0..1 rgb.
Color blackbody(float kelvin);

// ---- Random -----------------------------------------------------------------
inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
inline uint32_t hash2(int32_t x, int32_t y, uint32_t seed = 0) {
  return hash32((uint32_t)x * 0x9E3779B1U ^ hash32((uint32_t)y + 0x85EBCA6BU) ^ (seed * 0xC2B2AE35U));
}
inline float hashf(uint32_t h) { return (h >> 8) * (1.0f / 16777216.0f); }  // [0,1)

class Rng {
 public:
  explicit Rng(uint32_t seed = 1) { reseed(seed); }
  void reseed(uint32_t seed) { s_ = hash32(seed * 2654435761U + 0x1234567U) | 1u; next(); next(); }
  uint32_t next() {  // xorshift32* variant
    uint32_t x = s_; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s_ = x; return x * 0x2545F491U;
  }
  float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }           // [0,1)
  float range(float a, float b) { return a + (b - a) * uniform(); }
  float sym() { return uniform() * 2.f - 1.f; }                             // [-1,1)
  int irange(int a, int b) { return a + (int)(uniform() * (b - a + 1)) % (b - a + 1); }  // inclusive
  bool chance(float p) { return uniform() < p; }
  float gauss() {  // approx N(0,1)
    float s = 0; for (int i = 0; i < 4; ++i) s += uniform(); return (s - 2.f) * 1.7320508f;
  }
  Vec2 inDisc() { for (;;) { Vec2 v{sym(), sym()}; if (v.len2() <= 1.f) return v; } }
  Vec2 onCircle() { float a = uniform() * kTau; return {std::cos(a), std::sin(a)}; }
  uint32_t state() const { return s_; }

 private:
  uint32_t s_ = 1;
};

// ---- Noise ------------------------------------------------------------------
float valueNoise(float x, float y, uint32_t seed = 0);       // 0..1, smooth
float gradNoise(float x, float y, uint32_t seed = 0);        // -1..1 (perlin-like)
float fbm(float x, float y, int octaves = 4, float lac = 2.f, float gain = 0.5f, uint32_t seed = 0);  // 0..1
float worley(float x, float y, uint32_t seed = 0, float* cellId = nullptr);  // distance to nearest feature (0..~1)
float noise1(float x, uint32_t seed = 0);                    // -1..1 smooth 1D noise (for wobble)

// ---- Curves -----------------------------------------------------------------
// Piecewise linear curve over x. Empty curve evaluates to 0.
struct Curve {
  std::vector<Vec2> pts;
  Curve() = default;
  Curve(std::initializer_list<Vec2> l) : pts(l) {}
  float eval(float x) const;
  bool empty() const { return pts.empty(); }
  static Curve linear(float y0, float y1, float x0 = 0.f, float x1 = 1.f) { return Curve{{x0, y0}, {x1, y1}}; }
};

// Catmull-Rom point on segment p1..p2 (t in 0..1).
Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t);

}  // namespace ab
