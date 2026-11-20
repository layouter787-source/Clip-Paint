// Light module: "raios" (lightning & electricity). Lightning is a branching plasma channel, not a wobbly line.
#include "light_util.hpp"

namespace ab {
namespace lu {
namespace {

// ---------------------------------------------------------------------------------------------
// Chunked bolt brush: the stroke is cut into ~chunk px pieces, each becomes a stepped-leader channel
// ---------------------------------------------------------------------------------------------
class BoltStroke : public StrokeBase {
 public:
  enum Kind { Bolt, Forked, Ground, Chain, Storm };
  BoltStroke(BrushInfo i, Kind k) : kind_(k) {
    info_ = i;
    addParam("ramificacao", "Ramificação", k == Forked ? 0.34f : k == Ground ? 0.22f : 0.15f, 0.f, 0.6f);
    addParam("tortuosidade", "Tortuosidade", k == Forked ? 0.55f : 0.42f, 0.1f, 1.f);
    addParam("brilho", "Brilho", 1.f, 0.3f, 2.5f);
    addParam("trecho", "Tamanho do trecho (px)", k == Chain ? 70.f : 48.f, 20.f, 160.f);
  }
 protected:
  float minDist() const override { return 2.f; }
  void onBegin(ISurface& s, const InputSample&) override {
    start_ = chunkStart_ = prev_.p;
    glowPoint(s, start_, sp_.size * 0.5f, whiten(sp_.color, 0.7f), sp_.color, 0.8f * gain(), 0.9f, 3.f);
  }
  void onSegment(ISurface& s, const PathPoint&, const PathPoint& b) override {
    if (dist(chunkStart_, b.p) >= param("trecho")) { chunk(s, chunkStart_, b.p, b.pressure); chunkStart_ = b.p; }
  }
  void onEnd(ISurface& s) override {
    if (dist(chunkStart_, prev_.p) > 4.f) chunk(s, chunkStart_, prev_.p, prev_.pressure);
    Vec2 e = prev_.p;
    if (kind_ == Ground) {  // return stroke: ground flash + splash of ionised air
      softAt(s, e, sp_.size * 5.f, sp_.color, 0.22f * gain(), 0.f);
      streakAt(s, e, 0, sp_.size * 9.f, sp_.size * 0.5f, sp_.color, 0.35f * gain(), 0.f);
      glowPoint(s, e, sp_.size * 0.8f, whiten(sp_.color, 0.8f), sp_.color, gain(), 0.95f, 4.f);
    } else if (kind_ == Chain) {
      glowPoint(s, e, sp_.size * 0.7f, whiten(sp_.color, 0.8f), sp_.color, gain(), 0.95f, 3.5f);
    }
  }
  void chunk(ISurface& s, Vec2 a, Vec2 b, float pr) {
    float k = gain() * param("brilho");
    float r = std::max(0.9f, sp_.size * 0.22f * lerpf(0.6f, 1.f, pr));
    std::vector<BoltSeg> segs;
    float bp = param("ramificacao"), jag = param("tortuosidade");
    Vec2 bias{0, 0};
    int maxDepth = kind_ == Forked ? 3 : 2;
    if (kind_ == Ground) bias = {0, 1.f};
    genBolt(segs, a, b, rng_, jag, bp, 0, maxDepth, 1.f, bias);
    Color core = whiten(sp_.color, 0.85f);
    drawBolt(s, segs, r, core, sp_.color, sp_.color2, k);
    if (kind_ == Storm) {  // the whole cloud/sky around the channel lights up
      softAt(s, lerp(a, b, 0.5f), dist(a, b) * 0.9f + sp_.size * 3.f, sp_.color2, 0.05f * k, 0.f);
    }
    if (kind_ == Chain) {  // arc jumps between "targets": bright node balls + small sparks
      glowPoint(s, b, sp_.size * 0.5f, whiten(sp_.color, 0.8f), sp_.color, 0.9f * k, 0.9f, 3.f);
      for (int i = 0; i < 4; ++i) { float an = rng_.uniform() * kTau; streakAt(s, b + fromAngle(an, 5.f), an, 5.f, 0.5f, core, 0.8f * k, 0.6f); }
    }
  }
  Kind kind_;
  Vec2 start_, chunkStart_;
};

// ---------------------------------------------------------------------------------------------
// Tesla / electric arc that trails behind the pen
// ---------------------------------------------------------------------------------------------
class TeslaArc : public StrokeBase {
 public:
  explicit TeslaArc(BrushInfo i) { info_ = i; addParam("arcos", "Arcos por passo", 2.f, 1.f, 5.f); addParam("alcance", "Alcance do arco", 1.f, 0.4f, 2.5f); }
 protected:
  float minDist() const override { return 1.f; }
  void onBegin(ISurface& s, const InputSample&) override { acc_ = 0; glowPoint(s, prev_.p, sp_.size * 0.5f, whiten(sp_.color, 0.7f), sp_.color, gain(), 0.9f, 3.f); }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    acc_ += dist(a.p, b.p);
    glowPoint(s, b.p, sp_.size * 0.3f, whiten(sp_.color, 0.8f), sp_.color, 0.6f * gain(), 0.9f, 3.f);
    if (acc_ < 9.f) return;
    acc_ = 0;
    int n = (int)param("arcos");
    for (int i = 0; i < n; ++i) {
      float back = sp_.size * rng_.range(1.5f, 9.f) * param("alcance");
      float sd = std::max(0.f, path_.length() - back);
      Vec2 target = path_.at(sd).p + rng_.inDisc() * (sp_.size * 1.5f);
      std::vector<BoltSeg> segs;
      genBolt(segs, b.p, target, rng_, 0.9f, 0.04f, 0, 1, 1.f, {0, 0}, 6.f);
      drawBolt(s, segs, std::max(0.7f, sp_.size * 0.09f), whiten(sp_.color, 0.8f), sp_.color, sp_.color2, 0.7f * gain());
    }
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Short crackling sparks (static electricity) scattered around the path
// ---------------------------------------------------------------------------------------------
class SparkCrackle : public StrokeBase {
 public:
  explicit SparkCrackle(BrushInfo i) { info_ = i; addParam("densidade", "Densidade", 1.f, 0.2f, 3.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    acc_ += dist(a.p, b.p);
    float step = std::max(3.f, sp_.size * 0.6f / param("densidade"));
    while (acc_ >= step) {
      acc_ -= step;
      Vec2 p = b.p + rng_.inDisc() * (sp_.size * 1.8f);
      float a0 = rng_.uniform() * kTau, len = sp_.size * rng_.range(0.8f, 3.2f);
      std::vector<BoltSeg> segs;
      genBolt(segs, p, p + fromAngle(a0, len), rng_, 0.8f, 0.1f, 0, 1, 1.f, {0, 0}, std::max(3.f, len / 4.f));
      drawBolt(s, segs, std::max(0.6f, sp_.size * 0.05f), Color{1, 1, 1, 1}, sp_.color, sp_.color2, 0.65f * gain());
      if (rng_.chance(0.3f)) softAt(s, p, 1.5f, Color{1, 1, 1, 1}, 0.9f * gain(), 0.6f);
    }
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Sheet lightning: the cloud interior flashes (broad, structured by noise, rare inner channel)
// ---------------------------------------------------------------------------------------------
class SheetFlash : public StrokeBase {
 public:
  explicit SheetFlash(BrushInfo i) { info_ = i; addParam("intensidade", "Intensidade do clarão", 1.f, 0.3f, 2.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    static const Mask puff = puffMask(41, 1.f);
    walk_(a, b, sp_.size * 0.9f, [&](Vec2 p, float d) {
      float lobe = valueNoise(d * 0.02f, sp_.frame * 0.6f, sp_.seed);            // flash is patchy: several lobes lit at different times
      float r = sp_.size * rng_.range(1.8f, 3.4f);
      Dab dd = addDab(sp_.color, 0.11f * gain() * param("intensidade") * (0.3f + lobe)); dd.shape = &puff; dd.x = p.x; dd.y = p.y; dd.radius = r;
      dd.angle = rng_.uniform() * kTau; S_->drawDab(dd);
      softAt(*S_, p, r * 1.6f, sp_.color2, 0.025f * gain(), 0.f);
      if (rng_.chance(0.06f)) {  // occasional inner discharge seen through the cloud (dim, diffuse)
        std::vector<BoltSeg> segs; genBolt(segs, p, p + rng_.onCircle() * (r * 1.2f), rng_, 0.6f, 0.2f, 0, 1, 1.f);
        drawBolt(*S_, segs, sp_.size * 0.16f, sp_.color, sp_.color, sp_.color2, 0.35f * gain());
      }
    });
  }
  template <class F> void walk_(const PathPoint& a, const PathPoint& b, float spacing, F fn) {
    float L = dist(a.p, b.p); float pos = spacing - acc_;
    while (pos <= L) { fn(lerp(a.p, b.p, pos / L), lerpf(a.d, b.d, pos / L)); pos += spacing; }
    acc_ = L - (pos - spacing); if (acc_ < 0) acc_ = 0;
  }
  float acc_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Ball lightning: drifting plasma sphere with swirling arcs
// ---------------------------------------------------------------------------------------------
class BallLightning : public StrokeBase {
 public:
  explicit BallLightning(BrushInfo i) { info_ = i; addParam("espiral", "Arcos em espiral", 6.f, 2.f, 14.f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {  // short fading trail of ionised air
    glowLine(s, a.p, b.p, sp_.size * 0.25f, sp_.size * 0.25f, sp_.color, sp_.color, 0.35f * gain(), 0.6f, gc_, 3.5f);
  }
  void onEnd(ISurface& s) override {
    Vec2 c = prev_.p; float R = sp_.size * 1.6f;
    bloomAt(s, c, R * 4.f, sp_.color2, 0.35f * gain());
    softAt(s, c, R, sp_.color, 0.5f * gain(), 0.3f);
    int n = (int)param("espiral");
    for (int i = 0; i < n; ++i) {   // arcs wrapping the sphere like a swirl
      float a0 = i * kTau / n + rng_.sym() * 0.3f;
      Vec2 prevp = c + fromAngle(a0, R * 0.2f);
      std::vector<BoltSeg> segs;
      for (int k = 1; k <= 8; ++k) {
        float t = k / 8.f; float ang = a0 + t * 2.6f; float rr = R * (0.2f + 0.95f * t);
        Vec2 p = c + fromAngle(ang, rr) + rng_.inDisc() * (R * 0.06f);
        segs.push_back({prevp, p, 1.f - t * 0.4f, 0}); prevp = p;
      }
      drawBolt(s, segs, std::max(0.8f, sp_.size * 0.08f), whiten(sp_.color2, 0.8f), sp_.color2, sp_.color, 0.8f * gain());
    }
    softAt(s, c, R * 0.55f, Color{1, 1, 1, 1}, 0.8f * gain(), 0.5f);
    // a few tendrils escaping
    for (int i = 0; i < 4; ++i) { std::vector<BoltSeg> sg; genBolt(sg, c, c + rng_.onCircle() * (R * rng_.range(1.8f, 3.2f)), rng_, 0.7f, 0.1f, 0, 1, 0.8f); drawBolt(s, sg, sp_.size * 0.07f, Color{1, 1, 1, 1}, sp_.color2, sp_.color, 0.5f * gain()); }
  }
  GlowCarry gc_;
};

// ---------------------------------------------------------------------------------------------
// Energy beam: layered core, turbulent shell, travelling pulse rings, crackle
// ---------------------------------------------------------------------------------------------
class EnergyBeam : public StrokeBase {
 public:
  explicit EnergyBeam(BrushInfo i) { info_ = i; addParam("turbulencia", "Turbulência da borda", 1.f, 0.f, 2.5f); addParam("aneis", "Anéis de pulso", 1.f, 0.f, 2.f); addParam("faiscas", "Faíscas", 1.f, 0.f, 3.f); }
 protected:
  void onBegin(ISurface& s, const InputSample&) override { glowPoint(s, prev_.p, sp_.size * 1.3f, whiten(sp_.color, 0.6f), sp_.color, gain(), 0.9f, 3.f); lastRing_ = -1e9f; }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    float R = sp_.size * lerpf(0.7f, 1.f, b.pressure);
    float t = sp_.frame * 0.45f + sp_.time * 6.f;
    float turb = param("turbulencia");
    Vec2 n = b.dir.perp();
    // shell: noisy width
    float w = R * (1.25f + turb * 0.5f * (noise1(b.d * 0.08f - t * 3.f, sp_.seed) * 0.6f + noise1(b.d * 0.23f - t * 5.f, sp_.seed + 1) * 0.4f));
    Dab sh = addDab(sp_.color2, 0.07f * gain(), 0.f); sh.shape = &bloomMask();
    dabLine(s, a.p, b.p, w * 2.4f, w * 2.4f, sh, 0.12f);
    Dab mid = addDab(sp_.color, 0.3f * gain(), 0.35f);
    dabLine(s, a.p, b.p, w, w, mid, 0.25f);
    Dab core = addDab(whiten(sp_.color, 0.9f), 0.85f * gain(), 0.6f);
    dabLine(s, a.p, b.p, R * 0.5f, R * 0.5f, core, 0.4f);
    // wobbly edge filaments riding the shell
    for (int e = -1; e <= 1; e += 2) {
      float off = w * (0.85f + 0.25f * noise1(b.d * 0.15f + e * 4.f - t * 4.f, sp_.seed + 9));
      softAt(s, b.p + n * (e * off), R * 0.22f, whiten(sp_.color, 0.5f), 0.45f * gain(), 0.4f);
    }
    // pulse rings travelling along the beam
    float ringGap = R * 3.2f;
    if (param("aneis") > 0.05f && b.d - lastRing_ >= ringGap) {
      lastRing_ = b.d;
      streakAt(s, b.p, b.dir.angle() + kPi * 0.5f, w * 1.5f, R * 0.18f, whiten(sp_.color, 0.6f), 0.45f * gain() * param("aneis"), 0.4f);
    }
    // crackle jumping out of the shell
    if (rng_.chance(0.06f * param("faiscas"))) {
      Vec2 p = b.p + n * (rng_.sym() * w), q = p + rng_.onCircle() * (R * rng_.range(1.5f, 3.5f));
      std::vector<BoltSeg> sg; genBolt(sg, p, q, rng_, 0.8f, 0.1f, 0, 1, 1.f, {0, 0}, 5.f);
      drawBolt(s, sg, std::max(0.6f, R * 0.07f), Color{1, 1, 1, 1}, sp_.color, sp_.color2, 0.7f * gain());
    }
  }
  void onEnd(ISurface& s) override {  // beam head: bright orb with flare
    float R = sp_.size * 1.7f; Vec2 e = prev_.p;
    bloomAt(s, e, R * 4.f, sp_.color2, 0.4f * gain());
    softAt(s, e, R, sp_.color, 0.6f * gain(), 0.3f);
    softAt(s, e, R * 0.55f, Color{1, 1, 1, 1}, 0.95f * gain(), 0.6f);
  }
  float lastRing_ = -1e9f;
};

// ---------------------------------------------------------------------------------------------
// Cracking energy: dark fissures with a glowing molten seam (cracks grow with sharp forks)
// ---------------------------------------------------------------------------------------------
class EnergyCracks : public StrokeBase {
 public:
  explicit EnergyCracks(BrushInfo i) { info_ = i; addParam("ramificacao", "Ramificação", 0.28f, 0.f, 0.7f); addParam("brilho", "Brilho interno", 1.f, 0.f, 2.f); }
 protected:
  float minDist() const override { return 2.f; }
  void onBegin(ISurface&, const InputSample&) override { cs_ = prev_.p; }
  void onSegment(ISurface& s, const PathPoint&, const PathPoint& b) override { if (dist(cs_, b.p) > sp_.size * 5.f) { emit(s, cs_, b.p); cs_ = b.p; } }
  void onEnd(ISurface& s) override { if (dist(cs_, prev_.p) > 3.f) emit(s, cs_, prev_.p); }
  void emit(ISurface& s, Vec2 a, Vec2 b) {
    std::vector<BoltSeg> segs;
    genBolt(segs, a, b, rng_, 0.35f, param("ramificacao"), 0, 3, 1.f, {0, 0}, sp_.size * 0.9f);
    float r = std::max(1.f, sp_.size * 0.28f);
    Dab dark = addDab(Color{0.02f, 0.015f, 0.02f, 1}, 0.95f, 0.9f); dark.blend = Blend::Normal;
    for (auto& sg : segs) { float w = std::max(0.5f, r * 1.7f * sg.w); dabLine(s, sg.a, sg.b, w, w * 0.8f, dark, 0.3f); }
    Dab seam = addDab(sp_.color, 0.9f * gain() * param("brilho"), 0.7f);
    Dab halo = addDab(sp_.color2, 0.03f * gain() * param("brilho")); halo.shape = &bloomMask();
    for (auto& sg : segs) {
      float w = std::max(0.4f, r * 0.55f * sg.w);
      dabLine(s, sg.a, sg.b, w * 4.5f, w * 4.5f, halo, 0.2f);
      dabLine(s, sg.a, sg.b, w, w, seam, 0.4f);
    }
  }
  Vec2 cs_;
};

// ---------------------------------------------------------------------------------------------
// Welding arc: dazzling blue-white point, orange spark shower falling with gravity
// ---------------------------------------------------------------------------------------------
class WeldArc : public StrokeBase {
 public:
  explicit WeldArc(BrushInfo i) { info_ = i; addParam("faiscas", "Faíscas", 1.f, 0.f, 3.f); }
 protected:
  void onBegin(ISurface& s, const InputSample&) override { shower(s, prev_.p, 10); }
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override {
    // molten seam left behind: orange weld bead cooling to dark
    Dab bead = addDab(heatRamp(0.55f), 0.5f * gain(), 0.6f); bead.blend = Blend::Add;
    dabLine(s, a.p, b.p, sp_.size * 0.35f, sp_.size * 0.35f, bead, 0.4f, &c_);
    float fl = 0.7f + 0.3f * noise1(b.d * 0.4f, sp_.seed);
    glowPoint(s, b.p, sp_.size * 0.5f, Color{0.8f, 0.9f, 1.f, 1}, Color{0.3f, 0.5f, 1.f, 1}, 0.35f * gain() * fl, 0.95f, 5.f);
    if (rng_.chance(0.5f * param("faiscas"))) shower(s, b.p, 2);
  }
  void onHold(ISurface& s, float dt) override { shower(s, prev_.p, (int)(dt * 120.f * param("faiscas") + rng_.uniform())); glowPoint(s, prev_.p, sp_.size * 0.5f, Color{0.8f, 0.9f, 1.f, 1}, Color{0.3f, 0.5f, 1.f, 1}, 0.4f * gain(), 0.95f, 5.f); }
  void shower(ISurface& s, Vec2 p, int n) {
    float g = sp_.size * 40.f;
    for (int i = 0; i < n; ++i) {
      float a = -kPi * 0.5f + rng_.sym() * 1.7f, v = sp_.size * rng_.range(6.f, 20.f), life = rng_.range(0.3f, 0.9f);
      Vec2 vel = fromAngle(a, v);
      for (int k = 0; k < 9; ++k) {
        float t0 = life * k / 9.f, t1 = life * (k + 1) / 9.f;
        Vec2 p0 = p + vel * t0 + Vec2{0, g * t0 * t0 * 0.5f}, p1 = p + vel * t1 + Vec2{0, g * t1 * t1 * 0.5f};
        Dab d = addDab(heatRamp(1.f - 0.7f * k / 9.f), 0.9f * (1.f - k / 12.f) * gain(), 0.8f);
        dabLine(s, p0, p1, 0.8f, 0.6f, d, 0.8f);
      }
    }
  }
  float c_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Electric burst: radial bolts from the endpoint (shock / discharge)
// ---------------------------------------------------------------------------------------------
class ElectricBurst : public StrokeBase {
 public:
  explicit ElectricBurst(BrushInfo i) { info_ = i; addParam("raios", "Número de raios", 9.f, 3.f, 20.f); addParam("alcance", "Alcance", 1.f, 0.4f, 2.5f); }
 protected:
  void onSegment(ISurface& s, const PathPoint& a, const PathPoint& b) override { glowLine(s, a.p, b.p, sp_.size * 0.1f, sp_.size * 0.1f, sp_.color, sp_.color, 0.5f * gain(), 0.7f, gc_, 3.f); }
  void onEnd(ISurface& s) override {
    Vec2 c = prev_.p; int n = (int)param("raios"); float R = sp_.size * 8.f * param("alcance");
    bloomAt(s, c, R * 0.8f, sp_.color2, 0.3f * gain());
    std::vector<BoltSeg> segs;
    for (int i = 0; i < n; ++i) {
      float a = kTau * i / n + rng_.sym() * 0.25f;
      genBolt(segs, c, c + fromAngle(a, R * rng_.range(0.55f, 1.f)), rng_, 0.55f, 0.18f, 0, 2, 1.f);
    }
    drawBolt(s, segs, std::max(0.8f, sp_.size * 0.12f), Color{1, 1, 1, 1}, sp_.color, sp_.color2, gain());
    glowPoint(s, c, sp_.size * 1.2f, Color{1, 1, 1, 1}, sp_.color, gain(), 0.95f, 4.f);
  }
  GlowCarry gc_;
};

}  // namespace

void registerRaios(BrushLibrary& lib) {
  const Color W{1, 1, 1, 1};
  const Color blue{0.55f, 0.7f, 1.f, 1}, haloB{0.3f, 0.35f, 1.f, 1};
  reg<BoltStroke>(lib, mkInfo("raios.relampago", "Raio", "raios",
      "um raio é um canal de plasma que avança em degraus (líder escalonado) escolhendo caminhos tortuosos, bifurca, e os ramos são mais finos e fracos; o canal principal é branco e cercado de halo azulado.",
      "cada trecho do traço vira um canal por caminhada com 'memória' de curva, bifurcações com probabilidade decrescente, núcleo branco + bainha + halo, estreitando nos ramos; re-sorteado a cada frame.",
      12, blue, haloB, true, true, "arc", "night"), BoltStroke::Bolt);
  reg<BoltStroke>(lib, mkInfo("raios.ramificado", "Raio Ramificado (árvore)", "raios",
      "raios intra-nuvem e de nuvem a solo mostram muitas ramificações finas em forma de árvore invertida.",
      "mesmo modelo de canal mas com altíssima chance de bifurcação, profundidade 3 e ramos mais tortuosos.",
      12, Color{0.8f, 0.7f, 1, 1}, Color{0.5f, 0.3f, 1, 1}, true, true, "arc", "night"), BoltStroke::Forked);
  reg<BoltStroke>(lib, mkInfo("raios.solo", "Raio de Descarga ao Solo", "raios",
      "no raio nuvem-solo o líder desce em direção ao chão ramificando para baixo e o contato produz um clarão intenso (descarga de retorno) com salpico de ar ionizado.",
      "canal com viés para baixo, ramos descendentes, flash largo e estrela horizontal no ponto de impacto.",
      12, Color{0.85f, 0.9f, 1, 1}, Color{0.4f, 0.45f, 1, 1}, true, true, "up", "night"), BoltStroke::Ground);
  reg<BoltStroke>(lib, mkInfo("raios.corrente", "Raio Encadeado (chain lightning)", "raios",
      "o arco elétrico salta de um alvo condutor ao próximo mais próximo.",
      "trechos longos curtos conectados por canais fortes e esferas brilhantes em cada nó com faíscas.",
      11, Color{0.5f, 1, 1, 1}, Color{0.2f, 0.5f, 1, 1}, true, true, "wave", "dark"), BoltStroke::Chain);
  reg<BoltStroke>(lib, mkInfo("raios.tempestade", "Tempestade (raio + clarão do céu)", "raios",
      "durante o raio a nuvem inteira se acende e o céu fica roxo-azulado por um instante.",
      "canal principal mais clarão amplo e difuso sobre cada trecho.",
      14, Color{0.9f, 0.85f, 1, 1}, Color{0.45f, 0.35f, 1, 1}, true, true, "arc", "night"), BoltStroke::Storm);
  reg<TeslaArc>(lib, mkInfo("raios.tesla", "Arco Tesla (rastro elétrico)", "raios",
      "uma bobina de Tesla lança arcos finos e violáceos do eletrodo para qualquer ponto condutor próximo, sem cessar.",
      "a cada passo, arcos curtos e muito tortuosos ligam a ponta da caneta a pontos recentes do rastro.",
      10, Color{0.7f, 0.5f, 1, 1}, Color{0.4f, 0.2f, 1, 1}, true, true, "wave", "dark"));
  reg<SparkCrackle>(lib, mkInfo("raios.estalo", "Estalos Elétricos", "raios",
      "eletricidade estática faz minifaíscas brancas saltarem ao acaso, curtas e muito tortuosas.",
      "mini-canais de 1 a 3 raios do pincel, dispersos aleatoriamente em volta do traço, com pontinhos de impacto.",
      9, Color{0.7f, 0.9f, 1, 1}, Color{0.3f, 0.6f, 1, 1}, true, true, "wave", "dark"));
  reg<SheetFlash>(lib, mkInfo("raios.clarao-nuvem", "Relâmpago de Folha (clarão na nuvem)", "raios",
      "no relâmpago de folha só se vê um brilho difuso e irregular dentro da nuvem; o canal fica escondido.",
      "manchas grandes de ruído (nuvem) acesas por lobos independentes e variação por frame; raro canal fraco por trás.",
      30, Color{0.7f, 0.75f, 1, 1}, Color{0.45f, 0.4f, 1, 1}, true, true, "wave", "night"));
  reg<BallLightning>(lib, mkInfo("raios.bola", "Raio Globular", "raios",
      "relato de esfera luminosa de plasma flutuando, com filamentos em espiral e uma esteira de ar ionizado.",
      "esfera com núcleo branco, arcos espiralados em volta, tentáculos curtos e rastro fraco.",
      12, Color{0.5f, 0.9f, 1, 1}, Color{0.5f, 0.3f, 1, 1}, true, true, "wave", "dark"));
  reg<EnergyBeam>(lib, mkInfo("raios.feixe-energia", "Feixe de Energia (kamehameha)", "raios",
      "raio de energia de anime: núcleo branco, casca turbulenta colorida, anéis de pulso que correm pelo feixe, faíscas e cabeça brilhante.",
      "três camadas com largura modulada por ruído que anda com o frame, anéis perpendiculares a cada passo fixo, faíscas saltando da casca, orbe na ponta.",
      14, Color{0.3f, 0.75f, 1, 1}, Color{0.2f, 0.3f, 1, 1}, true, true, "wave", "dark"));
  reg<EnergyCracks>(lib, mkInfo("raios.rachadura", "Rachadura de Energia", "raios",
      "um objeto que racha mostrando energia interna: fissura escura e irregular com veio incandescente e bifurcações em ângulos agudos.",
      "canais em passos largos, desenhados primeiro como fenda escura (Normal) depois veio quente + halo (Add).",
      9, Color{1, 0.6f, 0.2f, 1}, Color{1, 0.2f, 0.05f, 1}, true, false, "wave", "paper"));
  reg<WeldArc>(lib, mkInfo("raios.solda", "Arco de Solda", "raios",
      "arco de solda: ponto ofuscante branco-azulado, cordão derretido laranja atrás e chuva de faíscas caindo com gravidade.",
      "ponto com flicker, cordão em rampa de calor, faíscas balísticas; hold() solta mais faíscas.",
      9, Color{0.85f, 0.92f, 1, 1}, Color{0.3f, 0.5f, 1, 1}, true, true, "line", "dark", 0.4f));
  reg<ElectricBurst>(lib, mkInfo("raios.descarga", "Descarga Elétrica (explosão)", "raios",
      "uma descarga que se espalha em todas as direções a partir de um ponto de contato.",
      "raios radiais bifurcados saindo do fim do traço + núcleo brilhante.",
      9, Color{0.6f, 0.8f, 1, 1}, Color{0.3f, 0.4f, 1, 1}, true, true, "line", "dark"));
  (void)W;
}

}  // namespace lu
}  // namespace ab
