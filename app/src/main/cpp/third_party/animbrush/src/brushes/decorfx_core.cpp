#include "decorfx_util.hpp"

namespace ab {
namespace dfx {

void disc(ISurface& s, Vec2 p, float r, Color c, float hard, float op, Blend b) {
  Dab d; d.x = p.x; d.y = p.y; d.radius = std::max(0.2f, r); d.color = c; d.hardness = hard; d.opacity = op * c.a; d.blend = b;
  s.drawDab(d);
}

void seg(ISurface& s, Vec2 a, Vec2 b, float r0, float r1, Color c, float op, Blend bl, float hard) {
  Dab d; d.color = c; d.hardness = hard; d.blend = bl;
  d.opacity = (op >= 0.999f ? 1.f : 1.f - std::pow(1.f - op, 0.2f)) * c.a;
  dabLine(s, a, b, std::max(0.2f, r0), std::max(0.2f, r1), d, 0.25f);
  if (dist(a, b) < 0.01f) dabRound(s, a, std::max(0.2f, r0), d);
}

void poly(ISurface& s, const std::vector<Vec2>& pts, float r0, float r1, Color c, float op, Blend bl, bool closed) {
  size_t n = pts.size();
  if (n < 2) return;
  size_t segs = closed ? n : n - 1;
  for (size_t i = 0; i < segs; ++i) {
    float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
    seg(s, pts[i], pts[(i + 1) % n], lerpf(r0, r1, t0), lerpf(r0, r1, t1), c, op, bl);
  }
}

void fillPoly(ISurface& s, const std::vector<Vec2>& pts, Color c, float op, Blend bl) {
  if (pts.size() < 3) return;
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (auto& p : pts) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
  int ix0 = std::max(0, (int)std::floor(x0) - 1), iy0 = std::max(0, (int)std::floor(y0) - 1);
  int ix1 = std::min(s.width(), (int)std::ceil(x1) + 1), iy1 = std::min(s.height(), (int)std::ceil(y1) + 1);
  if (ix1 <= ix0 || iy1 <= iy0) return;
  Patch pt(ix0, iy0, ix1 - ix0, iy1 - iy0);
  const int N = 3;
  size_t n = pts.size();
  for (int y = iy0; y < iy1; ++y)
    for (int x = ix0; x < ix1; ++x) {
      int hit = 0;
      for (int sy = 0; sy < N; ++sy)
        for (int sx = 0; sx < N; ++sx) {
          float px = x + (sx + 0.5f) / N, py = y + (sy + 0.5f) / N;
          bool in = false;
          for (size_t i = 0, j = n - 1; i < n; j = i++) {
            if (((pts[i].y > py) != (pts[j].y > py)) &&
                (px < (pts[j].x - pts[i].x) * (py - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x)) in = !in;
          }
          hit += in;
        }
      if (!hit) continue;
      float a = (float)hit / (N * N) * op * c.a;
      float* q = pt.at(x, y);
      q[0] = c.r * a; q[1] = c.g * a; q[2] = c.b * a; q[3] = a;
    }
  s.compositePatch(pt, bl, 1.f);
}

void ellipseDab(ISurface& s, Vec2 p, float rx, float ry, float ang, Color c, float hard, float op, Blend b) {
  Dab d; d.x = p.x; d.y = p.y; d.radius = std::max(0.2f, ry); d.aspect = std::max(0.05f, rx / std::max(0.2f, ry)); d.angle = ang;
  d.color = c; d.hardness = hard; d.opacity = op * c.a; d.blend = b;
  s.drawDab(d);
}

std::vector<Vec2> circlePts(Vec2 c, float rx, float ry, float rot, int n, float a0, float a1) {
  std::vector<Vec2> v;
  for (int i = 0; i <= n; ++i) {
    float a = lerpf(a0, a1, (float)i / n);
    v.push_back(c + rotate({std::cos(a) * rx, std::sin(a) * ry}, rot));
  }
  return v;
}

void sphere(ISurface& s, Vec2 c, float r, Color base, Vec2 ld, float spec) {
  disc(s, c, r, shade(base, 0.5f), 1.f);
  disc(s, c + ld * (r * 0.12f), r * 0.86f, base, 0.55f);
  disc(s, c + ld * (r * 0.42f), r * 0.3f, tint(base, Color{1, 1, 1, 1}, 0.7f), 0.2f, 0.8f);
  disc(s, c + ld * (r * 0.5f), r * 0.14f, Color{1, 1, 1, 1}, 0.4f, spec);
}

void glow(ISurface& s, Vec2 p, float r, Color c, float op, Blend b) { disc(s, p, r, c, 0.f, op, b); }

void star4(ISurface& s, Vec2 p, float r, Color c, float op, Blend b, float rot) {
  std::vector<Vec2> v;
  for (int i = 0; i < 8; ++i) v.push_back(p + fromAngle(rot + i * kPi / 4.f, (i & 1) ? r * 0.16f : r));
  fillPoly(s, v, c, op, b);
}

void fillRectPx(ISurface& s, int x, int y, int w, int h, Color c, float op, Blend b) {
  int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(s.width(), x + w), y1 = std::min(s.height(), y + h);
  if (x1 <= x0 || y1 <= y0) return;
  Patch pt(x0, y0, x1 - x0, y1 - y0);
  float a = op * c.a;
  for (int j = y0; j < y1; ++j)
    for (int i = x0; i < x1; ++i) { float* q = pt.at(i, j); q[0] = c.r * a; q[1] = c.g * a; q[2] = c.b * a; q[3] = a; }
  s.compositePatch(pt, b, 1.f);
}

Vec2 local(Vec2 o, Vec2 dir, Vec2 nrm, float u, float v) { return o + dir * u + nrm * v; }

Color hsv(float h, float s, float v) { Color c; hsv2rgb(h, s, v, c.r, c.g, c.b); c.a = 1; return c; }
Color paletteColor(int i) {
  static const uint32_t pal[8] = {0xE63946, 0xF4A261, 0xF9D342, 0x4CAF50, 0x2A9DF4, 0x6C5CE7, 0xE84393, 0x1ABC9C};
  return Color::hex(pal[((i % 8) + 8) % 8]);
}

Vec2 P(const Ctx& c, float s, float off) {
  PathPoint q = c.path->at(std::max(0.f, s));
  return q.p + q.dir.perp() * off;
}
Vec2 D(const Ctx& c, float s) { return c.path->at(std::max(0.f, s)).dir; }

// ---- ArcBrush ----------------------------------------------------------------------------------
ArcBrush::ArcBrush(BrushInfo info, ArcSpec spec, const std::vector<PSpec>& params) : spec_(std::move(spec)) {
  info_ = std::move(info);
  for (auto& p : params) addParam(p.key, p.label, p.def, p.mn, p.mx);
}

void ArcBrush::fillCtx(Ctx& c, const PathPoint& q) {
  c.s = surf_; c.sp = &sp_; c.rng = &rng_; c.self = this;
  c.p = q.p; c.prev = prev_; c.dir = q.dir.len2() > 1e-6f ? q.dir : Vec2{1, 0}; c.nrm = c.dir.perp(); c.first = first_;
  c.arc = q.d; c.pr = q.pressure; c.speed = q.speed; c.size = sp_.size; c.maxDist = maxDist_;
  c.bbMin = bbMin_; c.bbMax = bbMax_; c.idx = idx_; c.frame = sp_.frame; c.path = &path_; c.st = &st_;
  c.step = std::max(0.5f, spec_.spacing(sp_.size));
}

void ArcBrush::begin(ISurface& s, const StrokeParams& sp, const InputSample& in) {
  surf_ = &s; sp_ = sp; rng_.reseed(sp.seed); path_.reset(); st_.clear(); idx_ = 0;
  path_.add(in, 0.f);
  first_ = prev_ = {in.x, in.y}; bbMin_ = bbMax_ = first_; maxDist_ = 0; started_ = true;
  Ctx c; fillCtx(c, path_.at(0));
  c.isFirst = true;
  if (spec_.begin) spec_.begin(c);
  nextS_ = c.step;
  if (spec_.emitAtBegin && spec_.emit) { spec_.emit(c); prev_ = c.p; ++idx_; }
}

void ArcBrush::move(ISurface& s, const InputSample& in) {
  if (!started_) return;
  surf_ = &s;
  path_.add(in, 0.3f);
  Vec2 p = path_.back().p;
  bbMin_ = {std::min(bbMin_.x, p.x), std::min(bbMin_.y, p.y)};
  bbMax_ = {std::max(bbMax_.x, p.x), std::max(bbMax_.y, p.y)};
  maxDist_ = std::max(maxDist_, dist(first_, p));
  int guard = 0;
  while (nextS_ <= path_.length() && guard++ < 4000) {
    Ctx c; fillCtx(c, path_.at(nextS_));
    if (spec_.emit) spec_.emit(c);
    prev_ = c.p; ++idx_;
    nextS_ += c.step;
  }
}

void ArcBrush::end(ISurface& s) {
  if (!started_) return;
  surf_ = &s;
  if (spec_.finish) {
    Ctx c; fillCtx(c, path_.back());
    c.arc = path_.length();
    spec_.finish(c);
  }
  started_ = false;
}

BrushInfo mkInfo(const char* id, const char* name, const char* cat, const char* desc, float size, Color c1, Color c2,
                 const char* demo, const char* bg) {
  BrushInfo i; i.id = id; i.name = name; i.category = cat; i.description = desc; i.defaultSize = size;
  i.minSize = 1; i.maxSize = std::max(60.f, size * 5); i.suggestedColor = c1; i.suggestedColor2 = c2; i.demo = demo; i.demoBg = bg;
  i.needsPressure = true;
  return i;
}

void addArc(BrushLibrary& lib, BrushInfo info, std::function<ArcSpec()> maker, std::vector<PSpec> params) {
  BrushInfo copy = info;
  lib.add(std::move(info), [copy, maker, params]() -> std::unique_ptr<Brush> {
    return std::make_unique<ArcBrush>(copy, maker(), params);
  });
}

}  // namespace dfx

void registerDecorFxBrushes(BrushLibrary& lib) {
  dfx::registerDecor(lib);
  dfx::registerManga(lib);
  dfx::registerAnim(lib);
  dfx::registerPixel(lib);
}
}  // namespace ab
