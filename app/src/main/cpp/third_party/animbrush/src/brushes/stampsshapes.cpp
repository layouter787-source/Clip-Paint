// Module stampsshapes: zip stamps of shapes / symbols / texture tips. One brush per mask ("zip.<mask>").
// Every mask has its own behaviour (rolling gears, spinning wheels, rising hearts, twinkling kira flares,
// faceted gems, crowns, perpendicular flat tips, chains, ropes, speed lines, canvas-anchored meshes, spatter physics...).
#include <set>

#include "../../include/animbrush/brush.hpp"
#include "../../include/animbrush/library.hpp"

namespace ab {
namespace {

enum class M : uint8_t {
  Plain, Roll, Spin, Float, Note, Twinkle, Gem, Crown, Bevel, Flat, Chain, Rope, Lace, SpeedL, Wedge, Mesh, Burst, Spatter, Glow
};

struct Cfg {
  M m = M::Plain;
  float spacing = 1.f, opacity = 1.f, rot = 0.f;
  int rotMode = 0;  // 0 fixed, 1 along direction, 2 random, 3 direction+90
  float pSize = 0.6f, pOpac = 0.f, scatter = 0.f, sj = 0.f, hj = 0.f, satj = 0.f, vj = 0.f, aspect = 1.f, dissolve = 0.f;
  int count = 1;
  Blend blend = Blend::Normal;
  bool mirror = false, dbl = false;
  float k1 = 0.f, k2 = 0.f;
  std::string grain, glint;
  float grainDepth = 0.f;
  Cfg() = default;
  explicit Cfg(M mm) : m(mm) {}
  Cfg& sp(float v) { spacing = v; return *this; }
  Cfg& op(float v) { opacity = v; return *this; }
  Cfg& rotm(float deg, int mode) { rot = deg; rotMode = mode; return *this; }
  Cfg& ps(float a, float b = 0.f) { pSize = a; pOpac = b; return *this; }
  Cfg& scat(float s, int n = 1) { scatter = s; count = n; return *this; }
  Cfg& jit(float v) { sj = v; return *this; }
  Cfg& col(float h, float s, float v) { hj = h; satj = s; vj = v; return *this; }
  Cfg& bl(Blend b) { blend = b; return *this; }
  Cfg& asp(float a) { aspect = a; return *this; }
  Cfg& dis(float d) { dissolve = d; return *this; }
  Cfg& kk(float a, float b = 0.f) { k1 = a; k2 = b; return *this; }
  Cfg& gr(const char* n, float d) { grain = n; grainDepth = d; return *this; }
  Cfg& mir() { mirror = true; return *this; }
  Cfg& dbline() { dbl = true; return *this; }
  Cfg& gl(const char* n) { glint = n; return *this; }
};

class ShapeBrush : public Brush {
 public:
  ShapeBrush(BrushInfo bi, std::string mask, Cfg c) : mask_name_(std::move(mask)), c_(std::move(c)) {
    info_ = std::move(bi);
    addParam("espacamento", "Espaçamento", c_.spacing, 0.1f, 6.f);
    addParam("espalhar", "Espalhamento", c_.scatter, 0.f, 4.f);
    addParam("densidade", "Densidade", (float)c_.count, 1.f, 10.f);
    addParam("variacao", "Variação de tamanho", c_.sj, 0.f, 1.f);
    if (c_.m == M::Flat) addParam("angulo_fixo", "Ângulo fixo (0 = perpendicular ao traço)", c_.rotMode == 0 ? 1.f : 0.f, 0.f, 1.f);
  }

  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    s_ = &s; sp_ = sp; rng_.reseed(sp.seed);
    auto& bank = StampBank::instance();
    mask_ = bank.get(mask_name_);
    glint_ = c_.glint.empty() ? nullptr : bank.get(c_.glint);
    grain_ = c_.grain.empty() ? nullptr : bank.get(c_.grain);
    last_ = {in.x, in.y}; dir_ = {1, 0}; carry_ = arc_ = speed_ = 0; idx_ = 0; spin_ = 0;
    pressure_ = in.pressure; cells_.clear();
    if (c_.m == M::Note) staff(last_, last_);
    emit({in.x, in.y});
  }

  void move(ISurface& s, const InputSample& in) override {
    s_ = &s;
    Vec2 cur{in.x, in.y}; Vec2 seg = cur - last_; float L = seg.len();
    pressure_ = in.pressure;
    float dt = std::max(1e-4f, in.dt);
    speed_ += (L / dt - speed_) * 0.3f;
    if (L < 0.01f) return;
    dir_ = (dir_ * 0.5f + seg / L * 0.5f).norm();
    if (c_.m == M::Mesh) { meshStep(cur); last_ = cur; arc_ += L; return; }
    float pos = 0;
    Vec2 a = last_;
    for (;;) {
      float r = radius();
      float stepPx = std::max(1.f, param("espacamento") * r);
      pos += (pos == 0 && carry_ > 0) ? std::max(0.f, stepPx - carry_) : stepPx;
      if (pos > L) { carry_ = L - (pos - stepPx); if (carry_ < 0) carry_ = 0; break; }
      carry_ = 0;
      Vec2 p = a + seg / L * pos;
      arc_ += stepPx;
      emit(p);
    }
    if (c_.m == M::Note) staff(last_, cur);
    last_ = cur;
  }

  void end(ISurface&) override {}

 private:
  float radius() const {
    float k = 1.f - c_.pSize + c_.pSize * pressure_;
    return std::max(1.5f, sp_.size * k);
  }
  float speedNorm() const { return 1.f - std::exp(-speed_ / 900.f); }

  Color tint() {
    Color col = sp_.color;
    float dh = c_.hj > 0 ? rng_.sym() * c_.hj : 0.f, ds = c_.satj > 0 ? rng_.sym() * c_.satj : 0.f, dv = c_.vj > 0 ? rng_.sym() * c_.vj : 0.f;
    if (dh != 0 || ds != 0 || dv != 0) col = shiftHSV(col, dh, ds, dv);
    col.a = 1.f; return col;
  }
  float baseOpacity() const { return c_.opacity * sp_.opacity * (1.f - c_.pOpac + c_.pOpac * pressure_); }

  Dab mk(Vec2 p, float r, float ang, Color col, float op, Blend b, const Mask* m, float asp = -1.f) {
    Dab d; d.x = p.x; d.y = p.y; d.radius = std::max(0.3f, r); d.aspect = asp > 0 ? asp : c_.aspect; d.angle = ang;
    d.color = col; d.opacity = clampf(op); d.blend = b; d.shape = m; d.hardness = 1.f; d.lockAlpha = sp_.lockAlpha;
    d.seed = hash32(sp_.seed * 7919u + (uint32_t)idx_ * 131u);
    if (m == mask_ && grain_ && c_.grainDepth > 0) { d.grain = grain_; d.grainDepth = c_.grainDepth; d.grainScale = 1.f; }
    return d;
  }
  void put(const Dab& d) {
    s_->drawDab(d);
    if (c_.dbl) {  // hand-drawn double line for outline stamps
      Dab e = d; e.x += rng_.sym() * d.radius * 0.035f; e.y += rng_.sym() * d.radius * 0.035f; e.angle += rng_.sym() * 0.05f; e.opacity *= 0.55f;
      s_->drawDab(e);
    }
  }
  float angleFor(float jitterDeg = 0.f) {
    float dirA = std::atan2(dir_.y, dir_.x), a = c_.rot * kDeg2Rad;
    switch (c_.rotMode) {
      case 1: a += dirA; break;
      case 2: a += rng_.uniform() * kTau; break;
      case 3: a += dirA + kPi * 0.5f; break;
      default: break;
    }
    if (jitterDeg > 0) a += rng_.sym() * jitterDeg * kDeg2Rad;
    return a;
  }
  Vec2 spread(float r) {
    float sc = param("espalhar");
    return sc > 0 ? rng_.inDisc() * (sc * r) : Vec2{0, 0};
  }
  float sizeJit(float r) { float j = param("variacao"); return j > 0 ? r * std::max(0.25f, 1.f + rng_.sym() * j) : r; }
  int copies() { return std::max(1, (int)std::lround(param("densidade"))); }

  void staff(Vec2 a, Vec2 b) {  // 5 thin lines (musical staff) following the stroke
    float g = sp_.size * 0.22f;
    Vec2 n = dir_.perp();
    Dab pr = mk({0, 0}, 0.7f, 0, sp_.color, 0.45f * sp_.opacity, Blend::Normal, nullptr, 1.f);
    for (int k = -2; k <= 2; ++k) dabLine(*s_, a + n * (k * g), b + n * (k * g), 0.7f, 0.7f, pr, 0.5f, nullptr);
  }

  void meshStep(Vec2 p) {
    float cell = std::max(4.f, sp_.size * 1.9f * param("espacamento"));
    long gx = (long)std::lround(p.x / cell), gy = (long)std::lround(p.y / cell);
    float reach = sp_.size * 0.9f;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        long cx = gx + dx, cy = gy + dy;
        Vec2 c{cx * cell, cy * cell};
        if (dist(c, p) > reach) continue;
        long long key = ((long long)cx << 32) ^ (long long)(cy & 0xffffffffLL);
        if (!cells_.insert(key).second) continue;
        Dab d = mk(c, sp_.size, 0.f, sp_.color, baseOpacity(), c_.blend, mask_);
        put(d); ++idx_;
      }
  }

  void emit(Vec2 pos) {
    float r0 = radius();
    int n = copies();
    float dirA = std::atan2(dir_.y, dir_.x);
    switch (c_.m) {
      case M::Plain: {
        for (int i = 0; i < n; ++i) {
          float r = sizeJit(r0);
          Vec2 p = pos + spread(r0);
          float asp = c_.aspect + c_.k1 * speedNorm();
          Dab d = mk(p, r, angleFor(), tint(), baseOpacity() * (c_.k2 > 0 ? 1.f - c_.k2 * rng_.uniform() : 1.f), c_.blend, mask_, asp);
          d.mirrorX = c_.mirror && rng_.chance(0.5f);
          d.dissolve = c_.dissolve; put(d); ++idx_;
        }
        break;
      }
      case M::Bevel: {  // solid geometric tile with bevel: shadow, body, inner highlight
        float r = sizeJit(r0);
        float a = angleFor(c_.k1);
        if (c_.mirror && (idx_ & 1)) a += kPi;  // alternate flip so triangles tessellate
        Color col = tint();
        Vec2 sh{r * 0.06f, r * 0.09f};
        put(mk(pos + sh, r, a, Color{0, 0, 0, 1}, 0.22f * baseOpacity(), Blend::Multiply, mask_));
        put(mk(pos, r, a, col, baseOpacity(), c_.blend, mask_));
        put(mk(pos + Vec2{-r * 0.05f, -r * 0.07f}, r * 0.74f, a, shiftHSV(col, 0, -0.35f, 0.35f), 0.45f * baseOpacity(), Blend::Screen, mask_));
        put(mk(pos + Vec2{r * 0.05f, r * 0.07f}, r * 0.9f, a, shiftHSV(col, 0, 0.1f, -0.35f), 0.22f * baseOpacity(), Blend::Multiply, mask_));
        ++idx_; break;
      }
      case M::Roll: {  // gear / wheel rolls on the path: angle = arc / radius, neighbours counter-rotate
        float sgn = (idx_ & 1) ? -1.f : 1.f;
        float a = sgn * (arc_ / r0) + ((idx_ & 1) ? kPi / std::max(4.f, c_.k1) : 0.f) + c_.rot * kDeg2Rad;
        Color col = tint();
        if (c_.k2 > 0) put(mk(pos + Vec2{r0 * 0.05f, r0 * 0.08f}, r0, a, Color{0, 0, 0, 1}, c_.k2 * baseOpacity(), Blend::Multiply, mask_));
        put(mk(pos, r0, a, (idx_ & 1) ? shiftHSV(col, 0.02f, 0, -0.08f) : col, baseOpacity(), c_.blend, mask_));
        ++idx_; break;
      }
      case M::Spin: {  // spins with pen speed; motion-blur ghosts when fast; frame advances it for animation
        spin_ += 0.16f + speed_ * 0.0007f;
        float a = spin_ + sp_.frame * 0.38f + c_.rot * kDeg2Rad;
        Color col = tint();
        float sn = speedNorm();
        if (sn > 0.35f)
          for (int g = 3; g >= 1; --g) put(mk(pos, r0, a - g * 0.22f * sn * 2.f, col, baseOpacity() * 0.18f, c_.blend, mask_));
        put(mk(pos, r0, a, col, baseOpacity(), c_.blend, mask_));
        ++idx_; break;
      }
      case M::Float: {  // emotes: a main stamp on the path + copies rising, drifting with wind and fading
        float lift = sp_.size * (c_.k1 > 0 ? c_.k1 : 3.f);
        for (int i = 0; i < n; ++i) {
          float rise = std::pow(rng_.uniform(), 1.4f) * lift;
          float f = rise / lift;
          Vec2 p = pos + Vec2{rng_.sym() * r0 * 0.7f + sp_.wind * rise * 0.5f + std::sin(rise * 0.05f + idx_) * r0 * 0.4f, -rise};
          float r = sizeJit(r0) * (1.f - 0.55f * f) * (0.75f + 0.5f * pressure_);
          Color col = tint();
          put(mk(p, r, (rng_.sym() * 0.35f) + c_.rot * kDeg2Rad, col, baseOpacity() * (1.f - 0.85f * f), c_.blend, mask_));
          ++idx_;
        }
        break;
      }
      case M::Note: {  // notes sit on staff positions (pitch), stems up, slight tilt along the stroke
        for (int i = 0; i < n; ++i) {
          int pitch = rng_.irange(-4, 4);
          float g = sp_.size * 0.22f * 0.5f;
          Vec2 p = pos + dir_.perp() * (pitch * g) + dir_ * (rng_.sym() * r0 * 0.4f);
          float r = sizeJit(r0) * 0.9f;
          put(mk(p, r, rng_.sym() * 0.2f + dirA * 0.15f, tint(), baseOpacity(), c_.blend, mask_));
          ++idx_;
        }
        break;
      }
      case M::Twinkle: {  // stars/kira: power-law sizes (few big, many tiny), glints
        for (int i = 0; i < n; ++i) {
          float t = rng_.uniform();
          float r = r0 * (0.25f + 1.1f * t * t * t) * (1.f + (c_.sj > 0 ? rng_.sym() * c_.sj : 0.f));
          Vec2 p = pos + spread(r0);
          float a = c_.rotMode == 2 ? rng_.uniform() * kTau : (c_.rot + (rng_.chance(0.3f) ? 45.f * c_.k2 : 0.f)) * kDeg2Rad;
          Color col = tint();
          float tw = 0.55f + 0.45f * std::sin(sp_.frame * 0.9f + idx_ * 1.7f);  // twinkle over animation frames
          put(mk(p, r, a, col, baseOpacity() * (c_.k1 > 0 ? tw : 1.f), c_.blend, mask_));
          if (glint_ && rng_.chance(0.45f)) {
            Dab g = mk(p, r * 1.5f, 0.f, Color{1, 1, 0.9f, 1}, 0.8f * baseOpacity(), Blend::Add, glint_); put(g);
          }
          ++idx_;
        }
        break;
      }
      case M::Gem: {  // facet body + white inner light + chromatic dispersion fringes + glint
        float r = sizeJit(r0);
        Vec2 p = pos + spread(r0);
        float a = angleFor(c_.k1);
        Color col = tint();
        put(mk(p, r, a, col, baseOpacity(), Blend::Normal, mask_));
        put(mk(p + Vec2{-r * 0.04f, -r * 0.06f}, r * 0.7f, a, Color{1, 1, 1, 1}, 0.35f * baseOpacity(), Blend::Screen, mask_));
        put(mk(p + Vec2{r * 0.025f, 0}, r, a, Color{1.f, 0.15f, 0.2f, 1}, 0.18f * baseOpacity(), Blend::Add, mask_));
        put(mk(p - Vec2{r * 0.025f, 0}, r, a, Color{0.15f, 0.4f, 1.f, 1}, 0.18f * baseOpacity(), Blend::Add, mask_));
        const Mask* g = StampBank::instance().get("stamp_kira4");
        if (g && rng_.chance(0.6f)) put(mk(p + Vec2{-r * 0.3f, -r * 0.35f}, r * 0.8f, 0.f, Color{1, 1, 1, 1}, 0.9f * baseOpacity(), Blend::Add, g));
        ++idx_; break;
      }
      case M::Crown: {  // upright regalia with metal gradient: shadow, body, bright rim from top light
        float r = r0;
        Color col = tint();
        put(mk(pos + Vec2{r * 0.04f, r * 0.07f}, r, 0.f, Color{0, 0, 0, 1}, 0.25f * baseOpacity(), Blend::Multiply, mask_));
        put(mk(pos, r, 0.f, shiftHSV(col, 0, 0.05f, -0.12f), baseOpacity(), c_.blend, mask_));
        put(mk(pos + Vec2{0, -r * 0.1f}, r * 0.82f, 0.f, shiftHSV(col, -0.01f, -0.25f, 0.25f), 0.55f * baseOpacity(), Blend::Screen, mask_));
        const Mask* g = StampBank::instance().get("stamp_kira4");
        if (g && rng_.chance(0.5f)) put(mk(pos + Vec2{-r * 0.25f, -r * 0.2f}, r * 0.35f, 0.f, Color{1, 1, 0.85f, 1}, 0.8f * baseOpacity(), Blend::Add, g));
        ++idx_; break;
      }
      case M::Flat: {  // flat tip: long axis perpendicular to travel (or fixed), bristle streaks, dry-brush skipping when fast
        bool fixedAng = param("angulo_fixo") > 0.5f;
        float a = fixedAng ? c_.rot * kDeg2Rad : dirA + c_.rot * kDeg2Rad;
        float sn = speedNorm();
        Dab d = mk(pos, r0, a, tint(), baseOpacity() * (1.f - 0.35f * sn), c_.blend, mask_);
        d.dissolve = c_.dissolve + sn * c_.k1;
        d.mirrorX = false; put(d); ++idx_; break;
      }
      case M::Chain: {  // links alternate: face-on and edge-on (rolled 90 deg), highlight on each
        bool edge = (idx_ & 1) != 0;
        Color col = tint();
        float r = edge ? r0 * 0.38f : r0 * 0.85f;
        float asp = edge ? (r0 * 0.85f) / (r0 * 0.38f) : 1.f;
        put(mk(pos + Vec2{r0 * 0.02f, r0 * 0.05f}, r, dirA, Color{0, 0, 0, 1}, 0.25f * baseOpacity(), Blend::Multiply, mask_, asp));
        put(mk(pos, r, dirA, edge ? shiftHSV(col, 0, 0, -0.18f) : col, baseOpacity(), c_.blend, mask_, asp));
        put(mk(pos + Vec2{-r * 0.05f, -r * 0.1f}, r * 0.8f, dirA, Color{1, 1, 1, 1}, 0.28f * baseOpacity(), Blend::Screen, mask_, asp));
        ++idx_; break;
      }
      case M::Rope: {  // twisted strands: phase advances with arc length; light/dark alternate as strands cross over
        float ph = arc_ * 0.5f / std::max(4.f, r0);
        float s = std::sin(ph * kPi);
        Color col = shiftHSV(tint(), 0, 0, 0.14f * s);
        float r = r0 * (0.8f + 0.2f * std::fabs(s));
        Dab d = mk(pos, r, dirA, col, baseOpacity(), c_.blend, mask_, 1.f);
        d.mirrorX = (idx_ & 1) != 0; put(d); ++idx_; break;
      }
      case M::Lace: {  // doily edge tiles side by side (mirrored alternately), hem towards one side
        float r = r0;
        Dab d = mk(pos, r, dirA, tint(), baseOpacity(), c_.blend, mask_);
        d.mirrorX = (idx_ & 1) != 0; put(d); ++idx_; break;
      }
      case M::SpeedL: {  // speed lines: length & opacity scale with pen speed, trailing behind, scattered across the stroke
        float sn = speedNorm();
        for (int i = 0; i < n; ++i) {
          float off = rng_.sym() * r0 * 1.3f;
          float len = (0.8f + 2.8f * sn) * (0.5f + rng_.uniform());
          Vec2 p = pos - dir_ * (len * r0 * 0.6f) + dir_.perp() * off;
          Dab d = mk(p, r0 * 0.55f, dirA, tint(), baseOpacity() * (0.25f + 0.75f * sn), c_.blend, mask_, std::max(1.f, len));
          put(d); ++idx_;
        }
        break;
      }
      case M::Wedge: {  // calligraphic taper-in, direction-aligned; k1 = parallel copies (pile), k2 = tail length
        float ramp = smoothstep(0.f, std::max(10.f, r0 * 3.f), arc_);
        int copiesN = std::max(1, (int)c_.k1);
        for (int i = 0; i < copiesN; ++i) {
          float off = copiesN > 1 ? (i - (copiesN - 1) * 0.5f) * r0 * 0.55f : 0.f;
          float r = r0 * (0.25f + 0.75f * ramp) * (copiesN > 1 ? 0.5f : 1.f);
          Vec2 p = pos + dir_.perp() * off;
          put(mk(p, r, dirA, tint(), baseOpacity() * (0.45f + 0.55f * ramp), c_.blend, mask_, 1.f + c_.k2 * speedNorm()));
        }
        ++idx_; break;
      }
      case M::Burst: {  // fireworks: radial dash bursts, rainbow hue per burst, random orientation
        float r = sizeJit(r0);
        Vec2 p = pos + spread(r0);
        Color col = shiftHSV(sp_.color, rng_.uniform(), 0.0f, 0.f);
        put(mk(p, r, rng_.uniform() * kTau, col, baseOpacity(), c_.blend, mask_));
        ++idx_; break;
      }
      case M::Spatter: {  // flick physics: drops thrown forward along motion, big & near, tiny & far; stretched by speed
        float sn = speedNorm();
        for (int i = 0; i < n; ++i) {
          float t = std::pow(rng_.uniform(), 2.f);
          float d = r0 * (0.3f + 3.8f * t) * (0.6f + sn);
          Vec2 p = pos + dir_ * d + dir_.perp() * (rng_.gauss() * r0 * 0.55f);
          float r = r0 * (0.7f - 0.5f * t) * (0.5f + rng_.uniform());
          Dab dd = mk(p, r, dirA + rng_.sym() * 0.3f, tint(), baseOpacity() * (1.f - 0.4f * t), c_.blend, mask_, 1.f + 1.4f * sn * t);
          put(dd); ++idx_;
        }
        break;
      }
      case M::Glow: {  // bloom: wide coloured halo + hot core (colour shifts toward white)
        float r = sizeJit(r0);
        Color col = tint();
        Vec2 p = pos + spread(r0);
        put(mk(p, r, 0.f, col, 0.55f * baseOpacity(), Blend::Add, mask_));
        put(mk(p, r * 0.45f, 0.f, mix(col, Color{1, 1, 1, 1}, 0.7f), 0.6f * baseOpacity(), Blend::Add, mask_));
        ++idx_; break;
      }
    }
  }

  float param(const std::string& k) const {
    for (auto& p : params_) if (p.key == k) return p.value;
    return 0.f;
  }

  std::string mask_name_;
  Cfg c_;
  ISurface* s_ = nullptr;
  StrokeParams sp_;
  Rng rng_;
  const Mask *mask_ = nullptr, *glint_ = nullptr, *grain_ = nullptr;
  Vec2 last_{}, dir_{1, 0};
  float carry_ = 0, arc_ = 0, speed_ = 0, pressure_ = 1, spin_ = 0;
  int idx_ = 0;
  std::set<long long> cells_;
};

}  // namespace

void registerStampShapeBrushes(BrushLibrary& lib) {
  struct R {
    BrushLibrary& lib;
    void operator()(const char* mask, const char* name, const char* cat, const char* desc, const Cfg& cfg, Color col, float size, float mn, float mx,
                    const char* demo = "wave", const char* bg = "", bool additive = false, Color col2 = Color{1, 1, 1, 1}) {
      BrushInfo b;
      b.id = std::string("zip.") + mask; b.name = name; b.category = cat; b.description = desc; b.stampName = mask;
      b.defaultSize = size; b.minSize = mn; b.maxSize = mx; b.suggestedColor = col; b.suggestedColor2 = col2;
      b.demo = demo; b.demoBg = bg; b.additive = additive; b.needsPressure = cfg.pSize > 0.3f;
      b.animated = (cfg.m == M::Spin || (cfg.m == M::Twinkle && cfg.k1 > 0));
      b.tags = {"zip", "carimbo", cat};
      std::string m = mask;
      lib.add(b, [b, m, cfg]() -> std::unique_ptr<Brush> { return std::make_unique<ShapeBrush>(b, m, cfg); });
    }
  } R{lib};

  const Color gold = Color::hex(0xD9A521), red = Color::hex(0xE23A52), ink = Color::hex(0x23283A), steel = Color::hex(0x7A8696);
  const char* F = "carimbo-formas";
  const char* S = "carimbo-simbolos";
  const char* FL = "ponta-achatada";
  const char* NT = "ponta-rede";
  const char* PT = "ponta-particulas";

  // ---------------- basic shapes (geometry with bevel) ----------------
  R("stamp_circle", "Disco chanfrado", F, "Na vida real: botões, fichas e pastilhas têm borda arredondada e brilho no topo. Modelo: disco com sombra projetada, corpo e luz interna deslocada; espaçamento regular, tamanho por pressão.",
    Cfg(M::Bevel).sp(1.15f).jit(0.15f).col(0.04f, 0.1f, 0.12f), Color::hex(0x3F8FD6), 22, 4, 120);
  R("stamp_pcir_i", "Anel suave (interno)", F, "Na vida real: lente de luz com miolo vazio e aro difuso (bokeh). Modelo: anéis de bordas moles que se sobrepõem por soma de luz, deslocados aleatoriamente — lembram bolhas de bokeh.",
    Cfg(M::Plain).sp(0.7f).op(0.5f).scat(0.5f, 1).jit(0.5f).bl(Blend::Add).col(0.1f, 0.1f, 0.1f), Color::hex(0xFFC070), 22, 4, 120, "wave", "dark", true);
  R("stamp_pcir_o", "Anel suave (externo)", F, "Na vida real: halo de lâmpada fora de foco, mais brilhante na borda. Modelo: anéis grandes de aro externo com opacidade baixa e acumulada, cor variando por matiz.",
    Cfg(M::Plain).sp(0.9f).op(0.4f).scat(0.7f, 1).jit(0.6f).bl(Blend::Add).col(0.15f, 0.1f, 0.1f), Color::hex(0x70C8FF), 26, 6, 140, "wave", "dark", true);
  R("stamp_square", "Ladrilho quadrado", F, "Na vida real: azulejos e pixels ficam lado a lado com juntas finas. Modelo: quadrados que acompanham a direção do traço, com chanfro e variação sutil de tom, formando mosaico.",
    Cfg(M::Bevel).sp(2.05f).rotm(0, 1).col(0.02f, 0.08f, 0.1f), Color::hex(0xE0813A), 16, 4, 80);
  R("stamp_poly3", "Triângulo tesselado", F, "Na vida real: triângulos equiláteros encaixam virando o sentido a cada peça. Modelo: triângulos alternando 180° ao longo do traço, para formar faixas de padrão geométrico.",
    Cfg(M::Bevel).sp(1.35f).rotm(-90, 1).mir().col(0.03f, 0.1f, 0.08f), Color::hex(0x42B883), 18, 4, 90);
  R("stamp_poly3b", "Cunha triangular", F, "Na vida real: lascas e pontas de seta cortantes. Modelo: cunha orientada pela direção do movimento, comprimento cresce com a pressão, leve rotação aleatória como estilhaço.",
    Cfg(M::Bevel).sp(1.1f).rotm(0, 1).kk(10).ps(0.8f).scat(0.2f, 1), Color::hex(0x9B59B6), 20, 4, 100);
  R("stamp_poly5", "Pentágono facetado", F, "Na vida real: pentágonos formam bolas de futebol e cristais. Modelo: pentágonos girando aleatoriamente em múltiplos de 72° com bisel de luz, escala variável.",
    Cfg(M::Bevel).sp(1.4f).rotm(0, 2).jit(0.3f).col(0.05f, 0.1f, 0.1f), Color::hex(0xD35D6E), 18, 4, 90);
  R("stamp_poly6", "Colmeia hexagonal", F, "Na vida real: os favos das abelhas são hexágonos que encaixam sem folga. Modelo: hexágonos com espaçamento de encaixe (√3·r/2), cor em tons de mel e bisel de cera.",
    Cfg(M::Bevel).sp(1.74f).rotm(0, 0).col(0.02f, 0.1f, 0.12f), Color::hex(0xF2A900), 16, 4, 80);
  R("stamp_ppoly5_i", "Pentágono macio (interno)", F, "Na vida real: formas desfocadas de abertura de lente pentagonal (íris de 5 lâminas). Modelo: bokeh pentagonal aditivo com miolo suave, rotação aleatória.",
    Cfg(M::Plain).sp(0.8f).op(0.45f).rotm(0, 2).scat(0.5f, 1).jit(0.5f).bl(Blend::Add).col(0.12f, 0.1f, 0.1f), Color::hex(0xFFE08A), 22, 4, 120, "wave", "dark", true);
  R("stamp_ppoly5_o", "Pentágono macio (aro)", F, "Na vida real: bokeh de lente com aro mais brilhante. Modelo: pentágonos de aro luminoso e centro quase vazio, somados como luz.",
    Cfg(M::Plain).sp(0.85f).op(0.5f).rotm(0, 2).scat(0.6f, 1).jit(0.6f).bl(Blend::Add).col(0.15f, 0.1f, 0.1f), Color::hex(0x9BE0FF), 24, 4, 130, "wave", "dark", true);
  R("stamp_star", "Estrela cintilante", F, "Na vida real: estrelas visíveis têm brilhos muito diferentes: poucas grandes, muitas pequenas, e cintilam. Modelo: tamanhos em lei de potência, brilho que oscila com o quadro de animação e raios de brilho (glint) aditivos.",
    Cfg(M::Twinkle).sp(1.5f).scat(1.2f, 2).rotm(0, 2).gl("stamp_kira4").kk(1.f, 0.f).jit(0.2f), Color::hex(0xFFD84A), 20, 4, 100, "wave", "night", false);
  R("stamp_star_r", "Estrela arredondada", F, "Na vida real: estrelas de papel e adesivos têm pontas arredondadas e sobem em brilho no centro. Modelo: estrelas com bisel, rotação aleatória, cor variando entre amarelos e laranjas.",
    Cfg(M::Bevel).sp(1.6f).rotm(0, 2).jit(0.35f).col(0.05f, 0.1f, 0.1f).scat(0.4f, 1), Color::hex(0xFFC93C), 18, 4, 90);
  R("stamp_pentagram1", "Pentagrama (linhas)", F, "Na vida real: símbolo desenhado com compasso e régua, linhas finas e vértices exatos. Modelo: estrela de 5 pontas em linha dupla manuscrita, sempre com uma ponta para cima (não gira), pressão controla tamanho.",
    Cfg(M::Plain).sp(2.1f).rotm(0, 0).dbline().ps(0.6f), ink, 20, 4, 100);
  R("stamp_pentagram2", "Pentagrama (estrela cheia)", F, "Na vida real: selo desenhado de uma só volta de caneta. Modelo: estrela de linha com preenchimento levemente transparente, rotação alinhada ao traço (a ponta aponta para onde se vai).",
    Cfg(M::Plain).sp(2.0f).rotm(-90, 1).dbline(), Color::hex(0x6B2FA0), 20, 4, 100);
  R("stamp_pentagram3", "Pentagrama no círculo", F, "Na vida real: círculo mágico/selo inscrito. Modelo: selo em linha dupla que gira lentamente a cada carimbo (passo de 18°), como um sinete carimbado repetidamente.",
    Cfg(M::Plain).sp(2.2f).rotm(0, 3).dbline().kk(0), Color::hex(0x2E7D6B), 22, 4, 110);
  R("stamp_hexagram1", "Hexagrama fino", F, "Na vida real: estrela de seis pontas de dois triângulos sobrepostos. Modelo: linha fina com dupla traçada, orientação fixa, tamanho por pressão.",
    Cfg(M::Plain).sp(2.1f).dbline().ps(0.7f), Color::hex(0x2A5CAA), 20, 4, 100);
  R("stamp_hexagram2", "Hexagrama entrelaçado", F, "Na vida real: dois triângulos entrelaçados passando por cima e por baixo. Modelo: carimbo com bisel leve, girando aleatoriamente em múltiplos de 30° para efeito de ornamento.",
    Cfg(M::Plain).sp(2.0f).rotm(0, 2).dbline(), Color::hex(0x1B998B), 20, 4, 100);
  R("stamp_hexagram3", "Hexagrama no círculo", F, "Na vida real: selo de proteção em círculo duplo. Modelo: sinete que gira a cada passo e se afasta do traço alternando lados, como carimbos presos num selo cerimonial.",
    Cfg(M::Plain).sp(2.3f).rotm(0, 3).dbline().scat(0.15f, 1), Color::hex(0xC74B50), 22, 4, 110);

  // ---------------- kira / light flares ----------------
  R("stamp_kira4", "Brilho cruz 4 pontas", S, "Na vida real: o brilho em cruz vem da difração nas lâminas do diafragma ou do filtro estrela; é luz, não tinta. Modelo: raios aditivos que variam de tamanho em lei de potência e piscam com o quadro.",
    Cfg(M::Twinkle).sp(1.6f).scat(1.4f, 2).bl(Blend::Add).kk(1.f, 1.f).rotm(0, 0).jit(0.2f), Color::hex(0xFFF0B0), 22, 4, 120, "wave", "night", true);
  R("stamp_kira6", "Brilho 6 pontas", S, "Na vida real: seis raios surgem de lentes com diafragma hexagonal ou de cristais de gelo. Modelo: estrela de seis raios aditiva, rotação aleatória, cor entre branco e azul-gelo.",
    Cfg(M::Twinkle).sp(1.6f).scat(1.4f, 2).bl(Blend::Add).kk(1.f, 0.f).rotm(0, 2).col(0.04f, 0.05f, 0.05f), Color::hex(0xCFE8FF), 22, 4, 120, "wave", "night", true);
  R("stamp_kira8", "Brilho 8 pontas", S, "Na vida real: lentes de 8 lâminas e fontes muito brilhantes fazem raios longos de 8 pontas. Modelo: raio aditivo com centro quase branco; rotação por quadro (cintilação animada).",
    Cfg(M::Twinkle).sp(1.8f).scat(1.5f, 2).bl(Blend::Add).kk(1.f, 0.f).rotm(0, 2).jit(0.3f), Color::hex(0xFFE9C0), 24, 4, 130, "wave", "night", true);
  R("stamp_kira_diamond", "Estrela losango", S, "Na vida real: brilho de joia ou purpurina em forma de losango alongado. Modelo: losangos finos verticais com glint aditivo em parte deles, tamanho em lei de potência.",
    Cfg(M::Twinkle).sp(1.5f).scat(1.0f, 2).rotm(0, 0).gl("stamp_kira4").jit(0.2f), Color::hex(0xFFF3B0), 20, 4, 100, "wave", "night");
  R("stamp_kira_diamond_o", "Estrela losango (linha)", S, "Na vida real: desenho de brilho feito com linha fina, como em quadrinhos. Modelo: contorno duplo manuscrito, tamanho aleatório e giro de 0/45° leve.",
    Cfg(M::Twinkle).sp(1.6f).scat(1.0f, 2).rotm(0, 0).dbline().jit(0.3f), Color::hex(0xFFE08A), 20, 4, 100, "wave", "night");
  R("stamp_flare", "Clarão (flare)", S, "Na vida real: clarão de lente é luz espalhada que somada ao fundo clareia tudo ao redor da fonte. Modelo: halo largo aditivo + núcleo menor mais branco, desloca por passo.",
    Cfg(M::Glow).sp(0.35f).ps(0.7f, 0.4f).op(0.7f), Color::hex(0xFFB35C), 36, 8, 200, "wave", "dark", true);
  R("light02", "Bloom suave", S, "Na vida real: bloom é luz que vaza em volta de pontos muito claros (olho/câmera). Modelo: pequeno núcleo + halo largo aditivos, acumulam ao passar mais de uma vez sem estourar.",
    Cfg(M::Glow).sp(0.25f).ps(0.5f, 0.5f).op(0.6f).scat(0.1f, 1), Color::hex(0x8FD0FF), 30, 6, 180, "wave", "dark", true);

  // ---------------- gears / wheels ----------------
  R("stamp_gear_wheel01", "Engrenagem rolante", S, "Na vida real: engrenagens vizinhas giram em sentidos opostos e os dentes encaixam. Modelo: o ângulo vem do comprimento percorrido (rolagem), vizinhas contra-rotacionam com meio dente de defasagem e sombra projetada.",
    Cfg(M::Roll).sp(1.85f).kk(12.f, 0.3f).ps(0.4f), steel, 20, 5, 90);
  R("stamp_gear_wheel01o", "Engrenagem rolante (linha)", S, "Na vida real: esquema técnico de engrenagens. Modelo: contorno em dupla linha que gira por rolagem, alternando o sentido.",
    Cfg(M::Roll).sp(1.85f).kk(12.f, 0.f).dbline().ps(0.4f), ink, 20, 5, 90);
  R("stamp_gear_wheel02", "Roda dentada", S, "Na vida real: rodas dentadas de relógio rolam sobre a pista e giram proporcionalmente. Modelo: rolagem pura (ângulo = distância/raio) com sombra; todas giram no mesmo sentido como rodas de uma esteira.",
    Cfg(M::Roll).sp(1.9f).kk(16.f, 0.35f).ps(0.4f).kk(1.f, 0.35f), Color::hex(0xB08840), 20, 5, 90);
  R("stamp_gear_wheel02o", "Roda dentada (linha)", S, "Na vida real: planta técnica de roda com raios. Modelo: contorno duplo que rola pelo caminho, sentido alternado.",
    Cfg(M::Roll).sp(1.9f).kk(16.f, 0.f).dbline().ps(0.4f), Color::hex(0x5A4A2A), 20, 5, 90);
  R("stamp_pinwheel", "Cata-vento", S, "Na vida real: o cata-vento gira mais rápido quanto mais vento recebe. Modelo: o giro acumulado cresce com a velocidade da caneta e com o quadro; em alta velocidade surgem fantasmas de desfoque de movimento.",
    Cfg(M::Spin).sp(2.2f).ps(0.5f).col(0.5f, 0.2f, 0.1f), Color::hex(0xE8503A), 20, 5, 100, "wave", "sky");
  R("stamp_pinwheel_o", "Cata-vento (linha)", S, "Na vida real: ícone de cata-vento em traços. Modelo: contorno duplo manuscrito que gira com a velocidade, sem preenchimento.",
    Cfg(M::Spin).sp(2.2f).dbline().ps(0.5f), ink, 20, 5, 100);
  R("stamp_waterwheel", "Roda d'água", S, "Na vida real: a roda d'água gira num ritmo constante empurrada pela correnteza. Modelo: giro constante por quadro (animação) mais um pouco pela velocidade da mão, com leve rastro de movimento.",
    Cfg(M::Spin).sp(2.4f).ps(0.4f).kk(0.f), Color::hex(0x8A6A3F), 24, 5, 110);
  R("stamp_waterwheel_o", "Roda d'água (linha)", S, "Na vida real: desenho técnico de roda de pás. Modelo: contorno duplo que gira por quadro, mais devagar que a versão cheia.",
    Cfg(M::Spin).sp(2.4f).dbline().ps(0.4f), Color::hex(0x4A5D6A), 24, 5, 110);

  // ---------------- heart / notes / crowns / gems ----------------
  R("stamp_heart", "Corações flutuantes", S, "Na vida real: nos quadrinhos corações (emote) sobem, diminuem e somem. Modelo: um coração no traço mais cópias que sobem com vento e balanço lateral, menores e mais transparentes conforme sobem; pressão faz pulsar.",
    Cfg(M::Float).sp(1.7f).scat(0, 1).kk(3.2f).ps(0.4f).col(0.03f, 0.1f, 0.1f), red, 16, 4, 80);
  R("stamp_doublet_note", "Colcheias duplas", S, "Na vida real: notas escritas numa pauta ocupam alturas (notas) diferentes. Modelo: desenha uma pauta de 5 linhas ao longo do traço e coloca notas em posições de altura discretas sobre ela.",
    Cfg(M::Note).sp(2.6f).ps(0.3f), ink, 22, 6, 90);
  R("stamp_eighth_note", "Colcheia", S, "Na vida real: uma colcheia sozinha sobe a melodia. Modelo: nota única com pauta, ligeira inclinação seguindo a direção e posição de altura aleatória por carimbo.",
    Cfg(M::Note).sp(2.1f).scat(0, 2).ps(0.3f), Color::hex(0x2D3A8C), 22, 6, 90);
  R("stamp_king_crown", "Coroa de rei", S, "Na vida real: coroas são de metal polido, com luz forte no topo e sombra embaixo, e ficam sempre em pé. Modelo: nunca gira; sombra, corpo dourado, brilho superior deslocado e glint ocasional; tamanho por pressão.",
    Cfg(M::Crown).sp(2.3f).ps(0.7f), gold, 24, 6, 110);
  R("stamp_king_crown_o", "Coroa de rei (linha)", S, "Na vida real: brasão desenhado a traço. Modelo: contorno duplo manuscrito, sempre em pé, tamanho por pressão.",
    Cfg(M::Plain).sp(2.3f).dbline().ps(0.7f), Color::hex(0x7A5B10), 24, 6, 110);
  R("stamp_queen_crown", "Coroa de rainha", S, "Na vida real: coroa de rainha tem pontas arredondadas e pérolas. Modelo: corpo em ouro rosado com realce superior e brilho de pedra, sempre em pé.",
    Cfg(M::Crown).sp(2.3f).ps(0.7f), Color::hex(0xE0A8B0), 24, 6, 110);
  R("stamp_queen_crown_o", "Coroa de rainha (linha)", S, "Na vida real: ícone de realeza em traço. Modelo: contorno duplo, sempre em pé.",
    Cfg(M::Plain).sp(2.3f).dbline().ps(0.7f), Color::hex(0x8A4A5A), 24, 6, 110);
  R("stamp_diamond_icon", "Diamante (ícone)", S, "Na vida real: uma gema lapidada disfarça cores quando a luz atravessa (dispersão). Modelo: corpo tingido, luz interna branca, franjas vermelha/azul deslocadas aditivas e brilho em cruz.",
    Cfg(M::Gem).sp(1.9f).col(0.25f, 0.1f, 0.1f).ps(0.6f).scat(0.2f, 1), Color::hex(0x5BC8F5), 20, 5, 90, "wave", "dark");
  R("stamp_diamond_real_side", "Diamante lapidado (lateral)", S, "Na vida real: vista lateral mostra coroa, cintura e pavilhão com facetas que refletem diferentes cores. Modelo: máscara de facetas pintadas com cor variável + dispersão cromática + glints, sem girar demais.",
    Cfg(M::Gem).sp(2.0f).col(0.3f, 0.15f, 0.1f).kk(6.f).ps(0.6f), Color::hex(0xB6E4FF), 22, 5, 100, "wave", "dark");
  R("stamp_diamond_real_top", "Diamante lapidado (topo)", S, "Na vida real: visto de cima o brilhante mostra uma rosa de facetas. Modelo: facetas aleatoriamente giradas, cada gema com matiz próprio, luz interna e cruz de brilho.",
    Cfg(M::Gem).sp(2.0f).rotm(0, 2).col(0.4f, 0.2f, 0.1f).ps(0.6f), Color::hex(0xFFD0E8), 22, 5, 100, "wave", "dark");

  // ---------------- flat tips ----------------
  R("flat01", "Pincel chato: fios soltos", FL, "Na vida real: pincel chato velho tem fios que se abrem em leque quando arrastado. Modelo: ponta perpendicular ao traço, fios irregulares radiais, falha de tinta (dry-brush) cresce com a velocidade.",
    Cfg(M::Flat).sp(0.14f).rotm(0, 1).kk(0.5f).gr("@fibers", 0.6f).ps(0.6f), Color::hex(0x3A2A20), 24, 6, 120, "line");
  R("flat02", "Pincel chato: cerdas esparsas", FL, "Na vida real: poucas cerdas ressecadas deixam pontos soltos em fileira. Modelo: ponta de pontos alinhados na perpendicular; arrasta deixando linhas pontilhadas paralelas.",
    Cfg(M::Flat).sp(0.1f).rotm(0, 1).kk(0.4f).ps(0.6f), Color::hex(0x2F2F3D), 24, 6, 120, "line");
  R("flat03", "Pincel chato: grade de pontos", FL, "Na vida real: rolinho de textura/pente deixa fileiras regulares. Modelo: duas fileiras de pontos perpendiculares ao traço, espaçamento curto produz linhas paralelas.",
    Cfg(M::Flat).sp(0.12f).rotm(0, 1).kk(0.3f).ps(0.6f), Color::hex(0x35506B), 24, 6, 120, "line");
  R("flat04", "Pincel chato: poucos pontos", FL, "Na vida real: leque com poucas cerdas deixa 6-7 riscos finos. Modelo: ponta de poucos pontos grandes; viram riscos paralelos espaçados, largura por pressão.",
    Cfg(M::Flat).sp(0.1f).rotm(0, 1).ps(0.8f), Color::hex(0x6B3A2A), 24, 6, 120, "line");
  R("flat05", "Pincel chato: leque de grãos", FL, "Na vida real: pincel leque (fan brush) distribui cerdas em arco. Modelo: ponta de grãos alongados em leque; ao arrastar, riscos finos e curtos que se espalham em volta da direção.",
    Cfg(M::Flat).sp(0.11f).rotm(0, 1).kk(0.4f).ps(0.6f), Color::hex(0x7A5B2E), 24, 6, 120, "line");
  R("flat_calligraphy", "Pena caligráfica", FL, "Na vida real: a pena de ponta chata fixa em 45° faz traços grossos na diagonal e finos no sentido perpendicular. Modelo: elipse com ângulo fixo (param. ângulo fixo ligado); a espessura vem da geometria, não da pressão.",
    Cfg(M::Flat).sp(0.08f).rotm(-40, 0).ps(0.25f), ink, 14, 3, 60, "wave");
  R("flat_five_lines", "Pente de 5 linhas", FL, "Na vida real: o pente de pauta (rastrum) desenha cinco linhas paralelas de uma só vez. Modelo: cinco pontos perpendiculares ao traço, sempre acompanhando a curva, perfeito para pauta/cabos.",
    Cfg(M::Flat).sp(0.07f).rotm(0, 1).ps(0.2f), ink, 18, 4, 90, "wave");
  R("flat_line", "Pincel de listras", FL, "Na vida real: pincel de cerdas largas e marcadas deixa faixas alternadas de tinta e papel. Modelo: elipse com listras horizontais; a ponta fica perpendicular ao traço, mostrando riscos paralelos longos.",
    Cfg(M::Flat).sp(0.12f).rotm(0, 1).kk(0.25f).ps(0.5f), Color::hex(0x1F3A5F), 22, 6, 110, "wave");
  R("flat_real", "Pincel chato real", FL, "Na vida real: pincel chato carregado arrasta riscos, falhas nas bordas e acumula tinta no início. Modelo: marca de cerdas real, perpendicular ao traço, falha seca com a velocidade e grão de fibras do papel.",
    Cfg(M::Flat).sp(0.16f).rotm(0, 1).kk(0.5f).gr("@fibers", 0.5f).ps(0.5f), Color::hex(0x2B2B2B), 24, 6, 120, "wave");
  R("flat_rect", "Pincel retangular (chisel)", FL, "Na vida real: marcador/pincel de ponta quadrada pinta faixa de largura constante. Modelo: retângulo chanfrado que fica perpendicular ao traço e preenche faixa sólida; pressão só altera a largura levemente.",
    Cfg(M::Flat).sp(0.1f).rotm(0, 1).ps(0.25f), Color::hex(0x2E7D32), 18, 4, 100, "wave");

  // ---------------- rope/net ----------------
  R("net1", "Malha de linhas", NT, "Na vida real: tela de tecido é fiada em linhas paralelas iguais. Modelo: carimbo ancorado à grade do canvas (mesma célula em todo traço), portanto cruzamentos de traços se alinham como um tom/malha contínuo.",
    Cfg(M::Mesh).sp(1.0f), ink, 14, 6, 60, "wave");
  R("net2", "Grade quadriculada", NT, "Na vida real: papel milimetrado/quadriculado tem grade regular. Modelo: grade ancorada ao canvas; pintar mais de uma vez não duplica células.",
    Cfg(M::Mesh).sp(1.0f), Color::hex(0x2A6F97), 14, 6, 60, "wave");
  R("net3", "Rede cruzada", NT, "Na vida real: rede de pesca/cerca de arame forma losangos. Modelo: células de losango alinhadas ao canvas, estendendo-se onde você passa.",
    Cfg(M::Mesh).sp(1.0f), Color::hex(0x6B705C), 14, 6, 60, "wave");
  R("net4", "Rede densa com brilho", NT, "Na vida real: tela de seda tem fios que brilham nos cruzamentos. Modelo: malha de losangos com pontas luminosas, ancorada à grade.",
    Cfg(M::Mesh).sp(1.0f), Color::hex(0xA3B18A), 14, 6, 60, "wave");
  R("net_chain", "Corrente 3D", NT, "Na vida real: elos de corrente alternam: um de frente, outro de perfil (girado 90°). Modelo: elos alternados face/perfil, com sombra projetada e brilho; elos se sobrepõem 1/2 para encaixarem.",
    Cfg(M::Chain).sp(1.2f).rotm(0, 1).ps(0.3f), steel, 20, 5, 80, "wave");
  R("net_flash", "Relâmpago de traço", NT, "Na vida real: um clarão de luz cria um risco muito fino que afina nas pontas. Modelo: faixa alinhada ao movimento, comprimento e opacidade crescem com a velocidade da caneta.",
    Cfg(M::Wedge).sp(1.5f).kk(1.f, 2.f).ps(0.4f).bl(Blend::Add), Color::hex(0xFFF4C2), 22, 4, 100, "wave", "dark", true);
  R("net_lace", "Renda", NT, "Na vida real: a renda tem festões (scallops) repetidos e furos, e a barra fica de um lado. Modelo: segmentos espelhados alternadamente para continuidade, orientados pelo caminho.",
    Cfg(M::Lace).sp(1.95f).ps(0.3f), Color::hex(0xEDE6D6), 22, 6, 90, "wave", "dark");
  R("net_pile", "Pilha de fios", NT, "Na vida real: feixe de fios finos lado a lado, como cabos. Modelo: três linhas paralelas orientadas, afinando no início e abrindo conforme a pressão.",
    Cfg(M::Wedge).sp(1.2f).kk(3.f, 0.f).ps(0.7f), Color::hex(0x2F4858), 22, 5, 100, "wave");
  R("net_rope", "Corda trançada", NT, "Na vida real: corda é feita de cordões trançados cuja fase gira ao longo do comprimento. Modelo: fase avança com o caminho, alternando claro/escuro e espelhando a cada elo; cria sensação de torção.",
    Cfg(M::Rope).sp(0.55f).ps(0.3f), Color::hex(0xB8935A), 18, 5, 70, "wave");
  R("net_speed", "Linhas de velocidade", NT, "Na vida real: nos quadrinhos, linhas paralelas de velocidade ficam atrás do objeto e esticam com a velocidade. Modelo: traços de comprimento e opacidade proporcionais à velocidade, espalhados na perpendicular atrás da caneta.",
    Cfg(M::SpeedL).sp(0.7f).scat(0, 3).ps(0.3f), ink, 18, 4, 80, "line");
  R("net_wedge", "Cunha de pincel", NT, "Na vida real: pincel de cunha (sumi/shodō) entra afiado e engrossa. Modelo: fade de entrada por comprimento (taper), largura e opacidade pela pressão, ponta voltada à direção do traço.",
    Cfg(M::Wedge).sp(1.3f).kk(1.f, 1.5f).ps(0.9f), ink, 20, 4, 90, "wave");

  // ---------------- particle / spray tips ----------------
  R("circle01", "Fogos de artifício", PT, "Na vida real: fogos explodem em raios que se afastam do centro, cada explosão com uma cor. Modelo: explosões radiais com orientação aleatória, escala variável e matiz novo por explosão; esparsas.",
    Cfg(M::Burst).sp(3.0f).scat(0.8f, 1).jit(0.5f).ps(0.4f), Color::hex(0xFF5A5F), 24, 5, 110, "wave", "night", false);
  R("circle02", "Poeira de pontos", PT, "Na vida real: poeira fina em suspensão é rara e muito pequena. Modelo: pontos minúsculos aleatórios em volta do traço, cada passo solta 2-3 deles com opacidade variável.",
    Cfg(M::Plain).sp(0.5f).scat(1.4f, 3).rotm(0, 2).jit(0.5f).kk(0, 0.5f).ps(0.3f), ink, 22, 4, 100);
  R("circle03", "Pixels dissolvendo", PT, "Na vida real: ao se desfazer, uma imagem vira quadradinhos. Modelo: quadrados pequenos rotacionados em múltiplos de 15°, espalhados com tamanhos diferentes — efeito de dissolução digital.",
    Cfg(M::Plain).sp(0.5f).scat(1.3f, 3).rotm(0, 2).jit(0.6f).kk(0, 0.4f).ps(0.3f), Color::hex(0x4361EE), 22, 4, 100);
  R("circle04", "Faíscas alongadas", PT, "Na vida real: partículas soltas pela fricção deixam rastros curtos na direção do movimento. Modelo: pontos esticados ao longo do traço proporcional à velocidade, espalhados na perpendicular.",
    Cfg(M::Plain).sp(0.5f).scat(1.2f, 3).rotm(0, 1).kk(2.4f).jit(0.4f).ps(0.3f), Color::hex(0xF28C28), 22, 4, 100);
  R("circle05", "Chuva de riscos", PT, "Na vida real: gotas ou palhas vistas em movimento ficam riscadas na direção do deslocamento. Modelo: riscos curtos orientados à direção, mais longos com velocidade, espalhados em grupo.",
    Cfg(M::Plain).sp(0.5f).scat(1.4f, 4).rotm(0, 1).kk(1.6f).jit(0.5f).ps(0.3f), Color::hex(0x3A86FF), 22, 4, 100);
  R("circle_particle", "Esponja de partículas", PT, "Na vida real: esponja marinha carimba manchas irregulares com muitos furos. Modelo: carimbo girado aleatoriamente com 30% de pixels descartados, sobrepõe várias vezes para textura de esponja.",
    Cfg(M::Plain).sp(0.45f).rotm(0, 2).jit(0.2f).dis(0.25f).scat(0.25f, 1).col(0, 0, 0.08f).ps(0.5f).op(0.7f), Color::hex(0x2A9D8F), 24, 6, 120);
  R("circle_real", "Esponja real", PT, "Na vida real: esponja natural tem poros grandes e bordas ásperas, pinta em manchas variadas de tom. Modelo: carimbo de poros irregulares, rotação aleatória e variação de tom por carimbo, sobreposição suave.",
    Cfg(M::Plain).sp(0.6f).rotm(0, 2).jit(0.2f).col(0, 0.05f, 0.1f).op(0.65f).ps(0.5f).scat(0.3f, 1), Color::hex(0x8D6E63), 26, 6, 130);
  R("circle_rough", "Estêncil áspero", PT, "Na vida real: pincel de estêncil/pontilhado deixa bordas macias e granulosas, que se acumulam. Modelo: disco rugoso translúcido, baixa opacidade por carimbo, constrói tom ao repassar.",
    Cfg(M::Plain).sp(0.35f).rotm(0, 2).op(0.28f).dis(0.1f).ps(0.4f, 0.5f).scat(0.15f, 1), Color::hex(0x3D405B), 26, 6, 130);
  R("points01", "Pontilhismo colorido", PT, "Na vida real: no pontilhismo (Seurat) cores puras lado a lado se misturam no olho. Modelo: pontos espalhados com variação de matiz/saturação/valor, grupos mais densos com pressão.",
    Cfg(M::Plain).sp(0.55f).scat(1.2f, 3).rotm(0, 2).jit(0.5f).col(0.12f, 0.2f, 0.15f).ps(0.4f), Color::hex(0xE76F51), 22, 4, 100);
  R("points02", "Pontilhismo grande", PT, "Na vida real: bolinhas grandes de tinta ficam mais separadas, com sombra de relevo. Modelo: pontos maiores e mais raros, matiz variando, bisel pequeno por carimbo.",
    Cfg(M::Plain).sp(0.8f).scat(1.4f, 2).rotm(0, 2).jit(0.6f).col(0.15f, 0.2f, 0.15f).ps(0.5f), Color::hex(0x2A9D8F), 22, 4, 100);
  R("points03", "Granulado de areia", PT, "Na vida real: grãos de areia/spray muito finos aparecem de forma contínua. Modelo: nuvem densa de grãos minúsculos, opacidade média, nenhum padrão visível por causa da rotação aleatória.",
    Cfg(M::Plain).sp(0.3f).rotm(0, 2).op(0.6f).scat(0.2f, 1).jit(0.3f).ps(0.4f), Color::hex(0x8B6F47), 24, 4, 140);
  R("points04", "Fiapos verticais", PT, "Na vida real: chuvisco/pelos finos aparecem como pontos esticados na vertical. Modelo: faixa vertical de grãos sempre em pé (sem rotação), vários por passo deslocados, ótimo para fios, pelo ou chuva fina.",
    Cfg(M::Plain).sp(0.5f).scat(1.4f, 2).rotm(0, 0).jit(0.4f).op(0.7f).ps(0.4f), Color::hex(0x6C757D), 26, 4, 140);
  R("sputtering01", "Respingo grosso", PT, "Na vida real: ao sacudir o pincel, gotas grandes voam na direção do gesto e caem longe; as maiores ficam perto. Modelo: gotas lançadas à frente do traço, maiores e opacas perto, pequenas e fracas longe, esticadas pela velocidade.",
    Cfg(M::Spatter).sp(0.9f).scat(0, 3).ps(0.5f), Color::hex(0x1B1B1B), 22, 5, 110);
  R("sputtering02", "Respingo fino (névoa)", PT, "Na vida real: escova de dentes/aerógrafo solta névoa de gotinhas muito pequenas. Modelo: dezenas de gotas pequenas lançadas com ligeiro viés para frente, esticamento por velocidade e queda de opacidade com a distância.",
    Cfg(M::Spatter).sp(0.55f).scat(0, 6).ps(0.4f).op(0.85f), Color::hex(0x2B2D42), 22, 5, 110);
}

}  // namespace ab
