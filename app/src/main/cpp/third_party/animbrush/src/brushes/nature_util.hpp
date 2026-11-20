// Private helpers for the "nature" module: drawing primitives + two brush base classes.
#pragma once
#include <functional>
#include <vector>

#include "../../include/animbrush/animbrush.hpp"

namespace ab {
namespace nat {

using ColorFn = std::function<Color(float, float)>;

inline Color shade(Color c, float k) { return {clampf(c.r * k), clampf(c.g * k), clampf(c.b * k), c.a}; }
inline Color lighten(Color c, float k) { return mix(c, Color{1, 1, 1, c.a}, k); }
inline Color darken(Color c, float k) { return mix(c, Color{0, 0, 0, c.a}, k); }
inline Color jitter(Color c, Rng& r, float h, float s, float v) { return shiftHSV(c, r.sym() * h, r.sym() * s, r.sym() * v); }

inline Dab mk(Color c, float op = 1.f, float hard = 1.f) {
  Dab d; d.color = c; d.color.a = 1.f; d.opacity = op; d.hardness = hard; return d;
}
inline void disc(ISurface& s, Vec2 p, float r, Color c, float op = 1.f, float hard = 1.f) {
  Dab d = mk(c, op, hard); d.x = p.x; d.y = p.y; d.radius = std::max(0.3f, r); s.drawDab(d);
}
// ellipse: semi-axis rx along `ang`, ry across
inline void ell(ISurface& s, Vec2 p, float rx, float ry, float ang, Color c, float op = 1.f, float hard = 1.f) {
  Dab d = mk(c, op, hard); d.x = p.x; d.y = p.y; d.radius = std::max(0.3f, ry); d.aspect = std::max(0.05f, rx / std::max(0.3f, ry)); d.angle = ang; s.drawDab(d);
}
inline void maskDab(ISurface& s, const Mask* m, Vec2 p, float rx, float ry, float ang, Color c, float op = 1.f, bool mirror = false) {
  Dab d = mk(c, op, 1.f); d.x = p.x; d.y = p.y; d.radius = std::max(0.3f, ry); d.aspect = std::max(0.05f, rx / std::max(0.3f, ry));
  d.angle = ang; d.shape = m; d.mirrorX = mirror; s.drawDab(d);
}

inline std::vector<Vec2> bez(Vec2 a, Vec2 c, Vec2 b, int n) {
  std::vector<Vec2> v;
  for (int i = 0; i <= n; ++i) { float t = i / (float)n, u = 1 - t; v.push_back(a * (u * u) + c * (2 * u * t) + b * (t * t)); }
  return v;
}
inline float polyLen(const std::vector<Vec2>& p) { float l = 0; for (size_t i = 1; i < p.size(); ++i) l += dist(p[i - 1], p[i]); return l; }

// Variable-radius polyline (radius interpolated by arc length r0 -> r1)
inline void taper(ISurface& s, const std::vector<Vec2>& p, float r0, float r1, Color c, float op = 1.f, float hard = 1.f, float sp = 0.3f) {
  if (p.empty()) return;
  float L = std::max(1e-3f, polyLen(p)), acc = 0, carry = 0;
  Dab d = mk(c, op, hard);
  dabRound(s, p[0], r0, d);
  for (size_t i = 1; i < p.size(); ++i) {
    float a = acc / L, seg = dist(p[i - 1], p[i]); acc += seg;
    dabLine(s, p[i - 1], p[i], lerpf(r0, r1, a), lerpf(r0, r1, acc / L), d, sp, &carry);
  }
}
inline void line(ISurface& s, Vec2 a, Vec2 b, float r, Color c, float op = 1.f, float hard = 1.f) {
  Dab d = mk(c, op, hard); dabRound(s, a, r, d); float cr = 0; dabLine(s, a, b, r, r, d, 0.35f, &cr);
}

// Scanline polygon fill with per-pixel colour function (AA: 4 vertical subsamples, exact horizontal coverage).
inline void fillPoly(ISurface& s, const std::vector<Vec2>& poly, const ColorFn& fn, float opacity = 1.f) {
  if (poly.size() < 3) return;
  float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
  for (auto& p : poly) { minx = std::min(minx, p.x); maxx = std::max(maxx, p.x); miny = std::min(miny, p.y); maxy = std::max(maxy, p.y); }
  int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(s.width(), (int)std::ceil(maxx) + 1);
  int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(s.height(), (int)std::ceil(maxy) + 1);
  if (x1 <= x0 || y1 <= y0) return;
  Patch pt(x0, y0, x1 - x0, y1 - y0);
  std::vector<float> cov(x1 - x0);
  std::vector<float> xs;
  const int SS = 4;
  for (int y = y0; y < y1; ++y) {
    std::fill(cov.begin(), cov.end(), 0.f);
    for (int k = 0; k < SS; ++k) {
      float sy = y + (k + 0.5f) / SS;
      xs.clear();
      for (size_t i = 0; i < poly.size(); ++i) {
        Vec2 a = poly[i], b = poly[(i + 1) % poly.size()];
        if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs.push_back(a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x));
      }
      std::sort(xs.begin(), xs.end());
      for (size_t i = 0; i + 1 < xs.size(); i += 2) {
        float xa = std::max(xs[i], (float)x0), xb = std::min(xs[i + 1], (float)x1);
        if (xb <= xa) continue;
        for (int px = (int)std::floor(xa); px <= (int)std::floor(xb) && px < x1; ++px) {
          float ov = std::min(xb, px + 1.f) - std::max(xa, (float)px);
          if (ov > 0) cov[px - x0] += ov / SS;
        }
      }
    }
    for (int x = x0; x < x1; ++x) {
      float cv = clampf(cov[x - x0]);
      if (cv <= 0.002f) continue;
      Color c = fn(x + 0.5f, y + 0.5f);
      float a = cv * c.a * opacity;
      float* d = pt.at(x, y);
      d[0] = c.r * a; d[1] = c.g * a; d[2] = c.b * a; d[3] = a;
    }
  }
  s.compositePatch(pt, Blend::Normal, 1.f);
}
inline void fillPolyFlat(ISurface& s, const std::vector<Vec2>& poly, Color c, float opacity = 1.f) {
  fillPoly(s, poly, [c](float, float) { return c; }, opacity);
}

// Leaf: pointed ellipse with a lit half and a midrib. base = petiole point.
inline void leafShape(ISurface& s, Vec2 base, float ang, float len, float wid, Color c, float light = 0.f) {
  Vec2 dir = fromAngle(ang);
  Vec2 ctr = base + dir * (len * 0.5f);
  ell(s, ctr, len * 0.5f, wid * 0.5f, ang, c, 1.f, 1.f);
  if (wid > 3.f) {
    Vec2 nrm = dir.perp();
    ell(s, ctr - nrm * (wid * 0.14f), len * 0.4f, wid * 0.22f, ang, lighten(c, 0.18f + light), 0.8f, 0.6f);
    line(s, base, base + dir * (len * 0.9f), std::max(0.4f, wid * 0.04f), shade(c, 0.7f), 0.6f);
  }
}

// ----------------------------------------------------------------------------------------------
struct ParamSpec { const char* key; const char* label; float def, mn, mx; };

struct NCtx {
  ISurface& s;
  const StrokeParams& sp;
  Rng& rng;
  const Brush& br;
  const PathTracker& path;
  float P(const char* k) const { return br.param(k); }
};

// Draws once at stroke end (gesture brushes: trees, mountains, clouds...). A faint preview line is laid while drawing.
class EndBrush : public Brush {
 public:
  using Gen = std::function<void(NCtx&)>;
  EndBrush(BrushInfo bi, std::vector<ParamSpec> ps, Gen g, bool preview = true) : gen_(std::move(g)), preview_(preview) {
    info_ = std::move(bi);
    for (auto& p : ps) addParam(p.key, p.label, p.def, p.mn, p.mx);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    sp_ = sp; rng_.reseed(sp.seed); path_.reset(); path_.add(in, 0.5f);
    if (preview_) disc(s, {in.x, in.y}, 1.2f, previewColor(), 0.5f);
  }
  void move(ISurface& s, const InputSample& in) override {
    Vec2 prev = path_.size() ? path_.back().p : Vec2{in.x, in.y};
    path_.add(in, 0.5f);
    if (preview_) line(s, prev, {in.x, in.y}, 1.2f, previewColor(), 0.5f);
  }
  void end(ISurface& s) override {
    if (path_.size() == 0) return;
    NCtx c{s, sp_, rng_, *this, path_};
    gen_(c);
  }

 private:
  Color previewColor() const { return darken(sp_.color, 0.2f); }
  Gen gen_; bool preview_;
  StrokeParams sp_; Rng rng_; PathTracker path_;
};

// Emits something every `spacing` px along the stroke (grass, rain, fur, scattered objects...).
class LiveBrush : public Brush {
 public:
  using Emit = std::function<void(NCtx&, const PathPoint&, int)>;
  using Spacing = std::function<float(const StrokeParams&)>;
  LiveBrush(BrushInfo bi, std::vector<ParamSpec> ps, Spacing sp, Emit e) : spacing_(std::move(sp)), emit_(std::move(e)) {
    info_ = std::move(bi);
    for (auto& p : ps) addParam(p.key, p.label, p.def, p.mn, p.mx);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    sp_ = sp; rng_.reseed(sp.seed); path_.reset(); path_.add(in, 0.5f); idx_ = 0;
    NCtx c{s, sp_, rng_, *this, path_};
    emit_(c, path_.back(), idx_++);
    next_ = std::max(1.f, spacing_(sp_));
  }
  void move(ISurface& s, const InputSample& in) override {
    path_.add(in, 0.5f);
    NCtx c{s, sp_, rng_, *this, path_};
    while (next_ <= path_.length()) {
      emit_(c, path_.at(next_), idx_++);
      next_ += std::max(1.f, spacing_(sp_));
    }
  }
  void end(ISurface&) override {}

 private:
  Spacing spacing_; Emit emit_;
  StrokeParams sp_; Rng rng_; PathTracker path_; int idx_ = 0; float next_ = 0;
};

inline float windSway(const StrokeParams& sp, float phase, float amp) { return sp.wind * amp * (0.6f + 0.4f * std::sin(sp.time * 2.6f + phase)); }

// registration hooks of the nature sub-files
void registerTrees(BrushLibrary& lib);
void registerPlants(BrushLibrary& lib);
void registerTerrainWater(BrushLibrary& lib);
void registerClimateFur(BrushLibrary& lib);

inline void addEnd(BrushLibrary& lib, BrushInfo bi, std::vector<ParamSpec> ps, EndBrush::Gen g, bool preview = true) {
  BrushInfo copy = bi;
  lib.add(std::move(bi), [copy, ps, g, preview]() -> std::unique_ptr<Brush> { return std::make_unique<EndBrush>(copy, ps, g, preview); });
}
inline void addLive(BrushLibrary& lib, BrushInfo bi, std::vector<ParamSpec> ps, LiveBrush::Spacing sp, LiveBrush::Emit e) {
  BrushInfo copy = bi;
  lib.add(std::move(bi), [copy, ps, sp, e]() -> std::unique_ptr<Brush> { return std::make_unique<LiveBrush>(copy, ps, sp, e); });
}

inline BrushInfo info(const char* id, const char* name, const char* cat, const char* desc, float size, Color c1, Color c2 = Color{1, 1, 1, 1},
                      const char* demo = "wave", const char* bg = "") {
  BrushInfo b; b.id = id; b.name = name; b.category = cat; b.description = desc; b.defaultSize = size;
  b.minSize = std::max(1.f, size * 0.25f); b.maxSize = size * 5.f; b.suggestedColor = c1; b.suggestedColor2 = c2; b.demo = demo; b.demoBg = bg;
  b.needsPressure = true;
  return b;
}

}  // namespace nat
}  // namespace ab
