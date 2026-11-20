// Shared tree/plant growth helpers (nature module private).
#pragma once
#include "nature_util.hpp"

namespace ab {
namespace nat {

struct Tip { Vec2 p; float ang; float r; int depth; };

struct BCfg {
  float spread = 0.55f;     // child angle offset (rad)
  float lenRatio = 0.72f;   // child length / parent length
  float jitter = 0.12f;     // random bend per segment
  float photo = 0.5f;       // phototropism: bend toward up
  float droop = 0.f;        // bend toward down
  int kids = 2;
  int maxDepth = 4;
  float minR = 0.7f;
  float taperEnd = 0.8f;    // radius at end of a branch as fraction of start
  float radiusK = 1.0f;     // multiplier on Leonardo's rule (1 = exact)
  float sideProb = 0.25f;
  Color bark{0.36f, 0.25f, 0.15f, 1};
};

// trunk polyline from the stroke (or a synthetic vertical trunk for dots / short strokes)
inline std::vector<Vec2> trunkPoints(const PathTracker& path, const StrokeParams& sp, float synthLen = 9.f) {
  std::vector<Vec2> v;
  if (path.length() < sp.size * 2.f) {
    Vec2 b = path.points().front().p;
    for (int i = 0; i <= 12; ++i) { float t = i / 12.f; v.push_back({b.x + std::sin(t * 3.f) * sp.size * 0.35f, b.y - t * sp.size * synthLen}); }
    return v;
  }
  for (auto& p : path.resample(4.f)) v.push_back(p.p);
  if (v.size() < 3) { v.clear(); v.push_back(path.points().front().p); v.push_back(path.back().p); }
  return v;
}

inline Vec2 pathNormal(const std::vector<Vec2>& v, size_t i) {
  Vec2 a = v[i > 0 ? i - 1 : 0], b = v[std::min(v.size() - 1, i + 1)];
  return (b - a).norm().perp();
}

// Offset ribbon: dabs along the trunk at lateral offset `off` (fraction of local radius) with radius fraction `rf`.
inline void ribbon(ISurface& s, const std::vector<Vec2>& v, const std::function<float(float)>& radiusAt, float off, float rf, Color c, float op, float hard = 1.f) {
  Dab d = mk(c, op, hard);
  Vec2 prev; float prevR = 0, carry = 0;
  for (size_t i = 0; i < v.size(); ++i) {
    float t = i / (float)(v.size() - 1);
    float r = radiusAt(t);
    Vec2 p = v[i] + pathNormal(v, i) * (off * r);
    float rr = std::max(0.4f, r * rf);
    if (i == 0) dabRound(s, p, rr, d); else dabLine(s, prev, p, prevR, rr, d, 0.3f, &carry);
    prev = p; prevR = rr;
  }
}

// Draw a shaded, fissured trunk. radiusAt(t) t=0 base ... 1 top.
inline void drawTrunk(ISurface& s, Rng& rng, const std::vector<Vec2>& v, const std::function<float(float)>& radiusAt, Color bark, float fissure = 1.f, float light = 0.2f) {
  ribbon(s, v, radiusAt, 0, 1.f, bark, 1.f);
  ribbon(s, v, radiusAt, -0.5f, 0.5f, lighten(bark, light), 0.55f, 0.5f);   // lit side (left)
  ribbon(s, v, radiusAt, 0.58f, 0.42f, darken(bark, 0.4f), 0.6f, 0.5f);      // shaded side
  if (fissure > 0.f) {
    int K = 6;
    for (int k = 0; k < K; ++k) {
      float u = -0.8f + 1.6f * (k + rng.uniform() * 0.6f) / K;
      uint32_t sd = rng.next();
      Dab d = mk(darken(bark, 0.55f), 0.55f * fissure, 0.8f);
      Vec2 prev; bool pen = false;
      for (size_t i = 0; i < v.size(); ++i) {
        float t = i / (float)(v.size() - 1);
        float r = radiusAt(t);
        float wob = (valueNoise(i * 0.35f, k * 7.1f, sd) - 0.5f) * 0.4f;
        bool on = valueNoise(i * 0.22f, k * 3.3f + 50, sd) > 0.42f;
        Vec2 p = v[i] + pathNormal(v, i) * ((u + wob) * r);
        if (on && pen && r > 2.5f) { float cr = 0; d.radius = 0.6f; dabLine(s, prev, p, std::max(0.5f, r * 0.04f), std::max(0.5f, r * 0.04f), d, 0.5f, &cr); }
        pen = on; prev = p;
      }
    }
  }
}

inline void branchGrow(ISurface& s, Rng& rng, const StrokeParams& sp, const BCfg& g, Vec2 p, float ang, float len, float r, int depth, std::vector<Tip>& tips, float thickLight = 0.15f) {
  if (depth > g.maxDepth || r < g.minR || len < 3.f) { tips.push_back({p, ang, r, depth}); return; }
  int n = std::max(3, (int)(len / 7.f));
  std::vector<Vec2> pts{p};
  float a = ang, seg = len / n; Vec2 q = p;
  for (int i = 0; i < n; ++i) {
    a += rng.sym() * g.jitter;
    float da = std::remainder(-kPi * 0.5f - a, kTau); a += da * g.photo * 0.08f;
    float dd = std::remainder(kPi * 0.5f - a, kTau); a += dd * g.droop * 0.07f;
    a += windSway(sp, depth + i * 0.3f, 0.012f * (depth + 1));
    q += fromAngle(a) * seg; pts.push_back(q);
  }
  float r1 = r * g.taperEnd;
  taper(s, pts, r, r1, g.bark, 1.f, 1.f, 0.3f);
  if (r > 2.5f) {
    std::vector<Vec2> hl; for (auto& pp : pts) hl.push_back(pp + Vec2{-r * 0.3f, -r * 0.2f});
    taper(s, hl, r * 0.3f, r1 * 0.3f, lighten(g.bark, thickLight), 0.5f, 0.6f, 0.4f);
  }
  int k = g.kids;
  if (depth > 0 && rng.chance(0.25f)) k += 1;
  float rc = r1 / std::sqrt((float)k) * g.radiusK;
  for (int j = 0; j < k; ++j) {
    float off = (k == 1 ? 0.f : lerpf(-g.spread, g.spread, j / (float)(k - 1))) + rng.sym() * 0.3f * g.spread;
    branchGrow(s, rng, sp, g, q, a + off, len * g.lenRatio * (0.8f + 0.4f * rng.uniform()), rc, depth + 1, tips, thickLight);
  }
  if (depth < g.maxDepth && rng.chance(g.sideProb)) {
    float t = 0.4f + 0.4f * rng.uniform(); int idx = (int)(t * n);
    Vec2 sp0 = pts[idx];
    float side = rng.chance(0.5f) ? 1.f : -1.f;
    branchGrow(s, rng, sp, g, sp0, a + side * (0.7f + rng.uniform() * 0.5f), len * g.lenRatio * 0.7f, r1 * 0.55f, depth + 2, tips, thickLight);
  }
}

struct Lobe { Vec2 p; float r; float tint; };

// Volumetric foliage: lobes drawn in 3 passes (shadow / body / light) back-to-front.
inline void foliage(ISurface& s, Rng& rng, const std::vector<Lobe>& lobes0, Color base, const std::function<Color(const Lobe&)>& tintFn = nullptr, bool speckle = true) {
  std::vector<Lobe> lobes = lobes0;
  std::sort(lobes.begin(), lobes.end(), [](const Lobe& a, const Lobe& b) { return a.p.y < b.p.y; });
  auto col = [&](const Lobe& l) { return tintFn ? tintFn(l) : shiftHSV(base, (l.tint - 0.5f) * 0.05f, 0, (l.tint - 0.5f) * 0.12f); };
  for (auto& l : lobes) disc(s, l.p + Vec2{l.r * 0.1f, l.r * 0.22f}, l.r * 1.02f, shade(col(l), 0.5f), 1.f, 0.85f);
  for (auto& l : lobes) disc(s, l.p, l.r, shade(col(l), 0.88f), 1.f, 0.85f);
  for (auto& l : lobes) {
    disc(s, l.p + Vec2{-l.r * 0.22f, -l.r * 0.3f}, l.r * 0.62f, lighten(col(l), 0.18f), 0.85f, 0.45f);
    disc(s, l.p + Vec2{-l.r * 0.32f, -l.r * 0.42f}, l.r * 0.3f, lighten(col(l), 0.38f), 0.7f, 0.3f);
  }
  if (speckle) {
    for (auto& l : lobes) {
      int n = (int)(l.r * 0.5f);
      for (int i = 0; i < n; ++i) {
        Vec2 o = rng.inDisc() * (l.r * 0.85f);
        bool lit = (o.x + o.y) < 0;
        disc(s, l.p + o, std::max(0.7f, l.r * 0.07f), lit ? lighten(col(l), 0.3f) : shade(col(l), 0.55f), 0.55f, 0.7f);
      }
    }
  }
}

inline std::vector<Lobe> lobesAt(Rng& rng, const std::vector<Tip>& tips, float R, float density, float lift = 0.f) {
  std::vector<Lobe> v;
  for (auto& t : tips) {
    int n = std::max(1, (int)(4 * density));
    for (int i = 0; i < n; ++i) {
      Vec2 o = rng.inDisc() * (R * 0.9f); o.y -= lift * R;
      v.push_back({t.p + o, R * (0.45f + 0.5f * rng.uniform()), rng.uniform()});
    }
  }
  return v;
}

// Leaf stamps scattered over lobes (uses bank leaf masks when available).
inline void leafStamps(ISurface& s, Rng& rng, const std::vector<Lobe>& lobes, const char* mask, float size, Color base, float perLobe, const std::function<Color(const Lobe&)>& tintFn = nullptr) {
  const Mask* m = StampBank::instance().get(mask);
  for (auto& l : lobes) {
    int n = (int)(perLobe * (0.5f + rng.uniform()));
    for (int i = 0; i < n; ++i) {
      Vec2 o = rng.inDisc() * (l.r * 0.9f);
      float ang = rng.uniform() * kTau;
      Color c = tintFn ? tintFn(l) : base;
      c = shiftHSV(c, rng.sym() * 0.03f, 0, rng.sym() * 0.1f + (o.y < 0 ? 0.06f : -0.04f));
      float sz = size * (0.7f + 0.6f * rng.uniform());
      if (m) maskDab(s, m, l.p + o, sz, sz, ang, c, 0.95f, rng.chance(0.5f));
      else leafShape(s, l.p + o, ang, sz * 1.6f, sz * 0.9f, c);
    }
  }
}

}  // namespace nat
}  // namespace ab
