#include "../include/animbrush/brush.hpp"

namespace ab {

const PathPoint& PathTracker::add(const InputSample& in, float minDist) {
  t_ += in.dt;
  if (!pts_.empty()) {
    PathPoint& b = pts_.back();
    float d = dist(b.p, {in.x, in.y});
    if (d < minDist) {  // merge: keep latest pressure
      b.pressure = in.pressure; b.t = t_;
      return b;
    }
    PathPoint n;
    n.p = {in.x, in.y}; n.pressure = in.pressure; n.t = t_; n.d = b.d + d;
    n.tiltX = in.tiltX; n.tiltY = in.tiltY;
    Vec2 seg = (n.p - b.p) / d;
    n.dir = pts_.size() > 1 ? (b.dir * 0.55f + seg * 0.45f).norm() : seg;
    float sp = in.dt > 1e-5f ? d / in.dt : b.speed;
    n.speed = b.speed + (sp - b.speed) * 0.35f;
    if (pts_.size() == 1) b.dir = seg;
    pts_.push_back(n);
  } else {
    PathPoint n; n.p = {in.x, in.y}; n.pressure = in.pressure; n.t = t_;
    n.tiltX = in.tiltX; n.tiltY = in.tiltY;
    pts_.push_back(n);
  }
  return pts_.back();
}

PathPoint PathTracker::at(float s) const {
  if (pts_.empty()) return PathPoint();
  if (s <= 0 || pts_.size() == 1) return pts_.front();
  if (s >= pts_.back().d) return pts_.back();
  size_t lo = 0, hi = pts_.size() - 1;
  while (hi - lo > 1) { size_t m = (lo + hi) / 2; if (pts_[m].d <= s) lo = m; else hi = m; }
  const PathPoint &a = pts_[lo], &b = pts_[hi];
  float t = (s - a.d) / std::max(1e-6f, b.d - a.d);
  PathPoint o;
  o.p = lerp(a.p, b.p, t); o.pressure = lerpf(a.pressure, b.pressure, t); o.t = lerpf(a.t, b.t, t); o.d = s;
  o.dir = lerp(a.dir, b.dir, t).norm(); o.speed = lerpf(a.speed, b.speed, t);
  o.tiltX = lerpf(a.tiltX, b.tiltX, t); o.tiltY = lerpf(a.tiltY, b.tiltY, t);
  return o;
}

std::vector<PathPoint> PathTracker::resample(float spacing) const {
  std::vector<PathPoint> out;
  if (pts_.empty()) return out;
  spacing = std::max(0.25f, spacing);
  for (float s = 0; s <= length() + 1e-3f; s += spacing) out.push_back(at(s));
  return out;
}

void dabRound(ISurface& s, Vec2 p, float radius, const Dab& proto) {
  Dab d = proto; d.x = p.x; d.y = p.y; d.radius = radius; s.drawDab(d);
}

void dabLine(ISurface& s, Vec2 a, Vec2 b, float r0, float r1, const Dab& proto, float spacingFrac, float* carry) {
  float L = dist(a, b);
  float s0 = 0;
  if (carry) {
    float sp0 = std::max(0.5f, spacingFrac * std::max(0.3f, r0));
    s0 = sp0 - *carry;
    if (s0 < 0) s0 = 0;
  }
  if (L < 1e-4f) {
    if (!carry) dabRound(s, a, r0, proto);
    return;
  }
  float pos = s0, last = carry ? -1.f : 0.f;
  bool any = false;
  while (pos <= L + 1e-4f) {
    float t = pos / L;
    float r = lerpf(r0, r1, t);
    dabRound(s, lerp(a, b, t), r, proto);
    last = pos; any = true;
    pos += std::max(0.5f, spacingFrac * std::max(0.3f, r));
  }
  if (carry) *carry = any ? (L - last) : (*carry + L);
}

void dabPolyline(ISurface& s, const std::vector<PolyPt>& pts, const Dab& proto, float spacingFrac) {
  if (pts.empty()) return;
  if (pts.size() == 1) { Dab d = proto; d.opacity *= pts[0].a; dabRound(s, pts[0].p, pts[0].r, d); return; }
  { Dab d0 = proto; d0.opacity = proto.opacity * pts[0].a; dabRound(s, pts[0].p, pts[0].r, d0); }
  float carry = 0;
  for (size_t i = 0; i + 1 < pts.size(); ++i) {
    Dab d = proto; d.opacity = proto.opacity * lerpf(pts[i].a, pts[i + 1].a, 0.5f);
    dabLine(s, pts[i].p, pts[i + 1].p, pts[i].r, pts[i + 1].r, d, spacingFrac, &carry);
  }
}

}  // namespace ab
