// AnimBrush - module "stampsnature": zip stamps of living / natural things.
// One brush per mask (id "zip.<mask>"), each with a behaviour grounded in the real thing.
#include <memory>

#include "../../include/animbrush/dabbrush.hpp"
#include "../../include/animbrush/library.hpp"

namespace ab {
namespace {

// ---------------------------------------------------------------------------------------------
// mask helper: content bbox so that "radius" means the visible size of the drawing
// ---------------------------------------------------------------------------------------------
struct MI {
  const Mask* m = nullptr;
  float x0 = 0, y0 = 0, x1 = 1, y1 = 1, cx = 0.5f, cy = 0.5f, ext = 1.f;
};

MI makeMI(const std::string& name) {
  MI r;
  r.m = StampBank::instance().get(name);
  if (!r.m || r.m->empty()) { r.m = nullptr; return r; }
  int minx = r.m->w, miny = r.m->h, maxx = -1, maxy = -1;
  for (int y = 0; y < r.m->h; ++y)
    for (int x = 0; x < r.m->w; ++x)
      if (r.m->a[(size_t)y * r.m->w + x] > 40) { minx = std::min(minx, x); maxx = std::max(maxx, x); miny = std::min(miny, y); maxy = std::max(maxy, y); }
  if (maxx < 0) { r.m = nullptr; return r; }
  r.x0 = minx / (float)r.m->w; r.x1 = (maxx + 1) / (float)r.m->w;
  r.y0 = miny / (float)r.m->h; r.y1 = (maxy + 1) / (float)r.m->h;
  r.cx = (r.x0 + r.x1) * 0.5f; r.cy = (r.y0 + r.y1) * 0.5f;
  r.ext = std::max(r.x1 - r.x0, r.y1 - r.y0);
  return r;
}

struct PO {
  float aspect = 1, dissolve = 0, hard = 0.8f;
  Blend blend = Blend::Normal;
  bool mirror = false;
  Vec2 anchor{-1, -1};
  uint32_t seed = 0;
};

// Draw the mask so that its visible content spans 2R, `pos` being the anchor point (default: content centre).
void put(ISurface& s, const MI& mi, Vec2 pos, float R, float ang, Color c, float op, const PO& o = PO{}) {
  if (R < 0.2f || op <= 0.002f) return;
  Dab d;
  d.color = c; d.color.a = 1; d.opacity = clampf(op); d.blend = o.blend; d.dissolve = o.dissolve; d.seed = o.seed;
  d.angle = ang; d.aspect = o.aspect; d.mirrorX = o.mirror; d.hardness = o.hard;
  if (!mi.m) { d.x = pos.x; d.y = pos.y; d.radius = R; s.drawDab(d); return; }
  float M = R / std::max(0.05f, mi.ext);
  Vec2 a = o.anchor.x < 0 ? Vec2{mi.cx, mi.cy} : o.anchor;
  Vec2 l{(a.x - 0.5f) * 2.f * M * o.aspect, (a.y - 0.5f) * 2.f * M};
  if (o.mirror) l.x = -l.x;
  Vec2 ctr = pos - rotate(l, ang);
  d.x = ctr.x; d.y = ctr.y; d.radius = M; d.shape = mi.m;
  s.drawDab(d);
}

void dot(ISurface& s, Vec2 p, float r, Color c, float op, float hard = 0.9f, float aspect = 1, float ang = 0) {
  Dab d; d.x = p.x; d.y = p.y; d.radius = std::max(0.2f, r); d.color = c; d.color.a = 1; d.opacity = clampf(op);
  d.hardness = hard; d.aspect = aspect; d.angle = ang; s.drawDab(d);
}

Color C(float r, float g, float b) { return Color{r, g, b, 1}; }
Color darker(Color c, float v) { return shiftHSV(c, 0, 0.05f, -v); }
Color lighter(Color c, float v) { return shiftHSV(c, 0, -0.08f, v); }

enum class K {
  Flower, Tile, Petal, LeafPair, Ginkgo, Maple, Clover, Clover4, LeafDrift, Shards, Grass, Fern, Snow, SnowStreak,
  Dragonfly, DragonflyTrail, Butterfly, Feather, Drop, DropRow, Splash, Web, Foot, FootBoth, FootEmboss, FootRainbow,
  CloudStorm, CloudPuff, CloudWisp, Fur, Wash, Pebble
};

struct Spec {
  std::string slug, name, cat, mask, mask2, desc;
  K kind = K::Flower;
  float size = 20;
  Color c1{0, 0, 0, 1}, c2{1, 1, 1, 1};
  std::vector<Color> pal;
  float a = 0, b = 0, c = 0;   // kind specific tuning
  int variant = 0;
  std::string demo = "wave", bg = "white";
  bool animated = false;
  bool directional = false;
};

// ---------------------------------------------------------------------------------------------
class NatStamp : public Brush {
 public:
  explicit NatStamp(const Spec& sp) : S(sp) {
    addParam("densidade", "Densidade", 1.f, 0.4f, 2.5f);
    addParam("variacao", "Variação", 0.6f, 0.f, 1.f);
    addParam("cores_naturais", "Cores naturais (1) / cor do pincel (0)", 1.f, 0.f, 1.f);
    addParam("escala_pressao", "Escala pela pressão", 0.6f, 0.f, 1.f);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    P = sp; rng.reseed(sp.seed * 7919u + 13u);
    mi = makeMI(S.mask);
    mi2 = S.mask2.empty() ? MI{} : makeMI(S.mask2);
    if (!S.mask2.empty() && !mi2.m) mi2 = mi;
    mask_fill = makeMI("stamp_drop");
    cur = last = {in.x, in.y}; dir = {1, 0}; pressure = in.pressure; carry = 0; len = 0; idx = 0; started = false; active = true;
    prevStamp = cur;
    if (!S.directional) { emit(s, cur, dir, 0.f, true); started = true; }
  }
  void move(ISurface& s, const InputSample& in) override {
    if (!active) return;
    Vec2 p{in.x, in.y};
    pressure = in.pressure;
    Vec2 seg = p - last;
    float L = seg.len();
    if (L < 0.3f) return;
    dir = (dir * 0.5f + seg.norm() * 0.5f).norm();
    if (!started) { started = true; emit(s, last, dir, 0.f, true); }
    float R = baseR();
    float sp = std::max(1.5f, spacing(R) / std::max(0.2f, param("densidade")));
    float pos = sp - carry;
    while (pos <= L) {
      Vec2 q = lerp(last, p, pos / L);
      emit(s, q, dir, len + pos, false);
      R = baseR();
      sp = std::max(1.5f, spacing(R) / std::max(0.2f, param("densidade")));
      pos += sp;
    }
    carry = L - (pos - sp);
    if (carry < 0) carry = 0;
    len += L; last = p; cur = p;
  }
  void end(ISurface& s) override {
    if (!active) return;
    active = false;
    if (S.kind == K::Flower && started) {  // a bud at the end of the stem
      float R = S.size * 0.45f;
      Color c = lighter(pickCol(), 0.05f);
      put(s, mi, last, R, rng.uniform() * kTau, c, 0.95f);
    }
    if (S.kind == K::DragonflyTrail && started) trail(s, last, dir, 1.f);
  }

 private:
  Spec S;
  StrokeParams P;
  Rng rng;
  MI mi, mi2, mask_fill;
  Vec2 cur, last, dir{1, 0}, prevStamp;
  float pressure = 1, carry = 0, len = 0;
  int idx = 0;
  bool started = false, active = false;

  float baseR() const {
    float k = param("escala_pressao");
    return P.size * (1.f - k + k * (0.35f + 0.9f * clampf(pressure))) * (S.size > 0 ? 1.f : 1.f);
  }
  bool natural() const { return param("cores_naturais") > 0.5f; }
  float var() const { return param("variacao"); }

  float spacing(float R) const {
    switch (S.kind) {
      case K::Flower: return R * 1.9f;
      case K::Tile: return R * 2.05f;
      case K::Petal: return R * 2.4f;
      case K::LeafPair: return R * 1.5f;
      case K::Ginkgo: return R * 1.7f;
      case K::Maple: return R * 2.3f;
      case K::Clover: return R * 1.5f;
      case K::Clover4: return R * 1.3f;
      case K::LeafDrift: return R * 3.0f;
      case K::Shards: return R * 3.2f;
      case K::Grass: return R * (0.5f + 0.25f * S.variant * 0.1f);
      case K::Fern: return R * 1.1f;
      case K::Snow: case K::SnowStreak: return R * 1.6f;
      case K::Dragonfly: return R * 5.5f;
      case K::DragonflyTrail: return R * 7.f;
      case K::Butterfly: return R * 5.5f;
      case K::Feather: return R * 3.f;
      case K::Drop: return R * 1.3f;
      case K::DropRow: return R * 1.5f;
      case K::Splash: return R * 3.4f;
      case K::Web: return R * 4.2f;
      case K::Foot: case K::FootEmboss: return R * 3.2f;
      case K::FootBoth: case K::FootRainbow: return R * 4.6f;
      case K::CloudStorm: return R * 1.1f;
      case K::CloudPuff: return R * 1.0f;
      case K::CloudWisp: return R * 1.3f;
      case K::Fur: return R * 0.45f;
      case K::Wash: return R * 1.5f;
      case K::Pebble: return R * 1.1f;
    }
    return R * 2.f;
  }

  Color pickCol() {
    if (natural() && !S.pal.empty()) {
      Color c = S.pal[rng.irange(0, (int)S.pal.size() - 1)];
      return shiftHSV(c, rng.sym() * 0.012f * var(), rng.sym() * 0.06f * var(), rng.sym() * 0.08f * var());
    }
    return shiftHSV(P.color, rng.sym() * 0.03f * var(), rng.sym() * 0.08f * var(), rng.sym() * 0.1f * var());
  }

  float flap(int i) const { return std::fabs(std::cos(P.frame * 1.15f + i * 0.9f + P.time * 6.f)); }

  void emit(ISurface& s, Vec2 p, Vec2 d, float travelled, bool first) {
    float R = baseR();
    float dirA = std::atan2(d.y, d.x);
    float sideSign = (idx & 1) ? 1.f : -1.f;
    uint32_t sd = hash32(P.seed + idx * 131u);
    PO o; o.seed = sd;
    switch (S.kind) {
      // ---------------------------------------------------------------- flowers
      case K::Flower: {
        float sc = first ? 0.6f : 1.f;
        float r = R * sc * (1.f + rng.sym() * 0.22f * var());
        Color c = pickCol();
        float ang = (S.variant == 8 ? rng.sym() * 0.5f : rng.uniform() * kTau);
        o.mirror = rng.chance(0.5f);
        PO sh = o; put(s, mi, p + Vec2{r * 0.08f, r * 0.12f}, r * 1.03f, ang, C(0.1f, 0.1f, 0.12f), 0.16f, sh);  // soft contact shadow
        put(s, mi, p, r, ang, c, 0.98f, o);
        if (S.variant == 3) put(s, mi, p, r * 0.72f, ang + 0.2f, lighter(c, 0.12f), 0.85f, o);   // chrysanthemum: second tier of ray petals
        if (S.variant == 7) { put(s, mi, p, r * 0.8f, ang + 0.45f, lighter(c, 0.1f), 0.8f, o); put(s, mi, p, r * 0.5f, ang + 0.9f, darker(c, 0.08f), 0.5f, o); }  // carnation: ruffled layers
        if (S.variant == 8) { put(s, mi, p, r * 0.66f, ang, darker(c, 0.18f), 0.35f, o); put(s, mi, p - Vec2{r * 0.1f, r * 0.12f}, r * 0.4f, ang, darker(c, 0.3f), 0.35f, o); }  // rose: spiral heart
        if (S.variant == 5) { put(s, mi, p, r * 0.35f, ang, C(1, 0.97f, 0.85f), 0.55f, o); }       // bellflower: pale throat
        if (S.variant == 6) { put(s, mi, p, r * 0.45f, ang, C(1, 1, 1), 0.6f, o); put(s, mi, p, r * 0.18f, ang, C(0.95f, 0.85f, 0.25f), 0.8f, o); }  // morning glory
        if (S.variant == 2 || S.variant == 4) put(s, mi, p, r * 0.4f, ang, darker(c, 0.25f), 0.3f, o);  // sakura / ume: darker heart
        if (S.variant == 4) for (int k = 0; k < 5; ++k) { Vec2 a = fromAngle(ang + k * kTau / 5 + 0.3f, r * 0.2f); dot(s, p + a, r * 0.035f, C(0.95f, 0.8f, 0.3f), 0.9f); }
        break;
      }
      case K::Petal: {  // single petal tumbling down (sakura wind)
        int n = 3 + (int)(2 * var());
        for (int j = 0; j < n; ++j) {
          float fall = rng.range(0.f, 6.f) * R;
          Vec2 q = p + Vec2{rng.sym() * R * 1.6f + (P.wind * 0.6f + 0.15f) * fall + std::sin(fall * 0.05f + j) * R * 0.4f, fall};
          float f = fall / (6.f * R);
          float r = R * rng.range(0.45f, 0.9f);
          Color c = pickCol();
          float ang = rng.uniform() * kTau + fall * 0.04f;
          o.mirror = rng.chance(0.5f); o.aspect = 0.55f + 0.45f * std::fabs(std::cos(fall * 0.07f + j));  // seen edge-on while tumbling
          put(s, mi, q, r, ang, c, (1.f - 0.65f * f) * 0.95f, o);
        }
        break;
      }
      case K::Tile: {  // outline variants: wallpaper-like repeating motif, embossed
        float step = S.a * kDeg2Rad;
        float ang = S.c > 0.5f ? dirA + (idx & 1 ? S.b : -S.b) * kDeg2Rad : idx * step;
        float r = R * (1.f + 0.06f * std::sin(idx * 0.9f));
        o.mirror = (idx & 1) && S.c < 0.5f;
        Color line = natural() && !S.pal.empty() ? S.pal[0] : P.color;
        put(s, mi, p + Vec2{r * 0.035f, r * 0.045f}, r, ang, C(0.05f, 0.05f, 0.08f), 0.28f, o);    // emboss shadow
        put(s, mi, p - Vec2{r * 0.02f, r * 0.03f}, r, ang, lighter(line, 0.45f), 0.55f, o);        // emboss highlight
        put(s, mi, p, r, ang, line, 0.97f, o);
        break;
      }
      // ---------------------------------------------------------------- leaves
      case K::LeafPair: {  // leaves on a twig: alternate sides, petiole angle, size tapering
        Vec2 perp = d.perp();
        Color stem = C(0.36f, 0.25f, 0.13f);
        if (!first) dot(s, p, std::max(0.8f, R * 0.07f), stem, 0.9f, 1.f);
        for (int side = -1; side <= 1; side += 2) {
          if (S.variant == 1 && side == 1 && (idx & 1)) continue;  // alternate phyllotaxy for outline version
          float a = dirA + side * (50.f + rng.sym() * 14.f * var()) * kDeg2Rad;
          float r = R * (0.95f + rng.sym() * 0.2f * var());
          Color c = natural() ? shiftHSV(C(0.25f, 0.55f, 0.16f), rng.sym() * 0.025f, rng.sym() * 0.08f, rng.sym() * 0.12f) : pickCol();
          PO q = o; q.anchor = {mi.x0 + 0.02f, mi.cy}; q.mirror = side > 0;
          Vec2 base = p + perp * (float)side * R * 0.05f;
          if (S.variant == 1) { c = C(0.12f, 0.35f, 0.14f); put(s, mi, base, r, a, c, 0.95f, q); }
          else {
            put(s, mi, base + Vec2{r * 0.05f, r * 0.08f}, r, a, C(0, 0, 0), 0.14f, q);
            put(s, mi, base, r, a, darker(c, 0.12f), 1.f, q);
            put(s, mi, base - Vec2{0, r * 0.04f}, r * 0.86f, a, lighter(c, 0.1f), 0.55f, q);  // lit upper surface
          }
        }
        break;
      }
      case K::Ginkgo: {  // fan leaves fluttering on long petioles, golden
        Color stem = C(0.45f, 0.35f, 0.15f);
        if (!first) dot(s, p, std::max(0.8f, R * 0.06f), stem, 0.9f, 1.f);
        int sides = S.variant == 1 ? 1 : 2;
        for (int k = 0; k < sides; ++k) {
          float side = (sides == 1) ? sideSign : (k ? 1.f : -1.f);
          float flutter = std::sin(P.frame * 0.5f + idx * 1.7f + k) * 0.28f * (P.wind * 0.5f + 0.6f);
          float phi = dirA + side * (75.f + rng.sym() * 18.f * var()) * kDeg2Rad + flutter;
          float ang = phi + kPi * 0.5f;
          float r = R * (1.0f + rng.sym() * 0.15f * var());
          PO q = o; q.anchor = {mi.cx, mi.y1 - 0.01f};
          Color c = natural() ? shiftHSV(mix(C(0.98f, 0.82f, 0.15f), C(0.75f, 0.82f, 0.2f), rng.chance(0.2f) ? rng.uniform() : 0.f), 0, 0, rng.sym() * 0.08f) : pickCol();
          if (S.variant == 1) { put(s, mi, p, r, ang, C(0.55f, 0.4f, 0.1f), 0.95f, q); }
          else { put(s, mi, p + Vec2{r * 0.04f, r * 0.06f}, r, ang, C(0, 0, 0), 0.14f, q); put(s, mi, p, r, ang, c, 1.f, q); put(s, mi, p, r * 0.7f, ang, lighter(c, 0.12f), 0.5f, q); }
        }
        break;
      }
      case K::Maple: {  // tumbling leaves, autumn colour changes along the stroke, falling trail
        float t = clampf(travelled / 420.f);
        int n = 1 + (rng.chance(0.6f) ? 1 : 0) + (rng.chance(0.3f) ? 1 : 0);
        for (int j = 0; j < n; ++j) {
          float fall = j == 0 ? 0.f : rng.range(1.5f, 5.5f) * R;
          Vec2 q = p + Vec2{rng.sym() * R * 1.4f * (j ? 1.f : 0.f) + P.wind * fall * 0.5f, fall};
          Color c = natural() ? mix(S.c1, S.c2, clampf(t + rng.sym() * 0.25f)) : mix(P.color, P.color2, t);
          c = shiftHSV(c, rng.sym() * 0.015f, 0, rng.sym() * 0.08f);
          float r = R * rng.range(0.7f, 1.05f);
          float ang = rng.uniform() * kTau;
          PO q2 = o; q2.mirror = rng.chance(0.5f); q2.anchor = {mi.cx, mi.cy};
          q2.aspect = 0.62f + 0.38f * std::fabs(std::cos(fall * 0.08f + j * 2.f));  // tumbling about its long axis
          float f = fall / (5.5f * R);
          put(s, mi, q + Vec2{r * 0.06f, r * 0.1f}, r, ang, C(0, 0, 0), 0.15f * (1.f - f), q2);
          put(s, mi, q, r, ang, c, (1.f - 0.6f * f), q2);
          put(s, mi, q, r * 0.5f, ang, darker(c, 0.18f), 0.3f * (1.f - f), q2);
        }
        break;
      }
      case K::Clover: {  // ground cover patches, small clusters, occasional white blossom
        int n = 3 + (int)rng.range(0, 2);
        for (int j = 0; j < n; ++j) {
          Vec2 q = p + rng.inDisc() * R * 1.15f;
          float r = R * rng.range(0.42f, 0.8f);
          Color c = natural() ? shiftHSV(C(0.2f, 0.55f, 0.18f), rng.sym() * 0.03f, 0, rng.sym() * 0.12f) : pickCol();
          float ang = rng.uniform() * kTau;
          put(s, mi, q + Vec2{r * 0.05f, r * 0.08f}, r, ang, C(0, 0, 0), 0.18f, o);
          put(s, mi, q, r, ang, c, 1.f, o);
          put(s, mi, q, r * 0.35f, ang, lighter(c, 0.2f), 0.6f, o);
        }
        if (rng.chance(0.18f)) { Vec2 q = p + rng.inDisc() * R; dot(s, q, R * 0.14f, C(1, 0.95f, 0.97f), 0.95f); dot(s, q, R * 0.06f, C(0.98f, 0.8f, 0.4f), 0.9f); }
        break;
      }
      case K::Clover4: {  // clover patch with a rare lucky four-leaf
        int n = 2 + (int)rng.range(0, 2);
        for (int j = 0; j < n; ++j) {
          Vec2 q = p + rng.inDisc() * R * 1.2f;
          float r = R * rng.range(0.4f, 0.7f);
          Color c = shiftHSV(C(0.2f, 0.52f, 0.18f), rng.sym() * 0.03f, 0, rng.sym() * 0.12f);
          if (!natural()) c = pickCol();
          float ang = rng.uniform() * kTau;
          MI& m = mi2.m ? mi2 : mi;
          put(s, m, q + Vec2{r * 0.05f, r * 0.08f}, r, ang, C(0, 0, 0), 0.18f, o);
          put(s, m, q, r, ang, c, 1.f, o);
        }
        if (rng.chance(0.16f)) {  // the lucky one: brighter, slightly bigger, glint
          Vec2 q = p + rng.inDisc() * R * 0.6f;
          float r = R * 0.85f; float ang = rng.sym() * 0.5f;
          PO q2 = o; q2.anchor = {mi.cx, mi.cy};
          Color c = natural() ? C(0.3f, 0.78f, 0.25f) : lighter(P.color, 0.2f);
          put(s, mi, q + Vec2{r * 0.05f, r * 0.08f}, r, ang, C(0, 0, 0), 0.2f, q2);
          put(s, mi, q, r, ang, c, 1.f, q2);
          put(s, mi, q, r * 0.45f, ang, lighter(c, 0.2f), 0.6f, q2);
          dot(s, q + Vec2{r * 0.35f, -r * 0.35f}, r * 0.08f, C(1, 1, 0.85f), 0.95f, 0.3f);
        }
        break;
      }
      case K::LeafDrift: {  // leaves carried by a gust: trail of copies thinning behind
        float ang = dirA + rng.sym() * 0.5f * var();
        for (int j = 0; j < 4; ++j) {
          Vec2 q = p - d * (float)j * R * 1.1f + d.perp() * std::sin(j * 1.7f + idx) * R * 0.5f * (j > 0);
          float f = 1.f - j * 0.22f;
          Color c = natural() ? shiftHSV(C(0.45f, 0.55f, 0.15f), rng.sym() * 0.08f, 0, rng.sym() * 0.1f) : pickCol();
          if (natural() && rng.chance(0.3f)) c = C(0.78f, 0.48f, 0.12f);
          PO q2 = o; q2.mirror = (j & 1);
          put(s, mi, q, R * f * 1.1f, ang + j * 0.35f + P.wind * 0.3f, c, 0.95f * f, q2);
        }
        break;
      }
      case K::Shards: {  // debris burst outwards from the path
        int n = 3 + (int)(2 * var());
        for (int j = 0; j < n; ++j) {
          float a = rng.uniform() * kTau;
          float dd = rng.range(0.f, 3.2f) * R * (j ? 1.f : 0.f);
          float f = dd / (3.2f * R);
          Vec2 q = p + fromAngle(a, dd);
          Color c = natural() ? shiftHSV(C(0.62f, 0.42f, 0.2f), rng.sym() * 0.06f, rng.sym() * 0.1f, rng.sym() * 0.15f) : pickCol();
          PO q2 = o; q2.mirror = rng.chance(0.5f);
          put(s, mi, q, R * (0.9f - 0.55f * f), a + rng.sym(), c, 0.95f - 0.4f * f, q2);
        }
        break;
      }
      // ---------------------------------------------------------------- grass family
      case K::Grass: {
        int n = 1 + (int)(S.a * (0.6f + var()));
        for (int j = 0; j < n; ++j) {
          Vec2 q = p + Vec2{rng.sym() * R * S.b, rng.sym() * R * 0.28f};
          float tall = 0.7f + 0.8f * valueNoise(travelled * 0.03f + j * 3.1f, 4.f, P.seed);
          float r = R * tall * (0.75f + 0.5f * rng.uniform());
          float lean = (P.wind * 0.45f + (valueNoise(travelled * 0.05f, j * 7.f, P.seed + 5) - 0.5f) * 0.55f * var()) + std::sin(P.time * 3.f + travelled * 0.02f) * 0.05f * std::fabs(P.wind);
          PO q2 = o; q2.anchor = {mi.cx, mi.y1 - 0.005f}; q2.mirror = rng.chance(0.5f);
          Color c = natural() ? shiftHSV(S.c1, rng.sym() * 0.02f, rng.sym() * 0.07f, rng.sym() * 0.1f) : pickCol();
          float depth = rng.uniform();   // back blades are darker & smaller
          put(s, mi, q, r * (0.8f + 0.2f * depth), lean, darker(c, 0.2f * (1.f - depth)), 0.97f, q2);
          put(s, mi, q + Vec2{lean * r * 0.04f, -r * 0.05f}, r * 0.93f * (0.8f + 0.2f * depth), lean, lighter(c, 0.14f), 0.45f, q2);  // sunlit tips
          if (S.variant == 7 && rng.chance(0.4f)) {   // grass7: tiny wildflowers on thin stalks
            Vec2 tip = q + fromAngle(lean - kPi * 0.5f, r * rng.range(0.7f, 1.05f));
            dot(s, tip, R * 0.1f, rng.chance(0.5f) ? C(1, 1, 0.92f) : C(0.98f, 0.85f, 0.3f), 0.95f);
          }
        }
        break;
      }
      case K::Fern: {  // fronds radiating from the stroke, unfurling (size grows along the stroke)
        float grow = 0.3f + 0.9f * clampf(travelled / 260.f);
        float side = sideSign;
        float phi = -kPi * 0.5f + side * (18.f + rng.range(0, 28.f) * var()) * kDeg2Rad + P.wind * 0.25f;
        float r = R * grow * rng.range(0.8f, 1.15f);
        PO q = o; q.anchor = {mi.cx, mi.y1 - 0.005f}; q.mirror = side > 0;
        Color c = natural() ? shiftHSV(S.c1, rng.sym() * 0.02f, 0, rng.sym() * 0.1f) : pickCol();
        put(s, mi, p, r * 0.9f, phi + side * 0.12f, darker(c, 0.2f), 0.95f, q);
        put(s, mi, p, r, phi, c, 0.97f, q);
        put(s, mi, p + Vec2{0, -r * 0.05f}, r * 0.9f, phi, lighter(c, 0.14f), 0.4f, q);
        break;
      }
      // ---------------------------------------------------------------- snow
      case K::Snow: {
        int n = 3 + (int)(3 * var());
        for (int j = 0; j < n; ++j) {
          float z = rng.uniform();   // 0 far .. 1 near
          Vec2 q = p + rng.inDisc() * R * 3.4f + Vec2{P.wind * z * R, 0};
          float r = R * lerpf(0.25f, 0.95f, z * z);
          Color c = natural() ? C(0.86f, 0.94f, 1.f) : lighter(P.color, 0.2f);
          PO q2 = o; q2.mirror = rng.chance(0.5f);
          put(s, mi, q, r, rng.uniform() * kTau, c, lerpf(0.35f, 0.95f, z), q2);
          if (z > 0.8f) dot(s, q, r * 0.18f, C(1, 1, 1), 0.7f, 0.2f);
        }
        break;
      }
      case K::SnowStreak: {  // falling flakes with motion streak + fine snow dust
        int n = 2 + (int)(2 * var());
        for (int j = 0; j < n; ++j) {
          float z = rng.uniform();
          Vec2 q = p + rng.inDisc() * R * 3.f;
          float r = R * lerpf(0.25f, 0.8f, z);
          Vec2 fallv = Vec2{0.35f + P.wind * 0.5f, 1.f}.norm();
          Color c = natural() ? C(0.9f, 0.96f, 1.f) : lighter(P.color, 0.2f);
          for (int g = 3; g >= 1; --g) put(s, mi, q - fallv * (float)g * r * 0.9f, r * (1.f - g * 0.08f), (float)g * 0.15f + q.x * 0.03f, c, 0.14f * z * (4 - g), o);
          put(s, mi, q, r, q.x * 0.03f, c, lerpf(0.5f, 1.f, z), o);
        }
        for (int j = 0; j < 6; ++j) dot(s, p + rng.inDisc() * R * 3.4f, R * rng.range(0.03f, 0.07f), C(0.92f, 0.97f, 1.f), rng.range(0.5f, 0.9f));
        break;
      }
      // ---------------------------------------------------------------- insects & feather
      case K::Dragonfly: case K::DragonflyTrail: {
        float wob = std::sin(travelled * 0.025f);
        float wobD = std::cos(travelled * 0.025f);
        Vec2 q = p + d.perp() * wob * R * 0.55f;
        float ang = dirA + kPi * 0.5f + wobD * 0.25f;
        float r = R * 1.55f;
        PO q2 = o; q2.aspect = 0.6f + 0.4f * flap(idx);
        Color c = natural() ? (S.pal.empty() ? C(0.1f, 0.55f, 0.65f) : S.pal[idx % S.pal.size()]) : pickCol();
        if (S.kind == K::Dragonfly) {
          put(s, mi, q + Vec2{r * 0.45f, r * 0.55f}, r * 0.85f, ang, C(0, 0, 0), 0.16f, q2);  // shadow on the ground
          put(s, mi, q, r, ang, c, 0.9f, q2);
          put(s, mi, q, r * 0.55f, ang, darker(c, 0.3f), 0.3f, q2);
        } else {
          trail(s, q, d, 0.f);
          prevStamp = q;
        }
        break;
      }
      case K::Butterfly: {
        float wob = std::sin(travelled * 0.04f + idx);
        Vec2 q = p + d.perp() * wob * R * 0.9f + Vec2{0, std::sin(travelled * 0.09f) * R * 0.4f};
        float ang = dirA + kPi * 0.5f + wob * 0.5f * var();
        float r = R * 1.5f;
        PO q2 = o; q2.aspect = 0.28f + 0.72f * flap(idx);
        Color c = natural() ? S.pal[idx % S.pal.size()] : pickCol();
        put(s, mi, q + Vec2{r * 0.4f, r * 0.55f}, r * 0.8f, ang, C(0, 0, 0), 0.13f, q2);
        put(s, mi, q, r, ang, c, 0.98f, q2);
        put(s, mi, q, r * 0.78f, ang, C(0.1f, 0.07f, 0.05f), 0.5f, q2);     // dark scale pattern inside the pale margin
        put(s, mi, q, r * 0.62f, ang, c, 0.35f, q2);
        break;
      }
      case K::Feather: {
        float sway = std::sin(idx * 0.8f + P.frame * 0.25f) * 0.5f * (0.4f + var());
        float ang = dirA + sway + (idx & 1 ? 0.35f : -0.35f);
        float r = R * 1.9f;
        Vec2 q = p + d.perp() * sideSign * R * 0.8f + Vec2{0, (float)(idx % 3) * R * 0.3f};
        PO q2 = o; q2.anchor = {mi.x0 + 0.01f, mi.cy}; q2.mirror = (idx % 3) == 2;
        Color c = natural() ? S.pal[idx % S.pal.size()] : pickCol();
        put(s, mi, q + Vec2{r * 0.05f, r * 0.12f}, r, ang, C(0, 0, 0), 0.14f, q2);
        put(s, mi, q, r, ang, c, 0.98f, q2);
        put(s, mi, q, r * 0.94f, ang, lighter(c, 0.14f), 0.4f, q2);
        break;
      }
      // ---------------------------------------------------------------- water-ish
      case K::Drop: {  // droplets resting on a surface: refractive rim + highlight, some run
        for (int j = 0; j < 1 + (int)(2 * var()); ++j) {
          Vec2 q = p + rng.inDisc() * R * 1.8f;
          float r = R * std::pow(rng.uniform(), 1.4f) * 1.1f + R * 0.25f;
          float ang = rng.sym() * 0.14f;
          Color c = natural() ? C(0.3f, 0.55f, 0.85f) : P.color;
          if (rng.chance(0.18f)) {  // wet trail
            Vec2 to = q + Vec2{rng.sym() * 2.f, r * rng.range(2.f, 5.f)};
            dot(s, (q + to) * 0.5f, 1.f, c, 0.0f);
            Dab t; t.color = lighter(c, 0.05f); t.opacity = 0.22f; t.hardness = 0.5f;
            dabLine(s, q, to, r * 0.16f, r * 0.05f, t, 0.3f);
            q = to;
          }
          PO q2 = o; q2.anchor = {mi.cx, mi.cy};
          put(s, mi, q + Vec2{r * 0.07f, r * 0.1f}, r * 1.05f, ang, C(0, 0.05f, 0.15f), 0.2f, q2);          // cast shadow
          put(s, mi, q, r, ang, darker(c, 0.18f), 0.55f, q2);                                               // thick rim bends light
          put(s, mi, q + Vec2{-r * 0.03f, r * 0.05f}, r * 0.84f, ang, lighter(c, 0.3f), 0.5f, q2);          // clear body
          dot(s, q + Vec2{-r * 0.22f, -r * 0.05f}, r * 0.13f, C(1, 1, 1), 0.92f, 0.5f);                    // window reflection
          dot(s, q + Vec2{r * 0.22f, r * 0.42f}, r * 0.07f, C(1, 1, 1), 0.5f, 0.5f);
        }
        break;
      }
      case K::DropRow: {  // dew beads hanging from a thread
        if (!first) { Dab t; t.color = C(0.45f, 0.45f, 0.5f); t.opacity = 0.9f; t.hardness = 1.f; dabLine(s, prevStamp, p, 0.7f, 0.7f, t, 0.3f); }
        prevStamp = p;
        float r = R * (0.55f + 0.5f * pressure) * (1.f + rng.sym() * 0.2f * var());
        PO q = o; q.anchor = {mi.cx, mi.y0 + 0.01f};   // hang from the tip
        Color c = natural() ? C(0.2f, 0.45f, 0.8f) : P.color;
        PO f = o; f.anchor = q.anchor;
        MI& fill = mask_fill.m ? mask_fill : mi;
        PO ff = f; ff.anchor = {fill.cx, fill.y0 + 0.01f};
        put(s, fill, p, r, 0.f, lighter(c, 0.35f), 0.28f, ff);
        put(s, mi, p, r, 0.f, c, 0.95f, q);
        dot(s, p + Vec2{-r * 0.2f, r * 0.9f}, r * 0.1f, C(1, 1, 1), 0.85f, 0.5f);
        break;
      }
      case K::Splash: {
        float ang = rng.uniform() * kTau;
        float r = R * 1.25f * (0.8f + 0.4f * rng.uniform());
        Color c = natural() ? shiftHSV(C(0.62f, 0.05f, 0.08f), 0, 0, rng.sym() * 0.1f) : pickCol();
        PO q = o; q.mirror = rng.chance(0.5f);
        put(s, mi, p, r, ang, c, 0.97f, q);
        put(s, mi, p - Vec2{r * 0.12f, r * 0.14f}, r * 0.35f, ang, lighter(c, 0.35f), 0.35f, q);   // wet sheen
        int n = 7 + (int)(6 * var());
        for (int j = 0; j < n; ++j) {   // satellite droplets, thrown along the direction of motion
          float dd = r * (1.1f + std::pow(rng.uniform(), 0.8f) * 3.8f);
          Vec2 dv = (d * rng.range(0.4f, 1.f) + d.perp() * rng.sym() * 0.9f).norm();
          float size = r * 0.16f * (1.f - dd / (r * 5.f)) * rng.range(0.4f, 1.f) + r * 0.02f;
          dot(s, p + dv * dd, size, c, 0.95f, 0.9f, 1.f + dd / (r * 3.f), std::atan2(dv.y, dv.x));
        }
        break;
      }
      case K::Web: {
        float r = (first ? 1.5f : 0.9f + 0.6f * rng.uniform()) * R * 1.4f;
        Vec2 hub = p;
        if (!first) {   // anchor thread (catenary-ish) to the previous web with dew
          Vec2 a = prevStamp, b = hub;
          Vec2 mid = (a + b) * 0.5f + Vec2{0, dist(a, b) * 0.12f};
          Dab t; t.color = C(0.9f, 0.93f, 0.98f); t.opacity = 0.75f; t.hardness = 1.f;
          Vec2 pp = a; float carryT = 0.f;
          for (int k = 1; k <= 24; ++k) { float u = k / 24.f; Vec2 q = lerp(lerp(a, mid, u), lerp(mid, b, u), u); dabLine(s, pp, q, 0.55f, 0.55f, t, 0.5f, &carryT); pp = q; if (k % 4 == 0) { dot(s, q, 1.3f, C(1, 1, 1), 0.95f, 0.4f); } }
        }
        prevStamp = hub;
        float ang = rng.uniform() * kTau;
        Color c = natural() ? C(0.9f, 0.94f, 1.f) : P.color;
        put(s, mi, hub, r, ang, c, 0.9f, o);
        for (int k = 0; k < 10; ++k) {   // dew on the radial threads
          float a = rng.uniform() * kTau, rr = r * rng.range(0.15f, 0.95f);
          Vec2 q = hub + fromAngle(a, rr);
          dot(s, q + Vec2{0.6f, 0.8f}, r * 0.035f, C(0.05f, 0.08f, 0.15f), 0.35f, 0.8f);
          dot(s, q, r * 0.032f, C(0.8f, 0.92f, 1.f), 0.95f, 0.6f);
          dot(s, q - Vec2{0.5f, 0.5f}, r * 0.012f, C(1, 1, 1), 1.f, 0.5f);
        }
        break;
      }
      // ---------------------------------------------------------------- footprints
      case K::Foot: case K::FootEmboss: {
        float fade = std::max(0.22f, 1.f - travelled / 480.f);
        float shrink = std::max(0.55f, 1.f - travelled / 1500.f);
        float side = sideSign;
        float ang = dirA + kPi * 0.5f + side * 0.1f + rng.sym() * 0.05f * var() + 0.3f * (-0.2f);
        Vec2 q = p + d.perp() * side * R * 0.75f;
        float r = R * shrink * (1.f + rng.sym() * 0.05f);
        PO q2 = o; q2.mirror = side > 0;
        if (S.kind == K::Foot) {
          Color c = natural() ? C(0.33f, 0.21f, 0.12f) : P.color;
          dot(s, q - d * r * 0.6f, r * 0.55f, c, 0.12f * fade, 0.1f);                             // heel smear of mud
          put(s, mi, q, r, ang, c, 0.95f * fade, q2);
          put(s, mi, q + Vec2{0, r * 0.05f}, r * 0.7f, ang, darker(c, 0.15f), 0.35f * fade, q2);   // deeper pad centre
        } else {
          put(s, mi, q + Vec2{r * 0.06f, r * 0.08f}, r, ang, C(0.3f, 0.4f, 0.6f), 0.55f * fade, q2);
          put(s, mi, q - Vec2{r * 0.03f, r * 0.04f}, r, ang, C(1, 1, 1), 0.9f * fade, q2);
          put(s, mi, q, r, ang, C(0.55f, 0.65f, 0.8f), 0.7f * fade, q2);
        }
        break;
      }
      case K::FootBoth: case K::FootRainbow: {
        float fade = std::max(0.25f, 1.f - travelled / 520.f);
        float ang = dirA + kPi * 0.25f;
        PO q2 = o; q2.mirror = (idx & 1);
        float r = R * 1.45f * std::max(0.6f, 1.f - travelled / 1800.f);
        Vec2 q = p + d.perp() * ((idx & 1) ? 0.25f : -0.25f) * R;
        if (S.kind == K::FootBoth) {
          Color c = natural() ? C(0.2f, 0.19f, 0.22f) : P.color;
          put(s, mi, q + Vec2{r * 0.03f, r * 0.04f}, r, ang, C(0, 0, 0), 0.12f * fade, q2);
          put(s, mi, q, r, ang, c, 0.96f * fade, q2);
        } else {
          float h = std::fmod(travelled / 380.f, 1.f);
          float rr, g, b; hsv2rgb(h, 0.55f, 0.95f, rr, g, b);
          Color c = natural() ? C(rr, g, b) : mix(P.color, P.color2, clampf(travelled / 400.f));
          put(s, mi, q + Vec2{r * 0.03f, r * 0.04f}, r, ang, darker(c, 0.35f), 0.4f * fade, q2);
          put(s, mi, q, r, ang, c, 0.95f * fade, q2);
        }
        break;
      }
      // ---------------------------------------------------------------- clouds
      case K::CloudStorm: case K::CloudPuff: {
        PO q = o; q.mirror = rng.chance(0.5f);
        float r = R * (1.2f + 0.7f * rng.uniform());
        Vec2 pp = p + Vec2{rng.sym() * R * 0.6f, rng.sym() * R * 0.4f};
        float ang = rng.sym() * 0.25f;
        Color shade = S.kind == K::CloudStorm ? C(0.18f, 0.2f, 0.28f) : C(0.6f, 0.68f, 0.84f);
        Color body = S.kind == K::CloudStorm ? C(0.42f, 0.45f, 0.54f) : C(0.97f, 0.98f, 1.f);
        Color top = S.kind == K::CloudStorm ? C(0.7f, 0.72f, 0.8f) : C(1.f, 0.99f, 0.93f);
        put(s, mi, pp + Vec2{0, r * 0.14f}, r, ang, shade, 0.75f, q);             // shaded base
        put(s, mi, pp, r * 0.96f, ang, body, 0.8f, q);
        put(s, mi, pp + Vec2{-r * 0.05f, -r * 0.13f}, r * 0.8f, ang, top, S.kind == K::CloudStorm ? 0.3f : 0.6f, q);  // sunlit top
        break;
      }
      case K::CloudWisp: {
        PO q = o; q.mirror = rng.chance(0.5f);
        float ang = dirA + kPi * 0.5f + rng.sym() * 0.15f;
        float r = R * (1.3f + 0.8f * rng.uniform());
        Vec2 pp = p + d.perp() * rng.sym() * R * 0.5f;
        put(s, mi, pp + Vec2{0, r * 0.08f}, r, ang, C(0.7f, 0.77f, 0.9f), 0.35f, q);
        put(s, mi, pp, r, ang, C(0.97f, 0.97f, 1.f), 0.55f, q);
        break;
      }
      // ---------------------------------------------------------------- fur, ink wash, pebbles
      case K::Fur: {
        float ang = dirA + kPi * 0.5f + rng.sym() * 0.5f * (0.4f + var());
        float r = R * rng.range(0.7f, 1.2f);
        PO q = o; q.mirror = rng.chance(0.5f); q.anchor = {mi.cx, mi.y1 - 0.02f};
        Color c = natural() ? shiftHSV(S.c1, rng.sym() * 0.02f, rng.sym() * 0.05f, rng.sym() * 0.1f) : pickCol();
        Vec2 pp = p + d.perp() * rng.sym() * R * 0.4f;
        put(s, mi, pp, r * 1.15f, ang, darker(c, 0.28f), 0.5f, q);     // dense undercoat
        put(s, mi, pp, r, ang, c, 0.75f, q);
        put(s, mi, pp + Vec2{0, -r * 0.05f}, r * 0.6f, ang, lighter(c, 0.22f), 0.45f, q);  // guard hairs catch the light
        break;
      }
      case K::Wash: {  // shan-shui: pale far wash, middle wash, dark near accent (value layering = depth)
        float r = R * rng.range(0.9f, 1.25f);
        float ang = rng.sym() * 0.35f;
        Color ink = natural() ? C(0.1f, 0.12f, 0.14f) : P.color;
        PO q = o; q.mirror = rng.chance(0.5f);
        put(s, mi, p, r * 1.55f, ang, ink, 0.1f, q);
        put(s, mi, p + Vec2{-r * 0.15f, -r * 0.2f}, r * 1.05f, ang, ink, 0.2f, q);
        PO dry = q; dry.dissolve = 0.25f;
        put(s, mi, p + Vec2{-r * 0.3f, -r * 0.35f}, r * 0.55f, ang, ink, 0.42f, dry);
        break;
      }
      case K::Pebble: {
        float r = R * rng.range(0.4f, 1.15f);
        Vec2 q = p + rng.inDisc() * R * 0.9f;
        float ang = rng.uniform() * kTau;
        Color c = natural() ? shiftHSV(C(0.55f, 0.52f, 0.48f), rng.sym() * 0.05f, rng.sym() * 0.1f, rng.sym() * 0.2f) : pickCol();
        PO q2 = o; q2.mirror = rng.chance(0.5f);
        put(s, mi, q + Vec2{r * 0.14f, r * 0.2f}, r * 1.04f, ang, C(0, 0, 0), 0.32f, q2);   // contact shadow
        put(s, mi, q, r, ang, c, 1.f, q2);
        put(s, mi, q - Vec2{r * 0.1f, r * 0.14f}, r * 0.7f, ang, lighter(c, 0.2f), 0.35f, q2);  // top light
        break;
      }
    }
    prevStamp = (S.kind == K::Web || S.kind == K::DropRow) ? prevStamp : p;
    ++idx;
  }

  void trail(ISurface& s, Vec2 p, Vec2 d, float) {
    // stroboscopic dragonfly: successive wing-beat poses fading behind
    float dirA = std::atan2(d.y, d.x);
    Color c = natural() ? C(0.12f, 0.35f, 0.6f) : P.color;
    for (int k = 0; k < 4; ++k) {
      Vec2 q = p - d * (float)k * baseR() * 1.7f;
      PO o; o.aspect = 0.55f + 0.45f * flap(idx + k);
      float f = 1.f - k * 0.25f;
      put(s, mi, q, baseR() * 1.5f * (1.f - k * 0.05f), dirA + kPi * 0.5f, c, 0.95f * f, o);
    }
  }
};

void reg(BrushLibrary& lib, const Spec& sp) {
  BrushInfo i;
  i.id = "zip." + sp.mask;
  i.name = sp.name;
  i.category = sp.cat;
  i.description = sp.desc;
  i.stampName = sp.mask;
  i.defaultSize = sp.size;
  i.minSize = 3; i.maxSize = 140;
  i.suggestedColor = sp.c1; i.suggestedColor2 = sp.c2;
  i.demo = sp.demo; i.demoBg = sp.bg;
  i.animated = sp.animated; i.needsPressure = true;
  i.tags = {"carimbo", "natureza", sp.mask};
  Spec copy = sp;
  lib.add(i, [copy]() -> std::unique_ptr<Brush> { return std::make_unique<NatStamp>(copy); });
}

Spec S(const char* mask, const char* name, const char* cat, K k, float size, const char* desc) {
  Spec s; s.mask = mask; s.name = name; s.cat = cat; s.kind = k; s.size = size; s.desc = desc; return s;
}

}  // namespace

void registerStampNatureBrushes(BrushLibrary& lib) {
  const char* FL = "carimbo-flores";
  const char* LF = "carimbo-folhas";
  const char* GR = "carimbo-grama";
  const char* AN = "carimbo-animais";
  const char* EL = "carimbo-elementos";

  // ======================================================================================= flowers (filled)
  struct FlowerDef { const char* mask; const char* name; int var; std::vector<Color> pal; const char* real; const char* model; float size; };
  std::vector<FlowerDef> flowers = {
    {"stamp_flower2", "Flor de cerejeira (sakura)", 2, {C(1, .8f, .86f), C(.98f, .7f, .79f), C(1, .9f, .93f)},
     "Cinco pétalas com recorte em V, rosa-pálido que clareia nas pontas e escurece no coração; as flores brotam em cachos ao longo do galho.",
     "Flores espaçadas ao longo do traço, rotação livre, paleta de rosas, coração mais escuro por camada extra e sombra de contato; botão menor na ponta do traço.", 24},
    {"stamp_flower3", "Crisântemo", 3, {C(.98f, .85f, .2f), C(.96f, .95f, .88f), C(.92f, .52f, .16f)},
     "Dezenas de pétalas finas em raios, dispostas em camadas concêntricas; cores de amarelo, branco e bronze.",
     "Duas camadas de pétalas-raio (a interna menor, girada e mais clara) sobre sombra de contato; paleta de crisântemo.", 24},
    {"stamp_flower4", "Ameixeira (ume)", 4, {C(.9f, .35f, .45f), C(.95f, .55f, .6f), C(.85f, .2f, .36f)},
     "Flor redonda de cinco pétalas arredondadas com estames no centro; floresce ainda no inverno.",
     "Pétalas em tons de ameixa, coração escurecido e cinco estames dourados desenhados em volta do centro.", 22},
    {"stamp_flower5", "Campânula", 5, {C(.45f, .4f, .85f), C(.6f, .5f, .9f), C(.35f, .45f, .8f)},
     "Flor em sino de cinco lóbulos, azul-violeta, garganta clara.",
     "Cores azul/violeta aleatórias e garganta creme por camada pequena clara no centro.", 22},
    {"stamp_flower6", "Glória-da-manhã", 6, {C(.3f, .4f, .9f), C(.62f, .3f, .8f), C(.9f, .3f, .5f)},
     "Trombeta aberta, borda ondulada, estrela branca no fundo e miolo amarelo.",
     "Corpo colorido (azul, roxo, rosa) + estrela branca translúcida + miolo amarelo; a estrela dá o brilho típico.", 24},
    {"stamp_flower7", "Cravo", 7, {C(.9f, .15f, .25f), C(.98f, .6f, .7f), C(.97f, .94f, .94f)},
     "Pétalas franjadas em camadas amassadas, vermelho, rosa ou branco.",
     "Três camadas de recortes girados (efeito babado), a de cima mais clara, a de baixo mais escura.", 24},
    {"stamp_flower8", "Rosa", 8, {C(.8f, .1f, .15f), C(.95f, .35f, .4f), C(.98f, .8f, .3f), C(.98f, .6f, .65f)},
     "Pétalas enroladas em espiral, escuras no miolo apertado e claras nas bordas externas; a rosa fica de frente, quase sem girar.",
     "Rotação pequena (a rosa nasce 'de pé'), espiral escura em duas camadas de miolo e sombra, paleta vermelha/amarela/rosa.", 26},
  };
  for (auto& f : flowers) {
    Spec s = S(f.mask, f.name, FL, K::Flower, f.size, "");
    s.variant = f.var; s.pal = f.pal; s.c1 = f.pal[0]; s.c2 = darker(f.pal[0], 0.2f);
    s.desc = std::string("Na vida real: ") + f.real + " Modelo: " + f.model + " Tamanho cresce com a pressão.";
    s.bg = "paper"; s.demo = "wave";
    reg(lib, s);
  }

  // petal (flower1)
  { Spec s = S("stamp_flower1", "Chuva de pétalas", FL, K::Petal, 18,
      "Na vida real: pétalas soltas de sakura giram sobre o próprio eixo e planam com o vento, caindo devagar. Modelo: grupos de 3-5 pétalas por passo, caindo em ordem aleatória abaixo do traço com deriva do vento (StrokeParams.wind), rotação contínua, achatamento (aspect) simulando a pétala girando de lado e transparência crescente com a queda.");
    s.pal = {C(1, .8f, .86f), C(.98f, .7f, .79f), C(1, .9f, .93f)}; s.c1 = s.pal[0]; s.bg = "sky"; reg(lib, s); }

  // outline flowers: embossed repeating borders
  struct OutDef { const char* mask; const char* name; float step; Color line; const char* motif; float size; };
  std::vector<OutDef> outs = {
    {"stamp_flower1o", "Pétala em contorno (bordado)", 60, C(.8f, .3f, .45f), "pétala única", 16},
    {"stamp_flower2o", "Sakura em contorno (renda)", 36, C(.75f, .25f, .42f), "sakura", 22},
    {"stamp_flower3o", "Crisântemo em contorno (kamon)", 15, C(.55f, .38f, .1f), "crisântemo", 22},
    {"stamp_flower4o", "Ameixeira em contorno (kamon)", 36, C(.65f, .15f, .3f), "ume", 22},
    {"stamp_flower5o", "Campânula em contorno", 36, C(.28f, .25f, .65f), "campânula", 22},
    {"stamp_flower6o", "Glória-da-manhã em contorno", 36, C(.2f, .3f, .7f), "glória-da-manhã", 22},
    {"stamp_flower7o", "Cravo em contorno", 24, C(.7f, .1f, .2f), "cravo", 22},
    {"stamp_flower8o", "Rosa em contorno", 90, C(.6f, .08f, .12f), "rosa", 24},
  };
  for (auto& o : outs) {
    Spec s = S(o.mask, o.name, FL, K::Tile, o.size, "");
    s.a = o.step; s.pal = {o.line}; s.c1 = o.line; s.bg = "paper";
    s.desc = std::string("Na vida real: o desenho de ") + o.motif + " em contorno é o motivo dos brasões japoneses (kamon), carimbos de bordado e xilogravura: linha única, sempre idêntica, repetida em ritmo. Modelo: motivo repetido com rotação em degraus regulares (girando " + std::to_string((int)o.step) + "° a cada carimbo, nunca aleatório), tamanho com leve respiração, relevo (luz/sombra deslocadas) para parecer gravado no papel.";
    reg(lib, s);
  }

  // ======================================================================================= leaves
  { Spec s = S("stamp_leaf1", "Galho de folhas", LF, K::LeafPair, 20,
      "Na vida real: as folhas saem do galho alternadas, com um pecíolo que as afasta ~50° da direção de crescimento; a face de cima pega luz e fica mais clara, a de baixo, sombra. Modelo: traço = galho (fio marrom); em cada passo nascem folhas dos dois lados com ângulo ±50° (+jitter), âncora na base da folha, sombra, corpo e brilho superior.");
    s.directional = true; s.c1 = C(.25f, .55f, .16f); s.c2 = C(.1f, .3f, .1f); s.demo = "wave"; reg(lib, s); }
  { Spec s = S("stamp_leaf1o", "Galho de folhas em contorno", LF, K::LeafPair, 20,
      "Na vida real: ilustração botânica a nanquim, só o contorno das folhas, alternadas ao longo do ramo. Modelo: mesma filotaxia do galho colorido, mas folhas só em linha verde-escura e alternando lados (um lado por vez), sem sombra.");
    s.variant = 1; s.directional = true; s.c1 = C(.12f, .35f, .14f); reg(lib, s); }
  { Spec s = S("stamp_leaf2", "Folhas de ginkgo", LF, K::Ginkgo, 24,
      "Na vida real: leque com pecíolo longo que balança ao menor vento; fica dourado no outono, com variação verde. Modelo: leques nos dois lados do galho, ângulo base ±75° mais oscilação senoidal por frame (animação!), dourado com algumas verdes e brilho interno.");
    s.directional = true; s.animated = true; s.c1 = C(.98f, .82f, .15f); s.demo = "wave"; reg(lib, s); }
  { Spec s = S("stamp_leaf2o", "Ginkgo em contorno", LF, K::Ginkgo, 24,
      "Na vida real: ginkgo prensado/herbário, só o desenho do leque. Modelo: um leque por passo, alternando lados, cor de tinta marrom, oscilação por frame.");
    s.variant = 1; s.directional = true; s.animated = true; s.c1 = C(.5f, .35f, .1f); s.bg = "paper"; reg(lib, s); }
  { Spec s = S("stamp_leaf3", "Folhas de bordo em queda", LF, K::Maple, 22,
      "Na vida real: a folha de bordo muda de verde para amarelo, laranja e vermelho e cai girando (tombamento) em volta do eixo do pecíolo. Modelo: a cor percorre verde→amarelo→laranja→vermelho com o comprimento do traço; cada passo solta de 1 a 3 folhas que caem abaixo do traço, com achatamento por rotação (aspect), sombra e perda de opacidade na queda.");
    s.c1 = C(.95f, .65f, .1f); s.c2 = C(.75f, .12f, .08f); reg(lib, s); }
  { Spec s = S("stamp_leaf3o", "Bordo em contorno", LF, K::Tile, 22,
      "Na vida real: carimbo clássico de folha de bordo (símbolo), repetido como fita decorativa. Modelo: carimbo em linha, girando 25° a cada passo, gravado no papel.");
    s.a = 25; s.pal = {C(.7f, .2f, .08f)}; s.c1 = s.pal[0]; s.bg = "paper"; reg(lib, s); }
  { Spec s = S("stamp_leaf4", "Trevo florido (cobertura)", LF, K::Clover, 18,
      "Na vida real: o trevo cobre o chão em tapetes de folhas sobrepostas, com uma flor branca aqui e ali. Modelo: aglomerados de 3-5 folhas com rotação e tamanho aleatórios, sombra de contato entre elas e flor branca rara.");
    s.c1 = C(.2f, .55f, .18f); reg(lib, s); }
  { Spec s = S("stamp_leaf4o", "Trevo em contorno", LF, K::Tile, 20,
      "Na vida real: motivo de trevo/sakurakamon em linha, usado em fitas e carimbos. Modelo: repetição regular girando 36° por passo com relevo.");
    s.a = 36; s.pal = {C(.15f, .4f, .2f)}; s.c1 = s.pal[0]; s.bg = "paper"; reg(lib, s); }
  { Spec s = S("stamp_leaf5", "Trevo da sorte", LF, K::Clover4, 20,
      "Na vida real: em cerca de 1 a cada 5.000 trevos aparece o de quatro folhas. Modelo: tapete de trevos comuns (aglomerados) e, com ~16% de chance por passo, um trevo de quatro folhas maior, mais claro e com um brilho de sorte.");
    s.mask2 = "stamp_leaf4"; s.c1 = C(.2f, .52f, .18f); reg(lib, s); }
  { Spec s = S("stamp_leaf5o", "Trevo de quatro folhas em contorno", LF, K::Tile, 20,
      "Na vida real: amuleto desenhado em linha. Modelo: repetição alternando inclinação ±12° e espelhamento, relevo verde-escuro.");
    s.a = 0; s.b = 12; s.c = 1; s.directional = true; s.pal = {C(.12f, .45f, .22f)}; s.c1 = s.pal[0]; s.bg = "paper"; reg(lib, s); }
  { Spec s = S("stamp_leaves1", "Folhas levadas pelo vento", LF, K::LeafDrift, 22,
      "Na vida real: uma rajada carrega folhas em fila, as mais antigas ficando para trás e dispersas, girando. Modelo: cada passo desenha 4 cópias em esteira atrás do ponto, cada vez menores/transparentes, com ondulação lateral e rotação crescente.");
    s.directional = true; s.c1 = C(.45f, .55f, .15f); s.bg = "sky"; reg(lib, s); }
  { Spec s = S("stamp_leaves2", "Estilhaços de folha seca", LF, K::Shards, 22,
      "Na vida real: folha seca esfarela em lascas angulares quando pisada. Modelo: explosão de 3-5 estilhaços lançados radialmente do traço, menores e mais transparentes quanto mais longe, em tons de terra.");
    s.c1 = C(.62f, .42f, .2f); s.bg = "paper"; reg(lib, s); }

  // ======================================================================================= grass
  struct GrassDef { const char* mask; const char* name; int var; K k; Color c; float a, b, size; const char* real; };
  std::vector<GrassDef> grasses = {
    {"stamp_grass1", "Samambaia", 1, K::Fern, C(.12f, .42f, .16f), 0, 0, 30, "Na vida real: as frondes se desenrolam de dentro para fora e se abrem em leque a partir da base. Modelo: frondes alternando lados em ângulos 18-46° da vertical, crescendo (desenrolando) ao longo do traço, três camadas de sombra/luz."},
    {"stamp_grass2", "Ervas daninhas", 2, K::Grass, C(.35f, .65f, .2f), 1.5f, 1.4f, 24, "Na vida real: tufos esparsos, de alturas muito diferentes. Modelo: 1-3 tufos por passo, de pé (nunca giram com o traço), altura por ruído, inclinação do vento."},
    {"stamp_grass3", "Capim comprido", 3, K::Grass, C(.5f, .7f, .2f), 1.2f, 1.1f, 30, "Na vida real: lâminas longas que dobram para o vento, pontas mais claras ao sol. Modelo: lâminas altas, inclinação grande com o vento (StrokeParams.wind), ponta clara por camada extra."},
    {"stamp_grass4", "Capim seco do cerrado", 4, K::Grass, C(.72f, .68f, .3f), 1.4f, 1.2f, 30, "Na vida real: capim ressecado, palha amarelada, tombado em feixes. Modelo: tufos dourados com maior variação de altura e inclinação por ruído."},
    {"stamp_grass5", "Moita de capim denso", 5, K::Grass, C(.18f, .5f, .2f), 2.5f, 2.4f, 34, "Na vida real: touceira cerrada, centro escuro sombreado, pontas claras. Modelo: 2-4 touceiras por passo, camada de fundo mais escura (profundidade), pontas iluminadas."},
    {"stamp_grass6", "Juncos e taboas", 6, K::Grass, C(.42f, .5f, .22f), 0.8f, 0.8f, 36, "Na vida real: juncos altos em beira de água, retos e finos, quase sem dobrar. Modelo: poucas hastes por passo, altas, com pouca inclinação (ruído baixo)."},
    {"stamp_grass7", "Matinho com florzinhas", 7, K::Grass, C(.4f, .7f, .3f), 1.3f, 1.4f, 26, "Na vida real: ervas miúdas florescem com pontinhos brancos e amarelos na ponta da haste. Modelo: tufos pequenos com 40% de chance de florzinha na ponta de cada lâmina."},
  };
  for (auto& g : grasses) {
    Spec s = S(g.mask, g.name, GR, g.k, g.size, g.real);
    s.variant = g.var; s.c1 = g.c; s.a = g.a; s.b = g.b; s.bg = "sky"; s.demo = "line"; s.animated = true;
    s.pal = {g.c}; reg(lib, s);
  }

  // ======================================================================================= snow, animals, elements
  { Spec s = S("stamp_snow1", "Flocos de neve", EL, K::Snow, 18,
      "Na vida real: cada floco tem simetria de seis lados; os próximos da câmera parecem grandes e nítidos, os distantes, pequenos e esmaecidos (profundidade). Modelo: 3-6 flocos por passo espalhados num disco, escala e opacidade pela profundidade aleatória, brilho extra nos mais próximos, deriva do vento.");
    s.c1 = C(.86f, .94f, 1.f); s.bg = "night"; reg(lib, s); }
  { Spec s = S("stamp_snow2", "Nevasca com rastro", EL, K::SnowStreak, 18,
      "Na vida real: em queda, o floco deixa um rastro de movimento e há muita poeira de neve miúda. Modelo: cada floco com 3 fantasmas de movimento acima (direção da queda + vento) e pontinhos de neve fina em volta.");
    s.c1 = C(.9f, .96f, 1.f); s.bg = "night"; reg(lib, s); }

  { Spec s = S("stamp_dragonfly", "Libélula", AN, K::Dragonfly, 20,
      "Na vida real: a libélula bate as quatro asas, voa em zigue-zague suave e a sombra corre no chão. Modelo: um inseto a cada ~5 raios, cabeça sempre na direção do voo, ondulação lateral senoidal, batida de asas por frame (largura oscila 60-100%), cores iridescentes (turquesa, azul, vermelha) e sombra deslocada.");
    s.directional = true; s.animated = true; s.pal = {C(.1f, .55f, .65f), C(.15f, .4f, .8f), C(.8f, .2f, .15f)}; s.c1 = s.pal[0]; s.bg = "sky"; reg(lib, s); }
  { Spec s = S("stamp_dragonfly_o", "Libélula estroboscópica", AN, K::DragonflyTrail, 18,
      "Na vida real: foto de longa exposição/estroboscópica mostra cada pose do voo. Modelo: no traço, a libélula em contorno é repetida atrás com 4 poses de asa em opacidade decrescente, como quadros-fantasma de animação (smear).");
    s.directional = true; s.animated = true; s.c1 = C(.12f, .35f, .6f); s.bg = "paper"; reg(lib, s); }
  { Spec s = S("stamp_swallowtail", "Borboleta cauda-de-andorinha", AN, K::Butterfly, 22,
      "Na vida real: voo irregular, em ondas, com asas abrindo/fechando por completo; amarela com padrão escuro. Modelo: trajetória ondulada em dois eixos, batida de asas por frame (largura 28-100%, fecha quase de perfil), amarelo com camada interna escura simulando o padrão, sombra.");
    s.directional = true; s.animated = true; s.pal = {C(1, .86f, .2f), C(.98f, .75f, .15f)}; s.c1 = s.pal[0]; s.bg = "sky"; reg(lib, s); }
  { Spec s = S("stamp_feather", "Penas ao vento", AN, K::Feather, 22,
      "Na vida real: a pena cai balançando como pêndulo, o ar dobra a lâmina e ela muda de lado. Modelo: penas alternando em ambos os lados do traço, ângulo oscilando por frame, ordem de pilha variada, cor por paleta (branca, creme, azul, cinza) com leve brilho.");
    s.directional = true; s.animated = true; s.pal = {C(.97f, .97f, .94f), C(.35f, .55f, .85f), C(.8f, .78f, .72f), C(.6f, .62f, .68f)}; s.c1 = s.pal[0]; s.bg = "sky"; reg(lib, s); }

  { Spec s = S("stamp_drop", "Gotas sobre vidro", EL, K::Drop, 14,
      "Na vida real: gota em superfície é uma lente: borda espessa que entorta a luz (escura), centro claro, reflexo de janela e, às vezes, escorre deixando rastro molhado. Modelo: anel escuro, corpo claro, 2 reflexos brancos, sombra projetada e 18% de chance de rastro úmido.");
    s.c1 = C(.3f, .55f, .85f); s.demo = "wave"; s.bg = "sky"; reg(lib, s); }
  { Spec s = S("stamp_drop_o", "Orvalho no fio", EL, K::DropRow, 18,
      "Na vida real: gotas de orvalho pendem de uma teia/fio, grandes no meio, menores nas pontas. Modelo: o traço vira um fio fino; as gotas (contorno + preenchimento claro) penduradas pela ponta, tamanho pela pressão, reflexo de luz.");
    s.c1 = C(.2f, .45f, .8f); s.bg = "paper"; reg(lib, s); }
  { Spec s = S("stamp_splash", "Respingo de tinta", EL, K::Splash, 22,
      "Na vida real: ao bater, a gota espalha uma poça irregular e lança gotículas alongadas na direção do movimento, menores e mais finas quanto mais longe. Modelo: mancha com rotação aleatória + brilho úmido + 7-13 gotículas elípticas alinhadas ao traço.");
    s.c1 = C(.62f, .05f, .08f); reg(lib, s); }
  { Spec s = S("stamp_spider_net", "Teia de aranha", EL, K::Web, 24,
      "Na vida real: a teia é radial (fios secos) mais espiral; o orvalho forma pérolas que refletem luz, e fios-âncora ligam uma teia à outra. Modelo: teia grande no início, teias menores ao longo do traço ligadas por fio com curva (catenária), gotas de orvalho com sombra e brilho.");
    s.c1 = C(.9f, .94f, 1.f); s.bg = "night"; reg(lib, s); }

  { Spec s = S("stamp_cat_footprint_one", "Pegadas de gato", AN, K::Foot, 15,
      "Na vida real: o gato anda em linha, pata direita e esquerda alternadas; as marcas ficam mais fracas à medida que ele se afasta e a almofada central afunda mais. Modelo: espelha a pata a cada passo, desloca ±0.75R do centro, gira com a direção, passada = 3.2R, esmaece e encolhe com a distância, sujeira de lama atrás.");
    s.directional = true; s.c1 = C(.33f, .21f, .12f); s.bg = "paper"; s.demo = "line"; reg(lib, s); }
  { Spec s = S("stamp_cat_footprint_one_o", "Pegadas na neve (relevo)", AN, K::FootEmboss, 15,
      "Na vida real: pegada na neve é um relevo: lado sombreado e lado iluminado. Modelo: contorno desenhado em azul-sombra com highlight branco deslocado, esmaecendo ao longe.");
    s.directional = true; s.c1 = C(.55f, .65f, .8f); s.bg = "sky"; s.demo = "line"; reg(lib, s); }
  { Spec s = S("stamp_cat_footprints_both", "Pegadas de pata dupla", AN, K::FootBoth, 20,
      "Na vida real: ao trotar, patas da frente e de trás pousam quase juntas (par). Modelo: carimbo-par girado para a direção, alterna espelhamento, passada 4.6R, esmaece e encolhe, leve sombra.");
    s.directional = true; s.c1 = C(.2f, .19f, .22f); s.bg = "paper"; s.demo = "line"; reg(lib, s); }
  { Spec s = S("stamp_cat_footprints_both_o", "Pegadas arco-íris", AN, K::FootRainbow, 20,
      "Na vida real: pata suja de tinta colorida deixa marcas que mudam de cor a cada rodada de pintura. Modelo: par em contorno com cor que percorre o círculo cromático ao longo do traço (ou do degradê cor→cor2).");
    s.directional = true; s.c1 = C(.9f, .4f, .5f); s.bg = "paper"; s.demo = "line"; reg(lib, s); }

  // ======================================================================================= clouds / fur / ink / pebbles
  { Spec s = S("clouds01", "Nuvem de tempestade", EL, K::CloudStorm, 36,
      "Na vida real: cumulonimbus tem base escura e plana, corpo cinza-azulado e topo claro onde pega sol. Modelo: três passadas por nuvem (sombra, corpo, topo) com espelhamento e tamanho aleatórios.");
    s.c1 = C(.42f, .45f, .54f); s.bg = "sky"; reg(lib, s); }
  { Spec s = S("clouds02", "Cúmulo de bom tempo", EL, K::CloudPuff, 36,
      "Na vida real: cúmulos são brancos, com base azulada em sombra e topo quente de sol. Modelo: base azulada, corpo branco, topo creme; empilhamento denso para formar bancos.");
    s.c1 = C(.97f, .98f, 1.f); s.bg = "sky"; reg(lib, s); }
  { Spec s = S("clouds03", "Cirros e virga", EL, K::CloudWisp, 36,
      "Na vida real: cirros são fios de gelo esticados pelo vento em altitude; virga é chuva que evapora antes de tocar o chão. Modelo: fiapos alinhados perpendicularmente à direção do traço, duas camadas translúcidas (sombra e luz) sobre o céu.");
    s.directional = true; s.c1 = C(.97f, .97f, 1.f); s.bg = "sky"; reg(lib, s); }
  { Spec s = S("fur01", "Tufos de pelo", EL, K::Fur, 22,
      "Na vida real: o pelo tem subpelo denso escuro e pelos-guarda mais longos e claros que pegam a luz, todos deitados no sentido do corpo. Modelo: tufos muito densos, orientados pela direção do traço com ±jitter, em três camadas (subpelo, pelo, brilho).");
    s.directional = true; s.c1 = C(.55f, .38f, .2f); s.bg = "paper"; s.demo = "wave"; reg(lib, s); }
  { Spec s = S("oriental_shan_shui", "Rochedo de nanquim (shan shui)", EL, K::Wash, 28,
      "Na vida real: na pintura chinesa shan shui, a profundidade vem de camadas de tinta cada vez mais escuras: pálido (longe), médio e acento escuro (perto), com pincel seco nas bordas. Modelo: três lavagens sobrepostas por carimbo (10%, 20%, 42% de tinta), a escura menor, deslocada e com dissolve para efeito de pincel seco.");
    s.c1 = C(.1f, .12f, .14f); s.bg = "paper"; reg(lib, s); }
  { Spec s = S("pierced", "Seixos furados", EL, K::Pebble, 20,
      "Na vida real: pedras furadas pela água formam cascalho; cada uma projeta sombra no chão e tem luz no topo. Modelo: seixos de tamanhos 0.4-1.15R espalhados, cinzas/terrosos, sombra de contato projetada, luz no topo.");
    s.c1 = C(.55f, .52f, .48f); s.bg = "paper"; reg(lib, s); }
}

}  // namespace ab
