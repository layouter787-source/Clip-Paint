#include "../include/animbrush/core.hpp"

namespace ab {

void rgb2hsv(float r, float g, float b, float& h, float& s, float& v) {
  float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
  v = mx;
  float d = mx - mn;
  s = mx > 1e-6f ? d / mx : 0.f;
  if (d < 1e-6f) { h = 0; return; }
  if (mx == r) h = (g - b) / d + (g < b ? 6.f : 0.f);
  else if (mx == g) h = (b - r) / d + 2.f;
  else h = (r - g) / d + 4.f;
  h /= 6.f;
}

void hsv2rgb(float h, float s, float v, float& r, float& g, float& b) {
  h = h - std::floor(h);
  float i = std::floor(h * 6.f), f = h * 6.f - i;
  float p = v * (1.f - s), q = v * (1.f - f * s), t = v * (1.f - (1.f - f) * s);
  switch (((int)i) % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
}

Color shiftHSV(Color c, float dh, float ds, float dv) {
  float h, s, v; rgb2hsv(c.r, c.g, c.b, h, s, v);
  h += dh; s = clampf(s + ds); v = clampf(v + dv);
  Color o = c; hsv2rgb(h, s, v, o.r, o.g, o.b);
  return o;
}

Color blackbody(float k) {
  // Tanner Helland approximation
  float t = clampf(k, 1000.f, 12000.f) / 100.f;
  float r, g, b;
  if (t <= 66.f) r = 255.f; else r = 329.698727446f * std::pow(t - 60.f, -0.1332047592f);
  if (t <= 66.f) g = 99.4708025861f * std::log(t) - 161.1195681661f;
  else g = 288.1221695283f * std::pow(t - 60.f, -0.0755148492f);
  if (t >= 66.f) b = 255.f; else if (t <= 19.f) b = 0.f; else b = 138.5177312231f * std::log(t - 10.f) - 305.0447927307f;
  return {clampf(r / 255.f), clampf(g / 255.f), clampf(b / 255.f), 1.f};
}

static inline float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

float valueNoise(float x, float y, uint32_t seed) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float xf = x - xi, yf = y - yi;
  float a = hashf(hash2(xi, yi, seed)), b = hashf(hash2(xi + 1, yi, seed));
  float c = hashf(hash2(xi, yi + 1, seed)), d = hashf(hash2(xi + 1, yi + 1, seed));
  float u = fade(xf), v = fade(yf);
  return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}

static inline float grad(uint32_t h, float x, float y) {
  switch (h & 7) {
    case 0: return x + y; case 1: return -x + y; case 2: return x - y; case 3: return -x - y;
    case 4: return x; case 5: return -x; case 6: return y; default: return -y;
  }
}
float gradNoise(float x, float y, uint32_t seed) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float xf = x - xi, yf = y - yi;
  float a = grad(hash2(xi, yi, seed), xf, yf), b = grad(hash2(xi + 1, yi, seed), xf - 1, yf);
  float c = grad(hash2(xi, yi + 1, seed), xf, yf - 1), d = grad(hash2(xi + 1, yi + 1, seed), xf - 1, yf - 1);
  float u = fade(xf), v = fade(yf);
  return clampf(lerpf(lerpf(a, b, u), lerpf(c, d, u), v) * 1.1f, -1.f, 1.f);
}

float fbm(float x, float y, int oct, float lac, float gain, uint32_t seed) {
  float sum = 0, amp = 0.5f, norm = 0;
  for (int i = 0; i < oct; ++i) {
    sum += amp * valueNoise(x, y, seed + i * 131u);
    norm += amp; x *= lac; y *= lac; amp *= gain;
  }
  return sum / (norm > 0 ? norm : 1.f);
}

float worley(float x, float y, uint32_t seed, float* cellId) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float best = 1e9f; float id = 0;
  for (int j = -1; j <= 1; ++j)
    for (int i = -1; i <= 1; ++i) {
      uint32_t h = hash2(xi + i, yi + j, seed);
      float fx = xi + i + hashf(h), fy = yi + j + hashf(hash32(h + 7));
      float d = std::sqrt(sqr(fx - x) + sqr(fy - y));
      if (d < best) { best = d; id = hashf(hash32(h + 13)); }
    }
  if (cellId) *cellId = id;
  return best;
}

float noise1(float x, uint32_t seed) {
  int xi = (int)std::floor(x); float xf = x - xi;
  float a = hashf(hash2(xi, 77, seed)) * 2.f - 1.f, b = hashf(hash2(xi + 1, 77, seed)) * 2.f - 1.f;
  return lerpf(a, b, fade(xf));
}

float Curve::eval(float x) const {
  if (pts.empty()) return 0.f;
  if (x <= pts.front().x) return pts.front().y;
  if (x >= pts.back().x) return pts.back().y;
  for (size_t i = 1; i < pts.size(); ++i) {
    if (x <= pts[i].x) {
      float d = pts[i].x - pts[i - 1].x;
      float t = d > 1e-9f ? (x - pts[i - 1].x) / d : 0.f;
      return lerpf(pts[i - 1].y, pts[i].y, t);
    }
  }
  return pts.back().y;
}

Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
  float t2 = t * t, t3 = t2 * t;
  return (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * 0.5f;
}

}  // namespace ab
