// Light module: "luz" (light that paints: glow, neon, laser, rays, flares, bokeh ...)
#include "light_util.hpp"

namespace ab {
namespace lu {
namespace {

inline float flick(const StrokeParams& sp, uint32_t seed, float amt) {
  return std::max(0.2f, 1.f + amt * noise1(sp.frame * 1.31f + sp.time * 7.f, seed));
}

// walk along a->b dropping a callback every `spacing` px
template <class F>
void walk(const PathPoint& a, const PathPoint& b, float spacing, float& acc, F fn) {
  float L = dist(a.p, b.p);
  if (L < 1e-4f) return;
  float pos = spacing - acc;
  while (pos <= L) {
    float t = pos / L;
    PathPoint q = b;
    q.p = lerp(a.p, b.p, t); q.pressure = lerpf(a.pressure, b.pressure, t); q.d = lerpf(a.d, b.d, t);
    q.dir = b.dir; q.speed = lerpf(a.speed, b.speed, t);
    fn(q);
    pos += spacing;
  }
  acc = L - (pos - spacing);
  if (acc < 0) acc = 0;
}

// ---------------------------------------------------------------------------------------------
// Tube-like glowing strokes (soft light, neon, long exposure, laser, outline)
// ---------------------------------------------------------------------------------------------
class GlowTube : public StrokeBase {
 public:
  enum Kind { Soft, Neon, LongExp, Laser };
  GlowTube(BrushInfo i, Kind k) : kind_(k) {
    info_ = i;
    addParam("brilho", "Brilho", 1.f, 0.2f, 2.5f);
    addParam("halo", "Halo", 1.f, 0.3f, 2.5f);
    if (k == Neon) addParam("tremor", "Tremor (flicker)", 0.15f, 0.f, 1.f);
    if (k == LongExp) addParam("tremida", "Tremida da mão", 0.4f, 0.f, 1.f);
    if (k == Laser) addParam("faiscas", "Faíscas no fim", 1.f, 0.f, 1.f);
  }

 protected:
  void onBegin(ISurface& s, const InputSample&) override {
    gc_ = GlowCarry{}; last_ = prev_.p; jit_ = {0, 0};
    PathPoint p = prev_;
    segment(s, p, p, true);
  }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override { segment(s, a, b, false); }
  void onEnd(ISurface& s) override {
    if (kind_ == Laser && param("faiscas") > 0.f) {
      Vec2 e = prev_.p; float r = sp_.size;
      glowPoint(s, e, r * 1.2f, whiten(sp_.color, 0.6f), sp_.color, gain(), 0.9f, 4.f);
      for (int i = 0; i < 9; ++i) {  // sparks scattered by the impact
        float a = rng_.uniform() * kTau, l = r * rng_.range(2.f, 7.f);
        streakAt(s, e + fromAngle(a, l * 0.5f), a, l * 0.5f, std::max(0.4f, r * 0.12f), whiten(sp_.color, 0.7f), 0.7f * gain(), 0.6f);
      }
    }
    if (kind_ == Neon) {  // electrodes at the tube ends: small dim metallic nubs
      softAt(s, prev_.p, sp_.size * 0.5f, Color{1, 1, 1, 1}, 0.25f * gain(), 0.8f);
    }
  }
  void segment(ISurface& s, const PathPoint& a, const PathPoint& b, bool first) {
    float k = gain() * param("brilho");
    float haloK = param("halo");
    float r0 = rad(a), r1 = rad(b);
    Color col = sp_.color, halo = sp_.color;
    float white = 0.6f;
    Vec2 pa = a.p, pb = b.p;
    switch (kind_) {
      case Neon: k *= flick(sp_, sp_.seed, param("tremor")); white = 0.75f; r0 = r1 = sp_.size * lerpf(0.7f, 1.f, b.pressure); break;
      case LongExp: {
        float expo = clampf(260.f / (b.speed + 70.f), 0.35f, 2.2f);  // slow hand = more exposure
        k *= expo;
        col = mix(sp_.color, sp_.color2, 0.5f + 0.5f * noise1(b.d * 0.012f, sp_.seed));
        halo = col;
        float sh = param("tremida") * 2.2f;
        jit_.x += (noise1(b.d * 0.05f, sp_.seed + 1) - jit_.x * 0.2f) * 0.15f * sh;
        jit_.y += (noise1(b.d * 0.05f, sp_.seed + 2) - jit_.y * 0.2f) * 0.15f * sh;
        pb = pb + jit_ * 2.f; pa = first ? pb : last_; last_ = pb;
        white = 0.45f; break;
      }
      case Laser: r0 = r1 = sp_.size * 0.35f; white = 0.85f; haloK *= 0.8f; break;
      default: break;
    }
    GlowCarry& g = gc_;
    if (first) { glowPoint(s, pb, r0, col, halo, k, white, 3.2f * haloK); return; }
    glowLine(s, pa, pb, r0, r1, col, halo, k, white, g, 3.2f * haloK);
  }
  Kind kind_;
  GlowCarry gc_;
  Vec2 last_, jit_;
};

// ---------------------------------------------------------------------------------------------
// Glowing outline: hollow neon contour of a thick stroke (two parallel light lines)
// ---------------------------------------------------------------------------------------------
class GlowOutline : public StrokeBase {
 public:
  explicit GlowOutline(BrushInfo i) { info_ = i; addParam("brilho", "Brilho", 1.f, 0.2f, 2.5f); addParam("abertura", "Largura do vão", 1.f, 0.4f, 2.f); }
 protected:
  void onBegin(ISurface&, const InputSample&) override { gcA_ = gcB_ = GlowCarry{}; }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    float w = rad(b) * param("abertura");
    Vec2 na = a.dir.perp() * w, nb = b.dir.perp() * w;
    float k = gain() * param("brilho");
    float lr = std::max(1.2f, sp_.size * 0.14f);
    glowLine(s, a.p + na, b.p + nb, lr, lr, sp_.color, sp_.color, k, 0.6f, gcA_, 4.f);
    glowLine(s, a.p - na, b.p - nb, lr, lr, sp_.color, sp_.color, k, 0.6f, gcB_, 4.f);
  }
  GlowCarry gcA_, gcB_;
};

// ---------------------------------------------------------------------------------------------
// Light painting rays: god rays / crepuscular shafts + dust motes
// ---------------------------------------------------------------------------------------------
class GodRays : public StrokeBase {
 public:
  explicit GodRays(BrushInfo i) {
    info_ = i;
    addParam("angulo", "Ângulo dos raios (°)", 62.f, 0.f, 180.f);
    addParam("comprimento", "Comprimento", 1.f, 0.4f, 2.5f);
    addParam("poeira", "Poeira no ar", 1.f, 0.f, 2.f);
  }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    float sp = std::max(2.f, sp_.size * 0.55f);
    float ang = param("angulo") * kDeg2Rad;
    Vec2 dir = fromAngle(ang);
    walk(a, b, sp, acc_, [&](const PathPoint& q) {
      // shafts are bright where the occluder (foliage / window) lets light through: noise along the stroke
      float gate = smoothstep(0.35f, 0.75f, valueNoise(q.d * 0.045f, 3.1f, sp_.seed));
      float L = sp_.size * 9.f * param("comprimento") * (0.6f + 0.8f * hashf(hash2((int)q.d, 5, sp_.seed)));
      float w = sp_.size * (0.35f + 0.5f * hashf(hash2((int)q.d, 9, sp_.seed)));
      for (int j = 0; j < 4; ++j) {  // stacked streaks approximate a longitudinal fade
        float len = L * (1.f - j * 0.22f);
        streakAt(s, q.p + dir * (len * 0.5f), ang, len * 0.5f, w * (1.f + j * 0.15f), sp_.color, 0.07f * gain() * gate * (1.f + j * 0.5f), 0.f);
      }
      // dust motes caught in the shaft
      int nm = (int)(2.f * param("poeira") * gate + rng_.uniform());
      for (int m = 0; m < nm; ++m) {
        Vec2 p = q.p + dir * (L * rng_.uniform()) + q.dir.perp() * (w * rng_.sym() * 1.5f);
        softAt(s, p, rng_.range(0.7f, 1.6f), whiten(sp_.color, 0.7f), rng_.range(0.4f, 0.9f) * gain(), 0.5f);
      }
    });
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Lens flare: source + starburst + ghosts mirrored through the optical centre
// ---------------------------------------------------------------------------------------------
class LensFlare : public StrokeBase {
 public:
  explicit LensFlare(BrushInfo i) {
    info_ = i;
    addParam("fantasmas", "Reflexos internos", 6.f, 0.f, 10.f);
    addParam("estrela", "Estrela de difração", 1.f, 0.f, 2.f);
    addParam("anamorfico", "Anamórfico (faixa)", 0.f, 0.f, 1.f);
  }
 protected:
  void onBegin(ISurface& s, const InputSample& in) override {
    src_ = {in.x, in.y};
    float r = sp_.size;
    bloomAt(s, src_, r * 5.f, sp_.color, 0.5f * gain());
    softAt(s, src_, r * 0.9f, whiten(sp_.color, 0.7f), 0.9f * gain(), 0.5f);
    // diffraction starburst (aperture blades: 6 long spikes)
    float st = param("estrela");
    for (int i = 0; i < 6; ++i) {
      float a = kPi / 6.f * i + 0.2f;
      streakAt(s, src_, a, r * 7.f * st, std::max(0.5f, r * 0.07f), whiten(sp_.color, 0.8f), 0.55f * gain(), 0.3f);
    }
    if (param("anamorfico") > 0.01f) streakAt(s, src_, 0, r * 28.f * param("anamorfico"), r * 0.18f, sp_.color2, 0.65f * gain(), 0.f);
  }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {  // dragging the source leaves a soft comet-like tail
    softAt(s, b.p, sp_.size * 0.5f, sp_.color, 0.1f * gain(), 0.2f);
  }
  void onEnd(ISurface& s) override {
    Vec2 ctr{S_->width() * 0.5f, S_->height() * 0.5f};
    static const Mask hex = polyMask(6, 0.45f, 0.22f, 0.f), disc = polyMask(0, 0.5f, 0.18f);
    int n = (int)param("fantasmas");
    static const float ts[10] = {0.62f, 0.25f, -0.25f, -0.55f, -0.9f, 0.12f, -1.25f, 0.85f, -0.4f, 1.4f};
    for (int i = 0; i < n && i < 10; ++i) {
      Vec2 p = ctr + (src_ - ctr) * ts[i];
      float r = sp_.size * (0.7f + 1.6f * hashf(hash2(i, 4, sp_.seed)));
      Color c = hueC(std::fmod(0.55f + i * 0.17f, 1.f), 0.55f, 1.f);
      Dab d = addDab(c, 0.13f * gain()); d.shape = (i & 1) ? &hex : &disc; d.x = p.x; d.y = p.y; d.radius = r;
      d.angle = i * 0.4f; S_->drawDab(d);
    }
    // faint halo ring around the source (internal ghost ring)
    static const Mask ring = polyMask(0, 0.12f, 0.f);
    Dab d = addDab(sp_.color2, 0.12f * gain()); d.shape = &ring; d.x = src_.x; d.y = src_.y; d.radius = sp_.size * 8.f; S_->drawDab(d);
  }
  Vec2 src_;
};

// ---------------------------------------------------------------------------------------------
// Sparkler: ballistic white-hot particles that fork into tiny starbursts
// ---------------------------------------------------------------------------------------------
class Sparkler : public StrokeBase {
 public:
  explicit Sparkler(BrushInfo i) {
    info_ = i;
    addParam("densidade", "Densidade", 1.f, 0.2f, 3.f);
    addParam("gravidade", "Gravidade", 1.f, 0.f, 3.f);
    addParam("alcance", "Alcance", 1.f, 0.4f, 2.5f);
  }
 protected:
  void onBegin(ISurface& s, const InputSample&) override { emit(s, prev_.p, 14); glowPoint(s, prev_.p, sp_.size * 0.5f, Color{1, 0.85f, 0.5f, 1}, Color{1, 0.5f, 0.1f, 1}, gain(), 0.9f, 3.f); }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    walk(a, b, std::max(2.f, sp_.size * 0.4f), acc_, [&](const PathPoint& q) { emit(s, q.p, (int)(3.f * param("densidade") + rng_.uniform())); });
    glowPoint(s, b.p, sp_.size * 0.4f, Color{1, 0.9f, 0.6f, 1}, Color{1, 0.5f, 0.1f, 1}, 0.5f * gain(), 0.9f, 3.f);
  }
  void onHold(ISurface& s, float dt) override { emit(s, prev_.p, (int)(dt * 240.f * param("densidade") + rng_.uniform())); }
  void emit(ISurface& s, Vec2 p, int n) {
    float R = sp_.size * 9.f * param("alcance"), g = R * 1.2f * param("gravidade");
    for (int i = 0; i < n; ++i) {
      float a = rng_.uniform() * kTau, v = R * rng_.range(0.4f, 1.f), life = rng_.range(0.35f, 1.f);
      Vec2 vel = fromAngle(a, v), pos = p;
      const int steps = 10;
      for (int k = 0; k < steps; ++k) {
        float t0 = life * k / steps, t1 = life * (k + 1) / steps;
        Vec2 p0 = p + vel * t0 + Vec2{0, g * t0 * t0 * 0.5f}, p1 = p + vel * t1 + Vec2{0, g * t1 * t1 * 0.5f};
        float heat = 1.f - (float)k / steps;
        Color c = heatRamp(0.25f + 0.75f * heat);
        Dab d = addDab(c, 0.8f * heat * gain(), 0.8f);
        dabLine(s, p0, p1, 0.7f, 0.5f, d, 0.8f);
      }
      pos = p + vel * life + Vec2{0, g * life * life * 0.5f};
      if (rng_.chance(0.35f)) {  // a spark bursts into a tiny star: iron particle oxidising
        for (int q = 0; q < 5; ++q) { float aa = rng_.uniform() * kTau; streakAt(s, pos + fromAngle(aa, 2.f), aa, 2.5f, 0.4f, Color{1, 0.95f, 0.8f, 1}, 0.8f * gain(), 0.8f); }
        softAt(s, pos, 2.2f, Color{1, 0.8f, 0.4f, 1}, 0.6f * gain(), 0.2f);
      }
    }
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Fireflies: bioluminescent blinking points (phase from frame so animation blinks)
// ---------------------------------------------------------------------------------------------
class Fireflies : public StrokeBase {
 public:
  explicit Fireflies(BrushInfo i) { info_ = i; addParam("espaco", "Espaçamento", 1.f, 0.3f, 3.f); addParam("pulso", "Velocidade do pulso", 1.f, 0.f, 3.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    walk(a, b, sp_.size * 2.2f * param("espaco"), acc_, [&](const PathPoint& q) {
      Vec2 p = q.p + rng_.inDisc() * (sp_.size * 3.f);
      float phase = rng_.uniform() * kTau, rate = 0.35f * param("pulso") * rng_.range(0.7f, 1.4f);
      float v = 0.5f + 0.5f * std::sin(sp_.frame * rate * kTau * 0.25f + phase);
      v = smoothstep(0.1f, 0.95f, v);   // mostly dim with bright pulses, like real Photinus flashes
      float r = sp_.size * rng_.range(0.12f, 0.28f);
      Color halo = sp_.color2, body = sp_.color;
      glowPoint(s, p, r, body, halo, gain() * (0.18f + 0.95f * v), 0.7f, 5.5f);
    });
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Bokeh: out-of-focus highlights, polygon aperture, bright rim, depth-sized
// ---------------------------------------------------------------------------------------------
class Bokeh : public StrokeBase {
 public:
  explicit Bokeh(BrushInfo i) { info_ = i; addParam("lamelas", "Lâminas do diafragma (0=redondo)", 6.f, 0.f, 9.f); addParam("variacao", "Variação de cor", 0.12f, 0.f, 0.6f); addParam("densidade", "Densidade", 1.f, 0.3f, 3.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    int blades = (int)param("lamelas");
    static const Mask m0 = polyMask(0, 0.35f, 0.3f), m5 = polyMask(5, 0.3f, 0.3f), m6 = polyMask(6, 0.3f, 0.3f), m8 = polyMask(8, 0.3f, 0.3f);
    const Mask* m = blades == 0 ? &m0 : blades <= 5 ? &m5 : blades <= 6 ? &m6 : &m8;
    walk(a, b, sp_.size * 0.8f / param("densidade"), acc_, [&](const PathPoint& q) {
      float depth = rng_.uniform(); depth = depth * depth;  // many small far points, few huge near ones
      float r = sp_.size * (0.35f + 1.9f * depth) * (0.6f + 0.4f * q.pressure);
      Vec2 p = q.p + rng_.inDisc() * (sp_.size * 1.8f);
      Color c = shiftHSV(sp_.color, rng_.sym() * param("variacao"), 0.f, 0.f);
      Dab d = addDab(c, (0.10f + 0.18f * (1.f - depth)) * gain()); d.shape = m; d.x = p.x; d.y = p.y; d.radius = r;
      d.angle = blades > 0 ? rng_.uniform() * 0.2f : 0.f; S_->drawDab(d);
    });
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Glint / diamond sparkle: 4/8 point cross flares that twinkle
// ---------------------------------------------------------------------------------------------
class Glint : public StrokeBase {
 public:
  explicit Glint(BrushInfo i) { info_ = i; addParam("pontas", "Pontas (4/8)", 4.f, 4.f, 8.f); addParam("espaco", "Espaçamento", 1.f, 0.3f, 3.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    walk(a, b, sp_.size * 2.6f * param("espaco"), acc_, [&](const PathPoint& q) {
      Vec2 p = q.p + rng_.inDisc() * sp_.size;
      float ph = rng_.uniform() * kTau;
      float tw = 0.55f + 0.45f * std::sin(sp_.frame * 1.1f + ph);
      float sz = sp_.size * rng_.range(0.6f, 1.6f) * (0.4f + 0.6f * q.pressure) * (0.5f + 0.7f * tw);
      Color c = sp_.color;
      bloomAt(s, p, sz * 1.4f, c, 0.35f * gain());
      for (int i = 0; i < 2; ++i) streakAt(s, p, i * kPi * 0.5f, sz * 2.4f, std::max(0.4f, sz * 0.09f), whiten(c, 0.8f), 0.9f * gain(), 0.2f);
      if (param("pontas") > 6.f) for (int i = 0; i < 2; ++i) streakAt(s, p, kPi * 0.25f + i * kPi * 0.5f, sz * 1.3f, std::max(0.35f, sz * 0.06f), whiten(c, 0.8f), 0.6f * gain(), 0.2f);
      softAt(s, p, sz * 0.22f, Color{1, 1, 1, 1}, 0.95f * gain(), 0.7f);
    });
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Energy aura: turbulent tongues rising from the path (colour -> colour2 at tips)
// ---------------------------------------------------------------------------------------------
class EnergyAura : public StrokeBase {
 public:
  explicit EnergyAura(BrushInfo i) { info_ = i; addParam("altura", "Altura das chamas", 1.f, 0.3f, 3.f); addParam("turbulencia", "Turbulência", 1.f, 0.f, 2.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    walk(a, b, std::max(1.5f, sp_.size * 0.3f), acc_, [&](const PathPoint& q) {
      float t = sp_.frame * 0.35f + sp_.time * 3.f;
      float h = sp_.size * 3.5f * param("altura") * (0.45f + 0.9f * valueNoise(q.d * 0.04f, t, sp_.seed));
      float sway = (noise1(q.d * 0.03f + t * 1.7f, sp_.seed + 4) * param("turbulencia") + sp_.wind) * h * 0.35f;
      Vec2 base = q.p, tip = q.p + Vec2{sway, -h};
      Vec2 mid = lerp(base, tip, 0.5f);
      float ang = (tip - base).angle();
      float w = sp_.size * 0.55f;
      streakAt(s, mid, ang, h * 0.5f, w * 1.6f, sp_.color2, 0.07f * gain(), 0.f);
      streakAt(s, lerp(base, tip, 0.42f), ang, h * 0.42f, w, sp_.color, 0.12f * gain(), 0.1f);
      streakAt(s, lerp(base, tip, 0.3f), ang, h * 0.3f, w * 0.45f, whiten(sp_.color, 0.7f), 0.2f * gain(), 0.3f);
    });
    glowLine(s, a.p, b.p, sp_.size * 0.3f, sp_.size * 0.3f, sp_.color, sp_.color, gain(), 0.8f, gc_, 3.f);
  }
  float acc_ = 0; GlowCarry gc_;
};

// ---------------------------------------------------------------------------------------------
// Magic braid ribbon: three sine strands with depth shading + sparkles
// ---------------------------------------------------------------------------------------------
class MagicRibbon : public StrokeBase {
 public:
  explicit MagicRibbon(BrushInfo i) { info_ = i; addParam("onda", "Comprimento da onda", 1.f, 0.4f, 3.f); addParam("amplitude", "Amplitude", 1.f, 0.3f, 3.f); }
 protected:
  void onBegin(ISurface&, const InputSample&) override { for (int i = 0; i < 3; ++i) have_[i] = false; }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    float lam = sp_.size * 9.f * param("onda"), amp = sp_.size * 1.6f * param("amplitude");
    for (int i = 0; i < 3; ++i) {
      float ph = b.d / lam * kTau + i * kTau / 3.f;
      Vec2 p = b.p + b.dir.perp() * (amp * std::sin(ph));
      float front = 0.5f + 0.5f * std::cos(ph);  // strand in front is brighter and thicker
      Color c = mix(sp_.color, sp_.color2, i / 2.f);
      if (have_[i]) {
        Dab d = addDab(c, (0.18f + 0.55f * front) * gain(), 0.5f);
        dabLine(*S_, last_[i], p, sp_.size * (0.14f + 0.2f * front), sp_.size * (0.14f + 0.2f * front), d, 0.35f);
        Dab h = addDab(c, 0.04f * gain()); h.shape = &bloomMask();
        dabLine(*S_, last_[i], p, sp_.size * 0.9f, sp_.size * 0.9f, h, 0.3f);
      }
      last_[i] = p; have_[i] = true;
    }
    if (rng_.chance(0.25f)) {
      Vec2 p = b.p + rng_.inDisc() * (amp * 1.6f);
      softAt(s, p, rng_.range(0.7f, 1.8f), whiten(sp_.color2, 0.6f), 0.9f * gain(), 0.6f);
    }
  }
  Vec2 last_[3]; bool have_[3] = {false, false, false};
};

// ---------------------------------------------------------------------------------------------
// Plasma ball: filaments reaching out from a hot core (like a plasma globe)
// ---------------------------------------------------------------------------------------------
class PlasmaBall : public StrokeBase {
 public:
  explicit PlasmaBall(BrushInfo i) { info_ = i; addParam("filamentos", "Filamentos", 3.f, 1.f, 12.f); addParam("raio", "Alcance", 1.f, 0.4f, 2.5f); }
 protected:
  void onBegin(ISurface& s, const InputSample&) override { emitAt(s, prev_.p); }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override { walk(a, b, sp_.size * 3.f, acc_, [&](const PathPoint& q) { emitAt(s, q.p); }); }
  void onHold(ISurface& s, float) override { emitAt(s, prev_.p); }
  void emitAt(ISurface& s, Vec2 p) {
    float R = sp_.size * 3.2f * param("raio");
    bloomAt(s, p, R * 0.9f, sp_.color, 0.1f * gain());
    int n = (int)param("filamentos");
    std::vector<BoltSeg> segs;
    for (int i = 0; i < n; ++i) {
      float a = rng_.uniform() * kTau;
      genBolt(segs, p, p + fromAngle(a, R * rng_.range(0.7f, 1.f)), rng_, 0.55f, 0.08f, 0, 1, 1.f, {0, 0}, R * 0.14f);
    }
    drawBolt(s, segs, std::max(0.7f, sp_.size * 0.07f), whiten(sp_.color2, 0.8f), sp_.color2, sp_.color, 0.7f * gain());
    softAt(s, p, sp_.size * 0.35f, Color{1, 1, 1, 1}, 0.9f * gain(), 0.5f);
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Hologram: scanlines, chromatic split and per-frame glitch
// ---------------------------------------------------------------------------------------------
class Hologram : public StrokeBase {
 public:
  explicit Hologram(BrushInfo i) { info_ = i; addParam("linhas", "Espaço das linhas", 3.f, 2.f, 8.f); addParam("glitch", "Glitch", 0.5f, 0.f, 1.f); }
 protected:
  float minDist() const override { return 1.f; }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    walk(a, b, 1.5f, acc_, [&](const PathPoint& q) {
      float R = rad(q, 0.5f), step = param("linhas");
      for (float y = -R; y <= R; y += step) {
        int row = (int)std::floor((q.p.y + y) / step);
        float rowFlick = 0.55f + 0.45f * hashf(hash2(row, sp_.frame, sp_.seed));
        float glitch = hashf(hash2(row / 5, sp_.frame, sp_.seed + 7)) < 0.18f * param("glitch") ? (hashf(hash2(row, sp_.frame, 3)) - 0.5f) * 14.f : 0.f;
        float edge = 1.f - std::pow(std::fabs(y) / R, 3.f);
        Vec2 p = {q.p.x + glitch, q.p.y + y};
        streakAt(s, p + Vec2{1.2f, 0}, 0, 2.2f, 0.65f, Color{1, 0.1f, 0.4f, 1}, 0.18f * rowFlick * edge * gain(), 0.5f);   // chromatic fringe
        streakAt(s, p - Vec2{1.2f, 0}, 0, 2.2f, 0.65f, Color{0.1f, 0.5f, 1, 1}, 0.18f * rowFlick * edge * gain(), 0.5f);
        streakAt(s, p, 0, 2.4f, 0.7f, sp_.color, 0.55f * rowFlick * edge * gain(), 0.6f);
      }
    });
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Warm lamp / candle glow pool with real colour temperature
// ---------------------------------------------------------------------------------------------
class WarmGlow : public StrokeBase {
 public:
  explicit WarmGlow(BrushInfo i) { info_ = i; addParam("temperatura", "Temperatura (K)", 2200.f, 1200.f, 6500.f); addParam("tremor", "Tremor da chama", 0.35f, 0.f, 1.f); }
 protected:
  void onBegin(ISurface& s, const InputSample&) override { stamp(s, prev_.p, prev_.pressure); }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override { walk(a, b, std::max(1.f, sp_.size * 0.18f), acc_, [&](const PathPoint& q) { stamp(s, q.p, q.pressure); }); }
  void onHold(ISurface& s, float) override { stamp(s, prev_.p, 1.f); }
  void stamp(ISurface& s, Vec2 p, float pr) {
    float K = param("temperatura");
    float fl = flick(sp_, sp_.seed, param("tremor"));
    float r = sp_.size * lerpf(0.6f, 1.f, pr) * (0.92f + 0.08f * fl);
    bloomAt(s, p, r * 5.5f, blackbody(K - 900.f), 0.035f * gain() * fl);
    softAt(s, p, r * 2.4f, blackbody(K - 200.f), 0.04f * gain() * fl, 0.f);
    softAt(s, p, r * 1.0f, blackbody(K + 1400.f), 0.08f * gain() * fl, 0.2f);
    softAt(s, p, r * 0.35f, blackbody(K + 3500.f), 0.16f * gain(), 0.6f);
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Spotlight / searchlight cone with dust (drawn at stroke end: start = lamp, direction = stroke direction)
// ---------------------------------------------------------------------------------------------
class SpotCone : public StrokeBase {
 public:
  explicit SpotCone(BrushInfo i) { info_ = i; addParam("abertura", "Abertura (°)", 16.f, 4.f, 45.f); addParam("poeira", "Poeira", 1.f, 0.f, 2.f); }
 protected:
  void onBegin(ISurface& s, const InputSample& in) override { src_ = {in.x, in.y}; glowPoint(s, src_, sp_.size * 0.6f, sp_.color, sp_.color, gain(), 0.85f, 3.f); }
  void onEnd(ISurface& s) override {
    Vec2 e = prev_.p; Vec2 d = e - src_; float L = std::max(110.f, d.len() * 1.1f); float base = d.len() > 3.f ? d.angle() : kPi * 0.5f;
    float half = param("abertura") * kDeg2Rad;
    const int n = 22;
    for (int i = -n; i <= n; ++i) {
      float u = (float)i / n, a = base + u * half;
      float prof = 1.f - u * u;                                   // brighter on the axis
      float beamNoise = 0.7f + 0.3f * valueNoise(i * 0.7f, 2.f, sp_.seed);
      for (int j = 0; j < 4; ++j) {
        float len = L * (1.f - j * 0.2f);
        streakAt(s, src_ + fromAngle(a, len * 0.5f), a, len * 0.5f, std::max(1.f, len * std::tan(half / n) * 0.9f), sp_.color, 0.04f * gain() * prof * beamNoise * (1.f + j * 0.4f), 0.f);
      }
    }
    int nm = (int)(26 * param("poeira"));
    for (int m = 0; m < nm; ++m) {
      float u = rng_.sym(), t = rng_.uniform();
      Vec2 p = src_ + fromAngle(base + u * half * 0.9f, L * t);
      softAt(s, p, rng_.range(0.6f, 1.5f), whiten(sp_.color, 0.7f), rng_.range(0.3f, 0.9f) * gain() * (1.f - std::fabs(u) * 0.6f), 0.5f);
    }
  }
  Vec2 src_;
};

// ---------------------------------------------------------------------------------------------
// Fibre optics: thin coloured fibres that wander around the path, lit tips
// ---------------------------------------------------------------------------------------------
class FiberOptic : public StrokeBase {
 public:
  explicit FiberOptic(BrushInfo i) { info_ = i; addParam("fibras", "Número de fibras", 7.f, 3.f, 14.f); }
 protected:
  void onBegin(ISurface&, const InputSample&) override { n_ = (int)param("fibras"); for (int i = 0; i < 14; ++i) { have_[i] = false; gc_[i] = GlowCarry{}; } }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    for (int f = 0; f < n_; ++f) {
      float off = ((f + 0.5f) / n_ - 0.5f) * sp_.size * 3.f + std::sin(b.d * (0.02f + 0.004f * f) + f * 1.7f) * sp_.size * 1.2f;
      Vec2 p = b.p + b.dir.perp() * off;
      Color c = hueC(std::fmod(0.55f + f * (1.f / n_) * 0.8f, 1.f), 0.65f, 1.f);
      if (have_[f]) glowLine(s, last_[f], p, std::max(0.6f, sp_.size * 0.07f), std::max(0.6f, sp_.size * 0.07f), c, c, 0.5f * gain(), 0.5f, gc_[f], 3.f);
      last_[f] = p; have_[f] = true; col_[f] = c;
    }
  }
  void onEnd(ISurface& s) override { for (int f = 0; f < n_; ++f) if (have_[f]) glowPoint(s, last_[f], sp_.size * 0.2f, col_[f], col_[f], gain(), 0.85f, 4.f); }
  int n_ = 7; Vec2 last_[14]; Color col_[14]; bool have_[14]; GlowCarry gc_[14];
};

}  // namespace

void registerLuz(BrushLibrary& lib) {
  const Color W{1, 1, 1, 1};
  reg<GlowTube>(lib, mkInfo("luz.brilho", "Pincel de Luz (brilho suave)", "luz",
      "uma fonte de luz não é uma linha clara: a intensidade cai com o quadrado da distância, o centro satura para branco e só a cauda guarda a cor.",
      "núcleo branco-quente + corpo colorido + halo largo com queda 1/(1+k·d²) em Blend::Add; o núcleo clareia conforme o brilho aumenta.",
      12, Color{0.35f, 0.7f, 1, 1}, W, true, false), GlowTube::Soft);
  reg<GlowTube>(lib, mkInfo("luz.neon", "Tubo de Neon", "luz",
      "tubo de gás ionizado: miolo quase branco, cor saturada em volta, halo que ilumina a parede, cintilação leve da rede elétrica e pontas arredondadas com eletrodos.",
      "espessura constante (pouca variação por pressão), tremor por frame/tempo, eletrodo opaco na ponta final.",
      8, Color{1, 0.2f, 0.65f, 1}, W, true, true), GlowTube::Neon);
  reg<GlowTube>(lib, mkInfo("luz.laser", "Feixe Laser", "luz",
      "laser: feixe finíssimo e coerente, quase sem halo; no ponto de impacto a luz espalha e solta fagulhas.",
      "linha muito fina com núcleo branco, halo estreito; ao soltar, mancha de impacto + faíscas radiais.",
      10, Color{1, 0.1f, 0.1f, 1}, W, true, false, "line"), GlowTube::Laser);
  reg<GlowTube>(lib, mkInfo("luz.longa-exposicao", "Light Painting (longa exposição)", "luz",
      "foto de longa exposição: onde a lanterna se move devagar o sensor acumula mais luz; a mão treme e a cor oscila.",
      "ganho ∝ 1/velocidade, deslocamento suave por ruído (tremida), gradiente cor→cor2 ao longo do traço.",
      9, Color{0.2f, 1, 0.6f, 1}, Color{1, 0.3f, 0.8f, 1}, true, false), GlowTube::LongExp);
  reg<GlowOutline>(lib, mkInfo("luz.contorno-neon", "Contorno Luminoso Oco", "luz",
      "letreiro de duas linhas de neon que desenham o contorno de uma faixa, com o miolo vazio.",
      "duas linhas luminosas deslocadas pela normal do traço, largura do vão pela pressão.",
      16, Color{0.2f, 1, 0.9f, 1}, W, true, false));
  reg<GodRays>(lib, mkInfo("luz.raios-de-sol", "Raios de Sol (god rays)", "luz",
      "luz atravessando folhas/janelas forma colunas visíveis (dispersão de Tyndall em poeira), com bordas irregulares e partículas flutuando.",
      "feixes paralelos num ângulo fixo, abertos/fechados por ruído ao longo do traço (a oclusão), fade longitudinal e poeira cintilante.",
      14, Color{1, 0.9f, 0.6f, 1}, W, true, false, "wave", "night"));
  reg<LensFlare>(lib, mkInfo("luz.reflexo-lente", "Reflexo de Lente (lens flare)", "luz",
      "luz forte em uma objetiva gera estrela de difração (lâminas do diafragma) e reflexos internos espelhados em torno do centro óptico, hexagonais e coloridos.",
      "estrela de 6 pontas, bloom, e fantasmas alinhados no eixo fonte→centro da tela com formas hexagonais/discos e cores deslocadas.",
      12, Color{1, 0.85f, 0.55f, 1}, Color{0.4f, 0.7f, 1, 1}, true, false, "line", "dark", 0, false));
  reg<Sparkler>(lib, mkInfo("luz.estrelinha", "Estrelinha Faiscante", "luz",
      "partículas de ferro incandescente voam em balística, esfriam de branco a laranja e algumas explodem em mini-estrelas.",
      "partículas com gravidade, rampa de temperatura ao longo da vida, bifurcação em mini-estrela; hold() mantém a emissão.",
      9, Color{1, 0.8f, 0.4f, 1}, W, true, true, "wave", "dark", 0.6f));
  reg<Fireflies>(lib, mkInfo("luz.vagalumes", "Vaga-lumes", "luz",
      "vaga-lumes piscam com fase própria: longos instantes apagados e pulsos curtos amarelo-esverdeados.",
      "pontos aleatórios em volta do traço com brilho = pulso senoidal por frame com fase individual.",
      14, Color{0.8f, 1, 0.3f, 1}, Color{0.4f, 1, 0.2f, 1}, true, true, "wave", "night"));
  reg<Bokeh>(lib, mkInfo("luz.bokeh", "Bokeh", "luz",
      "pontos de luz fora de foco viram discos com a forma do diafragma (polígonos), mais brilhantes na borda, de tamanho conforme a distância.",
      "discos/polígonos com anel brilhante, tamanho com distribuição enviesada (muitos pequenos, poucos grandes), variação de cor.",
      18, Color{1, 0.8f, 0.5f, 1}, W, true, false, "wave", "night"));
  reg<Glint>(lib, mkInfo("luz.cintilar", "Brilho Estrela (glint)", "luz",
      "brilho de diamante/sol na água: cruz de difração de 4 ou 8 pontas que pisca.",
      "bloom + dois traços perpendiculares finíssimos (e diagonais opcionais) com tamanho pulsando por frame.",
      11, Color{1, 0.95f, 0.8f, 1}, W, true, true, "wave", "night"));
  reg<EnergyAura>(lib, mkInfo("luz.aura", "Aura de Energia", "luz",
      "aura estilo anime: línguas de energia que sobem do corpo, mais claras junto a ele e coloridas nas pontas, ondulando.",
      "chamas verticais de 3 camadas por ruído que evolui com o frame, inclinadas pelo vento.",
      12, Color{0.3f, 0.8f, 1, 1}, Color{0.3f, 0.2f, 1, 1}, true, true, "wave", "dark"));
  reg<MagicRibbon>(lib, mkInfo("luz.fita-magica", "Fita Mágica Trançada", "luz",
      "rastros de magia formam fitas entrelaçadas; o fio da frente é mais brilhante que o de trás, com faíscas soltas.",
      "3 fios senoidais defasados em 120°, brilho/espessura pela profundidade (cos), brilho extra e centelhas.",
      9, Color{0.6f, 0.3f, 1, 1}, Color{0.2f, 1, 0.9f, 1}, true, false, "wave", "night"));
  reg<PlasmaBall>(lib, mkInfo("luz.plasma", "Globo de Plasma", "luz",
      "globo de plasma: filamentos violeta saem do eletrodo central e procuram o vidro, mudando a cada instante.",
      "núcleo branco + filamentos gerados por caminhada tortuosa curta em genBolt, re-sorteados a cada emissão.",
      14, Color{0.5f, 0.2f, 1, 1}, Color{1, 0.5f, 1, 1}, true, true, "wave", "dark", 0.3f));
  reg<Hologram>(lib, mkInfo("luz.holograma", "Holograma", "luz",
      "projeção holográfica: linhas de varredura, aberração cromática e falhas horizontais que mudam a cada quadro.",
      "linhas finas com intensidade aleatória por frame, franjas vermelha/azul deslocadas, deslocamento de bandas (glitch).",
      12, Color{0.3f, 1, 0.9f, 1}, W, true, true, "wave", "dark"));
  reg<WarmGlow>(lib, mkInfo("luz.lampiao", "Luz de Vela/Lampião", "luz",
      "luz de chama é laranja a ~1900-2400 K e vai ficando amarelo-branca no centro; a poça de luz respira.",
      "camadas concêntricas coloridas por corpo negro (blackbody) — externa mais vermelha, núcleo mais branco —, com tremor.",
      16, Color{1, 0.7f, 0.3f, 1}, Color{1, 0.4f, 0.1f, 1}, true, true, "wave", "night"));
  reg<SpotCone>(lib, mkInfo("luz.holofote", "Holofote (cone de luz)", "luz",
      "feixe de holofote/farol: cone cuja intensidade cai da lâmpada para fora e que revela poeira no ar.",
      "leque de faixas finas com perfil de cos² e ruído; poeira dentro do cone; direção = início→fim do traço.",
      10, Color{1, 0.95f, 0.8f, 1}, W, true, false, "line", "night", 0, false));
  reg<FiberOptic>(lib, mkInfo("luz.fibra-optica", "Fibra Óptica", "luz",
      "feixe de fibras ópticas: fios finos de cores diferentes serpenteiam em paralelo e brilham apenas nas pontas.",
      "N fios com deslocamento senoidal próprio, cor por matiz, luz extra nas pontas.",
      12, Color{0.5f, 0.8f, 1, 1}, W, true, false, "wave", "dark"));
}

}  // namespace lu
}  // namespace ab
