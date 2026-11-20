#include "nature_tree_common.hpp"

namespace ab {
namespace nat {
namespace {

const Color kBark{0.36f, 0.25f, 0.15f, 1};
const Color kLeaf{0.27f, 0.52f, 0.17f, 1};

float trunkR(const NCtx& c, float k = 0.6f) { return c.sp.size * k; }

std::function<float(float)> taperFn(float r0, float r1, float flare = 0.5f) {
  return [=](float t) { return lerpf(r0, r1, t) * (1.f + flare * std::pow(std::max(0.f, 1.f - t * 7.f), 2.f)); };
}

Vec2 topDirOf(const std::vector<Vec2>& v) {
  size_t n = v.size(); Vec2 a = v[n > 4 ? n - 4 : 0];
  return (v.back() - a).norm();
}

// ---- broadleaf family (oak / autumn / cherry / birch share the growth engine) ---------------------------
struct Broad { BCfg g; float leafR; float lobeDensity; int style; };  // style 0 oak 1 cherry 2 autumn 3 birch

void broadleaf(NCtx& c, const Broad& B) {
  auto tr = trunkPoints(c.path, c.sp);
  float r0 = trunkR(c, B.style == 3 ? 0.4f : 0.62f);
  Color bark = B.g.bark;
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.55f), bark, B.style == 3 ? 0.25f : 1.f);
  if (B.style == 3) {  // birch lenticels
    for (size_t i = 3; i + 3 < tr.size(); ++i) if (c.rng.chance(0.28f)) {
      float t = i / (float)tr.size(); float r = lerpf(r0, r0 * 0.55f, t);
      Vec2 n = pathNormal(tr, i); Vec2 a = tr[i] + n * (c.rng.sym() * r * 0.6f);
      line(c.s, a - n * (r * 0.3f), a + n * (r * 0.3f), 0.7f + c.rng.uniform() * 0.8f, Color{0.1f, 0.1f, 0.1f, 1}, 0.85f);
    }
  }
  Vec2 top = tr.back(); float ang = topDirOf(tr).angle();
  BCfg g = B.g; float br = c.P("galhos");
  g.maxDepth = std::max(2, (int)std::round(g.maxDepth * br));
  float len = std::max(c.sp.size * 3.f, polyLen(tr) * 0.55f);
  std::vector<Tip> tips;
  float rTop = r0 * 0.55f;
  branchGrow(c.s, c.rng, c.sp, g, top, ang, len, rTop, 0, tips);
  // also lower side limbs on the trunk
  for (int i = 0; i < 2 + (int)(2 * br); ++i) {
    size_t idx = (size_t)(tr.size() * (0.55f + 0.35f * c.rng.uniform())); idx = std::min(idx, tr.size() - 1);
    float side = (i & 1) ? 1.f : -1.f;
    branchGrow(c.s, c.rng, c.sp, g, tr[idx], ang + side * (0.8f + c.rng.uniform() * 0.4f), len * 0.6f, rTop * 0.6f, 1, tips);
  }
  float R = c.sp.size * B.leafR * (0.8f + 0.4f * c.P("folhas"));
  float dens = B.lobeDensity * c.P("folhas");
  if (dens <= 0.02f) return;
  auto lobes = lobesAt(c.rng, tips, R, dens, 0.1f);
  Color base = c.sp.color2;
  float minY = 1e9f, maxY = -1e9f; for (auto& l : lobes) { minY = std::min(minY, l.p.y); maxY = std::max(maxY, l.p.y); }
  std::function<Color(const Lobe&)> tint;
  if (B.style == 2) {
    tint = [&](const Lobe& l) {
      float h = clampf((l.p.y - minY) / std::max(1.f, maxY - minY) * 0.8f + (l.tint - 0.5f) * 0.5f);
      Color gcol{0.35f, 0.5f, 0.12f, 1}, yel{0.9f, 0.75f, 0.12f, 1}, org{0.9f, 0.45f, 0.1f, 1}, red{0.7f, 0.15f, 0.08f, 1};
      if (h < 0.33f) return mix(gcol, yel, h / 0.33f);
      if (h < 0.66f) return mix(yel, org, (h - 0.33f) / 0.33f);
      return mix(org, red, (h - 0.66f) / 0.34f);
    };
  } else if (B.style == 1) {
    tint = [&](const Lobe& l) { return mix(base, Color{1.f, 0.82f, 0.88f, 1}, l.tint * 0.6f); };
  }
  if (B.style == 3) { foliage(c.s, c.rng, lobes, base, tint, false); }
  else foliage(c.s, c.rng, lobes, base, tint, true);
  if (B.style == 0) leafStamps(c.s, c.rng, lobes, "stamp_leaf1", c.sp.size * 0.22f, base, 2.5f);
  if (B.style == 2) leafStamps(c.s, c.rng, lobes, "stamp_leaf3", c.sp.size * 0.2f, base, 2.f, tint);
  if (B.style == 1) {  // falling petals + highlights
    for (int i = 0; i < 40; ++i) {
      Vec2 p = top + Vec2{c.rng.sym() * R * 4.f, c.rng.uniform() * R * 5.f + R};
      ell(c.s, p, 2.2f, 1.2f, c.rng.uniform() * kTau, mix(Color{1.f, 0.78f, 0.86f, 1}, Color{1, 1, 1, 1}, c.rng.uniform() * 0.5f), 0.85f, 0.8f);
    }
  }
  if (B.style == 3) {  // thin hanging twigs
    for (auto& t : tips) if (c.rng.chance(0.5f)) {
      std::vector<Vec2> tw = bez(t.p, t.p + Vec2{c.rng.sym() * 6.f, 10.f}, t.p + Vec2{c.rng.sym() * 10.f, 26.f + c.rng.uniform() * 14.f}, 8);
      taper(c.s, tw, 0.8f, 0.3f, Color{0.35f, 0.25f, 0.2f, 1}, 0.9f);
      for (size_t i = 2; i < tw.size(); ++i) ell(c.s, tw[i], 3.f, 1.6f, c.rng.uniform() * kTau, jitter(base, c.rng, 0.04f, 0.1f, 0.1f), 0.95f);
    }
  }
}

// ---- conifer -----------------------------------------------------------------------------------------------
void coniferTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 11.f);
  Vec2 base = tr.front(), top = tr.back();
  float H = dist(base, top); Vec2 dir = (top - base).norm(), nrm = dir.perp();
  float r0 = trunkR(c, 0.28f);
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.3f, 0.2f), darken(c.sp.color, 0.1f), 0.5f);
  int tiers = 6 + (int)(5 * c.P("andares"));
  float Wmax = H * 0.27f;
  Color col = c.sp.color2;
  for (int i = 0; i < tiers; ++i) {
    float t = 0.1f + 0.88f * (i / (float)(tiers - 1));
    float w = Wmax * std::pow(1.f - t, 0.85f) * (0.85f + 0.3f * c.rng.uniform()) + 3.f;
    float h = H / tiers * 2.1f;
    Vec2 ctr = base + dir * (H * t);
    Vec2 apex = ctr + dir * h;
    std::vector<Vec2> poly{apex};
    int nz = 6;
    for (int k = 0; k <= nz; ++k) {  // right side going down to bottom edge jagged
      float u = k / (float)nz; float side = 1.f;
      poly.push_back(ctr + nrm * (side * w * u) - dir * (h * 0.18f * u * u) - dir * ((k & 1) ? h * 0.12f : 0.f));
    }
    for (int k = nz; k >= 0; --k) {
      float u = k / (float)nz;
      poly.push_back(ctr - nrm * (w * u) - dir * (h * 0.18f * u * u) - dir * ((k & 1) ? h * 0.12f : 0.f));
    }
    // order: apex, right..., left... — close
    Vec2 cc = ctr;
    float tt = t;
    fillPoly(c.s, poly, [=](float x, float y) {
      Vec2 q = Vec2{x, y} - cc;
      float side = clampf(dot(q, nrm) / std::max(1.f, w), -1.f, 1.f);
      float down = clampf(-dot(q, dir) / std::max(1.f, h * 0.4f), 0.f, 1.f);
      float k = 0.9f - 0.22f * side - 0.25f * down + 0.1f * (tt - 0.5f);
      return shade(col, k);
    });
    // drooping needle sprays along the lower edge
    int ns = (int)(w * 0.9f);
    for (int k = 0; k < ns; ++k) {
      float u = c.rng.sym();
      Vec2 p = ctr + nrm * (u * w) - dir * (h * 0.18f * u * u);
      Vec2 q = p - dir * (3.f + c.rng.uniform() * 5.f) + nrm * (u * 2.f);
      line(c.s, p, q, 0.6f, shade(col, 0.55f + 0.4f * c.rng.uniform()), 0.8f);
    }
  }
  disc(c.s, top + dir * 3.f, 1.4f, shade(col, 0.8f), 1.f);
  if (c.P("neve") > 0.5f) {  // snow caps
    for (int i = 0; i < 40; ++i) {
      float t = 0.2f + 0.75f * c.rng.uniform();
      float w = Wmax * (1.f - t) * 0.9f;
      Vec2 p = base + dir * (H * t) + nrm * (c.rng.sym() * w * 0.8f);
      ell(c.s, p, 3.5f, 1.8f, nrm.angle() + kPi * 0.5f, Color{0.96f, 0.98f, 1.f, 1}, 0.9f, 0.7f);
    }
  }
}

// ---- palm ------------------------------------------------------------------------------------------------------
void palmTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 9.f);
  float r0 = trunkR(c, 0.4f);
  Color bark{0.52f, 0.38f, 0.24f, 1};
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.7f, 0.6f), bark, 0.2f, 0.12f);
  for (size_t i = 2; i + 1 < tr.size(); i += 2) {  // growth rings
    float t = i / (float)tr.size(); float r = lerpf(r0, r0 * 0.7f, t);
    Vec2 n = pathNormal(tr, i);
    line(c.s, tr[i] - n * r * 0.95f + Vec2{0, 1.f}, tr[i] + n * r * 0.95f + Vec2{0, 2.f}, 0.8f, darken(bark, 0.45f), 0.6f);
  }
  Vec2 top = tr.back();
  int nf = 11; float L = c.sp.size * 6.5f * (0.8f + 0.4f * c.P("folhas"));
  for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < nf; ++i) {
      float a = -kPi + (kPi * (i + 0.5f) / nf) + c.rng.sym() * 0.15f;
      if ((pass == 0) != (std::sin(a) > -0.3f)) { if (pass == 0) continue; }
      if (pass == 1 && std::sin(a) > -0.3f) continue;
      float Lf = L * (0.8f + 0.35f * c.rng.uniform());
      float sway = windSway(c.sp, i, 0.25f);
      Vec2 dirv = fromAngle(a + sway);
      Vec2 end = top + dirv * Lf + Vec2{0, Lf * 0.38f};
      Vec2 ctrl = top + dirv * (Lf * 0.65f) + Vec2{0, -Lf * 0.3f};
      auto rach = bez(top, ctrl, end, 22);
      Color lc = jitter(c.sp.color2, c.rng, 0.02f, 0.05f, 0.08f);
      taper(c.s, rach, 1.8f, 0.6f, shade(lc, 0.55f), 1.f);
      for (size_t k = 2; k < rach.size(); ++k) {
        float t = k / (float)(rach.size() - 1);
        Vec2 tg = (rach[std::min(rach.size() - 1, k + 1)] - rach[k - 1]).norm();
        float ll = Lf * 0.2f * std::pow(std::sin(kPi * std::min(1.f, t * 1.05f)), 0.6f) * (1.f - 0.4f * t) + 2.f;
        for (int sd = -1; sd <= 1; sd += 2) {
          Vec2 d2 = rotate(tg, sd * (1.2f + 0.2f * t)) + Vec2{0, 0.55f};
          d2 = d2.norm();
          Vec2 q = rach[k] + d2 * ll;
          line(c.s, rach[k], q, 1.1f, shade(lc, sd < 0 ? 1.0f : 0.8f), 0.95f);
        }
      }
    }
  for (int i = 0; i < 3; ++i) disc(c.s, top + Vec2{(i - 1) * 5.f, 5.f + (i & 1) * 2.f}, 4.5f, Color{0.4f, 0.28f, 0.1f, 1}, 1.f);
}

// ---- willow ------------------------------------------------------------------------------------------------------
void willowTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 8.f);
  float r0 = trunkR(c, 0.6f);
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.5f), c.sp.color, 1.f);
  BCfg g; g.bark = c.sp.color; g.kids = 2; g.spread = 0.7f; g.droop = 0.9f; g.photo = 0.2f; g.maxDepth = 3; g.lenRatio = 0.75f; g.minR = 0.6f;
  std::vector<Tip> tips;
  Vec2 top = tr.back();
  branchGrow(c.s, c.rng, c.sp, g, top, topDirOf(tr).angle(), c.sp.size * 6.f, r0 * 0.5f, 0, tips);
  Color col = c.sp.color2;
  int strands = (int)(60 * c.P("folhas")) + 20;
  for (int i = 0; i < strands; ++i) {
    const Tip& t = tips[c.rng.irange(0, (int)tips.size() - 1)];
    Vec2 p = t.p + Vec2{c.rng.sym() * 14.f, c.rng.sym() * 6.f};
    float L = c.sp.size * (4.f + 5.f * c.rng.uniform());
    std::vector<Vec2> st; float ph = c.rng.uniform() * 6.f;
    for (int k = 0; k <= 14; ++k) { float u = k / 14.f; st.push_back(p + Vec2{std::sin(u * 3.f + ph) * 3.f * u + windSway(c.sp, ph, 12.f) * u * u, L * u}); }
    Color sc = jitter(col, c.rng, 0.03f, 0.1f, 0.1f);
    taper(c.s, st, 0.9f, 0.4f, shade(sc, 0.8f), 0.9f);
    for (size_t k = 3; k < st.size(); ++k) if (c.rng.chance(0.8f)) ell(c.s, st[k] + Vec2{c.rng.sym() * 2, 0}, 3.2f, 1.1f, kPi * 0.5f + c.rng.sym() * 0.6f, lighten(sc, c.rng.uniform() * 0.25f), 0.95f);
  }
}

// ---- bare winter tree ----------------------------------------------------------------------------------------------
void bareTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp);
  float r0 = trunkR(c, 0.6f);
  Color bark = darken(c.sp.color, 0.2f);
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.5f), bark, 1.f, 0.1f);
  BCfg g; g.bark = bark; g.kids = 2; g.maxDepth = 6; g.spread = 0.5f; g.photo = 0.7f; g.minR = 0.35f; g.lenRatio = 0.74f; g.jitter = 0.22f; g.sideProb = 0.5f; g.taperEnd = 0.7f;
  std::vector<Tip> tips; float br = c.P("galhos");
  g.maxDepth = 4 + (int)(2 * br);
  branchGrow(c.s, c.rng, c.sp, g, tr.back(), topDirOf(tr).angle(), c.sp.size * 6.5f, r0 * 0.55f, 0, tips);
  for (int i = 0; i < 3; ++i) {
    size_t idx = (size_t)(tr.size() * (0.5f + 0.4f * c.rng.uniform())); idx = std::min(idx, tr.size() - 1);
    branchGrow(c.s, c.rng, c.sp, g, tr[idx], topDirOf(tr).angle() + ((i & 1) ? 1.f : -1.f) * (0.9f + c.rng.uniform() * 0.3f), c.sp.size * 4.f, r0 * 0.3f, 2, tips);
  }
  if (c.P("neve") > 0.5f) for (auto& t : tips) if (c.rng.chance(0.5f)) ell(c.s, t.p + Vec2{0, -1}, 3.f, 1.2f, 0, Color{0.95f, 0.97f, 1, 1}, 0.9f);
}

// ---- bonsai ----------------------------------------------------------------------------------------------------------
void bonsaiTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 5.f);
  float r0 = trunkR(c, 0.9f);
  // add S wobble (wired trunk)
  std::vector<Vec2> w = tr;
  for (size_t i = 0; i < w.size(); ++i) { float t = i / (float)w.size(); w[i] += pathNormal(tr, i) * (std::sin(t * 9.f) * c.sp.size * 0.7f * t); }
  drawTrunk(c.s, c.rng, w, taperFn(r0, r0 * 0.35f, 0.9f), c.sp.color, 1.f);
  Color base = c.sp.color2;
  float R = c.sp.size * (2.2f + 0.6f * c.P("folhas"));
  struct Pad { Vec2 p; float rr; };
  std::vector<Pad> pads;
  float ts[4] = {0.45f, 0.62f, 0.8f, 1.0f};
  for (int i = 0; i < 4; ++i) {
    size_t idx = std::min(w.size() - 1, (size_t)(ts[i] * (w.size() - 1)));
    float side = (i & 1) ? 1.f : -1.f;
    Vec2 n = pathNormal(w, idx);
    Vec2 end = i == 3 ? w.back() + Vec2{0, -R * 0.2f} : w[idx] + n * (side * R * 1.5f) + Vec2{0, -R * 0.2f};
    if (i < 3) { auto br = bez(w[idx], w[idx] + n * (side * R * 0.7f) + Vec2{0, R * 0.2f}, end, 10); taper(c.s, br, r0 * 0.3f, r0 * 0.15f, c.sp.color, 1.f); }
    pads.push_back({end + Vec2{0, -R * 0.15f}, R * (i == 3 ? 0.95f : 0.8f)});
  }
  for (auto& p : pads) {   // flat cloud pads, 3 layers
    ell(c.s, p.p + Vec2{0, p.rr * 0.18f}, p.rr * 1.1f, p.rr * 0.42f, 0, shade(base, 0.5f), 1.f, 0.85f);
    ell(c.s, p.p, p.rr, p.rr * 0.38f, 0, shade(base, 0.85f), 1.f, 0.85f);
    ell(c.s, p.p + Vec2{-p.rr * 0.1f, -p.rr * 0.1f}, p.rr * 0.8f, p.rr * 0.24f, 0, lighten(base, 0.2f), 0.9f, 0.5f);
    for (int k = 0; k < 24; ++k) { Vec2 o{c.rng.sym() * p.rr, c.rng.sym() * p.rr * 0.3f}; if ((o.x / p.rr) * (o.x / p.rr) + sqr(o.y / (p.rr * 0.35f)) > 1.f) continue;
      disc(c.s, p.p + o, 1.5f, c.rng.chance(0.5f) ? lighten(base, 0.35f) : shade(base, 0.5f), 0.7f); }
  }
}

// ---- acacia (umbrella) ---------------------------------------------------------------------------------------------------
void acaciaTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 8.f);
  float r0 = trunkR(c, 0.45f);
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.6f), c.sp.color, 1.f);
  BCfg g; g.bark = c.sp.color; g.kids = 3; g.spread = 0.9f; g.photo = 0.05f; g.maxDepth = 2; g.lenRatio = 0.8f; g.jitter = 0.1f; g.sideProb = 0.f;
  std::vector<Tip> tips;
  branchGrow(c.s, c.rng, c.sp, g, tr.back(), topDirOf(tr).angle(), c.sp.size * 4.f, r0 * 0.6f, 0, tips);
  Color base = c.sp.color2;
  float R = c.sp.size * 2.4f * (0.8f + 0.4f * c.P("folhas"));
  std::sort(tips.begin(), tips.end(), [](const Tip& a, const Tip& b) { return a.p.y < b.p.y; });
  for (int pass = 0; pass < 3; ++pass)
    for (auto& t : tips) {
      float kk = pass == 0 ? 0.5f : (pass == 1 ? 0.85f : 1.15f);
      Vec2 o = pass == 0 ? Vec2{0, R * 0.22f} : (pass == 1 ? Vec2{0, 0} : Vec2{-R * 0.1f, -R * 0.2f});
      float sc = pass == 2 ? 0.75f : 1.f;
      Color cc = pass == 2 ? lighten(base, 0.2f) : shade(base, kk);
      ell(c.s, t.p + o, R * 1.5f * sc, R * 0.42f * sc, 0, cc, pass == 2 ? 0.85f : 1.f, pass == 2 ? 0.5f : 0.8f);
    }
}

// ---- baobab -----------------------------------------------------------------------------------------------------------------
void baobabTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 6.f);
  float r0 = trunkR(c, 1.7f);
  auto rf = [=](float t) { return r0 * (t < 0.25f ? lerpf(1.25f, 1.0f, t / 0.25f) : lerpf(1.0f, 0.62f, std::pow((t - 0.25f) / 0.75f, 0.8f))); };
  drawTrunk(c.s, c.rng, tr, rf, Color{0.45f, 0.38f, 0.3f, 1}, 1.4f, 0.12f);
  BCfg g; g.bark = Color{0.45f, 0.38f, 0.3f, 1}; g.kids = 2; g.spread = 0.6f; g.maxDepth = 2; g.lenRatio = 0.7f; g.photo = 0.5f; g.taperEnd = 0.75f; g.sideProb = 0.f; g.jitter = 0.08f;
  std::vector<Tip> tips; Vec2 top = tr.back();
  for (int i = 0; i < 5; ++i) branchGrow(c.s, c.rng, c.sp, g, top + Vec2{(i - 2) * r0 * 0.35f, 0}, topDirOf(tr).angle() + (i - 2) * 0.4f, c.sp.size * 3.f, r0 * 0.28f, 0, tips);
  float R = c.sp.size * 1.3f * c.P("folhas");
  if (R > 0.5f) { auto lobes = lobesAt(c.rng, tips, R, 1.f, 0.1f); foliage(c.s, c.rng, lobes, c.sp.color2, nullptr, true); }
}

// ---- cactus --------------------------------------------------------------------------------------------------------------------
void cactusBrush(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 7.f);
  float r0 = trunkR(c, 0.85f);
  Color g0 = c.sp.color;
  auto cyl = [&](const std::vector<Vec2>& v, float r, Color col) {
    auto rf = [r](float) { return r; };
    ribbon(c.s, v, rf, 0, 1.f, col, 1.f);
    ribbon(c.s, v, rf, -0.5f, 0.5f, lighten(col, 0.18f), 0.6f, 0.5f);
    ribbon(c.s, v, rf, 0.6f, 0.4f, darken(col, 0.35f), 0.65f, 0.5f);
    for (int k = -2; k <= 2; ++k) ribbon(c.s, v, rf, k * 0.34f, 0.07f, darken(col, 0.5f), 0.5f, 0.8f);
    for (size_t i = 1; i < v.size(); ++i) if (c.rng.chance(0.5f)) {
      Vec2 n = pathNormal(v, i); float u = (c.rng.irange(-2, 2)) * 0.34f * r;
      Vec2 p = v[i] + n * u;
      line(c.s, p, p + n * 3.f + Vec2{0, -1}, 0.4f, Color{0.95f, 0.93f, 0.8f, 1}, 0.9f);
    }
  };
  cyl(tr, r0, g0);
  disc(c.s, tr.back(), r0, g0, 1.f); disc(c.s, tr.back() + Vec2{-r0 * 0.25f, -r0 * 0.2f}, r0 * 0.5f, lighten(g0, 0.15f), 0.6f, 0.5f);
  for (int i = 0; i < 2; ++i) {
    size_t idx = (size_t)(tr.size() * (0.35f + 0.25f * i)); float side = i == 0 ? -1.f : 1.f;
    Vec2 a = tr[idx]; float armR = r0 * 0.6f;
    float h = c.sp.size * (2.5f + 1.5f * c.rng.uniform());
    std::vector<Vec2> arm{a, a + Vec2{side * h * 0.5f, 0}, a + Vec2{side * h, -2.f}};
    std::vector<Vec2> arm2; for (int k = 0; k <= 8; ++k) { Vec2 p = bez(a, a + Vec2{side * h, 0}, a + Vec2{side * h, -h * 1.1f}, 8)[k]; arm2.push_back(p); }
    cyl(arm2, armR, g0);
    disc(c.s, arm2.back(), armR, g0, 1.f);
    if (c.rng.chance(0.7f)) disc(c.s, arm2.back() + Vec2{0, -armR}, 3.5f, Color{0.95f, 0.4f, 0.55f, 1}, 1.f);
  }
}

// ---- bamboo -----------------------------------------------------------------------------------------------------------------------
void bambooBrush(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 10.f);
  float r = trunkR(c, 0.35f);
  Color gc = c.sp.color;
  float total = polyLen(tr), acc = 0, nodeStep = c.sp.size * 4.f;
  std::vector<float> nodes; float nx = nodeStep * (0.7f + 0.3f * c.rng.uniform());
  while (nx < total) { nodes.push_back(nx); nx += nodeStep * (0.8f + 0.4f * c.rng.uniform()); }
  auto rf = [r](float) { return r; };
  ribbon(c.s, tr, rf, 0, 1.f, gc, 1.f);
  ribbon(c.s, tr, rf, -0.45f, 0.5f, lighten(gc, 0.25f), 0.7f, 0.5f);
  ribbon(c.s, tr, rf, 0.6f, 0.4f, darken(gc, 0.35f), 0.7f, 0.5f);
  for (size_t i = 1; i < tr.size(); ++i) {
    float seg = dist(tr[i - 1], tr[i]); float prevAcc = acc; acc += seg;
    for (float nd : nodes) if (nd > prevAcc && nd <= acc) {
      Vec2 n = pathNormal(tr, i);
      line(c.s, tr[i] - n * (r * 1.12f), tr[i] + n * (r * 1.12f), 1.7f, darken(gc, 0.45f), 1.f);
      line(c.s, tr[i] - n * (r * 1.05f) + Vec2{0, -2.2f}, tr[i] + n * (r * 1.05f) + Vec2{0, -2.2f}, 0.8f, lighten(gc, 0.3f), 0.8f);
      if (acc > total * 0.35f && c.rng.chance(0.7f)) {
        float side = c.rng.chance(0.5f) ? 1.f : -1.f;
        Vec2 b0 = tr[i] + n * (side * r);
        Vec2 b1 = b0 + n * (side * c.sp.size * 2.2f) + Vec2{0, -c.sp.size * 0.8f};
        taper(c.s, bez(b0, b0 + n * (side * c.sp.size * 1.2f) + Vec2{0, -4.f}, b1, 8), 1.4f, 0.7f, darken(gc, 0.2f), 1.f);
        if (c.P("folhas") > 0.05f) for (int k = 0; k < 4; ++k) {
          float la = (side > 0 ? 0.f : kPi) + (k - 1.5f) * 0.55f + 0.5f;
          leafShape(c.s, b1, la, c.sp.size * 1.8f, c.sp.size * 0.42f, jitter(c.sp.color2, c.rng, 0.03f, 0.1f, 0.1f));
        }
      }
    }
  }
}

// ---- mangrove ---------------------------------------------------------------------------------------------------------------------
void mangroveTree(NCtx& c) {
  auto tr = trunkPoints(c.path, c.sp, 6.f);
  float r0 = trunkR(c, 0.4f); Vec2 base = tr.front();
  int nr = 9;
  for (int i = 0; i < nr; ++i) {
    size_t idx = (size_t)(tr.size() * (0.1f + 0.35f * c.rng.uniform())); Vec2 a = tr[std::min(idx, tr.size() - 1)];
    float side = (i % 2 ? 1.f : -1.f); float reach = c.sp.size * (1.5f + 3.5f * c.rng.uniform());
    Vec2 end{base.x + side * reach, base.y + c.sp.size * 0.6f};
    Vec2 ctl{a.x + side * reach * 0.9f, a.y - c.sp.size * 0.6f};
    taper(c.s, bez(a, ctl, end, 14), r0 * 0.28f, r0 * 0.1f, darken(c.sp.color, 0.1f), 1.f);
  }
  drawTrunk(c.s, c.rng, tr, taperFn(r0, r0 * 0.6f, 0.2f), c.sp.color, 0.8f);
  Vec2 top = tr.back(); std::vector<Tip> tips;
  for (int i = 0; i < 5; ++i) tips.push_back({top + Vec2{c.rng.sym() * c.sp.size * 2.f, c.rng.sym() * c.sp.size * 0.8f}, 0, 1, 0});
  float R = c.sp.size * 1.6f * (0.8f + 0.4f * c.P("folhas"));
  foliage(c.s, c.rng, lobesAt(c.rng, tips, R, 1.2f, 0.2f), darken(c.sp.color2, 0.1f));
}

// ---- forest line ------------------------------------------------------------------------------------------------------------------
void forestLine(NCtx& c) {
  struct T { Vec2 base; float d; bool pine; float h; float hue; };
  std::vector<T> ts;
  float L = c.path.length();
  float step = c.sp.size * 1.7f / std::max(0.3f, c.P("densidade"));
  for (float s = 0; s <= L; s += step * (0.6f + 0.8f * c.rng.uniform())) {
    PathPoint p = c.path.at(s);
    T t; t.d = c.rng.uniform(); t.base = p.p + Vec2{c.rng.sym() * 4.f, (t.d - 0.5f) * c.sp.size * 2.5f};
    t.pine = c.rng.chance(0.55f); t.h = c.sp.size * (3.5f + 3.f * c.rng.uniform()) * lerpf(0.6f, 1.1f, t.d); t.hue = c.rng.uniform();
    ts.push_back(t);
  }
  std::sort(ts.begin(), ts.end(), [](const T& a, const T& b) { return a.base.y < b.base.y; });
  Color haze{0.72f, 0.82f, 0.9f, 1};
  for (auto& t : ts) {
    float far = (1.f - t.d) * 0.6f;
    Color leaf = mix(t.pine ? shade(c.sp.color2, 0.7f) : c.sp.color2, haze, far); leaf = shiftHSV(leaf, (t.hue - 0.5f) * 0.04f, 0, 0);
    Color bark = mix(c.sp.color, haze, far);
    float w = t.h * (t.pine ? 0.3f : 0.38f);
    line(c.s, t.base, t.base + Vec2{0, -t.h * 0.4f}, std::max(1.f, t.h * 0.03f), bark, 1.f);
    if (t.pine) {
      for (int k = 0; k < 3; ++k) {
        float yb = t.base.y - t.h * (0.12f + 0.22f * k), hh = t.h * 0.5f * (1.f - k * 0.18f), ww = w * (1.f - k * 0.25f);
        std::vector<Vec2> poly{{t.base.x, yb - hh}, {t.base.x + ww, yb}, {t.base.x - ww, yb}};
        fillPoly(c.s, poly, [=](float x, float) { return shade(leaf, 0.75f + 0.3f * clampf((t.base.x - x) / std::max(1.f, ww) * 0.5f + 0.5f)); });
      }
    } else {
      Vec2 cc = t.base + Vec2{0, -t.h * 0.62f};
      disc(c.s, cc + Vec2{1.f, w * 0.15f}, w * 0.95f, shade(leaf, 0.55f), 1.f, 0.8f);
      disc(c.s, cc, w * 0.9f, shade(leaf, 0.85f), 1.f, 0.8f);
      disc(c.s, cc + Vec2{-w * 0.25f, -w * 0.3f}, w * 0.55f, lighten(leaf, 0.2f), 0.9f, 0.5f);
    }
  }
}

// ---- falling leaves (live) -------------------------------------------------------------------------------------------------------------
void registerFalling(BrushLibrary& lib) {
  BrushInfo bi = info("arvores.folhas-caindo", "Folhas caindo", "arvores",
    "Na vida real: uma folha solta oscila como um pêndulo ao cair (arrasto + sustentação), girando devagar; as de outono têm cores do verde ao vermelho. "
    "Modelo: a cada passo do traço nasce uma folha (máscara de folha/ginkgo/bordo) deslocada para baixo com rotação aleatória, tombamento (achatamento pelo cos), sombra suave e flutuação lateral pelo vento.",
    14, Color{0.7f, 0.3f, 0.1f, 1}, Color{0.9f, 0.7f, 0.1f, 1}, "wave", "sky");
  bi.animated = true;
  addLive(lib, bi, {{"densidade", "Densidade", 1.f, 0.3f, 3.f}},
    [](const StrokeParams& sp) { return sp.size * 1.4f; },
    [](NCtx& c, const PathPoint& pp, int idx) {
      const char* names[4] = {"stamp_leaf3", "stamp_leaf2", "stamp_leaf1", "stamp_leaf4"};
      int n = std::max(1, (int)std::round(c.P("densidade")));
      for (int k = 0; k < n; ++k) {
        float fall = c.rng.uniform() * c.sp.size * 6.f;
        Vec2 p = pp.p + Vec2{c.rng.sym() * c.sp.size * 2.f + std::sin(fall * 0.15f + idx) * c.sp.size * 0.8f + windSway(c.sp, idx, c.sp.size * 2.f), fall};
        float sz = c.sp.size * (0.5f + 0.7f * c.rng.uniform());
        float ang = c.rng.uniform() * kTau;
        float tumble = std::fabs(std::cos(fall * 0.12f + idx));
        float h = c.rng.uniform();
        Color col = h < 0.3f ? mix(Color{0.8f, 0.55f, 0.1f, 1}, Color{0.9f, 0.75f, 0.1f, 1}, c.rng.uniform()) : (h < 0.7f ? mix(Color{0.85f, 0.35f, 0.1f, 1}, Color{0.7f, 0.15f, 0.08f, 1}, c.rng.uniform()) : mix(c.sp.color, c.sp.color2, c.rng.uniform()));
        const Mask* m = StampBank::instance().get(names[c.rng.irange(0, 3)]);
        ell(c.s, p + Vec2{sz * 0.25f, sz * 0.4f}, sz * 0.9f, sz * 0.9f * (0.3f + 0.7f * tumble), ang, Color{0, 0, 0, 1}, 0.12f, 0.2f);
        if (m) maskDab(c.s, m, p, sz, sz * (0.3f + 0.7f * tumble), ang, col, 1.f, c.rng.chance(0.5f));
        else leafShape(c.s, p, ang, sz * 2.f, sz * 1.2f * (0.3f + 0.7f * tumble), col);
      }
    });
}

}  // namespace

void registerTrees(BrushLibrary& lib) {
  const std::vector<ParamSpec> bp = {{"galhos", "Ramificação", 1.f, 0.5f, 1.5f}, {"folhas", "Folhagem", 1.f, 0.f, 2.f}};
  auto reg = [&](const char* id, const char* nm, const char* desc, Color c1, Color c2, std::vector<ParamSpec> ps, EndBrush::Gen g, float size = 12.f) {
    BrushInfo bi = info(id, nm, "arvores", desc, size, c1, c2, "up", "sky");
    bi.animated = true;
    addEnd(lib, bi, std::move(ps), std::move(g));
  };
  // oak
  { Broad b; b.g.bark = kBark; b.g.kids = 2; b.g.maxDepth = 4; b.g.spread = 0.6f; b.g.photo = 0.55f; b.leafR = 1.4f; b.lobeDensity = 1.f; b.style = 0;
    reg("arvores.carvalho", "Árvore folhosa (carvalho)",
      "Na vida real: o tronco afina com a altura, se divide em galhos que obedecem à regra de Leonardo (a soma das áreas dos galhos filhos = área do galho-pai), cada galho curva-se para a luz (fototropismo) e a copa é feita de massas de folhas iluminadas por cima. "
      "Modelo: o traço é o tronco (casca com fissuras e flare de raiz); a copa cresce por recursão de 4-6 níveis com raios r_filho = r/√k, curvatura para cima e ruído; as pontas recebem lóbulos de folhagem pintados em 3 passes (sombra, corpo, luz) com folhas de carimbo por cima; o vento (StrokeParams.wind/time) balança os galhos.",
      kBark, kLeaf, bp, [b](NCtx& c) { broadleaf(c, b); }); }
  { Broad b; b.g.bark = Color{0.3f, 0.2f, 0.18f, 1}; b.g.kids = 2; b.g.maxDepth = 4; b.g.spread = 0.65f; b.g.photo = 0.4f; b.leafR = 1.3f; b.lobeDensity = 1.2f; b.style = 1;
    reg("arvores.cerejeira", "Cerejeira (sakura)",
      "Na vida real: a sakura floresce antes das folhas; os galhos são escuros e tortuosos, cobertos de nuvens de flores rosa-claro que caem como chuva de pétalas. "
      "Modelo: crescimento de galhos escuros mais tortuosos; copa de lóbulos com mistura rosa/branco em vez de verde; pétalas soltas espalhadas abaixo da copa.",
      Color{0.3f, 0.2f, 0.18f, 1}, Color{0.98f, 0.68f, 0.78f, 1}, bp, [b](NCtx& c) { broadleaf(c, b); }); }
  { Broad b; b.g.bark = kBark; b.g.kids = 2; b.g.maxDepth = 4; b.g.spread = 0.6f; b.g.photo = 0.5f; b.leafR = 1.4f; b.lobeDensity = 1.f; b.style = 2;
    reg("arvores.outono", "Árvore de outono",
      "Na vida real: no outono a clorofila some e os carotenoides/antocianinas aparecem; a copa vai do verde (interior/baixo) ao amarelo, laranja e vermelho nas pontas mais expostas. "
      "Modelo: mesmo motor de crescimento do carvalho; cor de cada lóbulo varia com a altura e ruído em gradiente verde→amarelo→laranja→vermelho; folhas de bordo em carimbo.",
      kBark, kLeaf, bp, [b](NCtx& c) { broadleaf(c, b); }); }
  { Broad b; b.g.bark = Color{0.92f, 0.92f, 0.88f, 1}; b.g.kids = 2; b.g.maxDepth = 5; b.g.spread = 0.38f; b.g.photo = 0.9f; b.g.jitter = 0.08f; b.g.taperEnd = 0.7f; b.leafR = 0.75f; b.lobeDensity = 0.7f; b.style = 3;
    reg("arvores.betula", "Bétula (vidoeiro)",
      "Na vida real: casca branca com marcas horizontais pretas (lenticelas), galhos finos e ascendentes, ramos finais pendentes e folhagem clara e esparsa. "
      "Modelo: tronco branco com listras negras horizontais, ângulo de ramificação estreito e fototropismo forte, folhagem pequena rala e raminhos pendentes com folhinhas.",
      Color{0.92f, 0.92f, 0.88f, 1}, Color{0.5f, 0.68f, 0.2f, 1}, bp, [b](NCtx& c) { broadleaf(c, b); }); }
  reg("arvores.pinheiro", "Pinheiro / conífera",
    "Na vida real: crescimento monopodial (um líder central) com galhos em verticilos; os de baixo são longos e pendem sob o peso, os de cima curtos, dando a silhueta de cone; as agulhas formam saias densas escuras por baixo. "
    "Modelo: tronco-líder; andares sobrepostos de baixo para cima, largura ∝ (1-t)^0.85, borda inferior irregular (dentes) com agulhas pendentes, luz à esquerda e sombra sob cada andar; opcional neve.",
    Color{0.3f, 0.2f, 0.12f, 1}, Color{0.12f, 0.36f, 0.2f, 1}, {{"andares", "Andares", 1.f, 0.f, 2.f}, {"neve", "Neve", 0.f, 0.f, 1.f}}, coniferTree, 12.f);
  reg("arvores.palmeira", "Palmeira",
    "Na vida real: tronco único sem galhos, curvo e anelado pelas cicatrizes das folhas; no topo, uma coroa de frondes pinadas que se arqueiam e pendem, com folíolos pendurados dos dois lados; cocos sob a coroa. "
    "Modelo: tronco com anéis; 11 frondes em arco de Bézier (as de trás desenhadas primeiro), folíolos pares cujo comprimento segue sin(πt) e pendem para baixo, balanço pelo vento.",
    Color{0.52f, 0.38f, 0.24f, 1}, Color{0.22f, 0.55f, 0.18f, 1}, {{"folhas", "Tamanho da copa", 1.f, 0.4f, 1.6f}}, palmTree, 12.f);
  reg("arvores.salgueiro", "Salgueiro-chorão",
    "Na vida real: ramos finos e longos pendem verticalmente da copa quase até o chão, com folhas estreitas, balançando ao vento. "
    "Modelo: galhos principais com forte curvatura para baixo; dezenas de fios pendentes (curvas senoidais) com folhinhas alternadas e deslocamento pelo vento crescente com a queda.",
    Color{0.33f, 0.25f, 0.17f, 1}, Color{0.5f, 0.7f, 0.22f, 1}, {{"folhas", "Quantidade de fios", 1.f, 0.2f, 2.f}}, willowTree, 12.f);
  reg("arvores.seca", "Árvore seca (inverno)",
    "Na vida real: sem folhas, vê-se a arquitetura: ramificação fina e fractal, galhos que afinam até gravetos, com neve acumulada em cima. "
    "Modelo: recursão profunda (até 6 níveis) com ruído alto, sem folhagem; opção de neve nas pontas.",
    Color{0.3f, 0.24f, 0.2f, 1}, Color{1, 1, 1, 1}, {{"galhos", "Ramificação", 1.f, 0.5f, 1.5f}, {"neve", "Neve", 0.f, 0.f, 1.f}}, bareTree, 12.f);
  reg("arvores.bonsai", "Bonsai",
    "Na vida real: tronco torcido com arame, galhos horizontais e copa em 'nuvens' achatadas, a forma é desenhada pelo cultivador. "
    "Modelo: tronco ondulado grosso, 3-4 galhos laterais e almofadas de folhagem achatadas em 3 camadas com pontilhado nas bordas.",
    Color{0.33f, 0.23f, 0.15f, 1}, Color{0.2f, 0.45f, 0.18f, 1}, {{"folhas", "Tamanho das nuvens", 1.f, 0.4f, 1.6f}}, bonsaiTree, 10.f);
  reg("arvores.acacia", "Acácia (guarda-chuva)",
    "Na vida real: a acácia da savana tem tronco curto que se abre em galhos quase horizontais e uma copa plana como guarda-chuva (os herbívoros comem a parte baixa). "
    "Modelo: poucos galhos de ângulo largo e copa em 3 camadas elípticas achatadas, luz no topo.",
    Color{0.3f, 0.22f, 0.15f, 1}, Color{0.35f, 0.5f, 0.15f, 1}, {{"folhas", "Largura da copa", 1.f, 0.4f, 1.6f}}, acaciaTree, 12.f);
  reg("arvores.baoba", "Baobá",
    "Na vida real: tronco enorme em forma de garrafa (armazena água), casca lisa cinza-parda com sulcos, poucos galhos grossos e curtos no topo. "
    "Modelo: perfil de raio alargado na base e afinando a 62%, sulcos verticais acentuados, 5 galhos curtos e grossos com copa pequena.",
    Color{0.45f, 0.38f, 0.3f, 1}, Color{0.3f, 0.5f, 0.15f, 1}, {{"folhas", "Folhagem", 1.f, 0.f, 2.f}}, baobabTree, 8.f);
  reg("arvores.cacto", "Cacto saguaro",
    "Na vida real: coluna carnuda com costelas verticais (que se expandem ao absorver água), espinhos pálidos nos vértices, braços que saem na horizontal e sobem. "
    "Modelo: cilindro com costelas escuras, luz à esquerda, espinhos nas costelas, braços em curva (horizontal → vertical) e flor rosa no topo.",
    Color{0.28f, 0.5f, 0.28f, 1}, Color{0.95f, 0.4f, 0.55f, 1}, {}, cactusBrush, 10.f);
  reg("arvores.bambu", "Bambu",
    "Na vida real: colmos retos e ocos divididos em entrenós por nós salientes; ramos finos nascem nos nós superiores com folhas lanceoladas. "
    "Modelo: colmo com nós (anel escuro + brilho) a cada ~4 larguras com espaçamento irregular, ramos alternados e folhas lanceoladas em leque.",
    Color{0.5f, 0.68f, 0.28f, 1}, Color{0.3f, 0.55f, 0.2f, 1}, {{"folhas", "Folhas", 1.f, 0.f, 1.f}}, bambooBrush, 8.f);
  reg("arvores.mangue", "Mangue",
    "Na vida real: raízes-escora arqueiam a partir do tronco e mergulham na lama, sustentando a árvore na maré; copa densa e escura. "
    "Modelo: raízes em arcos de Bézier saindo de várias alturas para o nível do solo, tronco fino, copa de lóbulos escuros.",
    Color{0.3f, 0.22f, 0.15f, 1}, Color{0.2f, 0.42f, 0.2f, 1}, {{"folhas", "Copa", 1.f, 0.4f, 1.6f}}, mangroveTree, 9.f);
  { BrushInfo bi = info("arvores.floresta", "Floresta (fileira de árvores)",  "arvores",
      "Na vida real: em uma floresta distante as árvores se sobrepõem e a névoa (perspectiva atmosférica) clareia e azula as mais distantes. "
      "Modelo: ao longo do traço nascem árvores (pinheiros e redondas) com profundidade aleatória; as distantes são menores e misturadas ao azul-névoa; ordenadas do fundo para a frente.",
      9.f, Color{0.3f, 0.2f, 0.12f, 1}, Color{0.2f, 0.45f, 0.22f, 1}, "line", "sky");
    addEnd(lib, bi, {{"densidade", "Densidade", 1.f, 0.4f, 2.5f}}, forestLine); }
  registerFalling(lib);
}

}  // namespace nat
}  // namespace ab
