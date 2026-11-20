// light module, part 2: fogo (fire), fumaca (smoke/fog), ceu (sky).
#include <functional>

#include "light_util.hpp"

namespace ab {
namespace lu {
namespace {

struct EC {
  ISurface& s;
  const StrokeParams& sp;
  Rng& rng;
  Vec2 p;
  Vec2 dir;
  float pressure;
  float size;      // radius px scaled by pressure
  int idx;
  float arc;       // arc length so far
  float k;         // gain
};

// Emits an element every `spacingFrac*size` px along the path. Parametric & deterministic per frame.
class EmitBrush : public StrokeBase {
 public:
  using Fn = std::function<void(EC&)>;
  EmitBrush(BrushInfo bi, float spacingFrac, Fn emit, Fn atEnd = nullptr, Fn atHold = nullptr)
      : spacing_(spacingFrac), emit_(std::move(emit)), atEnd_(std::move(atEnd)), atHold_(std::move(atHold)) {
    info_ = std::move(bi);
    addParam("intensity", "Intensidade", 1.f, 0.2f, 2.f);
    addParam("altura", "Altura/escala", 1.f, 0.3f, 2.5f);
  }

 protected:
  void onBegin(ISurface& s, const InputSample& in) override { next_ = 0; idx_ = 0; call(s, path_.back(), emit_); next_ = step(); }
  void onSegment(ISurface& s, const PathPoint&, const PathPoint& cur) override {
    while (next_ <= path_.length()) { call(s, path_.at(next_), emit_); next_ += step(); }
    (void)cur;
  }
  void onEnd(ISurface& s) override { if (atEnd_) call(s, path_.back(), atEnd_); }
  void onHold(ISurface& s, float) override { if (atHold_) call(s, path_.back(), atHold_); }

 private:
  float step() const { return std::max(1.5f, spacing_ * sp_.size); }
  void call(ISurface& s, const PathPoint& p, const Fn& f) {
    float sz = rad(p);
    EC c{s, sp_, rng_, p.p, p.dir, p.pressure, sz, idx_++, p.d, gain() * param("intensity")};
    f(c);
  }
  float spacing_; Fn emit_, atEnd_, atHold_;
  float next_ = 0; int idx_ = 0;
};

void regE(BrushLibrary& lib, BrushInfo bi, float spacing, EmitBrush::Fn e, EmitBrush::Fn end = nullptr, EmitBrush::Fn hold = nullptr) {
  lib.add(bi, [bi, spacing, e, end, hold]() -> std::unique_ptr<Brush> { return std::make_unique<EmitBrush>(bi, spacing, e, end, hold); });
}

// One tongue of flame rising from `base`. H height, w half width at the root.
void tongue(EC& c, Vec2 base, float H, float w, int kind, float lean, float heat0 = 1.f, float alpha = 0.06f) {
  uint32_t sd = hash32(c.sp.seed + (uint32_t)c.idx * 131u);
  float fr = c.sp.frame * 0.53f + c.sp.time * 3.1f;
  const int N = 20;
  for (int i = 0; i < N; ++i) {
    float t = i / (float)(N - 1);
    float sway = gradNoise(t * 2.2f + (sd & 255) * 0.1f, fr, sd) * w * 1.6f * t + (lean + c.sp.wind * 0.55f) * H * t * t;
    float rr = w * std::pow(1.f - t, 0.8f) * (0.55f + 0.45f * std::sin(t * 2.4f + 0.4f)) + 0.4f;
    Vec2 p{base.x + sway, base.y - H * t * (0.92f + 0.1f * valueNoise(t * 3.f, fr, sd))};
    float heat = clampf((1.f - t * 1.15f) * heat0 + 0.12f);
    Color col = heatRamp(heat, kind);
    softAt(c.s, p, rr * 1.9f, col, alpha * c.k * 0.55f, 0.f);          // outer lick
    softAt(c.s, p, rr * 1.0f, heatRamp(clampf(heat + 0.2f), kind), alpha * c.k, 0.2f);  // body
    if (t < 0.55f) softAt(c.s, p, rr * 0.5f, heatRamp(clampf(heat + 0.45f), kind), alpha * 0.9f * c.k, 0.4f);  // hot core
  }
}

void ember(EC& c, Vec2 base, float travel, int kind) {
  uint32_t sd = hash32(c.sp.seed + c.idx * 977u + 5u);
  Rng r(sd + c.sp.frame * 3u);
  for (int i = 0; i < 3; ++i) {
    float t = r.uniform();
    Vec2 p{base.x + r.sym() * travel * 0.4f + c.sp.wind * travel * t * t + std::sin(t * 9.f + i) * 4.f, base.y - t * travel * r.range(0.4f, 1.f)};
    float life = 1.f - t;
    Color col = heatRamp(0.25f + 0.7f * life, kind);
    softAt(c.s, p, 3.f * life + 0.8f, col, 0.9f * c.k * (0.4f + life), 0.6f);
    bloomAt(c.s, p, 7.f * life + 2.f, col, 0.35f * c.k * life);
  }
}

void registerFlames(BrushLibrary& lib) {
  auto flameBrush = [&](const char* id, const char* name, int kind, const char* real, float sizeDef, const char* demo = "wave", float spacing = 0.6f) {
    BrushInfo bi = mkInfo(id, name, "fogo", real,
                          "linguas de chama independentes (20 elipses aditivas empilhadas) com flutuacao por ruido de gradiente que muda a cada frame, "
                          "empuxo, inclinacao pelo vento e cor por rampa de corpo negro (branco-amarelo-laranja-vermelho-escuro).",
                          sizeDef, kind == 0 ? Color{1, 0.55f, 0.1f, 1} : (kind == 1 ? Color{0.2f, 0.5f, 1, 1} : (kind == 2 ? Color{0.2f, 1, 0.3f, 1} : Color{0.7f, 0.2f, 1, 1})),
                          Color{1, 0.9f, 0.6f, 1}, true, true, demo, "dark");
    regE(lib, bi, spacing, [kind](EC& c) {
      float H = c.size * c.rng.range(3.2f, 5.f) * 1.f;
      tongue(c, c.p, H, c.size * 0.9f, kind, c.rng.sym() * 0.15f);
    });
  };
  flameBrush("fogo.chama", "Chama realista", 0, "o fogo e gas incandescente: o ar quente sobe (empuxo), a turbulencia cria linguas que tremem, e a cor depende da temperatura (branco no nucleo, laranja, vermelho, some em fumaca).", 14);
  flameBrush("fogo.chama-azul", "Chama azul", 1, "chama de gas/alta temperatura queima mais limpa e azulada (combustao completa).", 13);
  flameBrush("fogo.chama-verde", "Chama verde magica", 2, "sais de cobre colorem a chama de verde; aqui vira fogo magico.", 13);
  flameBrush("fogo.chama-roxa", "Chama roxa magica", 3, "chama alquimica/fogo fatuo; mesma fisica do fogo com rampa roxa.", 13);

  {  // candle: a single steady teardrop at the stroke start (+ trail of tiny glow)
    BrushInfo bi = mkInfo("fogo.vela", "Chama de vela", "fogo",
                          "a chama de vela e laminar e quase imovel: gota alongada, base azul perto do pavio (combustao completa), miolo escuro de cera vaporizada, ponta amarela.",
                          "gota aditiva em camadas (halo quente, corpo amarelo, miolo transparente, base azul) na posicao inicial; so oscila levemente com o frame.",
                          10, Color{1, 0.7f, 0.2f, 1}, Color{0.3f, 0.5f, 1, 1}, true, true, "dot", "dark", 0.f, false);
    regE(lib, bi, 1e6f, [](EC& c) {
      float fr = c.sp.frame * 0.7f + c.sp.time * 4.f;
      float lean = gradNoise(fr, 3.f, c.sp.seed) * 0.12f + c.sp.wind * 0.25f;
      Vec2 b = c.p; float s = c.size;
      bloomAt(c.s, b + Vec2{0, -s * 3.f}, s * 9.f, Color{1, 0.6f, 0.2f, 1}, 0.55f * c.k);
      for (int i = 0; i < 16; ++i) {
        float t = i / 15.f;
        Vec2 p{b.x + lean * s * 8.f * t * t, b.y - t * s * 6.f};
        float r = s * 1.3f * std::sin(clampf(t * 1.05f) * kPi * 0.92f + 0.12f) + 0.5f;
        softAt(c.s, p, r * 1.5f, Color{1, 0.65f, 0.15f, 1}, 0.10f * c.k, 0.1f);
        softAt(c.s, p, r * 0.9f, Color{1, 0.9f, 0.5f, 1}, 0.14f * c.k, 0.3f);
      }
      softAt(c.s, b + Vec2{lean * s, -s * 2.0f}, s * 0.55f, Color{0.25f, 0.1f, 0.05f, 1}, 0.25f, 0.2f);  // dark wax-vapour core (Normal-ish via low add)
      softAt(c.s, b + Vec2{0, -s * 0.2f}, s * 0.9f, Color{0.15f, 0.35f, 1, 1}, 0.45f * c.k, 0.1f);       // blue base
    });
  }
  {  // fire trail: flames follow the stroke, tallest at the head (current pen position)
    BrushInfo bi = mkInfo("fogo.rastro", "Rastro de fogo", "fogo",
                          "um objeto em chamas deixa para tras fogo que sobe e se dissipa; quanto mais velho o ponto, mais baixo e frio.",
                          "linguas pequenas com altura proporcional a velocidade, sempre empurradas contra o sentido do movimento, com brasas.",
                          11, Color{1, 0.5f, 0.1f, 1}, Color{1, 0.9f, 0.5f, 1}, true, true, "wave", "dark");
    regE(lib, bi, 0.3f, [](EC& c) {
      Vec2 back = c.dir * -0.5f;
      tongue(c, c.p, c.size * c.rng.range(2.5f, 4.2f), c.size * 0.85f, 0, back.x * 0.4f);
      if (c.idx % 3 == 0) ember(c, c.p, c.size * 5.f, 0);
    });
  }
  {  // torch
    BrushInfo bi = mkInfo("fogo.tocha", "Tocha", "fogo",
                          "uma tocha tem um cabo escuro e uma massa de chama larga e ondulante com fagulhas e fumaca no topo.",
                          "o traco vira o cabo (madeira); no fim do gesto surge a chama grande com varias linguas, fagulhas e halo de luz quente.",
                          12, Color{1, 0.55f, 0.1f, 1}, Color{0.35f, 0.2f, 0.1f, 1}, true, true, "up", "dark", 0.f);
    regE(lib, bi, 0.5f, [](EC& c) {
      Dab d; d.color = Color{0.25f, 0.15f, 0.08f, 1}; d.hardness = 0.9f; d.x = c.p.x; d.y = c.p.y; d.radius = c.size * 0.45f; c.s.drawDab(d);
    }, [](EC& c) {
      Vec2 top = c.p;
      bloomAt(c.s, top + Vec2{0, -c.size * 3}, c.size * 14.f, Color{1, 0.5f, 0.15f, 1}, 0.5f * c.k);
      for (int i = 0; i < 6; ++i) {
        EC cc = c; cc.idx = i + 40;
        tongue(cc, top + Vec2{(i - 2.5f) * c.size * 0.45f, 0}, c.size * c.rng.range(3.f, 6.5f), c.size * 0.9f, 0, c.rng.sym() * 0.2f, 1.f, 0.06f);
      }
      for (int i = 0; i < 4; ++i) { EC cc = c; cc.idx = 90 + i; ember(cc, top, c.size * 9.f, 0); }
    });
  }
  {  // campfire
    BrushInfo bi = mkInfo("fogo.fogueira", "Fogueira", "fogo",
                          "toras apoiadas em cruz, brasas vermelhas no leito e chamas que sobem da massa de brasa; a luz alaranjada ilumina o chao ao redor.",
                          "toras escuras (elipses com veio) ao longo do traco, leito de brasas pulsante (aditivo) e linguas de fogo mais altas no centro.",
                          12, Color{1, 0.5f, 0.1f, 1}, Color{0.3f, 0.17f, 0.08f, 1}, true, true, "line", "dark");
    regE(lib, bi, 0.55f, [](EC& c) {
      Dab log; log.color = Color{0.2f, 0.12f, 0.07f, 1}; log.hardness = 0.85f; log.x = c.p.x; log.y = c.p.y + c.size * 0.5f;
      log.radius = c.size * 0.55f; log.aspect = 2.2f; log.angle = c.rng.sym() * 0.5f; c.s.drawDab(log);
      bloomAt(c.s, c.p + Vec2{0, c.size * 0.2f}, c.size * 2.8f, Color{1, 0.35f, 0.05f, 1}, 0.4f * c.k);
      float mid = 0.55f + 0.45f * std::sin(c.arc * 0.02f + 1.f);
      tongue(c, c.p, c.size * c.rng.range(2.5f, 5.5f) * mid + c.size, c.size * 0.9f, 0, c.rng.sym() * 0.15f, 1.f, 0.06f);
      if (c.idx % 2 == 0) ember(c, c.p, c.size * 6.f, 0);
    });
  }
  {  // embers & sparks rising
    BrushInfo bi = mkInfo("fogo.brasas", "Brasas e fagulhas", "fogo",
                          "fagulhas sao particulas incandescentes que sobem com o ar quente, esfriando de amarelo para vermelho ate apagar, com deriva do vento.",
                          "tres particulas por passo com trajetoria fechada: subida + vento + sinuosidade, brilho decai com a vida; halo aditivo.",
                          9, Color{1, 0.6f, 0.15f, 1}, Color{1, 0.9f, 0.5f, 1}, true, true, "wave", "dark");
    regE(lib, bi, 0.6f, [](EC& c) { ember(c, c.p, c.size * 14.f, 0); });
  }
  {  // fireball explosion at end
    BrushInfo bi = mkInfo("fogo.explosao", "Bola de fogo / explosao", "fogo",
                          "uma explosao e uma bola de gas quente que se expande em couve-flor turbulenta: nucleo branco, borda laranja e fumaca escura que sobe.",
                          "no fim do gesto: dezenas de bolhas aditivas distribuidas em esfera por ruido (maiores e mais frias na borda) + anel de fumaca escura (Normal) + flash.",
                          22, Color{1, 0.5f, 0.1f, 1}, Color{0.12f, 0.1f, 0.1f, 1}, true, true, "dot", "dark", 0.f, false);
    regE(lib, bi, 1e6f, [](EC&) {}, [](EC& c) {
      Vec2 o = c.p; float R = c.size * 5.f; Rng& r = c.rng;
      for (int i = 0; i < 26; ++i) {   // smoke first (under)
        Vec2 d = r.inDisc();
        Dab sm; sm.color = Color{0.1f, 0.09f, 0.09f, 1}; sm.hardness = 0.f; sm.opacity = 0.18f; sm.x = o.x + d.x * R * 1.1f; sm.y = o.y + d.y * R * 0.9f - R * 0.5f; sm.radius = R * r.range(0.3f, 0.6f);
        c.s.drawDab(sm);
      }
      for (int i = 0; i < 40; ++i) {
        Vec2 d = r.inDisc() * R; float rr = d.len() / R;
        float rad = R * r.range(0.25f, 0.5f) * (1.f - 0.35f * (1.f - rr));
        softAt(c.s, o + d, rad, heatRamp(0.85f - rr * 0.65f), 0.2f * c.k, 0.f);
      }
      for (int i = 0; i < 18; ++i) { Vec2 d = r.inDisc() * R * 0.45f; softAt(c.s, o + d, R * 0.3f, heatRamp(1.f), 0.2f * c.k, 0.f); }
      bloomAt(c.s, o, R * 2.6f, Color{1, 0.6f, 0.2f, 1}, 0.6f * c.k);
    });
  }
  {  // burning paper edge
    BrushInfo bi = mkInfo("fogo.papel-queimado", "Borda de papel queimando", "fogo",
                          "o papel queima de uma frente: a borda fica preta e quebradica, e um fio de brasa laranja corre na linha de combustao, comendo para dentro.",
                          "ao longo do traco: faixa de cinza/carvao irregular (ruido) + linha de brasa aditiva que pulsa + pontos de brasa e fiapo de fumaca.",
                          9, Color{1, 0.45f, 0.08f, 1}, Color{0.05f, 0.04f, 0.04f, 1}, true, true, "wave", "paper");
    regE(lib, bi, 0.18f, [](EC& c) {
      Vec2 n = c.dir.perp();
      float w = c.size * (0.8f + 1.6f * valueNoise(c.arc * 0.05f, 2.f, c.sp.seed));
      Dab ch; ch.color = Color{0.06f, 0.05f, 0.05f, 1}; ch.hardness = 0.6f; ch.opacity = 0.9f;
      ch.x = c.p.x + n.x * w * 0.8f; ch.y = c.p.y + n.y * w * 0.8f; ch.radius = w; c.s.drawDab(ch);
      float pulse = 0.6f + 0.4f * gradNoise(c.arc * 0.08f, c.sp.frame * 0.8f, 3u);
      Dab ember = addDab(heatRamp(0.35f + 0.5f * pulse), 0.55f * pulse * c.k, 0.5f); ember.x = c.p.x; ember.y = c.p.y; ember.radius = c.size * 0.6f; c.s.drawDab(ember);
      bloomAt(c.s, c.p, c.size * 2.2f, Color{1, 0.4f, 0.05f, 1}, 0.25f * c.k * pulse);
    });
  }
  {  // lava
    BrushInfo bi = mkInfo("fogo.lava", "Lava", "fogo",
                          "lava fresca e laranja incandescente; ao esfriar forma crosta escura que racha e deixa a luz do magma vazar pelas fendas.",
                          "base de magma luminoso (aditivo), crosta escura por celulas de Worley cobrindo ~70% e fendas brilhantes nas fronteiras das celulas.",
                          16, Color{1, 0.4f, 0.05f, 1}, Color{0.1f, 0.06f, 0.05f, 1}, true, true, "wave", "dark");
    regE(lib, bi, 0.25f, [](EC& c) {
      softAt(c.s, c.p, c.size * 1.5f, heatRamp(0.55f), 0.5f * c.k, 0.3f);
      int n = 10;
      for (int i = 0; i < n; ++i) {
        Vec2 q = c.p + c.rng.inDisc() * c.size * 1.3f;
        float cid; float w = worley(q.x / (c.size * 0.7f), q.y / (c.size * 0.7f), 4, &cid);
        float crust = smoothstep(0.18f, 0.45f, w);
        Dab d; d.x = q.x; d.y = q.y; d.radius = c.size * 0.35f; d.hardness = 0.5f; d.opacity = 0.8f * crust;
        d.color = Color{0.08f + 0.05f * cid, 0.05f, 0.05f, 1}; c.s.drawDab(d);
        softAt(c.s, q, c.size * 0.25f, heatRamp(0.7f), 0.5f * (1.f - crust) * c.k, 0.4f);
      }
    });
  }
}

// --------------------------------------------------------------------------------------------
void puff(EC& c, const Mask* m, Vec2 p, float r, Color col, float a, float rot) {
  Dab d; d.shape = m; d.x = p.x; d.y = p.y; d.radius = r; d.angle = rot; d.color = col; d.opacity = a; c.s.drawDab(d);
}

void registerSmoke(BrushLibrary& lib) {
  static const Mask puffs[3] = {puffMask(1), puffMask(2), puffMask(3)};
  auto smokeBrush = [&](const char* id, const char* name, const char* real, Color col, Color tipCol, float rise, float spread, float alpha, float size,
                        bool glow, bool additiveGlowCol = false) {
    BrushInfo bi = mkInfo(id, name, "fumaca", real,
                          "particulas-bolha (mascaras fbm) sobem em coluna: cada uma cresce, clareia/esmaece, deriva com o vento e com ruido de dobra (curl), com transparencia caindo com a idade.",
                          size, col, tipCol, glow, true, "wave", glow ? "dark" : "sky", 0.f);
    regE(lib, bi, 0.6f, [=](EC& c) {
      Rng r(hash32(c.sp.seed + c.idx * 31u + c.sp.frame * 7u));
      const int N = 6;
      for (int i = 0; i < N; ++i) {
        float t = (i + r.uniform()) / N;
        float rr = c.size * (1.f + spread * t * 2.2f) * r.range(0.8f, 1.2f);
        float curl = gradNoise(c.p.x * 0.01f + t * 2.f, c.p.y * 0.01f + c.sp.frame * 0.2f, c.sp.seed) * c.size * 2.f * t;
        Vec2 p{c.p.x + curl + c.sp.wind * c.size * 5.f * t * t, c.p.y - rise * c.size * 4.f * t};
        Color cc = mix(col, tipCol, t);
        float a = alpha * (1.f - t) * (0.5f + 0.5f * (1.f - t)) * c.k;
        puff(c, &puffs[r.irange(0, 2)], p, rr, cc, a, r.uniform() * kTau);
      }
    });
  };
  smokeBrush("fumaca.cinza", "Fumaça", "fumaca e ar quente carregando particulas de fuligem: sobe em coluna, se alarga e fica mais clara e transparente ao se misturar com o ar.", Color{0.35f, 0.35f, 0.37f, 1}, Color{0.75f, 0.75f, 0.78f, 1}, 1.f, 1.f, 0.55f, 14, false);
  smokeBrush("fumaca.preta", "Fumaça densa preta", "queima de oleo/pneu libera fuligem em quantidade: coluna negra opaca que sobe mais devagar e demora a clarear.", Color{0.04f, 0.04f, 0.045f, 1}, Color{0.3f, 0.3f, 0.32f, 1}, 0.8f, 0.9f, 0.8f, 16, false);
  smokeBrush("fumaca.vapor", "Vapor", "vapor e gotinhas de agua: branco, sobe rapido, se dissolve logo; mais leve que a fumaca.", Color{1, 1, 1, 1}, Color{0.9f, 0.95f, 1, 1}, 1.5f, 1.3f, 0.4f, 12, false);
  smokeBrush("fumaca.magica", "Fumaça mágica", "fumaca de feitico: mesma dinamica da fumaca real mas emissiva, com cor que muda ao subir e brilho proprio.", Color{0.6f, 0.2f, 1, 1}, Color{0.2f, 0.9f, 1, 1}, 1.1f, 1.2f, 0.35f, 14, true);
  {  // fog bank
    BrushInfo bi = mkInfo("fumaca.neblina", "Névoa / neblina", "fumaca",
                          "neblina e nuvem rente ao solo: camadas muito largas e de baixo contraste que se deslocam na horizontal e engolem os detalhes distantes.",
                          "puffs enormes e quase transparentes, deriva horizontal, mais densa perto da linha do traco, sem subida.",
                          40, Color{0.85f, 0.88f, 0.92f, 1}, Color{1, 1, 1, 1}, false, true, "line", "night", 0.f);
    regE(lib, bi, 0.5f, [](EC& c) {
      static const Mask pm = puffMask(9, 0.8f);
      Rng r(hash32(c.sp.seed + c.idx * 13u));
      for (int i = 0; i < 3; ++i) puff(c, &pm, c.p + Vec2{r.sym() * c.size, r.sym() * c.size * 0.3f}, c.size * r.range(1.2f, 2.f), Color{0.82f, 0.86f, 0.92f, 1}, 0.12f * c.k, r.uniform() * kTau);
    });
  }
  {  // dust cloud
    BrushInfo bi = mkInfo("fumaca.poeira", "Nuvem de poeira", "fumaca",
                          "poeira levantada por um impacto/pisada: nuvem marrom-clara que se espalha para os lados e cai, com graos soltos.",
                          "puffs que se expandem lateralmente (nao sobem), cor de terra, com graos de poeira pontilhados ao redor.",
                          13, Color{0.6f, 0.5f, 0.38f, 1}, Color{0.85f, 0.78f, 0.65f, 1}, false, true, "wave", "sky");
    regE(lib, bi, 0.5f, [](EC& c) {
      static const Mask pm = puffMask(4);
      Rng r(hash32(c.sp.seed + c.idx * 17u + c.sp.frame));
      for (int i = 0; i < 4; ++i) {
        float t = r.uniform();
        Vec2 p = c.p + Vec2{r.sym() * c.size * 3.f * t, -t * c.size * 1.2f};
        puff(c, &pm, p, c.size * (0.8f + t * 1.4f), mix(Color{0.55f, 0.45f, 0.33f, 1}, Color{0.85f, 0.78f, 0.65f, 1}, t), 0.4f * (1.f - t * 0.7f), r.uniform() * kTau);
      }
      for (int i = 0; i < 6; ++i) { Dab d; d.color = Color{0.5f, 0.42f, 0.32f, 1}; d.hardness = 0.9f; d.opacity = 0.6f; d.radius = r.range(0.5f, 1.4f); Vec2 q = c.p + r.inDisc() * c.size * 3.f; d.x = q.x; d.y = q.y; c.s.drawDab(d); }
    });
  }
  {  // ink in water
    BrushInfo bi = mkInfo("fumaca.tinta-agua", "Tinta na água", "fumaca",
                          "tinta solta na agua forma filamentos que se enrolam em vortices e afinam, com franjas translucidas.",
                          "filamentos que seguem um campo de ruido (fluxo) a partir do traco: cada um e um caminho curvo de raio decrescente e alpha decrescente.",
                          9, Color{0.1f, 0.2f, 0.7f, 1}, Color{0.6f, 0.2f, 0.8f, 1}, false, true, "wave", "white");
    regE(lib, bi, 0.9f, [](EC& c) {
      Rng r(hash32(c.sp.seed + c.idx * 7u));
      for (int k = 0; k < 2; ++k) {
        Vec2 p = c.p; float a = r.uniform() * kTau; float len = c.size * r.range(6.f, 14.f);
        for (float s = 0; s < len; s += 1.5f) {
          float t = s / len;
          a += gradNoise(p.x * 0.02f, p.y * 0.02f + c.sp.frame * 0.1f, c.sp.seed) * 0.28f;
          p += fromAngle(a, 1.5f);
          Dab d; d.color = mix(c.sp.color, c.sp.color2, t); d.hardness = 0.2f; d.opacity = 0.16f * (1.f - t); d.x = p.x; d.y = p.y; d.radius = c.size * 0.5f * (1.f - t * 0.8f) + 0.5f;
          c.s.drawDab(d);
        }
      }
    });
  }
}

// --------------------------------------------------------------------------------------------
void registerSky(BrushLibrary& lib) {
  {  // starfield
    BrushInfo bi = mkInfo("ceu.estrelas", "Campo de estrelas", "ceu",
                          "no ceu real poucas estrelas sao brilhantes e a maioria e quase invisivel (magnitude); as brilhantes mostram cintilacao e cores (azuis quentes, laranjas frias).",
                          "distribuicao de magnitude (lei de potencia): 85% pontos finos, 12% medias com halo, 3% grandes com cruz de brilho; cintila com o frame.",
                          26, Color{1, 1, 1, 1}, Color{0.7f, 0.8f, 1, 1}, true, true, "wave", "night");
    regE(lib, bi, 0.4f, [](EC& c) {
      Rng r(hash32(c.sp.seed + c.idx * 53u));
      for (int i = 0; i < 6; ++i) {
        Vec2 q = c.p + r.inDisc() * c.size * 1.6f;
        float m = r.uniform(); float tw = 0.7f + 0.3f * std::sin(c.sp.frame * 1.7f + r.uniform() * 6.f);
        Color tint = r.chance(0.5f) ? Color{0.75f, 0.85f, 1, 1} : Color{1, 0.88f, 0.7f, 1};
        if (m < 0.85f) softAt(c.s, q, 0.9f, tint, 0.7f * tw * c.k, 0.8f);
        else if (m < 0.97f) { softAt(c.s, q, 1.5f, whiten(tint, 0.6f), 0.9f * tw * c.k, 0.8f); bloomAt(c.s, q, 5.f, tint, 0.4f * tw * c.k); }
        else { bloomAt(c.s, q, 12.f, tint, 0.5f * tw * c.k); softAt(c.s, q, 2.2f, Color{1, 1, 1, 1}, tw * c.k, 0.8f);
               streakAt(c.s, q, 0, 11.f * tw, 0.7f, Color{1, 1, 1, 1}, 0.8f * c.k); streakAt(c.s, q, kPi * 0.5f, 11.f * tw, 0.7f, Color{1, 1, 1, 1}, 0.8f * c.k); }
      }
    });
  }
  {  // shooting star
    BrushInfo bi = mkInfo("ceu.estrela-cadente", "Estrela cadente", "ceu",
                          "um meteoro e um graozinho que queima na atmosfera: cabeca muito brilhante e cauda que esmaece rapido, mais fina no fim.",
                          "ao longo do traco: linha aditiva cuja intensidade cresce em direcao a cabeca (posicao atual) e afina para a cauda; ponto-cabeca com halo.",
                          5, Color{0.85f, 0.92f, 1, 1}, Color{0.4f, 0.6f, 1, 1}, true, false, "line", "night");
    regE(lib, bi, 0.25f, [](EC& c) {
      float L = c.arc; (void)L;
      glowPoint(c.s, c.p, c.size * 0.55f, Color{0.85f, 0.92f, 1, 1}, Color{0.4f, 0.6f, 1, 1}, 0.55f * c.k, 0.8f, 2.5f);
    }, [](EC& c) { glowPoint(c.s, c.p, c.size * 1.2f, Color{1, 1, 1, 1}, Color{0.6f, 0.75f, 1, 1}, 1.2f * c.k, 0.9f, 3.5f); });
  }
  {  // aurora
    BrushInfo bi = mkInfo("ceu.aurora", "Aurora boreal", "ceu",
                          "particulas solares excitam o oxigenio/nitrogenio na alta atmosfera: cortinas verticais, base verde nitida e topo vermelho/violeta difuso, ondulando.",
                          "colunas verticais de raios (aditivas) a partir do traco: altura/intensidade por ruido, gradiente verde->ciano->violeta no alto, ondula com o frame.",
                          30, Color{0.2f, 1, 0.5f, 1}, Color{0.7f, 0.2f, 1, 1}, true, true, "wave", "night");
    regE(lib, bi, 0.12f, [](EC& c) {
      float h = c.size * (4.f + 5.f * valueNoise(c.arc * 0.02f, c.sp.frame * 0.1f, c.sp.seed));
      const int N = 18;
      for (int i = 0; i < N; ++i) {
        float t = i / (float)(N - 1);
        float sway = gradNoise(c.arc * 0.01f + t, c.sp.frame * 0.15f, 5u) * c.size * 0.8f * t;
        Color col = t < 0.45f ? mix(Color{0.2f, 1, 0.45f, 1}, Color{0.2f, 0.9f, 0.9f, 1}, t / 0.45f) : mix(Color{0.2f, 0.9f, 0.9f, 1}, c.sp.color2, (t - 0.45f) / 0.55f);
        Dab d = addDab(col, 0.07f * (1.f - t * 0.8f) * c.k, 0.f); d.x = c.p.x + sway; d.y = c.p.y - h * t; d.radius = c.size * 0.35f; d.aspect = 1.f; c.s.drawDab(d);
        streakAt(c.s, {c.p.x + sway, c.p.y - h * t}, kPi * 0.5f, h / N * 1.6f, c.size * 0.3f, col, 0.07f * (1.f - t * 0.7f) * c.k);
      }
    });
  }
  {  // rainbow
    BrushInfo bi = mkInfo("ceu.arco-iris", "Arco-íris", "ceu",
                          "a luz refrata em gotas: sete cores em ordem fixa (vermelho por fora, violeta por dentro), transparente e com bordas suaves, e as bandas sao paralelas.",
                          "7 faixas paralelas deslocadas pela normal do traco, cada uma com cor espectral, translucidas (0.45) e bordas suaves; sobreposicao suave entre vizinhas.",
                          28, Color{1, 0, 0, 1}, Color{0.5f, 0, 1, 1}, false, false, "arc", "sky", 0.f, false);
    regE(lib, bi, 0.12f, [](EC& c) {
      static const Color band[7] = {{1, 0.15f, 0.15f, 1}, {1, 0.55f, 0.1f, 1}, {1, 0.92f, 0.2f, 1}, {0.3f, 0.85f, 0.35f, 1}, {0.25f, 0.6f, 1, 1}, {0.3f, 0.3f, 0.85f, 1}, {0.6f, 0.3f, 0.85f, 1}};
      Vec2 n = c.dir.perp(); float w = c.size * 2.f / 7.f;
      for (int i = 0; i < 7; ++i) {
        float off = (i - 3.f) * w; Dab d; d.color = band[i]; d.hardness = 0.55f; d.opacity = 0.42f * c.k;
        d.x = c.p.x + n.x * off; d.y = c.p.y + n.y * off; d.radius = w * 0.85f; c.s.drawDab(d);
      }
    });
  }
  {  // sun
    BrushInfo bi = mkInfo("ceu.sol", "Sol com raios", "ceu",
                          "o sol e um disco quase branco, com halo largo laranja (espalhamento atmosferico) e raios crepusculares em leque.",
                          "no ponto inicial: disco quente, bloom de duas escalas e 12 raios longos aditivos de comprimento irregular; gira devagar com o tempo.",
                          24, Color{1, 0.85f, 0.4f, 1}, Color{1, 0.5f, 0.1f, 1}, true, true, "dot", "sky", 0.f, false);
    regE(lib, bi, 1e6f, [](EC& c) {
      Vec2 o = c.p; float s = c.size;
      bloomAt(c.s, o, s * 9.f, Color{1, 0.6f, 0.25f, 1}, 0.55f * c.k);
      for (int i = 0; i < 12; ++i) {
        float a = i / 12.f * kTau + c.sp.time * 0.1f; float L = s * (4.f + 4.f * hashf(hash32(i * 17 + c.sp.seed)));
        streakAt(c.s, o + fromAngle(a, L * 0.6f), a, L, s * 0.14f, Color{1, 0.85f, 0.5f, 1}, 0.3f * c.k);
      }
      softAt(c.s, o, s * 1.6f, Color{1, 0.9f, 0.55f, 1}, 0.9f * c.k, 0.8f);
      softAt(c.s, o, s * 1.1f, Color{1, 1, 0.95f, 1}, 1.f * c.k, 0.9f);
    });
  }
  {  // moon
    BrushInfo bi = mkInfo("ceu.lua", "Lua com halo", "ceu",
                          "a lua e um disco cinza-claro com mares escuros e crateras e um halo frio de espalhamento.",
                          "disco com gradiente, manchas (mares) por fbm, crateras com sombra/realce deslocados e halo azulado aditivo.",
                          30, Color{0.93f, 0.93f, 0.88f, 1}, Color{0.6f, 0.75f, 1, 1}, true, false, "dot", "night", 0.f, false);
    regE(lib, bi, 1e6f, [](EC& c) {
      Vec2 o = c.p; float R = c.size;
      bloomAt(c.s, o, R * 4.f, Color{0.55f, 0.7f, 1, 1}, 0.4f * c.k);
      Dab d; d.x = o.x; d.y = o.y; d.radius = R; d.hardness = 0.97f; d.color = Color{0.9f, 0.9f, 0.84f, 1}; c.s.drawDab(d);
      Rng& r = c.rng;
      for (int i = 0; i < 9; ++i) {  // maria
        Vec2 q = o + r.inDisc() * R * 0.7f; Dab m; m.x = q.x; m.y = q.y; m.radius = R * r.range(0.12f, 0.3f); m.hardness = 0.1f; m.color = Color{0.55f, 0.56f, 0.55f, 1}; m.opacity = 0.35f;
        if (dist(q, o) < R * 0.85f) c.s.drawDab(m);
      }
      for (int i = 0; i < 14; ++i) {  // craters
        Vec2 q = o + r.inDisc() * R * 0.85f; float cr = R * r.range(0.04f, 0.11f);
        Dab sh; sh.x = q.x - cr * 0.25f; sh.y = q.y - cr * 0.25f; sh.radius = cr; sh.hardness = 0.7f; sh.color = Color{0.5f, 0.5f, 0.48f, 1}; sh.opacity = 0.5f; c.s.drawDab(sh);
        Dab hl; hl.x = q.x + cr * 0.2f; hl.y = q.y + cr * 0.2f; hl.radius = cr * 0.85f; hl.hardness = 0.7f; hl.color = Color{0.96f, 0.96f, 0.92f, 1}; hl.opacity = 0.7f; c.s.drawDab(hl);
      }
      Dab term; term.x = o.x + R * 0.35f; term.y = o.y; term.radius = R; term.hardness = 0.f; term.color = Color{0.1f, 0.12f, 0.2f, 1}; term.opacity = 0.35f; c.s.drawDab(term);
    });
  }
  {  // nebula / galaxy
    BrushInfo bi = mkInfo("ceu.nebulosa", "Nebulosa / galáxia", "ceu",
                          "nebulosas sao nuvens de gas ionizado: filamentos coloridos (H-alfa vermelho, O-III azul-esverdeado) com estrelas jovens embutidas.",
                          "gas aditivo por fbm distorcido (domain warping) com matizes variando ao longo do traco, mais um campo de estrelas finas.",
                          36, Color{0.8f, 0.2f, 0.7f, 1}, Color{0.2f, 0.5f, 1, 1}, true, true, "wave", "night");
    regE(lib, bi, 0.3f, [](EC& c) {
      for (int i = 0; i < 10; ++i) {
        Vec2 q = c.p + c.rng.inDisc() * c.size * 1.4f;
        float n = fbm(q.x * 0.012f + 4.f, q.y * 0.012f, 5, 2.f, 0.55f, c.sp.seed);
        Color col = mix(c.sp.color, c.sp.color2, clampf(n * 1.4f - 0.2f + c.arc * 0.002f));
        softAt(c.s, q, c.size * (0.4f + 0.5f * n), col, 0.09f * n * 2.f * c.k, 0.f);
      }
      if (c.rng.chance(0.7f)) softAt(c.s, c.p + c.rng.inDisc() * c.size * 1.4f, 0.9f, Color{1, 1, 1, 1}, 0.9f, 0.8f);
    });
  }
  {  // comet
    BrushInfo bi = mkInfo("ceu.cometa", "Cometa", "ceu",
                          "um cometa tem nucleo com coma brilhante e duas caudas: de poeira (curva, amarelada) e de ions (reta, azulada, aponta contra o sol).",
                          "cabeca na posicao final com coma; duas caudas aditivas: poeira curvada e plasma azul reto, afinando.",
                          9, Color{0.95f, 0.95f, 0.8f, 1}, Color{0.4f, 0.65f, 1, 1}, true, false, "arc", "night", 0.f, false);
    regE(lib, bi, 0.5f, [](EC&) {}, [](EC& c) {
      Vec2 h = c.p; Vec2 back = (c.dir * -1.f).norm();
      for (int i = 0; i < 70; ++i) {
        float t = i / 69.f; float L = c.size * 16.f * t;
        Vec2 pd = h + back * L + back.perp() * (L * L / (c.size * 40.f));
        softAt(c.s, pd, c.size * (0.9f - 0.7f * t) + 0.6f, Color{1, 0.9f, 0.6f, 1}, 0.07f * (1.f - t) * c.k, 0.f);
        Vec2 pi = h + back * (L * 1.25f);
        softAt(c.s, pi, c.size * (0.45f - 0.35f * t) + 0.4f, Color{0.4f, 0.65f, 1, 1}, 0.09f * (1.f - t) * c.k, 0.3f);
      }
      glowPoint(c.s, h, c.size * 0.8f, Color{1, 1, 0.9f, 1}, Color{0.7f, 0.85f, 1, 1}, 1.1f * c.k, 0.9f, 4.f);
    });
  }
  {  // sunset bands
    BrushInfo bi = mkInfo("ceu.por-do-sol", "Brilho de pôr do sol", "ceu",
                          "no crepusculo o ceu faz gradiente de laranja no horizonte, rosa, lilas e azul-noite, com faixas largas e suaves.",
                          "faixas largas muito suaves empilhadas perpendicularmente ao traco, cor interpolada pela ordem do gradiente, mistura Screen.",
                          34, Color{1, 0.55f, 0.15f, 1}, Color{0.35f, 0.2f, 0.6f, 1}, true, false, "line", "night", 0.f, false);
    regE(lib, bi, 0.5f, [](EC& c) {
      static const Color g[5] = {{1, 0.75f, 0.3f, 1}, {1, 0.5f, 0.2f, 1}, {0.95f, 0.35f, 0.4f, 1}, {0.6f, 0.3f, 0.6f, 1}, {0.25f, 0.2f, 0.5f, 1}};
      for (int i = 0; i < 5; ++i) softAt(c.s, c.p + Vec2{0, -i * c.size * 0.55f}, c.size * 1.1f, g[i], 0.16f * c.k, 0.f);
    });
  }
}

}  // namespace

void registerFogo(BrushLibrary& lib) { registerFlames(lib); }
void registerFumaca(BrushLibrary& lib) { registerSmoke(lib); }
void registerCeu(BrushLibrary& lib) { registerSky(lib); }

}  // namespace lu
}  // namespace ab
