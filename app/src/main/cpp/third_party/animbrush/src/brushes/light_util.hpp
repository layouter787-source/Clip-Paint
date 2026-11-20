// Private helpers of the "light" module (luz / raios / fogo / fumaca / ceu).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "../../include/animbrush/animbrush.hpp"

namespace ab {
namespace lu {

// ---- registration helpers ---------------------------------------------------------------
inline BrushInfo mkInfo(const char* id, const char* name, const char* cat, const char* real, const char* model,
                        float size, Color c1, Color c2, bool additive, bool animated, const char* demo = "wave",
                        const char* bg = "", float hold = 0, bool pressure = true) {
  BrushInfo i;
  i.id = id; i.name = name; i.category = cat;
  i.description = std::string("Na vida real: ") + real + " Modelo: " + model;
  i.tags = {cat};
  i.defaultSize = size; i.minSize = std::max(1.f, size * 0.25f); i.maxSize = size * 6.f;
  i.suggestedColor = c1; i.suggestedColor2 = c2;
  i.additive = additive; i.animated = animated; i.needsPressure = pressure;
  i.demo = demo; i.demoBg = bg; i.demoHold = hold;
  return i;
}

template <class B, class... A>
void reg(BrushLibrary& lib, const BrushInfo& info, A... args) {
  lib.add(info, [info, args...]() -> std::unique_ptr<Brush> { return std::make_unique<B>(info, args...); });
}

// ---- colour helpers -------------------------------------------------------------------------
inline Color whiten(Color c, float t) { return mix(c, Color{1, 1, 1, 1}, clampf(t)); }
inline Color scaleC(Color c, float k) { return {c.r * k, c.g * k, c.b * k, c.a}; }
inline Color hueC(float h, float s = 1.f, float v = 1.f) { Color c; hsv2rgb(h, s, v, c.r, c.g, c.b); c.a = 1; return c; }

// Flame / heat ramp. h: 1 = white-hot ... 0 = cold. kind 0 fire, 1 blue, 2 green, 3 purple.
inline Color heatRamp(float h, int kind = 0) {
  h = clampf(h);
  Color stops[5];
  switch (kind) {
    case 1: stops[0] = {0.0f, 0.0f, 0.08f, 1}; stops[1] = {0.05f, 0.15f, 0.8f, 1}; stops[2] = {0.2f, 0.55f, 1.0f, 1}; stops[3] = {0.7f, 0.9f, 1.0f, 1}; stops[4] = {1, 1, 1, 1}; break;
    case 2: stops[0] = {0.0f, 0.06f, 0.0f, 1}; stops[1] = {0.05f, 0.5f, 0.1f, 1}; stops[2] = {0.25f, 0.95f, 0.25f, 1}; stops[3] = {0.8f, 1.0f, 0.6f, 1}; stops[4] = {1, 1, 0.95f, 1}; break;
    case 3: stops[0] = {0.04f, 0.0f, 0.08f, 1}; stops[1] = {0.4f, 0.05f, 0.7f, 1}; stops[2] = {0.75f, 0.25f, 1.0f, 1}; stops[3] = {1.0f, 0.7f, 1.0f, 1}; stops[4] = {1, 0.97f, 1, 1}; break;
    default: stops[0] = {0.12f, 0.02f, 0.01f, 1}; stops[1] = {0.75f, 0.12f, 0.02f, 1}; stops[2] = {1.0f, 0.5f, 0.06f, 1}; stops[3] = {1.0f, 0.82f, 0.3f, 1}; stops[4] = {1, 0.97f, 0.8f, 1}; break;
  }
  float f = h * 4.f; int i = std::min(3, (int)f);
  return mix(stops[i], stops[i + 1], f - i);
}

// ---- shared masks (immutable, built once) -------------------------------------------------
// Inverse-square-like bloom falloff: 1/(1+k d^2) windowed to 0 at the edge -> long glow tails.
inline const Mask& bloomMask() {
  static const Mask m = [] {
    Mask k; k.name = "@bloom"; k.w = k.h = 128; k.a.resize(128 * 128);
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) {
      float nx = (x + 0.5f) / 64.f - 1.f, ny = (y + 0.5f) / 64.f - 1.f, d = std::sqrt(nx * nx + ny * ny);
      float v = 1.f / (1.f + 26.f * d * d) * (1.f - smoothstep(0.35f, 1.f, d));
      k.a[(size_t)y * 128 + x] = (uint8_t)(clampf(v) * 255.f);
    }
    return k;
  }();
  return m;
}

// Regular polygon (sides>=3) or disc (sides=0), filled with bright rim -> bokeh / aperture ghosts.
inline Mask polyMask(int sides, float rim, float fill, float rot = 0.f) {
  Mask m; m.name = "@poly"; m.w = m.h = 128; m.a.resize(128 * 128);
  float apothem = sides >= 3 ? std::cos(kPi / sides) : 1.f;
  for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) {
    float nx = (x + 0.5f) / 64.f - 1.f, ny = (y + 0.5f) / 64.f - 1.f;
    float d;
    if (sides >= 3) {
      float ang = std::atan2(ny, nx) - rot, rr = std::sqrt(nx * nx + ny * ny);
      float sec = kTau / sides; float a = std::fmod(ang + kPi * 4.f, sec) - sec * 0.5f;
      d = rr * std::cos(a) / apothem;
    } else d = std::sqrt(nx * nx + ny * ny);
    float inside = 1.f - smoothstep(0.93f, 1.0f, d);
    float edge = smoothstep(1.f - rim - 0.1f, 1.f - rim * 0.5f, d);
    float v = inside * (fill + (1.f - fill) * edge);
    m.a[(size_t)y * 128 + x] = (uint8_t)(clampf(v) * 255.f);
  }
  return m;
}

// Fluffy smoke puff: fbm blob with radial window (random per `seed`).
inline Mask puffMask(uint32_t seed, float contrast = 1.f) {
  Mask m; m.name = "@puff"; m.w = m.h = 128; m.a.resize(128 * 128);
  for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) {
    float nx = (x + 0.5f) / 64.f - 1.f, ny = (y + 0.5f) / 64.f - 1.f, d = std::sqrt(nx * nx + ny * ny);
    float wx = nx + 0.35f * (fbm(nx * 2.f + 3.f, ny * 2.f, 3, 2.f, 0.5f, seed) - 0.5f);
    float wy = ny + 0.35f * (fbm(nx * 2.f, ny * 2.f + 7.f, 3, 2.f, 0.5f, seed + 5) - 0.5f);
    float n = fbm(wx * 3.2f + 10.f, wy * 3.2f, 5, 2.f, 0.52f, seed + 11);
    float v = (1.f - smoothstep(0.15f, 0.95f, d)) * (0.35f + 0.9f * n);
    v = clampf((v - 0.15f) * 1.6f * contrast);
    m.a[(size_t)y * 128 + x] = (uint8_t)(v * 255.f);
  }
  return m;
}

// ---- dab helpers ----------------------------------------------------------------------------
inline Dab addDab(Color c, float a, float hardness = 0.f) {
  Dab d; d.color = c; d.color.a = 1; d.opacity = a; d.hardness = hardness; d.blend = Blend::Add; return d;
}
inline void bloomAt(ISurface& s, Vec2 p, float r, Color c, float a) {
  Dab d = addDab(c, a); d.shape = &bloomMask(); d.x = p.x; d.y = p.y; d.radius = r; s.drawDab(d);
}
inline void softAt(ISurface& s, Vec2 p, float r, Color c, float a, float hard = 0.f) {
  Dab d = addDab(c, a, hard); d.x = p.x; d.y = p.y; d.radius = r; s.drawDab(d);
}
// elongated streak dab centred at p, along `ang`; halfLen/halfWidth in px
inline void streakAt(ISurface& s, Vec2 p, float ang, float halfLen, float halfW, Color c, float a, float hard = 0.f, Blend bl = Blend::Add) {
  Dab d = addDab(c, a, hard); d.blend = bl; d.x = p.x; d.y = p.y; d.radius = std::max(0.3f, halfW);
  d.aspect = std::max(0.05f, halfLen / std::max(0.3f, halfW)); d.angle = ang; s.drawDab(d);
}

struct GlowCarry { float c[3] = {0, 0, 0}; };

// Light "tube" = wide inverse-square bloom + coloured body + hot core. k: overall gain; white: how much the core whitens.
inline void glowLine(ISurface& s, Vec2 a, Vec2 b, float r0, float r1, Color col, Color halo, float k, float white, GlowCarry& gc,
                     float haloScale = 3.2f) {
  Dab wide = addDab(halo, 0.05f * k); wide.shape = &bloomMask();
  Dab body = addDab(col, 0.25f * k, 0.35f);
  Dab core = addDab(whiten(col, white), 0.85f * k, 0.7f);
  dabLine(s, a, b, r0 * haloScale, r1 * haloScale, wide, 0.09f, &gc.c[0]);
  dabLine(s, a, b, r0 * 1.15f, r1 * 1.15f, body, 0.3f, &gc.c[1]);
  dabLine(s, a, b, r0 * 0.45f, r1 * 0.45f, core, 0.6f, &gc.c[2]);
}
inline void glowPoint(ISurface& s, Vec2 p, float r, Color col, Color halo, float k, float white, float haloScale = 3.2f) {
  bloomAt(s, p, r * haloScale, halo, 0.55f * k);
  softAt(s, p, r * 1.15f, col, 0.55f * k, 0.35f);
  softAt(s, p, r * 0.45f, whiten(col, white), 0.9f * k, 0.7f);
}

// ---- generic stroke base class --------------------------------------------------------------
class StrokeBase : public Brush {
 public:
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    sp_ = sp; S_ = &s; path_.reset(); carry_ = 0;
    rng_.reseed(sp.seed + (info_.animated ? (uint32_t)sp.frame * 7919u + 13u : 0u));
    path_.add(in, 0.f);
    prev_ = path_.back();
    active_ = true;
    onBegin(s, in);
  }
  void move(ISurface& s, const InputSample& in) override {
    if (!active_) return;
    size_t n = path_.size();
    path_.add(in, minDist());
    if (path_.size() == n) return;
    PathPoint cur = path_.back();
    onSegment(s, prev_, cur);
    prev_ = cur;
  }
  void end(ISurface& s) override { if (!active_) return; active_ = false; onEnd(s); }
  void hold(ISurface& s, float dt) override { if (active_) onHold(s, dt); }

 protected:
  virtual float minDist() const { return 1.5f; }
  virtual void onBegin(ISurface&, const InputSample&) {}
  virtual void onSegment(ISurface&, const PathPoint&, const PathPoint&) {}
  virtual void onEnd(ISurface&) {}
  virtual void onHold(ISurface&, float) {}
  float rad(const PathPoint& p, float lo = 0.35f) const { return sp_.size * lerpf(lo, 1.f, p.pressure); }
  float gain() const { return sp_.opacity * sp_.intensity; }

  StrokeParams sp_;
  PathTracker path_;
  Rng rng_;
  ISurface* S_ = nullptr;
  float carry_ = 0;
  PathPoint prev_;
  bool active_ = false;
};

// ---- lightning geometry ----------------------------------------------------------------------
struct BoltSeg { Vec2 a, b; float w; int lvl; };

// Stepped-leader style channel: steering random walk toward b with kinks; side branches spawn with
// probability decreasing with depth, taper & dim.
inline void genBolt(std::vector<BoltSeg>& out, Vec2 a, Vec2 b, Rng& r, float jag, float branchP, int depth, int maxDepth,
                    float w, Vec2 bias = {0, 0}, float stepLen = 0.f) {
  float len = dist(a, b);
  if (len < 1.f) return;
  float step = stepLen > 0.f ? stepLen : clampf(len / 7.f, 5.f, 16.f);
  Vec2 dir0 = (b - a).norm();
  Vec2 cur = a;
  float kink = 0;
  int guard = 0;
  while (dist(cur, b) > step * 0.9f && guard++ < 400) {
    Vec2 toward = (b - cur).norm();
    float ang = std::atan2(toward.y, toward.x);
    kink = kink * 0.55f + r.gauss() * jag;
    if (r.chance(0.07f)) kink += (r.chance(0.5f) ? 1.f : -1.f) * jag * 2.2f;
    kink = clampf(kink, -1.0f, 1.0f);
    Vec2 d = fromAngle(ang + kink);
    if (bias.len2() > 0.f) d = (d + bias * 0.35f).norm();
    float st = step * r.range(0.6f, 1.35f);
    Vec2 nxt = cur + d * st;
    out.push_back({cur, nxt, w, depth});
    if (depth < maxDepth && r.chance(branchP)) {
      float sgn = r.chance(0.5f) ? 1.f : -1.f;
      float turn = r.range(0.3f, 0.95f) * sgn;
      Vec2 bd = rotate(dir0, turn);
      if (bias.len2() > 0.f) bd = (bd + bias * 0.8f).norm();
      float remain = dist(nxt, b);
      float bl = std::max(14.f, remain * r.range(0.18f, 0.5f)) * (depth == 0 ? 1.f : 0.7f);
      genBolt(out, nxt, nxt + bd * bl, r, jag * 1.25f, branchP * 0.55f, depth + 1, maxDepth, w * 0.55f, bias, step * 0.8f);
    }
    cur = nxt;
  }
  out.push_back({cur, b, w, depth});
}

// Draw a bolt in three passes: wide halo, coloured sheath, white-hot core. Segment width ~ radius*w.
inline void drawBolt(ISurface& s, const std::vector<BoltSeg>& segs, float radius, Color core, Color mid, Color halo, float k) {
  Dab h = addDab(halo, 0.020f * k); h.shape = &bloomMask();
  Dab m = addDab(mid, 0.20f * k, 0.3f);
  Dab c = addDab(core, 0.95f * k, 0.8f);
  for (const auto& sg : segs) {
    float f = std::sqrt(sg.w);
    dabLine(s, sg.a, sg.b, radius * 5.f * f, radius * 5.f * f, h, 0.14f);
  }
  for (const auto& sg : segs) {
    float f = sg.w;
    Dab mm = m; mm.opacity *= 0.5f + 0.5f * f;
    dabLine(s, sg.a, sg.b, std::max(0.7f, radius * 1.35f * f), std::max(0.7f, radius * 1.35f * f), mm, 0.35f);
  }
  for (const auto& sg : segs) {
    float f = sg.w;
    dabLine(s, sg.a, sg.b, std::max(0.55f, radius * 0.5f * f), std::max(0.55f, radius * 0.5f * f), c, 0.5f);
  }
}

// ---- registration entry points of the sub-files ----------------------------------------------------
void registerLuz(BrushLibrary& lib);
void registerRaios(BrushLibrary& lib);
void registerFogo(BrushLibrary& lib);
void registerFumaca(BrushLibrary& lib);
void registerCeu(BrushLibrary& lib);

}  // namespace lu
}  // namespace ab
