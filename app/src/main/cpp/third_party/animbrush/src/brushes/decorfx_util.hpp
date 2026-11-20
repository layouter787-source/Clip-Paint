// Private helpers of the decorfx module: drawing primitives + ArcBrush (arc-length parametrised brush).
#pragma once
#include <functional>
#include <map>
#include <set>
#include <memory>

#include "../../include/animbrush/animbrush.hpp"

namespace ab {
namespace dfx {

// ---- drawing primitives (all emit dabs / patches to an ISurface) ------------------------------
void disc(ISurface& s, Vec2 p, float r, Color c, float hard = 1.f, float op = 1.f, Blend b = Blend::Normal);
// Round-capped segment, radii r0->r1 (RADIUS, not width). Opacity is compensated for dab overlap.
void seg(ISurface& s, Vec2 a, Vec2 b, float r0, float r1, Color c, float op = 1.f, Blend bl = Blend::Normal, float hard = 1.f);
void poly(ISurface& s, const std::vector<Vec2>& pts, float r0, float r1, Color c, float op = 1.f, Blend bl = Blend::Normal, bool closed = false);
void fillPoly(ISurface& s, const std::vector<Vec2>& pts, Color c, float op = 1.f, Blend bl = Blend::Normal);
void ellipseDab(ISurface& s, Vec2 p, float rx, float ry, float ang, Color c, float hard, float op, Blend b = Blend::Normal);
std::vector<Vec2> circlePts(Vec2 c, float rx, float ry, float rot, int n, float a0 = 0, float a1 = kTau);
void sphere(ISurface& s, Vec2 c, float r, Color base, Vec2 lightDir = {-0.45f, -0.55f}, float spec = 0.9f);
void glow(ISurface& s, Vec2 p, float r, Color c, float op, Blend b = Blend::Add);  // soft halo
void star4(ISurface& s, Vec2 p, float r, Color c, float op = 1.f, Blend b = Blend::Normal, float rot = 0.f);
void fillRectPx(ISurface& s, int x, int y, int w, int h, Color c, float op = 1.f, Blend b = Blend::Normal);
Vec2 local(Vec2 o, Vec2 dir, Vec2 nrm, float u, float v);  // o + dir*u + nrm*v

inline Color shade(Color c, float k) { return {clampf(c.r * k), clampf(c.g * k), clampf(c.b * k), c.a}; }
inline Color tint(Color c, Color t, float k) { return mix(c, t, k); }
Color hsv(float h, float s, float v);
Color paletteColor(int i);  // festive palette

// ---- ArcBrush ------------------------------------------------------------------------------------
struct PSpec { const char* key; const char* label; float def, mn, mx; };

struct Ctx {
  ISurface* s = nullptr;
  const StrokeParams* sp = nullptr;
  Rng* rng = nullptr;
  const Brush* self = nullptr;
  Vec2 p, prev, dir{1, 0}, nrm{0, 1}, first;
  float arc = 0, pr = 1, speed = 0, size = 10, maxDist = 0, step = 1;
  Vec2 bbMin, bbMax;
  int idx = 0, frame = 0;
  bool isFirst = false;
  const PathTracker* path = nullptr;
  std::vector<float>* st = nullptr;
  float P(const char* key) const { return self->param(key); }
  float& S(size_t i) const { if (st->size() <= i) st->resize(i + 1, 0.f); return (*st)[i]; }
  uint32_t seed(int k = 0) const { return hash32(sp->seed * 2654435761u + (uint32_t)idx * 40503u + (uint32_t)k * 977u); }
  float rnd(int k = 0) const { return hashf(seed(k)); }  // deterministic per element
};

struct ArcSpec {
  std::function<float(float size)> spacing = [](float s) { return s; };
  std::function<void(Ctx&)> emit;
  std::function<void(Ctx&)> begin;   // before first emit
  std::function<void(Ctx&)> finish;  // at end(); ctx.p = last point, ctx.arc = length, ctx.path valid
  bool emitAtBegin = false;          // emit immediately at the first sample (stamps)
};

class ArcBrush : public Brush {
 public:
  ArcBrush(BrushInfo info, ArcSpec spec, const std::vector<PSpec>& params);
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override;
  void move(ISurface& s, const InputSample& in) override;
  void end(ISurface& s) override;

 private:
  void fillCtx(Ctx& c, const PathPoint& q);
  ArcSpec spec_;
  StrokeParams sp_;
  Rng rng_;
  PathTracker path_;
  std::vector<float> st_;
  float nextS_ = 0;
  Vec2 prev_, first_, bbMin_, bbMax_;
  float maxDist_ = 0;
  int idx_ = 0;
  ISurface* surf_ = nullptr;
  bool started_ = false;
};

BrushInfo mkInfo(const char* id, const char* name, const char* cat, const char* desc, float size, Color c1, Color c2,
                 const char* demo = "wave", const char* bg = "");
// `maker` builds a fresh ArcSpec per brush instance (so lambdas can own per-stroke state).
void addArc(BrushLibrary& lib, BrushInfo info, std::function<ArcSpec()> maker, std::vector<PSpec> params = {});
// Point on the stroke at arc length s, offset sideways by `off` px.
Vec2 P(const Ctx& c, float s, float off = 0.f);
// Stroke direction at arc length s.
Vec2 D(const Ctx& c, float s);

void registerDecor(BrushLibrary& lib);
void registerManga(BrushLibrary& lib);
void registerAnim(BrushLibrary& lib);
void registerPixel(BrushLibrary& lib);

}  // namespace dfx
}  // namespace ab
