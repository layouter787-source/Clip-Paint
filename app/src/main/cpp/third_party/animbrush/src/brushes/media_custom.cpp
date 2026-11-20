// Module "media" - custom brushes: watercolour (stroke buffer + edge darkening + granulation + bleed),
// multi-bristle brushes (oil, acrylic, sumi-e, palette knife...), spray can with drips.
#include "media_util.hpp"

namespace ab { namespace media {
namespace {

// ------------------------------------------------------------------------------------------------
// helpers
static std::vector<float> boxBlur(const std::vector<float>& src, int w, int h, int rad) {
  if (rad <= 0) return src;
  std::vector<float> tmp(src.size()), out(src.size());
  const float inv = 1.f / (2 * rad + 1);
  for (int y = 0; y < h; ++y) {
    float acc = 0;
    for (int x = -rad; x <= rad; ++x) acc += src[(size_t)y * w + std::min(w - 1, std::max(0, x))];
    for (int x = 0; x < w; ++x) {
      tmp[(size_t)y * w + x] = acc * inv;
      acc += src[(size_t)y * w + std::min(w - 1, x + rad + 1)] - src[(size_t)y * w + std::max(0, x - rad)];
    }
  }
  for (int x = 0; x < w; ++x) {
    float acc = 0;
    for (int y = -rad; y <= rad; ++y) acc += tmp[(size_t)std::min(h - 1, std::max(0, y)) * w + x];
    for (int y = 0; y < h; ++y) {
      out[(size_t)y * w + x] = acc * inv;
      acc += tmp[(size_t)std::min(h - 1, y + rad + 1) * w + x] - tmp[(size_t)std::max(0, y - rad) * w + x];
    }
  }
  return out;
}

static IRect unite(IRect a, IRect b) {
  if (a.empty()) return b; if (b.empty()) return a;
  return {std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

// ================================================================================================
// WATERCOLOUR
// ================================================================================================
struct WCfg {
  float opacity = 0.6f;   // pigment load
  float edge = 0.6f;      // edge darkening (pigment pools at the drying edge)
  float granulate = 0.5f; // pigment settles into paper valleys
  float wet = 0.f;        // bleed radius (px, scaled by size) - wet-on-wet
  float dry = 0.f;        // dry-brush skipping on paper tooth (0..1)
  float hardness = 0.6f;
  float spacing = 0.12f;
  float radLo = 0.55f;    // pressure -> radius
  float salt = 0.f;       // salt blotches
  int blooms = 0;         // number of backruns / cauliflower blooms
  Blend blend = Blend::Multiply;
  const char* mask = nullptr;
  bool rotate = false;
  float rough = 1.f;      // paper roughness scale
};

class Watercolor : public Brush {
 public:
  Watercolor(BrushInfo i, WCfg c) : cfg_(c) {
    info_ = std::move(i);
    addParam("pigmento", "Pigmento", c.opacity, 0.05f, 1.f);
    addParam("umidade", "Umidade (sangrar)", c.wet, 0.f, 14.f);
    addParam("granulacao", "Granulação", c.granulate, 0.f, 1.f);
    addParam("borda", "Escurecer borda", c.edge, 0.f, 1.f);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    sp_ = sp; rng_.reseed(sp.seed); W_ = s.width(); H_ = s.height();
    buf_ = std::make_unique<RasterSurface>(W_, H_);
    base_ = s.readPatch({0, 0, W_, H_});
    paper_ = StampBank::instance().get("@rough");
    mask_ = cfg_.mask ? StampBank::instance().get(cfg_.mask) : nullptr;
    path_.reset(); dirty_ = {}; carry_ = 0; blooms_.clear();
    path_.add(in, 0.f);
    stamp(in.x, in.y, in.pressure, 0.f);
    render(s, dirty_, false);
  }
  void move(ISurface& s, const InputSample& in) override {
    if (!buf_) return;
    Vec2 prev = path_.back().p;
    const PathPoint& pt = path_.add(in, 0.5f);
    float L = dist(prev, pt.p);
    if (L < 0.01f) return;
    float R = radiusAt(in.pressure);
    float spacing = std::max(0.6f, cfg_.spacing * R);
    float pos = spacing - carry_;
    IRect step{};
    float sn = 1.f - std::exp(-pt.speed / 900.f);
    while (pos <= L) {
      Vec2 p = lerp(prev, pt.p, pos / L);
      step = unite(step, stamp(p.x, p.y, in.pressure, sn));
      pos += spacing;
    }
    carry_ = L - (pos - spacing);
    if (!step.empty()) render(s, step, false);
  }
  void end(ISurface& s) override {
    if (!buf_) return;
    if (cfg_.blooms > 0 && path_.size() > 1) {
      float Lp = path_.length();
      for (int i = 0; i < cfg_.blooms; ++i) {
        PathPoint q = path_.at(rng_.range(0.1f, 0.9f) * Lp);
        float R = sp_.size * rng_.range(0.7f, 1.6f);
        blooms_.push_back({q.p + rng_.inDisc() * (sp_.size * 0.5f), R});
      }
    }
    if (!dirty_.empty()) {
      IRect r = dirty_; int m = (int)(param("umidade") * sp_.size * 0.15f + 6);
      for (auto& b : blooms_) r = unite(r, {(int)(b.c.x - b.r - 3), (int)(b.c.y - b.r - 3), (int)(b.c.x + b.r + 3), (int)(b.c.y + b.r + 3)});
      r = {r.x0 - m, r.y0 - m, r.x1 + m, r.y1 + m};
      render(s, r, true);
    }
    buf_.reset(); base_ = Patch();
  }

 private:
  struct Bloom { Vec2 c; float r; };
  float radiusAt(float p) const { return sp_.size * lerpf(cfg_.radLo, 1.f, clampf(p)); }
  IRect stamp(float x, float y, float pressure, float speedN) {
    Dab d; d.x = x; d.y = y; d.radius = radiusAt(pressure); d.hardness = cfg_.hardness; d.opacity = 1.f;
    d.color = Color{cfg_.dry * (0.35f + 0.65f * speedN), 0, 0, 1};  // R channel carries 'dryness' for the render pass
    if (mask_) { d.shape = mask_; d.angle = cfg_.rotate ? rng_.uniform() * kTau : 0.f; d.mirrorX = rng_.chance(0.5f); d.radius *= 1.1f; }
    buf_->drawDab(d);
    float e = d.radius * (mask_ ? 1.5f : 1.f) + 3.f;
    IRect r{(int)(x - e), (int)(y - e), (int)(x + e) + 1, (int)(y + e) + 1};
    dirty_ = unite(dirty_, r);
    return r;
  }

  void render(ISurface& s, IRect r, bool final) {
    const float wetPx = final ? param("umidade") * std::max(1.f, sp_.size * 0.15f) : 0.f;
    int margin = 3 + (int)wetPx;
    r = {std::max(0, r.x0 - margin), std::max(0, r.y0 - margin), std::min(W_, r.x1 + margin), std::min(H_, r.y1 + margin)};
    if (r.empty()) return;
    const int w = r.w(), h = r.h();
    Patch cov = buf_->readPatch(r);
    std::vector<float> c((size_t)w * h), dr((size_t)w * h);
    for (int i = 0; i < w * h; ++i) { float a = cov.px[i * 4 + 3]; c[i] = a; dr[i] = a > 1e-4f ? cov.px[i * 4] / a : 0.f; }
    std::vector<float> near_ = boxBlur(c, w, h, 2);
    std::vector<float> wide;
    if (final && wetPx > 0.5f) wide = boxBlur(boxBlur(c, w, h, std::max(1, (int)wetPx)), w, h, std::max(1, (int)(wetPx * 0.6f)));
    const float pig = param("pigmento"), edgeK = param("borda"), gran = param("granulacao");
    Patch layer(r.x0, r.y0, w, h);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
      size_t i = (size_t)y * w + x;
      int px = r.x0 + x, py = r.y0 + y;
      float cc = c[i];
      if (!wide.empty()) cc = std::max(cc, wide[i] * 0.85f) * (0.8f + 0.2f * cc);
      float g = paper_ ? paper_->sampleTiled(px / (256.f * cfg_.rough), py / (256.f * cfg_.rough)) : 0.5f;
      float edge = std::max(0.f, (wide.empty() ? c[i] : cc) - (wide.empty() ? near_[i] : near_[i])) ;
      float a = cc * pig * (1.f + edgeK * 5.f * edge);
      a *= lerpf(1.f, 0.35f + 0.65f * g, gran);
      if (cfg_.dry > 0.f) a *= smoothstep(dr[i] - 0.12f, dr[i] + 0.12f, g * 1.05f);
      if (cfg_.salt > 0.f) { float wd = worley(px * 0.11f, py * 0.11f, 7); a *= 1.f - cfg_.salt * (1.f - smoothstep(0.1f, 0.24f, wd)); }
      for (auto& b : blooms_) {
        float d = std::sqrt(sqr(px - b.c.x) + sqr(py - b.c.y)) / b.r;
        float wob = 1.f + 0.12f * (valueNoise(px * 0.08f, py * 0.08f, 3) - 0.5f) * 4.f;
        d *= wob;
        if (d < 1.2f) { float inside = 1.f - smoothstep(0.75f, 0.95f, d); float rim = smoothstep(0.8f, 0.95f, d) * (1.f - smoothstep(1.0f, 1.2f, d));
          a = a * (1.f - 0.6f * inside) + cc * rim * pig * 0.9f; }
      }
      a = clampf(a, 0.f, 0.97f);
      float shade = 1.f - 0.28f * clampf(edge * 4.f) * edgeK - 0.14f * gran * (1.f - g);
      float* o = &layer.px[i * 4];
      o[0] = sp_.color.r * shade * a; o[1] = sp_.color.g * shade * a; o[2] = sp_.color.b * shade * a; o[3] = a;
    }
    Patch orig(r.x0, r.y0, w, h);
    for (int y = r.y0; y < r.y1; ++y) std::copy(base_.at(r.x0, y), base_.at(r.x0, y) + 4 * w, orig.at(r.x0, y));
    s.compositePatch(orig, Blend::Smudge, 1.f);
    s.compositePatch(layer, cfg_.blend, sp_.opacity);
  }

  WCfg cfg_;
  StrokeParams sp_;
  Rng rng_;
  int W_ = 0, H_ = 0;
  std::unique_ptr<RasterSurface> buf_;
  Patch base_;
  const Mask* paper_ = nullptr;
  const Mask* mask_ = nullptr;
  PathTracker path_;
  IRect dirty_{};
  float carry_ = 0;
  std::vector<Bloom> blooms_;
};

// ================================================================================================
// BRISTLE BRUSH  (oil, acrylic, sumi-e, knife, fan, sponge...)
// ================================================================================================
enum class Tip { Flat, Round, Fan, Filbert };
struct BCfg {
  Tip tip = Tip::Flat;
  int n = 16;
  float spread = 1.f;      // fraction of radius covered by bristles
  float bw = 0.2f;         // bristle width (fraction of radius)
  float alpha = 0.9f;
  float deplete = 0.02f;   // paint load lost per (10*radius px)
  float pickup = 0.f;      // canvas colour carried into the brush
  bool smear = false;      // deposit with Blend::Smudge (wet paint replaces/mixes) instead of Normal
  float dry = 0.f;         // bristles skipped at speed -> streaks
  float impasto = 0.f;     // ridge highlight/shadow strength
  float colorVar = 0.04f;  // per-bristle value variation
  float hardness = 0.8f;
  float bleed = 0.f;       // soft ink halo around the stroke (sumi-e)
  float radLo = 0.45f;     // pressure->radius
  bool dots = false;       // sponge/stipple: bristles stamp dots instead of lines
  float jitter = 0.f;      // per-move random shift (scumbling)
  const char* grain = nullptr;
  float grainDepth = 0.f;
};

class Bristle : public Brush {
 public:
  Bristle(BrushInfo i, BCfg c) : cfg_(c) {
    info_ = std::move(i);
    addParam("carga", "Carga de tinta", 1.f, 0.1f, 1.f);
    addParam("cerdas", "Abertura das cerdas", 1.f, 0.4f, 1.6f);
    if (c.dry > 0) addParam("secura", "Secura", c.dry, 0.f, 1.f);
    if (c.pickup > 0) addParam("mistura", "Mistura com a tela", c.pickup, 0.f, 1.f);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    sp_ = sp; rng_.reseed(sp.seed * 7 + 13); path_.reset(); path_.add(in, 0.f);
    grain_ = cfg_.grain ? StampBank::instance().get(cfg_.grain) : nullptr;
    bs_.assign(cfg_.n, B{});
    for (int i = 0; i < cfg_.n; ++i) {
      B& b = bs_[i];
      float u = cfg_.n > 1 ? (i + rng_.range(-0.3f, 0.3f)) / (cfg_.n - 1) * 2.f - 1.f : 0.f;
      switch (cfg_.tip) {
        case Tip::Flat: b.o = {u, rng_.sym() * 0.06f}; break;
        case Tip::Filbert: b.o = {u, -(1.f - u * u) * 0.55f + rng_.sym() * 0.05f}; break;
        case Tip::Fan: { float a = u * 1.15f; b.o = {std::sin(a), -std::cos(a) * 0.45f}; break; }
        case Tip::Round: { Vec2 d = rng_.inDisc(); b.o = d; break; }
      }
      b.w = rng_.range(0.7f, 1.3f); b.tone = rng_.sym() * cfg_.colorVar; b.col = sp.color; b.load = rng_.range(0.85f, 1.f) * param("carga");
      b.skip = rng_.uniform();
      b.col = shiftHSV(sp.color, 0, rng_.sym() * 0.02f, b.tone);
      b.have = false; b.carry = 0;
    }
    dotCarry_ = 0;
    place(s, in.pressure, true, 0.f);
  }
  void move(ISurface& s, const InputSample& in) override {
    if (bs_.empty()) return;
    const PathPoint& pt = path_.add(in, 0.6f);
    float sn = 1.f - std::exp(-pt.speed / 900.f);
    place(s, in.pressure, false, sn);
  }
  void end(ISurface&) override { bs_.clear(); }

 private:
  struct B { Vec2 o; float w = 1, tone = 0, load = 1, skip = 0, carry = 0; Color col; Vec2 last; bool have = false; };
  void place(ISurface& s, float pressure, bool first, float sn) {
    const PathPoint& pt = path_.back();
    Vec2 dir = pt.dir.len2() > 0.01f ? pt.dir : Vec2{1, 0};
    Vec2 perp = dir.perp();
    float R = sp_.size * lerpf(cfg_.radLo, 1.15f, clampf(pressure)) ;
    float spreadK = cfg_.spread * param("cerdas");
    float dryK = cfg_.dry > 0 ? param("secura") : 0.f;
    float segLen = 0;
    for (auto& b : bs_) {
      Vec2 jit = cfg_.jitter > 0 ? rng_.inDisc() * (cfg_.jitter * R) : Vec2{};
      Vec2 p = pt.p + perp * (b.o.x * R * spreadK) + dir * (b.o.y * R * 0.6f * spreadK) + jit;
      if (!b.have) { b.last = p; b.have = true; if (!first) continue; }
      segLen = dist(b.last, p);
      float rb = std::max(0.5f, R * cfg_.bw * b.w * (0.8f + 0.4f * pressure));
      bool skip = dryK > 0 && b.skip < dryK * (0.25f + 0.85f * sn) * (1.f - b.load * 0.5f);
      if (cfg_.pickup > 0 && b.load > 0.0f) {
        Color c = s.getColor(p.x, p.y, rb * 1.6f);
        if (c.a > 0.15f) { float k = clampf(param("mistura") * 0.12f * segLen / std::max(1.f, rb) * 3.f, 0, 0.5f); b.col.r = lerpf(b.col.r, c.r, k); b.col.g = lerpf(b.col.g, c.g, k); b.col.b = lerpf(b.col.b, c.b, k); }
      }
      Dab d; d.color = b.col; d.color.a = 1.f; d.hardness = cfg_.hardness;
      float op = cfg_.alpha * std::pow(std::max(0.f, b.load), 0.6f) * sp_.opacity;
      d.opacity = clampf(op);
      d.lockAlpha = sp_.lockAlpha;
      if (cfg_.smear) { d.blend = Blend::Smudge; d.colorAlpha = 1.f; d.lockAlpha = false; }
      if (grain_) { d.grain = grain_; d.grainDepth = cfg_.grainDepth; d.grainScale = 1.f; d.grainBias = (pressure - 0.5f) * 0.6f; }
      d.seed = rng_.next();
      if (!skip && b.load > 0.02f) {
        if (cfg_.dots) {
          b.carry += segLen;
          if (b.carry >= R * 0.4f || first) { b.carry = 0; d.x = p.x; d.y = p.y; d.radius = rb * rng_.range(0.7f, 1.4f); d.opacity = clampf(op * rng_.range(0.6f, 1.f)); s.drawDab(d); }
        } else {
          dabLine(s, b.last, p, rb, rb, d, 0.3f);
        }
        if (cfg_.impasto > 0 && segLen > 0.2f) {
          Vec2 n = perp * (rb * 0.7f);
          Dab hl; hl.color = Color{1, 1, 0.95f, 1}; hl.blend = Blend::Overlay; hl.lockAlpha = true; hl.hardness = 0.5f; hl.opacity = cfg_.impasto * 0.55f;
          dabLine(s, b.last - n, p - n, rb * 0.45f, rb * 0.45f, hl, 0.4f);
          Dab sh = hl; sh.color = Color{0.1f, 0.05f, 0.05f, 1}; sh.opacity = cfg_.impasto * 0.5f;
          dabLine(s, b.last + n, p + n, rb * 0.45f, rb * 0.45f, sh, 0.4f);
        }
        b.load -= cfg_.deplete * segLen / std::max(1.f, R * 10.f) * 10.f * (0.5f + b.w * 0.5f);
        if (b.load < 0) b.load = 0;
      }
      b.last = p;
    }
    if (cfg_.bleed > 0 && !first) {
      Dab h; h.color = sp_.color; h.hardness = 0.f; h.opacity = cfg_.bleed * sp_.opacity; h.radius = R * 1.5f * spreadK; h.x = pt.p.x; h.y = pt.p.y; s.drawDab(h);
    }
  }
  BCfg cfg_;
  StrokeParams sp_;
  Rng rng_;
  PathTracker path_;
  std::vector<B> bs_;
  const Mask* grain_ = nullptr;
  float dotCarry_ = 0;
};

// ================================================================================================
// SPRAY CAN with drips
// ================================================================================================
class SprayCan : public DabBrush {
 public:
  SprayCan(BrushInfo i, DabBrushSpec s) : DabBrush(std::move(i), std::move(s)) {
    addParam("gotejar", "Escorrimento", 1.f, 0.f, 1.f);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    DabBrush::begin(s, sp, in); sp_ = sp; pos_ = {in.x, in.y}; held_ = 0; drips_.clear(); rng_.reseed(sp.seed + 91);
  }
  void move(ISurface& s, const InputSample& in) override { DabBrush::move(s, in); pos_ = {in.x, in.y}; held_ = std::max(0.f, held_ - 0.05f); }
  void hold(ISurface& s, float dt) override {
    DabBrush::hold(s, dt);
    held_ += dt;
    float k = param("gotejar");
    if (held_ > 0.7f && k > 0 && rng_.chance(std::min(1.f, dt * 2.5f * k))) {
      Drip d; d.p = pos_ + Vec2{rng_.sym() * sp_.size * 0.7f, sp_.size * rng_.range(0.1f, 0.5f)}; d.w = rng_.range(0.07f, 0.12f) * sp_.size; d.v = rng_.range(25, 60); d.life = rng_.range(0.6f, 1.8f);
      drips_.push_back(d);
    }
    for (auto& d : drips_) {
      if (d.life <= 0) continue;
      Vec2 np = d.p + Vec2{0, d.v * dt}; Dab dab; dab.color = sp_.color; dab.hardness = 0.9f; dab.opacity = 0.9f;
      dabLine(s, d.p, np, d.w, d.w * 0.85f, dab, 0.3f);
      d.p = np; d.w *= 0.995f; d.life -= dt; d.v *= 0.985f;
      if (d.life <= 0) { Dab bead = dab; bead.opacity = 1; dabRound(s, d.p, d.w * 1.5f, bead); }
    }
  }
 private:
  struct Drip { Vec2 p; float w, v, life; };
  StrokeParams sp_; Vec2 pos_; float held_ = 0; std::vector<Drip> drips_; Rng rng_;
};

}  // namespace

// ================================================================================================
void registerCustom(BrushLibrary& lib) {
  // ---------------- watercolour ----------------
  auto wc = [&](const char* id, const char* name, const char* real, const char* model, float size, WCfg c, Color col,
                const char* zip = nullptr, float mn = 4, float mx = 120) {
    BrushInfo b = mk(id, name, "aquarela", real, model, size, col, mn, mx);
    b.demoBg = "paper";
    if (zip) b.stampName = zip;
    lib.add(b, [b, c]() -> std::unique_ptr<Brush> { return std::make_unique<Watercolor>(b, c); });
  };
  { WCfg c; c.opacity = 0.55f; c.edge = 0.5f; c.granulate = 0.5f; c.wet = 7.f; c.hardness = 0.5f;
    wc("aquarela.umido-umido", "Aquarela úmido sobre úmido", "papel molhado: o pigmento se espalha para além do traço com bordas suaves, cores se fundem; o pigmento ainda acumula nas margens.",
       "buffer de traço (sem acúmulo interno), sangramento por desfoque no fim do traço, escurecimento de borda, granulação do papel.", 26, c, Color{0.15f, 0.4f, 0.75f, 1}); }
  { WCfg c; c.opacity = 0.65f; c.edge = 1.f; c.granulate = 0.6f; c.hardness = 0.8f; c.radLo = 0.7f;
    wc("aquarela.umido-seco", "Aquarela úmido sobre seco", "papel seco: bordas nítidas, o pigmento se junta na borda quando seca (anel mais escuro).",
       "sem sangramento; forte escurecimento de borda; granulação média.", 24, c, Color{0.8f, 0.25f, 0.2f, 1}); }
  { WCfg c; c.opacity = 0.9f; c.edge = 0.15f; c.granulate = 0.9f; c.dry = 0.85f; c.hardness = 0.9f; c.rough = 0.7f; c.spacing = 0.1f;
    wc("aquarela.pincel-seco", "Aquarela pincel seco", "pincel quase sem água arrasta pigmento só nos picos do papel: traço quebrado com muitas falhas brancas, mais quebrado quanto mais rápido.",
       "limiar do grão do papel cresce com a velocidade (secura gravada no buffer).", 20, c, Color{0.25f, 0.2f, 0.15f, 1}); }
  { WCfg c; c.opacity = 0.35f; c.edge = 0.25f; c.granulate = 0.25f; c.wet = 3.f; c.hardness = 0.9f; c.radLo = 0.9f;
    wc("aquarela.lavis", "Lavis plano (wash)", "pincel largo carregado de água: camada translúcida uniforme, quase sem granulação.",
       "grande, baixa saturação, borda leve.", 48, c, Color{0.5f, 0.7f, 0.9f, 1}, nullptr, 10, 160); }
  { WCfg c; c.opacity = 0.22f; c.edge = 0.15f; c.granulate = 0.2f; c.hardness = 0.8f;
    wc("aquarela.veladura", "Veladura (glaze)", "camada finíssima de cor por cima da seca: a cor de baixo transparece e as duas se misturam opticamente.",
       "Multiply com pigmento baixo: sobrepor aprofunda a cor sem tapar.", 34, c, Color{0.9f, 0.6f, 0.1f, 1}); }
  { WCfg c; c.opacity = 0.6f; c.edge = 0.6f; c.granulate = 0.7f; c.wet = 3.f; c.salt = 0.85f; c.hardness = 0.6f;
    wc("aquarela.sal", "Aquarela com sal", "grãos de sal absorvem a água e pigmento: sobram manchas claras em estrela.",
       "máscara celular (worley) esvazia o pigmento em blotches.", 30, c, Color{0.2f, 0.35f, 0.6f, 1}); }
  { WCfg c; c.opacity = 0.6f; c.edge = 0.9f; c.granulate = 0.45f; c.wet = 5.f; c.blooms = 3; c.hardness = 0.55f;
    wc("aquarela.floracao", "Floração (backrun)", "água nova numa área quase seca empurra o pigmento: cria uma 'couve-flor' clara com anel escuro.",
       "bolhas aleatórias ao longo do traço esvaziadas no miolo e com anel de pigmento.", 28, c, Color{0.6f, 0.25f, 0.5f, 1}); }
  { WCfg c; c.opacity = 0.97f; c.edge = 0.f; c.granulate = 0.12f; c.hardness = 0.85f; c.blend = Blend::Normal; c.radLo = 0.6f;
    wc("aquarela.guache", "Guache", "tinta opaca mate: cobre o que está embaixo, seca sem brilho, ligeiramente giz.",
       "Normal blend opaco com granulação mínima.", 22, c, Color{0.85f, 0.3f, 0.2f, 1}); }
  { WCfg c; c.opacity = 0.5f; c.edge = 0.1f; c.granulate = 0.2f; c.wet = 12.f; c.hardness = 0.35f;
    wc("aquarela.tinta-na-agua", "Tinta difundindo na água", "gota de tinta em água parada: expande em nuvem, volutas e perde densidade.",
       "grande sangramento com baixíssima definição de borda.", 22, c, Color{0.1f, 0.1f, 0.3f, 1}); }
  { WCfg c; c.opacity = 0.7f; c.edge = 0.4f; c.granulate = 1.f; c.hardness = 0.6f; c.rough = 1.4f;
    wc("aquarela.granulado", "Pigmento granulado", "pigmentos pesados (ultramarino, siena) assentam nos vales do papel: aspecto sujo-granulado.",
       "granulação máxima + escurecimento nos vales.", 26, c, Color{0.35f, 0.3f, 0.5f, 1}); }
  { WCfg c; c.opacity = 0.75f; c.edge = 0.5f; c.wet = 10.f; c.granulate = 0.5f; c.hardness = 0.6f; c.spacing = 0.28f; c.mask = "water_bleed"; c.rotate = true;
    wc("zip.water_bleed", "Mancha que sangra", "gota de aquarela solta numa área úmida: mancha de contorno orgânico com tons de profundidade.",
       "carimbo de mancha irregular girado ao acaso + sangramento no fim do traço.", 24, c, Color{0.2f, 0.5f, 0.6f, 1}, "water_bleed"); }
  { WCfg c; c.opacity = 0.95f; c.edge = 0.9f; c.wet = 2.f; c.granulate = 0.4f; c.hardness = 1.f; c.spacing = 0.45f; c.mask = "water_point"; c.rotate = true;
    wc("zip.water_point", "Pingo de pigmento", "toque do pincel carregado: mancha densa de pigmento, escura no centro e borda definida.",
       "carimbo de gota densa com borda marcada, giro aleatório.", 20, c, Color{0.55f, 0.1f, 0.2f, 1}, "water_point"); }
  { WCfg c; c.opacity = 0.8f; c.edge = 0.7f; c.dry = 0.35f; c.wet = 3.f; c.blooms = 2; c.granulate = 0.8f; c.hardness = 1.f; c.spacing = 0.3f; c.mask = "water_real"; c.rotate = true;
    wc("zip.water_real", "Aquarela real (rasgada)", "toque largo de aquarela com bordas rasgadas, respingos e partes de pigmento seco.",
       "carimbo natural girado ao acaso, secura parcial e floração.", 26, c, Color{0.3f, 0.45f, 0.25f, 1}, "water_real"); }

  // ---------------- bristle brushes ----------------
  auto br = [&](const char* id, const char* name, const char* cat, const char* real, const char* model, float size, BCfg c, Color col,
                const char* zip = nullptr, const char* bg = "") {
    BrushInfo b = mk(id, name, cat, real, model, size, col, 3, 120);
    b.demoBg = bg; if (zip) b.stampName = zip;
    lib.add(b, [b, c]() -> std::unique_ptr<Brush> { return std::make_unique<Bristle>(b, c); });
  };
  { BCfg c; c.tip = Tip::Flat; c.n = 18; c.spread = 1.f; c.bw = 0.14f; c.alpha = 0.95f; c.deplete = 0.03f; c.pickup = 0.5f; c.smear = true; c.impasto = 0.5f; c.colorVar = 0.05f;
    br("oleo.chata", "Óleo – pincel chato", "oleo-acrilica", "cerdas de porco em linha: arrasta tinta em fita larga com sulcos paralelos; mistura com a tinta úmida por baixo e vai descarregando.",
       "18 cerdas independentes (carga, tom e largura próprios), depositam com Smudge para misturar com a tela úmida, impasto com luz/sombra nos sulcos.", 24, c, Color{0.75f, 0.2f, 0.15f, 1}); }
  { BCfg c; c.tip = Tip::Round; c.n = 22; c.spread = 0.7f; c.bw = 0.2f; c.alpha = 0.95f; c.deplete = 0.025f; c.pickup = 0.4f; c.smear = true; c.impasto = 0.35f; c.radLo = 0.3f;
    br("oleo.redondo", "Óleo – pincel redondo", "oleo-acrilica", "pincel redondo termina em ponta: fino com pouca pressão, abre ao pressionar; ótimo para detalhes e contornos.",
       "cerdas em disco, o raio de abertura cresce com a pressão.", 14, c, Color{0.2f, 0.35f, 0.65f, 1}); }
  { BCfg c; c.tip = Tip::Filbert; c.n = 20; c.spread = 1.f; c.bw = 0.15f; c.alpha = 0.95f; c.deplete = 0.028f; c.pickup = 0.45f; c.smear = true; c.impasto = 0.4f;
    br("oleo.filbert", "Óleo – pincel filbert", "oleo-acrilica", "filbert tem ponta oval: traços com bordas arredondadas e mistura macia nas pontas.",
       "bristles dispostos em arco, as pontas do arco ficam recuadas.", 22, c, Color{0.2f, 0.55f, 0.3f, 1}); }
  { BCfg c; c.tip = Tip::Fan; c.n = 26; c.spread = 1.15f; c.bw = 0.07f; c.alpha = 0.35f; c.deplete = 0.01f; c.pickup = 0.9f; c.smear = true; c.colorVar = 0.01f;
    br("oleo.leque", "Óleo – pincel leque (esfumar)", "oleo-acrilica", "pincel em leque: cerdas finas e abertas, serve para esfumar e unir tons, deixa texturas de pelos finos.",
       "cerdas em arco aberto, quase só pickup (mistura), pouca tinta depositada.", 28, c, Color{0.9f, 0.7f, 0.3f, 1}); }
  { BCfg c; c.tip = Tip::Flat; c.n = 26; c.spread = 1.f; c.bw = 0.1f; c.alpha = 0.9f; c.deplete = 0.1f; c.dry = 0.85f; c.pickup = 0.1f; c.colorVar = 0.07f;
    br("oleo.seco", "Óleo – pincel seco", "oleo-acrilica", "pincel quase sem tinta: só os picos da tela recebem pigmento e cada cerda deixa seu risco; acaba rápido.",
       "cerdas com secura por velocidade, carga esgota depressa.", 24, c, Color{0.9f, 0.85f, 0.7f, 1}); }
  { BCfg c; c.tip = Tip::Flat; c.n = 32; c.spread = 1.f; c.bw = 0.1f; c.alpha = 1.f; c.deplete = 0.02f; c.pickup = 0.6f; c.smear = true; c.impasto = 0.9f; c.colorVar = 0.03f;
    br("oleo.impasto", "Óleo – impasto grosso", "oleo-acrilica", "tinta em camada grossa: o pincel deixa relevo; a luz bate nas cristas e as sombras ficam nos sulcos.",
       "32 cerdas em linha com highlight (Overlay) e sombra nos dois lados de cada sulco.", 26, c, Color{0.9f, 0.7f, 0.15f, 1}); }
  { BCfg c; c.tip = Tip::Flat; c.n = 56; c.spread = 1.f; c.bw = 0.09f; c.alpha = 1.f; c.deplete = 0.005f; c.pickup = 0.85f; c.smear = true; c.impasto = 0.7f; c.colorVar = 0.015f; c.radLo = 0.8f;
    br("oleo.espatula", "Espátula (palette knife)", "oleo-acrilica", "lâmina de aço: arrasta a tinta em placas lisas, deixa crista nas bordas e raspa o que estava embaixo.",
       "56 'cerdas' juntas formam uma lâmina lisa com forte pickup e crista (impasto).", 24, c, Color{0.8f, 0.45f, 0.2f, 1}); }
  { BCfg c; c.tip = Tip::Flat; c.n = 16; c.spread = 1.f; c.bw = 0.17f; c.alpha = 1.f; c.deplete = 0.008f; c.pickup = 0.f; c.hardness = 0.95f; c.colorVar = 0.02f;
    br("acrilica.chata", "Acrílica – pincel chato", "oleo-acrilica", "acrílica seca rápido: cobre opaca, traços definidos que não se misturam; cerdas deixam leves sulcos.",
       "como o chato a óleo mas sem pickup e com bordas duras.", 22, c, Color{0.15f, 0.55f, 0.75f, 1}); }
  { BCfg c; c.tip = Tip::Round; c.n = 20; c.spread = 1.f; c.bw = 0.25f; c.alpha = 0.7f; c.deplete = 0.06f; c.dry = 0.55f; c.jitter = 0.15f; c.pickup = 0.f; c.colorVar = 0.05f;
    br("oleo.esfumado-granulado", "Scumble (velatura seca)", "oleo-acrilica", "camada seca e irregular passada por cima: deixa a cor de baixo aparecer em poros.",
       "cerdas em disco, jitter por movimento e secura por velocidade.", 26, c, Color{0.85f, 0.8f, 0.7f, 1}); }
  { BCfg c; c.tip = Tip::Round; c.n = 30; c.spread = 1.f; c.bw = 0.25f; c.alpha = 0.8f; c.deplete = 0.01f; c.dots = true; c.jitter = 0.05f; c.colorVar = 0.08f; c.pickup = 0.f;
    br("oleo.esponja", "Esponja / pontilhado", "oleo-acrilica", "esponja natural: carimba furinhos irregulares; cada toque tem poros diferentes.",
       "30 pontos por toque (dots), tamanho aleatório e variação de tom.", 20, c, Color{0.35f, 0.5f, 0.3f, 1}); }
  { BCfg c; c.tip = Tip::Flat; c.n = 20; c.spread = 1.f; c.bw = 0.15f; c.alpha = 0.9f; c.deplete = 0.02f; c.pickup = 0.3f; c.smear = true; c.impasto = 0.3f; c.grain = "@canvas"; c.grainDepth = 0.7f; c.colorVar = 0.04f;
    br("oleo.tela-trama", "Pincel sobre tela texturizada", "oleo-acrilica", "tela de linho: a tinta só adere nos fios, a trama aparece nas áreas pouco carregadas.",
       "grão de trama na deposição; mistura moderada.", 22, c, Color{0.6f, 0.3f, 0.2f, 1}); }
  { BCfg c; c.tip = Tip::Round; c.n = 30; c.spread = 0.85f; c.bw = 0.3f; c.alpha = 0.34f; c.deplete = 0.012f; c.dry = 0.75f; c.bleed = 0.05f; c.hardness = 0.7f; c.radLo = 0.25f; c.colorVar = 0.02f;
    br("tinta.sumie", "Sumi-ê (pincel de nanquim)", "tinta", "pincel de pelo largo e pontudo: o centro fica molhado e escuro, as bordas secas deixam fios brancos (hakuboku); tinta vaza um pouco no papel.",
       "30 cerdas de baixa opacidade acumulando no centro, secura por velocidade, halo de bleed.", 14, c, Color{0.04f, 0.04f, 0.05f, 1}, nullptr, "paper"); }
  { BCfg c; c.tip = Tip::Round; c.n = 22; c.spread = 0.85f; c.bw = 0.28f; c.alpha = 0.4f; c.deplete = 0.1f; c.dry = 0.95f; c.colorVar = 0.02f; c.radLo = 0.3f;
    br("tinta.pincel-seco", "Pincel seco de nanquim", "tinta", "pincel quase sem tinta arrastado rápido: apenas filamentos pretos entrecortados (feibai).",
       "alta secura e depleção rápida.", 14, c, Color{0.02f, 0.02f, 0.03f, 1}, nullptr, "paper"); }

  // ---------------- spray can ----------------
  {
    DabBrushSpec s; s[Set::Spacing].set(0.9f); s.stamp.count = 14; s.stamp.scatter = 8.f; s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.06f, 0.1f));
    s[Set::Hardness].set(0.85f); s[Set::Opacity].set(0.9f); s[Set::JitterRadius].set(0.7f); s[Set::DabsPerSecond].set(40.f);
    BrushInfo b = mk("aerografo.lata-spray", "Spray (lata) com escorrimento", "aerografo", "tinta spray: névoa de gotas, mais densa no centro; se a lata fica parada a tinta acumula e escorre em fios.",
                     "disco de gotas com densidade uniforme; segurando parado (hold) nascem escorrimentos com gravidade e gota na ponta.", 28, Color{0.9f, 0.15f, 0.2f, 1}, 8, 120);
    b.demoHold = 2.5f; b.demo = "line"; b.demoBg = "white";
    lib.add(b, [b, s]() -> std::unique_ptr<Brush> { return std::make_unique<SprayCan>(b, s); });
  }
}

}}  // namespace ab::media
