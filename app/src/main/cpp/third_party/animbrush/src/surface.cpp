#include "../include/animbrush/surface.hpp"

#include <cstring>

namespace ab {

float Mask::sample(float u, float v) const {
  if (a.empty()) return 0.f;
  float x = u * w - 0.5f, y = v * h - 0.5f;
  int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
  float fx = x - x0, fy = y - y0;
  auto px = [&](int xx, int yy) -> float {
    if (xx < 0 || yy < 0 || xx >= w || yy >= h) return 0.f;
    return a[(size_t)yy * w + xx] * (1.f / 255.f);
  };
  float c00 = px(x0, y0), c10 = px(x0 + 1, y0), c01 = px(x0, y0 + 1), c11 = px(x0 + 1, y0 + 1);
  return lerpf(lerpf(c00, c10, fx), lerpf(c01, c11, fx), fy);
}

float Mask::sampleTiled(float u, float v) const {
  if (a.empty()) return 1.f;
  float x = u * w - 0.5f, y = v * h - 0.5f;
  int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
  float fx = x - x0, fy = y - y0;
  auto wrap = [](int i, int n) { i %= n; return i < 0 ? i + n : i; };
  auto px = [&](int xx, int yy) -> float { return a[(size_t)wrap(yy, h) * w + wrap(xx, w)] * (1.f / 255.f); };
  float c00 = px(x0, y0), c10 = px(x0 + 1, y0), c01 = px(x0, y0 + 1), c11 = px(x0 + 1, y0 + 1);
  return lerpf(lerpf(c00, c10, fx), lerpf(c01, c11, fx), fy);
}

// ------------------------------------------------------------------------------------------
static inline float blendCh(Blend m, float cb, float cs) {
  switch (m) {
    case Blend::Multiply: return cb * cs;
    case Blend::Screen: return cb + cs - cb * cs;
    case Blend::Overlay: return cb <= 0.5f ? 2.f * cb * cs : 1.f - 2.f * (1.f - cb) * (1.f - cs);
    case Blend::Darken: return std::min(cb, cs);
    case Blend::Lighten: return std::max(cb, cs);
    default: return cs;
  }
}

void blendPixel(float* d, Color cs, float sa, Blend mode, bool lockAlpha) {
  if (sa <= 0.f) return;
  sa = std::min(sa, 1.f);
  float da = d[3];
  switch (mode) {
    case Blend::Erase:
    case Blend::Dissolve:
    case Blend::EraseSimilar: case Blend::EraseDark: case Blend::EraseLight: case Blend::EraseEdge: {
      float k = 1.f - sa; d[0] *= k; d[1] *= k; d[2] *= k; d[3] *= k; return;
    }
    case Blend::Add: {
      if (lockAlpha) { d[0] += cs.r * sa * da; d[1] += cs.g * sa * da; d[2] += cs.b * sa * da; return; }
      d[0] = std::min(1.f, d[0] + cs.r * sa); d[1] = std::min(1.f, d[1] + cs.g * sa); d[2] = std::min(1.f, d[2] + cs.b * sa);
      d[3] = da + sa * (1.f - da); return;
    }
    case Blend::Normal: case Blend::Smudge: case Blend::Soften: {
      if (lockAlpha) {
        float k = 1.f - sa;
        d[0] = d[0] * k + cs.r * da * sa; d[1] = d[1] * k + cs.g * da * sa; d[2] = d[2] * k + cs.b * da * sa;
        return;
      }
      float k = 1.f - sa;
      d[0] = d[0] * k + cs.r * sa; d[1] = d[1] * k + cs.g * sa; d[2] = d[2] * k + cs.b * sa;
      d[3] = da * k + sa; return;
    }
    default: {  // separable W3C modes
      float r = cs.r, g = cs.g, b = cs.b;
      if (da > 1e-5f) {
        float br = std::min(1.f, d[0] / da), bg = std::min(1.f, d[1] / da), bb = std::min(1.f, d[2] / da);
        r = (1.f - da) * cs.r + da * blendCh(mode, br, cs.r);
        g = (1.f - da) * cs.g + da * blendCh(mode, bg, cs.g);
        b = (1.f - da) * cs.b + da * blendCh(mode, bb, cs.b);
      }
      float k = 1.f - sa;
      if (lockAlpha) {
        d[0] = d[0] * k + r * da * sa; d[1] = d[1] * k + g * da * sa; d[2] = d[2] * k + b * da * sa; return;
      }
      d[0] = d[0] * k + r * sa; d[1] = d[1] * k + g * sa; d[2] = d[2] * k + b * sa; d[3] = da * k + sa;
      return;
    }
  }
}

// ------------------------------------------------------------------------------------------
RasterSurface::RasterSurface(int w, int h) : w_(w), h_(h), px_((size_t)w * h * 4, 0.f) {}

void RasterSurface::clear(Color c) {
  for (size_t i = 0; i < (size_t)w_ * h_; ++i) {
    px_[i * 4] = c.r * c.a; px_[i * 4 + 1] = c.g * c.a; px_[i * 4 + 2] = c.b * c.a; px_[i * 4 + 3] = c.a;
  }
}

void RasterSurface::drawDab(const Dab& D) {
  if (D.radius <= 0.f || D.opacity <= 0.f) return;
  const float b = std::max(0.05f, D.radius);
  const float a = b * std::max(0.05f, D.aspect);
  const float ca = std::cos(D.angle), sa = std::sin(D.angle);
  const float ex = std::fabs(a * ca) + std::fabs(b * sa) + 1.5f;
  const float ey = std::fabs(a * sa) + std::fabs(b * ca) + 1.5f;
  int x0 = std::max(0, (int)std::floor(D.x - ex)), x1 = std::min(w_, (int)std::ceil(D.x + ex));
  int y0 = std::max(0, (int)std::floor(D.y - ey)), y1 = std::min(h_, (int)std::ceil(D.y + ey));
  if (x1 <= x0 || y1 <= y0) return;

  const bool special = D.blend == Blend::Soften;
  Patch tmp;
  if (special) tmp = readPatch({x0 - 2, y0 - 2, x1 + 2, y1 + 2});

  const float rmin = std::min(a, b);
  const float hard = clampf(D.hardness, 0.f, 0.999f);
  const float invHard = 1.f / (1.f - hard);
  const bool useGrain = D.grain && D.grainDepth > 0.f && !D.grain->empty();
  const float gs = D.grainScale > 0 ? D.grainScale : 1.f;
  const bool procedural = D.shape == nullptr || D.shape->empty();

  for (int py = y0; py < y1; ++py) {
    for (int px = x0; px < x1; ++px) {
      float dx = px + 0.5f - D.x, dy = py + 0.5f - D.y;
      float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
      float nu = u / a, nv = v / b;
      float cov;
      if (procedural) {
        float d2 = nu * nu + nv * nv;
        if (d2 >= 1.f) continue;
        float d = std::sqrt(d2);
        float fall = d <= hard ? 1.f : 1.f - smoothstep(0.f, 1.f, (d - hard) * invHard);
        float aa = clampf((1.f - d) * rmin + 0.5f);
        cov = fall * aa;
      } else {
        if (std::fabs(nu) >= 1.f || std::fabs(nv) >= 1.f) continue;
        float su = D.mirrorX ? -nu : nu;
        cov = D.shape->sample(su * 0.5f + 0.5f, nv * 0.5f + 0.5f);
        if (D.invertShape) cov = 1.f - cov;
      }
      if (cov <= 0.002f) continue;
      float al = cov * D.opacity;
      if (useGrain) {
        float g = D.grain->sampleTiled(px * gs / D.grain->w, py * gs / D.grain->h);
        g = clampf(g + D.grainBias);
        al *= lerpf(1.f, smoothstep(0.32f, 0.68f, g), D.grainDepth);  // tooth gates ink: binary-ish, so overlaps don't wash it out
      }
      if (D.dissolve > 0.f && D.blend != Blend::Dissolve) {
        if (hashf(hash2(px, py, D.seed)) < D.dissolve) continue;
      }
      float* dst = pixel(px, py);
      switch (D.blend) {
        case Blend::Smudge: {
          float k = 1.f - al, ca2 = D.colorAlpha;
          dst[0] = dst[0] * k + D.color.r * ca2 * al; dst[1] = dst[1] * k + D.color.g * ca2 * al;
          dst[2] = dst[2] * k + D.color.b * ca2 * al; dst[3] = dst[3] * k + ca2 * al;
          break;
        }
        case Blend::Dissolve: {
          float h = hashf(hash2(px, py, D.seed + 17));
          if (h < al * (D.dissolve > 0 ? D.dissolve : 1.f) * 1.0f + (D.dissolve > 0 ? 0 : 0)) {
            dst[0] = dst[1] = dst[2] = dst[3] = 0.f;
          }
          break;
        }
        case Blend::EraseSimilar: {
          float pa = dst[3];
          if (pa <= 1e-4f) break;
          float dr = dst[0] / pa - D.refColor.r, dg = dst[1] / pa - D.refColor.g, db = dst[2] / pa - D.refColor.b;
          float dist = std::sqrt((dr * dr + dg * dg + db * db) * (1.f / 3.f)) * 1.7320508f * 0.57735f;
          float w = 1.f - smoothstep(D.tolerance, D.tolerance * 1.6f + 0.02f, dist);
          blendPixel(dst, D.color, al * w, Blend::Erase, false);
          break;
        }
        case Blend::EraseDark: case Blend::EraseLight: {
          float pa = dst[3];
          if (pa <= 1e-4f) break;
          float lum = 0.2126f * dst[0] / pa + 0.7152f * dst[1] / pa + 0.0722f * dst[2] / pa;
          float w = D.blend == Blend::EraseDark ? 1.f - smoothstep(D.tolerance, D.tolerance + 0.15f, lum)
                                                : smoothstep(1.f - D.tolerance - 0.15f, 1.f - D.tolerance, lum);
          blendPixel(dst, D.color, al * w, Blend::Erase, false);
          break;
        }
        case Blend::EraseEdge: {
          float pa = dst[3];
          if (pa <= 1e-4f) break;
          float w = 1.f - smoothstep(D.edgeLimit * 0.6f, D.edgeLimit, pa);
          blendPixel(dst, D.color, al * w, Blend::Erase, false);
          break;
        }
        case Blend::Soften: {
          float acc[4] = {0, 0, 0, 0}; int n = 0;
          for (int oy = -2; oy <= 2; ++oy)
            for (int ox = -2; ox <= 2; ++ox) {
              int sx = px + ox, sy = py + oy;
              if (!tmp.contains(sx, sy)) continue;
              const float* q = tmp.at(sx, sy);
              acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; acc[3] += q[3]; ++n;
            }
          if (n) for (int c = 0; c < 4; ++c) dst[c] = lerpf(dst[c], acc[c] / n, al);
          break;
        }
        default:
          blendPixel(dst, D.color, al, D.blend, D.lockAlpha);
      }
    }
  }
}

Color RasterSurface::getColor(float x, float y, float radius) const {
  radius = std::max(0.5f, radius);
  int n = (int)clampf(std::ceil(radius * 2.f), 1.f, 16.f);
  double acc[4] = {0, 0, 0, 0}; double wsum = 0;
  for (int j = 0; j < n; ++j)
    for (int i = 0; i < n; ++i) {
      float fx = (i + 0.5f) / n * 2.f - 1.f, fy = (j + 0.5f) / n * 2.f - 1.f;
      float r2 = fx * fx + fy * fy;
      if (r2 > 1.f) continue;
      int sx = (int)std::floor(x + fx * radius), sy = (int)std::floor(y + fy * radius);
      float w = 1.f - r2 * 0.6f; wsum += w;
      if (sx < 0 || sy < 0 || sx >= w_ || sy >= h_) continue;
      const float* p = pixel(sx, sy);
      for (int c = 0; c < 4; ++c) acc[c] += p[c] * w;
    }
  if (wsum <= 0) return {0, 0, 0, 0};
  float a = (float)(acc[3] / wsum);
  if (a <= 1e-5f) return {0, 0, 0, 0};
  return {(float)(acc[0] / wsum) / a, (float)(acc[1] / wsum) / a, (float)(acc[2] / wsum) / a, a};
}

Patch RasterSurface::readPatch(IRect r) const {
  r.x0 = std::max(0, r.x0); r.y0 = std::max(0, r.y0); r.x1 = std::min(w_, r.x1); r.y1 = std::min(h_, r.y1);
  if (r.empty()) return Patch();
  Patch p(r.x0, r.y0, r.w(), r.h());
  for (int y = r.y0; y < r.y1; ++y)
    std::memcpy(p.at(r.x0, y), pixel(r.x0, y), sizeof(float) * 4 * r.w());
  return p;
}

void RasterSurface::compositePatch(const Patch& p, Blend mode, float opacity) {
  for (int y = std::max(0, p.y0); y < std::min(h_, p.y0 + p.h); ++y) {
    for (int x = std::max(0, p.x0); x < std::min(w_, p.x0 + p.w); ++x) {
      const float* s = p.at(x, y);
      float* d = pixel(x, y);
      if (mode == Blend::Smudge) {  // replace-lerp
        for (int c = 0; c < 4; ++c) d[c] = lerpf(d[c], s[c], opacity);
        continue;
      }
      float sa = s[3] * opacity;
      if (sa <= 1e-6f && mode != Blend::Add) continue;
      if (mode == Blend::Normal && !false) {
        float k = 1.f - sa;
        d[0] = d[0] * k + s[0] * opacity; d[1] = d[1] * k + s[1] * opacity; d[2] = d[2] * k + s[2] * opacity; d[3] = d[3] * k + sa;
        continue;
      }
      if (mode == Blend::Add) {
        d[0] = std::min(1.f, d[0] + s[0] * opacity); d[1] = std::min(1.f, d[1] + s[1] * opacity); d[2] = std::min(1.f, d[2] + s[2] * opacity);
        d[3] = d[3] + sa * (1.f - d[3]);
        continue;
      }
      if (s[3] <= 1e-6f) continue;
      Color cs{s[0] / s[3], s[1] / s[3], s[2] / s[3], 1.f};
      blendPixel(d, cs, sa, mode, false);
    }
  }
}

std::vector<uint8_t> RasterSurface::toRGBA8(Color bg) const {
  std::vector<uint8_t> out((size_t)w_ * h_ * 4);
  for (size_t i = 0; i < (size_t)w_ * h_; ++i) {
    const float* p = &px_[i * 4];
    float k = 1.f - p[3];
    float r = p[0] + bg.r * bg.a * k, g = p[1] + bg.g * bg.a * k, b = p[2] + bg.b * bg.a * k, a = p[3] + bg.a * k;
    if (a > 1e-5f && bg.a < 1.f) { r /= a; g /= a; b /= a; }
    out[i * 4] = (uint8_t)(clampf(r) * 255.f + 0.5f); out[i * 4 + 1] = (uint8_t)(clampf(g) * 255.f + 0.5f);
    out[i * 4 + 2] = (uint8_t)(clampf(b) * 255.f + 0.5f); out[i * 4 + 3] = (uint8_t)(clampf(a) * 255.f + 0.5f);
  }
  return out;
}

std::vector<uint8_t> RasterSurface::toRGBA8Checker(int cell) const {
  std::vector<uint8_t> out((size_t)w_ * h_ * 4);
  for (int y = 0; y < h_; ++y)
    for (int x = 0; x < w_; ++x) {
      float c = (((x / cell) + (y / cell)) & 1) ? 0.78f : 0.9f;
      const float* p = pixel(x, y);
      float k = 1.f - p[3];
      size_t o = ((size_t)y * w_ + x) * 4;
      out[o] = (uint8_t)(clampf(p[0] + c * k) * 255.f + 0.5f); out[o + 1] = (uint8_t)(clampf(p[1] + c * k) * 255.f + 0.5f);
      out[o + 2] = (uint8_t)(clampf(p[2] + c * k) * 255.f + 0.5f); out[o + 3] = 255;
    }
  return out;
}

}  // namespace ab
