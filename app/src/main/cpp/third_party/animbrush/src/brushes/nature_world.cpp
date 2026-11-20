// nature module part 2: plantas, terreno, agua, clima, pelos.
#include "nature_util.hpp"

namespace ab {
namespace nat {
namespace {

using EG = EndBrush::Gen;
const std::vector<ParamSpec> kDens = {{"densidade", "Densidade", 1.f, 0.3f, 3.f}};
const std::vector<ParamSpec> kNone = {};

Color greenAt(Rng& r, float t, Color base) { return jitter(mix(shade(base, 0.55f), lighten(base, 0.2f), t), r, 0.03f, 0.1f, 0.06f); }

// ------------------------------------------------------------------ plantas
void registerPlantsImpl(BrushLibrary& lib) {
  auto L = [&](const char* id, const char* nm, const char* cat, const char* desc, float size, Color c1, Color c2, float spacing, LiveBrush::Emit e,
               std::vector<ParamSpec> ps = kDens, const char* demo = "wave", const char* bg = "sky") {
    BrushInfo bi = info(id, nm, cat, desc, size, c1, c2, demo, bg); bi.animated = true;
    addLive(lib, bi, ps, [spacing](const StrokeParams& sp) { return sp.size * spacing; }, e);
  };
  auto blade = [](ISurface& s, Vec2 root, float h, float w, float lean, Color base, Rng& r, float wind) {
    Vec2 tip{root.x + lean * h + wind * h * 0.5f, root.y - h};
    Vec2 ctl{root.x + lean * h * 0.1f, root.y - h * 0.6f};
    auto pts = bez(root, ctl, tip, 10);
    int n = (int)pts.size();
    for (int i = 0; i + 1 < n; ++i) {
      float t = (float)i / n;
      line(s, pts[i], pts[i + 1], std::max(0.35f, w * (1.f - t)), greenAt(r, t, base), 1.f);
    }
  };
  L("plantas.grama", "Grama / campo", "plantas",
    "Na vida real: cada folha de capim nasce do solo, e uma lamina estreita que curva sob o proprio peso e o vento; tufos tem alturas diferentes, mais escuros na base e mais claros nas pontas.",
    9, Color{0.3f, 0.65f, 0.2f, 1}, Color{0.8f, 0.9f, 0.4f, 1}, 0.22f,
    [blade](NCtx& c, const PathPoint& p, int) {
      int n = (int)(3 + c.P("densidade") * 2);
      for (int i = 0; i < n; ++i) { float h = c.sp.size * c.rng.range(2.2f, 6.f); blade(c.s, p.p + Vec2{c.rng.sym() * c.sp.size * 0.9f, 0}, h, c.sp.size * 0.14f, c.rng.sym() * 0.35f, c.sp.color, c.rng, windSway(c.sp, p.d * 0.05f, 0.5f)); }
    });
  L("plantas.trigo", "Campo de trigo", "plantas",
    "Na vida real: colmo fino e reto com espiga de graos em escamas alternadas no topo, dourado e inclinado pelo vento.",
    9, Color{0.85f, 0.7f, 0.25f, 1}, Color{0.95f, 0.85f, 0.45f, 1}, 0.5f,
    [](NCtx& c, const PathPoint& p, int) {
      float h = c.sp.size * c.rng.range(5.f, 8.f), lean = c.rng.sym() * 0.15f + c.sp.wind * 0.3f;
      Vec2 root = p.p + Vec2{c.rng.sym() * c.sp.size, 0}, tip{root.x + lean * h, root.y - h};
      line(c.s, root, tip, c.sp.size * 0.07f + 0.4f, shade(c.sp.color, 0.75f));
      for (int i = 0; i < 9; ++i) { float t = i / 9.f; Vec2 q = lerp(tip, tip + Vec2{lean * 4, c.sp.size * 2.4f}, t); float s = (i & 1) ? 1.f : -1.f;
        ell(c.s, q + Vec2{s * c.sp.size * 0.22f, 0}, c.sp.size * 0.42f, c.sp.size * 0.16f, -kPi * 0.5f + s * 0.6f, jitter(c.sp.color2, c.rng, 0.02f, 0.1f, 0.1f)); }
    });
  L("plantas.junco", "Juncos e taboas", "plantas",
    "Na vida real: haste alta e lisa nas margens de lagos; a taboa tem uma espiga marrom cilindrica no topo e folhas em fita.",
    9, Color{0.3f, 0.5f, 0.2f, 1}, Color{0.4f, 0.25f, 0.12f, 1}, 0.9f,
    [blade](NCtx& c, const PathPoint& p, int idx) {
      float h = c.sp.size * c.rng.range(7.f, 12.f);
      for (int i = 0; i < 2; ++i) blade(c.s, p.p + Vec2{i * c.sp.size * 0.5f, 0}, h * 0.7f, c.sp.size * 0.2f, c.rng.sym() * 0.3f, c.sp.color, c.rng, c.sp.wind * 0.4f);
      Vec2 tip = p.p + Vec2{c.sp.wind * h * 0.15f, -h};
      line(c.s, p.p, tip, c.sp.size * 0.1f, shade(c.sp.color, 0.7f));
      if (idx % 2 == 0) ell(c.s, tip + Vec2{0, c.sp.size * 0.9f}, c.sp.size * 1.2f, c.sp.size * 0.38f, kPi * 0.5f, c.sp.color2);
    });
  L("plantas.flores-campo", "Campo de flores", "plantas",
    "Na vida real: flores silvestres de cores variadas sobre hastes verdes finas, cada uma voltada para a luz.",
    10, Color{0.3f, 0.6f, 0.2f, 1}, Color{1, 0.8f, 0.2f, 1}, 0.9f,
    [](NCtx& c, const PathPoint& p, int) {
      float h = c.sp.size * c.rng.range(3.5f, 6.5f); Vec2 root = p.p, tip{root.x + c.rng.sym() * c.sp.size + c.sp.wind * 8.f, root.y - h};
      line(c.s, root, tip, c.sp.size * 0.07f + 0.4f, Color{0.25f, 0.5f, 0.18f, 1});
      leafShape(c.s, lerp(root, tip, 0.35f), -kPi * 0.8f, c.sp.size * 1.2f, c.sp.size * 0.45f, Color{0.3f, 0.6f, 0.22f, 1});
      static const uint32_t pal[6] = {0xF2C14E, 0xE05263, 0xF7F7F7, 0x9B5DE5, 0xFF8FA3, 0xF58A07};
      Color pc = Color::hex(pal[c.rng.irange(0, 5)]); float r = c.sp.size * c.rng.range(0.55f, 0.9f); int pn = c.rng.irange(5, 8);
      for (int i = 0; i < pn; ++i) { float a = i * kTau / pn; ell(c.s, tip + fromAngle(a, r * 0.8f), r * 0.65f, r * 0.38f, a, shade(pc, 0.92f + 0.12f * (i & 1))); }
      disc(c.s, tip, r * 0.38f, c.rng.chance(0.5f) ? Color{0.9f, 0.6f, 0.1f, 1} : Color{0.35f, 0.2f, 0.1f, 1});
    });
  L("plantas.arbusto", "Arbusto / sebe", "plantas",
    "Na vida real: massa densa de folhas em tufos; a luz bate em cima e à esquerda, e o interior fica escuro.",
    16, Color{0.2f, 0.5f, 0.2f, 1}, Color{0.5f, 0.8f, 0.3f, 1}, 0.5f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int pass = 0; pass < 3; ++pass) for (int i = 0; i < 6; ++i) {
        Vec2 q = p.p + c.rng.inDisc() * c.sp.size * 1.1f; float r = c.sp.size * c.rng.range(0.45f, 0.85f) * (1.f - pass * 0.18f);
        Color col = mix(shade(c.sp.color, 0.5f + 0.0f), c.sp.color2, pass * 0.45f); col = jitter(col, c.rng, 0.02f, 0.06f, 0.05f);
        disc(c.s, q + Vec2{-pass * r * 0.18f, -pass * r * 0.22f}, r, col, 1.f, 0.9f);
      }
    });
  L("plantas.musgo", "Musgo", "plantas",
    "Na vida real: almofadas densas de plantinhas minúsculas que cobrem pedra e tronco, verde-vivo na luz e escuro nas frestas.",
    8, Color{0.3f, 0.5f, 0.15f, 1}, Color{0.6f, 0.8f, 0.3f, 1}, 0.25f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int i = 0; i < 18; ++i) { Vec2 q = p.p + c.rng.inDisc() * c.sp.size; disc(c.s, q, c.rng.range(0.6f, 1.6f), mix(shade(c.sp.color, 0.7f), c.sp.color2, c.rng.uniform()), 1.f, 0.7f); }
    });
  L("plantas.cogumelos", "Cogumelos", "plantas",
    "Na vida real: haste clara e chapéu arredondado com manchas, lamelas escuras embaixo.",
    9, Color{0.85f, 0.2f, 0.15f, 1}, Color{0.95f, 0.9f, 0.8f, 1}, 4.f,
    [](NCtx& c, const PathPoint& p, int) {
      float s = c.sp.size * c.rng.range(0.8f, 1.4f); Vec2 b = p.p;
      ell(c.s, b + Vec2{0, -s * 0.9f}, s * 0.35f, s * 0.95f, kPi * 0.5f, c.sp.color2);
      ell(c.s, b + Vec2{0, -s * 1.7f}, s * 1.35f, s * 0.38f, 0, shade(c.sp.color2, 0.4f));
      ell(c.s, b + Vec2{0, -s * 2.0f}, s * 1.25f, s * 0.95f, 0, c.sp.color);
      ell(c.s, b + Vec2{-s * 0.3f, -s * 2.3f}, s * 0.8f, s * 0.5f, 0, lighten(c.sp.color, 0.15f), 0.8f, 0.5f);
      for (int i = 0; i < 5; ++i) disc(c.s, b + Vec2{c.rng.sym() * s * 0.9f, -s * (1.7f + c.rng.uniform() * 0.9f)}, s * c.rng.range(0.1f, 0.2f), Color{1, 1, 1, 1});
    }, kNone);
  L("plantas.algas", "Algas / kelp", "plantas",
    "Na vida real: o kelp e preso ao fundo por uma raiz e flutua para cima por bolsas de ar, oscilando com a corrente.",
    10, Color{0.15f, 0.45f, 0.25f, 1}, Color{0.4f, 0.7f, 0.3f, 1}, 1.8f,
    [](NCtx& c, const PathPoint& p, int) {
      float h = c.sp.size * c.rng.range(7.f, 13.f); std::vector<Vec2> pts; float ph = c.rng.uniform() * 6.f + c.sp.time * 1.5f + c.sp.frame * 0.4f;
      for (int i = 0; i <= 20; ++i) { float t = i / 20.f; pts.push_back(p.p + Vec2{std::sin(t * 5.f + ph) * c.sp.size * 0.9f * t + c.sp.wind * h * 0.2f * t, -h * t}); }
      taper(c.s, pts, c.sp.size * 0.35f, c.sp.size * 0.1f, c.sp.color);
      for (int i = 3; i < 20; i += 3) { Vec2 q = pts[i]; ell(c.s, q + Vec2{c.sp.size * 0.5f * ((i / 3) & 1 ? 1 : -1), 0}, c.sp.size * 0.9f, c.sp.size * 0.25f, ((i / 3) & 1) ? -0.5f : 0.5f - kPi, mix(c.sp.color, c.sp.color2, i / 20.f)); }
    }, kNone, "up");
  addEnd(lib, [&] { BrushInfo bi = info("plantas.samambaia", "Samambaia", "plantas",
      "Na vida real: fronde arqueada em que cada pinas se divide em pinulas menores, auto-similar, enrolada na ponta jovem. Modelo: fractal recursivo - eixo curvo seguindo o traço, pares de folíolos decrescentes e sub-folíolos.", 10, Color{0.2f, 0.55f, 0.2f, 1}, Color{0.5f, 0.8f, 0.35f, 1}, "arc", "sky"); bi.animated = true; return bi; }(), kNone,
    [](NCtx& c) {
      auto& pts = c.path.points(); if (pts.size() < 2) return; float L = c.path.length(); int n = (int)(L / (c.sp.size * 1.3f));
      for (int i = 0; i < n; ++i) { float t = (float)i / n; PathPoint q = c.path.at(L * t); float sc = std::sqrt(1.f - t) * 0.9f + 0.1f; float len = c.sp.size * 5.f * sc;
        for (int sd = -1; sd <= 1; sd += 2) { Vec2 d = rotate(q.dir, sd * (1.1f - 0.4f * t)); Vec2 e = q.p + d * len; line(c.s, q.p, e, 0.6f, shade(c.sp.color, 0.8f));
          for (int k = 1; k <= 6; ++k) { float u = k / 7.f; Vec2 b = lerp(q.p, e, u); for (int s2 = -1; s2 <= 1; s2 += 2) ell(c.s, b + rotate(d, s2 * 1.2f) * len * 0.12f, len * 0.13f * (1.f - u * 0.5f), len * 0.045f, rotate(d, s2 * 1.1f).angle(), mix(c.sp.color, c.sp.color2, t)); } }
        }
      line(c.s, pts.front().p, pts.back().p, 0.7f, shade(c.sp.color, 0.6f));
    });
  addEnd(lib, [&] { BrushInfo bi = info("plantas.hera", "Hera trepadeira", "plantas",
      "Na vida real: caule fino que se agarra à superfície, folhas alternadas em pares com ângulos variados e gavinhas enroladas. Modelo: o traço vira o caule; folhas lobadas alternam dos lados com tamanho variável; gavinhas em espiral nos nós.", 8, Color{0.15f, 0.45f, 0.2f, 1}, Color{0.45f, 0.75f, 0.3f, 1}, "wave", "paper"); bi.animated = true; return bi; }(), kDens,
    [](NCtx& c) {
      float L = c.path.length(); std::vector<Vec2> v; for (float s = 0; s <= L; s += 3.f) v.push_back(c.path.at(s).p);
      taper(c.s, v, c.sp.size * 0.22f, c.sp.size * 0.12f, shade(c.sp.color, 0.55f));
      float step = c.sp.size * 2.4f / c.P("densidade"); int idx = 0;
      for (float s = step; s < L; s += step, ++idx) { PathPoint q = c.path.at(s); float side = (idx & 1) ? 1.f : -1.f; float ang = q.dir.angle() + side * c.rng.range(0.9f, 1.5f);
        float len = c.sp.size * c.rng.range(2.2f, 3.4f); leafShape(c.s, q.p, ang, len, len * 0.8f, greenAt(c.rng, c.rng.uniform(), c.sp.color2), 0.05f);
        if (idx % 3 == 0) { std::vector<Vec2> sp; for (int i = 0; i < 18; ++i) { float a = i * 0.45f; float r = c.sp.size * (0.15f + 0.04f * (18 - i)); sp.push_back(q.p + fromAngle(ang + side, c.sp.size * 1.6f) + Vec2{std::cos(a) * r, std::sin(a) * r}); } taper(c.s, sp, 0.7f, 0.3f, shade(c.sp.color, 0.7f)); } }
    });
  addEnd(lib, [&] { BrushInfo bi = info("plantas.coral", "Coral ramificado", "plantas",
      "Na vida real: coral-galho cresce por bifurcações sucessivas de pólipos calcários, formando um leque irregular. Modelo: ramificação dicotômica recursiva com espessura decrescente e pontas arredondadas.", 6, Color{0.95f, 0.4f, 0.4f, 1}, Color{1, 0.7f, 0.5f, 1}, "up", "night"); return bi; }(), kNone,
    [](NCtx& c) {
      std::function<void(Vec2, float, float, int)> grow = [&](Vec2 p, float ang, float w, int d) {
        float len = c.sp.size * (2.2f - d * 0.25f); Vec2 e = p + fromAngle(ang, len) ; line(c.s, p, e, std::max(0.8f, w), mix(c.sp.color, c.sp.color2, d / 6.f)); if (d >= 6) { disc(c.s, e, std::max(1.f, w), c.sp.color2); return; }
        grow(e, ang + c.rng.range(0.25f, 0.7f), w * 0.78f, d + 1); grow(e, ang - c.rng.range(0.25f, 0.7f), w * 0.78f, d + 1); };
      grow(c.path.back().p, -kPi * 0.5f, c.sp.size * 0.7f, 0);
    }, false);
}

// ------------------------------------------------------------------ terreno
void ridgeLine(std::vector<Vec2>& out, Vec2 a, Vec2 b, Rng& r, float rough, int depth) {
  if (depth == 0) { out.push_back(b); return; }
  Vec2 m = (a + b) * 0.5f; m.y += r.sym() * rough * dist(a, b) * 0.5f;
  ridgeLine(out, a, m, r, rough, depth - 1); ridgeLine(out, m, b, r, rough, depth - 1);
}

void registerTerrainImpl(BrushLibrary& lib) {
  addEnd(lib, [&] { BrushInfo bi = info("terreno.montanhas", "Cadeia de montanhas", "terreno",
      "Na vida real: picos são fractais (a mesma rugosidade em várias escalas); o lado iluminado é claro, o oposto escuro, picos altos têm neve, e cadeias distantes são mais claras e azuladas (perspectiva atmosférica). Modelo: crista por deslocamento do ponto médio (3 camadas de profundidade) a partir do traço, preenchimento com faces luz/sombra e neve acima de um limiar.", 14, Color{0.35f, 0.38f, 0.5f, 1}, Color{1, 1, 1, 1}, "line", "sky"); return bi; }(),
    {{"aspereza", "Aspereza", 1.f, 0.4f, 1.8f}},
    [](NCtx& c) {
      auto& pts = c.path.points(); float x0 = 1e9f, x1 = -1e9f, base = -1e9f, top = 1e9f; for (auto& p : pts) { x0 = std::min(x0, p.p.x); x1 = std::max(x1, p.p.x); base = std::max(base, p.p.y); top = std::min(top, p.p.y); }
      if (x1 - x0 < 10) return; float H = std::max(30.f, base - top + c.sp.size * 4.f);
      for (int layer = 0; layer < 3; ++layer) {
        float amp = H * (1.f - 0.3f * (2 - layer) * 0.5f); float yb = base + c.sp.size * (2 - layer) * 0.4f; Rng r(c.sp.seed + layer * 91u);
        std::vector<Vec2> ridge; ridge.push_back({x0, yb - amp * 0.2f});
        int peaks = 3 + layer; for (int i = 0; i < peaks; ++i) { float u0 = x0 + (x1 - x0) * i / peaks, u1 = x0 + (x1 - x0) * (i + 1) / peaks; float pk = yb - amp * r.range(0.45f, 1.f);
          ridgeLine(ridge, ridge.back(), {(u0 + u1) * 0.5f, pk}, r, c.P("aspereza") * 0.6f, 4); ridgeLine(ridge, ridge.back(), {u1, yb - amp * r.range(0.1f, 0.3f)}, r, c.P("aspereza") * 0.6f, 4); }
        std::vector<Vec2> poly = ridge; poly.push_back({x1, yb + 5}); poly.push_back({x0, yb + 5});
        Color far = mix(c.sp.color, Color{0.7f, 0.8f, 0.95f, 1}, (2 - layer) * 0.3f); float snowY = yb - amp * 0.72f; float tilt = 1.f;
        fillPoly(c.s, poly, [&, far, snowY](float x, float y) { (void)x; return y < snowY ? Color{0.95f, 0.97f, 1, 1} : far; });
        for (size_t i = 1; i < ridge.size(); ++i) { Vec2 a = ridge[i - 1], b = ridge[i]; bool lit = b.y < a.y; (void)tilt; if (!lit) { std::vector<Vec2> sh{a, b, {b.x, yb + 5}, {a.x, yb + 5}}; fillPolyFlat(c.s, sh, Color{0.1f, 0.12f, 0.25f, 1}, 0.28f); } }
      }
    });
  auto L = [&](const char* id, const char* nm, const char* desc, float size, Color c1, Color c2, float spacing, LiveBrush::Emit e, const char* demo = "wave", const char* bg = "sky") {
    BrushInfo bi = info(id, nm, "terreno", desc, size, c1, c2, demo, bg);
    addLive(lib, bi, kNone, [spacing](const StrokeParams& sp) { return sp.size * spacing; }, e);
  };
  L("terreno.rochas", "Rochas facetadas", "Na vida real: rocha fraturada tem faces planas; a face voltada para a luz é clara, a lateral média e o topo/base escuros; fendas escuras entre faces.",
    14, Color{0.5f, 0.5f, 0.52f, 1}, Color{0.85f, 0.85f, 0.88f, 1}, 3.f,
    [](NCtx& c, const PathPoint& p, int) {
      float s = c.sp.size * c.rng.range(0.8f, 1.5f); int n = c.rng.irange(6, 8); std::vector<Vec2> v;
      for (int i = 0; i < n; ++i) { float a = i * kTau / n + c.rng.sym() * 0.2f; float rr = s * c.rng.range(0.7f, 1.1f); v.push_back(p.p + Vec2{std::cos(a) * rr * 1.1f, std::sin(a) * rr * 0.8f}); }
      fillPolyFlat(c.s, v, c.sp.color);
      Vec2 ctr = p.p + Vec2{-s * 0.15f, -s * 0.15f};
      for (int i = 0; i < n; ++i) { std::vector<Vec2> f{ctr, v[i], v[(i + 1) % n]}; Vec2 m = (v[i] + v[(i + 1) % n]) * 0.5f - ctr; float lit = clampf(0.5f - 0.5f * dot(m.norm(), Vec2{0.6f, 0.8f})); fillPolyFlat(c.s, f, mix(c.sp.color2, shade(c.sp.color, 0.45f), lit)); }
      for (int i = 0; i < n; ++i) line(c.s, ctr, v[i], 0.7f, shade(c.sp.color, 0.35f), 0.5f);
    });
  L("terreno.pedregulhos", "Pedregulhos / seixos", "Na vida real: seixos de rio são lisos e arredondados, com luz suave e sombra projetada.", 5,
    Color{0.55f, 0.5f, 0.45f, 1}, Color{0.85f, 0.8f, 0.7f, 1}, 0.9f,
    [](NCtx& c, const PathPoint& p, int) {
      Vec2 q = p.p + c.rng.inDisc() * c.sp.size * 1.2f; float rx = c.sp.size * c.rng.range(0.5f, 1.1f), ry = rx * c.rng.range(0.55f, 0.85f); Color b = jitter(c.sp.color, c.rng, 0.03f, 0.1f, 0.12f);
      ell(c.s, q + Vec2{rx * 0.15f, ry * 0.4f}, rx * 1.05f, ry * 0.5f, 0, Color{0, 0, 0, 1}, 0.25f, 0.2f);
      ell(c.s, q, rx, ry, 0, shade(b, 0.7f)); ell(c.s, q + Vec2{-rx * 0.1f, -ry * 0.15f}, rx * 0.82f, ry * 0.8f, 0, b, 1.f, 0.7f); ell(c.s, q + Vec2{-rx * 0.3f, -ry * 0.4f}, rx * 0.35f, ry * 0.25f, 0, lighten(b, 0.4f), 0.8f, 0.4f);
    }, "wave", "paper");
  addEnd(lib, [&] { BrushInfo bi = info("terreno.dunas", "Dunas de areia", "terreno",
      "Na vida real: o vento forma dunas com barlavento suave e sotavento íngreme (crista afiada), cobertas por marcas de ondulação. Modelo: cristas senoidais assimétricas ao longo do traço, face de sombra escura e linhas de ondulação.", 14, Color{0.9f, 0.75f, 0.45f, 1}, Color{0.6f, 0.4f, 0.2f, 1}, "line", "sky"); return bi; }(), kNone,
    [](NCtx& c) {
      auto& pts = c.path.points(); float x0 = pts.front().p.x, x1 = pts.back().p.x; if (x0 > x1) std::swap(x0, x1); if (x1 - x0 < 20) return;
      for (int k = 0; k < 4; ++k) { float yb = pts.front().p.y + k * c.sp.size * 1.6f; std::vector<Vec2> top; for (float x = x0; x <= x1; x += 3.f) { float t = (x - x0) / (x1 - x0) * (2.f + k * 0.7f) + k * 0.4f; float f = t - std::floor(t); float h = (f < 0.7f ? f / 0.7f : (1.f - f) / 0.3f) * c.sp.size * 2.4f; top.push_back({x, yb - h}); }
        std::vector<Vec2> poly = top; poly.push_back({x1, yb + c.sp.size * 6}); poly.push_back({x0, yb + c.sp.size * 6}); fillPolyFlat(c.s, poly, shade(c.sp.color, 0.9f - k * 0.04f));
        for (size_t i = 1; i < top.size(); ++i) if (top[i].y > top[i - 1].y + 0.6f) line(c.s, top[i - 1], top[i], 1.1f, c.sp.color2, 0.6f); }
    });
  addEnd(lib, [&] { BrushInfo bi = info("terreno.terra-rachada", "Terra rachada", "terreno",
      "Na vida real: lama que seca encolhe e racha em polígonos (junções em T de ~120°). Modelo: bordas de células de Worley ao longo do traço, desenhadas escuras e com lasca clara ao lado.", 10, Color{0.45f, 0.3f, 0.18f, 1}, Color{0.8f, 0.65f, 0.45f, 1}, "wave", "paper"); return bi; }(), kNone,
    [](NCtx& c) {
      float cell = c.sp.size * 2.4f; for (float s = 0; s < c.path.length(); s += 2.f) { PathPoint q = c.path.at(s);
        for (int i = 0; i < 6; ++i) { Vec2 p = q.p + Vec2{c.rng.sym(), c.rng.sym()} * c.sp.size * 2.f; float cid; float d1 = worley(p.x / cell, p.y / cell, c.sp.seed, &cid); float d2 = worley(p.x / cell + 0.06f, p.y / cell + 0.06f, c.sp.seed, &cid);
          if (std::fabs(d1 - d2) > 0.0f && d1 > 0.38f) disc(c.s, p, 0.9f, c.sp.color, 0.9f); if (d1 < 0.2f) disc(c.s, p, 0.8f, mix(c.sp.color2, c.sp.color, cid), 0.3f); } }
    });
  L("terreno.estratos", "Estratos de rocha", "Na vida real: camadas de sedimento empilhadas em faixas de cores e espessuras diferentes, às vezes inclinadas.",
    12, Color{0.7f, 0.5f, 0.35f, 1}, Color{0.4f, 0.28f, 0.2f, 1}, 0.3f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int k = 0; k < 6; ++k) { float h = c.sp.size * (0.5f + 0.4f * hashf(hash32(k * 5 + c.sp.seed))); Vec2 o = p.p + Vec2{0, k * c.sp.size * 0.8f}; Color b = mix(c.sp.color, c.sp.color2, hashf(hash32(k * 11 + c.sp.seed)));
        ell(c.s, o, c.sp.size * 0.7f, h * 0.6f, 0, jitter(b, c.rng, 0.0f, 0.04f, 0.04f)); }
    }, "line", "paper");
  L("terreno.rio", "Rio / córrego", "Na vida real: leito sinuoso, mais estreito nas curvas internas, com margens escuras e brilho na superfície.", 10,
    Color{0.3f, 0.6f, 0.85f, 1}, Color{0.8f, 0.95f, 1, 1}, 0.25f,
    [](NCtx& c, const PathPoint& p, int) {
      float w = c.sp.size * (0.8f + 0.5f * valueNoise(p.d * 0.02f, 3, c.sp.seed)); ell(c.s, p.p, w * 1.1f, w * 1.1f, 0, Color{0.55f, 0.45f, 0.3f, 1}); ell(c.s, p.p, w, w, 0, mix(c.sp.color, shade(c.sp.color, 0.7f), 0.3f));
      if (c.rng.chance(0.4f)) ell(c.s, p.p + c.rng.inDisc() * w * 0.6f, w * 0.5f, w * 0.06f, p.dir.angle(), c.sp.color2, 0.7f, 0.5f);
    }, "wave", "paper");
}

// ------------------------------------------------------------------ agua
void registerWaterImpl(BrushLibrary& lib) {
  auto L = [&](const char* id, const char* nm, const char* desc, float size, Color c1, Color c2, float spacing, LiveBrush::Emit e, const char* demo = "wave", const char* bg = "sky", std::vector<ParamSpec> ps = kDens) {
    BrushInfo bi = info(id, nm, "agua", desc, size, c1, c2, demo, bg); bi.animated = true;
    addLive(lib, bi, ps, [spacing](const StrokeParams& sp) { return sp.size * spacing; }, e);
  };
  L("agua.ondas-mar", "Ondas do mar", "Na vida real: onda e a soma de harmônicas; a crista quebra em espuma branca e cada onda tem um lado de sombra; as filas seguem a linha da costa.",
    12, Color{0.12f, 0.45f, 0.7f, 1}, Color{1, 1, 1, 1}, 0.5f,
    [](NCtx& c, const PathPoint& p, int) {
      float ph = p.d * 0.05f + c.sp.frame * 0.6f; for (int k = 0; k < 3; ++k) { float y = p.p.y + k * c.sp.size * 1.1f; float h = std::sin(ph + k) * c.sp.size * 0.5f + std::sin(ph * 2.1f + k) * c.sp.size * 0.2f;
        ell(c.s, {p.p.x, y + h}, c.sp.size * 0.8f, c.sp.size * 0.55f, 0, mix(shade(c.sp.color, 0.7f), c.sp.color, k / 2.f));
        if (k == 0) { ell(c.s, {p.p.x, y + h - c.sp.size * 0.3f}, c.sp.size * 0.65f, c.sp.size * 0.14f, 0, c.sp.color2, 0.9f, 0.6f); if (c.rng.chance(0.4f)) disc(c.s, {p.p.x + c.rng.sym() * c.sp.size, y + h - c.sp.size * 0.5f}, c.rng.range(0.6f, 1.6f), c.sp.color2); } }
    }, "line", "sky");
  L("agua.ondulacoes", "Ondulações", "Na vida real: uma pedra na água gera anéis concêntricos que se expandem e enfraquecem, vistos em perspectiva como elipses.",
    12, Color{0.9f, 0.97f, 1, 1}, Color{0.2f, 0.45f, 0.65f, 1}, 6.f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int k = 0; k < 4; ++k) { float r = c.sp.size * (0.8f + k * 0.9f); float f = 1.f - k / 4.f; auto pts = std::vector<Vec2>(); for (int i = 0; i <= 48; ++i) { float a = i * kTau / 48; pts.push_back(p.p + Vec2{std::cos(a) * r, std::sin(a) * r * 0.38f}); }
        for (size_t i = 1; i < pts.size(); ++i) line(c.s, pts[i - 1], pts[i], 0.7f + f * 1.2f, c.sp.color, 0.25f + 0.55f * f); }
    }, "wave", "night", kNone);
  L("agua.bolhas", "Bolhas", "Na vida real: bolha de sabão/água é esfera quase transparente com borda mais visível e dois reflexos (janela) em lados opostos, tons iridescentes.",
    10, Color{0.7f, 0.9f, 1, 1}, Color{1, 0.8f, 1, 1}, 2.f,
    [](NCtx& c, const PathPoint& p, int) {
      float r = c.sp.size * c.rng.range(0.35f, 1.2f); Vec2 q = p.p + c.rng.inDisc() * c.sp.size; disc(c.s, q, r, c.sp.color, 0.2f, 0.12f);
      for (int i = 0; i < 24; ++i) { float a = i * kTau / 24; disc(c.s, q + fromAngle(a, r * 0.95f), 0.9f, mix(c.sp.color, c.sp.color2, 0.5f + 0.5f * std::sin(a * 2 + 1)), 1.f, 0.6f); }
      ell(c.s, q + Vec2{-r * 0.42f, -r * 0.48f}, r * 0.28f, r * 0.12f, -0.8f, Color{1, 1, 1, 1}, 0.9f, 0.7f); disc(c.s, q + Vec2{r * 0.45f, r * 0.45f}, r * 0.1f, Color{1, 1, 1, 1}, 0.6f, 0.7f);
    }, "wave", "night");
  L("agua.espuma", "Espuma", "Na vida real: espuma e um aglomerado de bolhas brancas de tamanhos variados, com sombras finas entre elas.",
    10, Color{1, 1, 1, 1}, Color{0.6f, 0.8f, 0.9f, 1}, 0.35f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int i = 0; i < 6; ++i) { Vec2 q = p.p + c.rng.inDisc() * c.sp.size * 1.2f; float r = c.sp.size * c.rng.range(0.15f, 0.55f); disc(c.s, q, r + 0.5f, c.sp.color2, 0.5f); disc(c.s, q, r, c.sp.color); disc(c.s, q + Vec2{-r * 0.3f, -r * 0.3f}, r * 0.3f, Color{1, 1, 1, 1}, 0.7f); }
    }, "wave", "night");
  L("agua.respingo", "Respingo (coroa)", "Na vida real: uma gota/pedra faz uma coroa de filetes que lançam gotículas em arco (balística).",
    10, Color{0.75f, 0.9f, 1, 1}, Color{0.3f, 0.55f, 0.8f, 1}, 40.f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int i = 0; i < 12; ++i) { float a = -kPi * (0.15f + 0.7f * i / 11.f); float v = c.sp.size * c.rng.range(2.f, 4.f); Vec2 top = p.p + Vec2{std::cos(a) * v * 1.2f, std::sin(a) * v};
        std::vector<Vec2> arc = bez(p.p, p.p + Vec2{std::cos(a) * v * 0.5f, std::sin(a) * v * 1.2f}, top, 8); taper(c.s, arc, c.sp.size * 0.18f, c.sp.size * 0.07f, c.sp.color, 0.85f); disc(c.s, top, c.sp.size * 0.2f, c.sp.color); }
      ell(c.s, p.p, c.sp.size * 1.6f, c.sp.size * 0.4f, 0, c.sp.color2, 0.5f, 0.4f);
    }, "dot", "night", kNone);
  L("agua.cachoeira", "Cachoeira", "Na vida real: queda d'água tem fios verticais brancos que se esticam com a velocidade e termina numa névoa de espuma.",
    12, Color{0.85f, 0.95f, 1, 1}, Color{0.4f, 0.6f, 0.8f, 1}, 0.12f,
    [](NCtx& c, const PathPoint& p, int) {
      float h = c.sp.size * c.rng.range(6.f, 12.f); float x = p.p.x + c.rng.sym() * c.sp.size; taper(c.s, {{x, p.p.y}, {x + c.rng.sym(), p.p.y + h}}, c.sp.size * 0.16f, c.sp.size * 0.07f, mix(c.sp.color, c.sp.color2, c.rng.uniform() * 0.5f), 0.6f);
      if (c.rng.chance(0.2f)) disc(c.s, {x, p.p.y + h}, c.sp.size * c.rng.range(0.7f, 1.4f), c.sp.color, 0.35f, 0.1f);
    }, "line", "sky");
  L("agua.pingos", "Pingos escorrendo", "Na vida real: gota escorre por gravidade deixando um rastro fino e termina numa bolha mais grossa.",
    5, Color{0.25f, 0.5f, 0.9f, 1}, Color{1, 1, 1, 1}, 4.f,
    [](NCtx& c, const PathPoint& p, int) {
      float h = c.sp.size * c.rng.range(5.f, 14.f); line(c.s, p.p, p.p + Vec2{c.rng.sym() * 2, h}, c.sp.size * 0.14f, c.sp.color, 0.8f); disc(c.s, p.p + Vec2{0, h}, c.sp.size * 0.45f, c.sp.color); disc(c.s, p.p + Vec2{-1, h - 1}, c.sp.size * 0.14f, c.sp.color2, 0.8f, 0.7f);
    }, "line", "white", kNone);
}

// ------------------------------------------------------------------ clima
void registerClimateImpl(BrushLibrary& lib) {
  auto L = [&](const char* id, const char* nm, const char* cat, const char* desc, float size, Color c1, Color c2, float spacing, LiveBrush::Emit e, const char* demo = "wave", const char* bg = "sky", std::vector<ParamSpec> ps = kDens) {
    BrushInfo bi = info(id, nm, cat, desc, size, c1, c2, demo, bg); bi.animated = true;
    addLive(lib, bi, ps, [spacing](const StrokeParams& sp) { return sp.size * spacing; }, e);
  };
  L("clima.chuva", "Chuva", "clima", "Na vida real: gotas caem rápido e o olho vê riscos inclinados pelo vento; as de longe são curtas e fracas, as de perto longas e nítidas.",
    8, Color{0.65f, 0.78f, 0.95f, 1}, Color{1, 1, 1, 1}, 0.14f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int k = 0; k < (int)(1 + c.P("densidade")); ++k) { float depth = c.rng.uniform(); float len = c.sp.size * (1.5f + 4.f * depth); Vec2 a = p.p + c.rng.inDisc() * c.sp.size * 2.f; Vec2 b = a + Vec2{c.sp.wind * len * 0.5f + len * 0.15f, len};
        taper(c.s, {a, b}, 0.3f + depth * 0.9f, 0.2f, c.sp.color, 0.25f + 0.55f * depth); if (depth > 0.8f && c.rng.chance(0.3f)) ell(c.s, b, c.sp.size * 0.5f, c.sp.size * 0.14f, 0, c.sp.color, 0.4f, 0.5f); }
    }, "wave", "night");
  L("clima.neve", "Nevasca / flocos", "clima", "Na vida real: flocos caem devagar, balançando, e as de perto parecem grandes e borradas, as de longe pontinhos nítidos.",
    9, Color{1, 1, 1, 1}, Color{0.8f, 0.9f, 1, 1}, 0.2f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int k = 0; k < (int)(1 + c.P("densidade") * 1.5f); ++k) { float depth = c.rng.uniform(); Vec2 q = p.p + c.rng.inDisc() * c.sp.size * 2.4f + Vec2{std::sin(c.sp.time * 2 + k) * 2.f, 0}; float r = c.sp.size * (0.08f + 0.28f * depth);
        disc(c.s, q, r * (1.f + depth), c.sp.color, 0.25f + 0.4f * depth, depth > 0.7f ? 0.2f : 0.9f); }
    }, "wave", "night");
  L("clima.granizo", "Granizo", "clima", "Na vida real: pedras de gelo opacas, esféricas, que quicam; riscos curtos e brancos com ponto de brilho.",
    8, Color{0.92f, 0.96f, 1, 1}, Color{0.6f, 0.75f, 0.9f, 1}, 0.4f,
    [](NCtx& c, const PathPoint& p, int) { Vec2 q = p.p + c.rng.inDisc() * c.sp.size * 2.f; float r = c.sp.size * c.rng.range(0.15f, 0.3f); taper(c.s, {q - Vec2{0, r * 3}, q}, r * 0.4f, r, c.sp.color2, 0.4f); disc(c.s, q, r, c.sp.color); disc(c.s, q + Vec2{-r * 0.3f, -r * 0.3f}, r * 0.3f, Color{1, 1, 1, 1}); },
    "wave", "night", kNone);
  auto cloud = [&](const char* id, const char* nm, const char* desc, Color top, Color bot, float spread, float flat, int lumps, const char* bg) {
    BrushInfo bi = info(id, nm, "clima", desc, 16, top, bot, "wave", bg); bi.animated = false;
    addEnd(lib, bi, kDens, [=](NCtx& c) {
      float L = c.path.length(); int n = (int)(L / (c.sp.size * 0.7f / c.P("densidade"))) + 1; std::vector<std::pair<Vec2, float>> bl;
      for (int i = 0; i < n; ++i) { PathPoint q = c.path.at(L * i / std::max(1, n - 1)); for (int k = 0; k < lumps; ++k) bl.push_back({q.p + Vec2{c.rng.sym() * c.sp.size * spread, c.rng.sym() * c.sp.size * spread * flat}, c.sp.size * c.rng.range(0.6f, 1.3f)}); }
      for (auto& b : bl) ell(c.s, b.first, b.second, b.second * (flat + 0.2f), 0, bot, 1.f, 0.55f);
      for (auto& b : bl) ell(c.s, b.first + Vec2{-b.second * 0.12f, -b.second * 0.28f}, b.second * 0.85f, b.second * (flat + 0.2f) * 0.8f, 0, mix(bot, top, 0.6f), 1.f, 0.5f);
      for (auto& b : bl) ell(c.s, b.first + Vec2{-b.second * 0.2f, -b.second * 0.42f}, b.second * 0.55f, b.second * (flat + 0.2f) * 0.5f, 0, top, 0.9f, 0.45f);
    });
  };
  cloud("clima.cumulo", "Nuvem cúmulo", "Na vida real: cúmulo é feito de bolhas de ar quente subindo: topo branco iluminado, base achatada e cinza-azulada.", Color{1, 1, 1, 1}, Color{0.62f, 0.7f, 0.82f, 1}, 1.4f, 0.55f, 4, "sky");
  cloud("clima.tempestade", "Nuvem de tempestade", "Na vida real: cumulonimbo é imenso, base escura quase preta e topo em bigorna; a luz mal passa.", Color{0.7f, 0.74f, 0.82f, 1}, Color{0.22f, 0.25f, 0.33f, 1}, 1.7f, 0.5f, 5, "sky");
  cloud("clima.estrato", "Nuvens estrato", "Na vida real: estrato são camadas largas e achatadas, uniformes, que cobrem o céu como um lençol.", Color{0.92f, 0.94f, 0.97f, 1}, Color{0.7f, 0.74f, 0.8f, 1}, 2.6f, 0.1f, 3, "sky");
  addEnd(lib, [&] { BrushInfo bi = info("clima.cirros", "Cirros (nuvens de gelo)", "clima", "Na vida real: cirros são cristais de gelo em altitude, fiapos finos e longos esticados pelo vento. Modelo: dezenas de fios translúcidos que seguem o traço e se curvam para a ponta.", 10, Color{1, 1, 1, 1}, Color{0.8f, 0.9f, 1, 1}, "wave", "sky"); return bi; }(), kNone,
    [](NCtx& c) { float L = c.path.length(); for (int i = 0; i < 40; ++i) { PathPoint q = c.path.at(L * c.rng.uniform()); std::vector<Vec2> v; Vec2 p = q.p + Vec2{0, c.rng.sym() * c.sp.size}; float len = c.sp.size * c.rng.range(3.f, 9.f);
      for (float s = 0; s < len; s += 3.f) { v.push_back(p + q.dir * s + Vec2{0, s * s * 0.004f}); } taper(c.s, v, c.sp.size * 0.18f, 0.2f, c.sp.color, 0.35f, 0.2f); } });
  addEnd(lib, [&] { BrushInfo bi = info("clima.geada", "Geada / cristais de gelo", "clima", "Na vida real: cristal de gelo cresce por difusão com simetria hexagonal, ramos a 60° que se bifurcam (dendrito). Modelo: crescimento recursivo em 6 direções a partir do ponto final, ramos laterais a 60°, tamanho decrescente.", 10, Color{0.75f, 0.92f, 1, 1}, Color{1, 1, 1, 1}, "dot", "night"); return bi; }(), kNone,
    [](NCtx& c) { Vec2 o = c.path.back().p; std::function<void(Vec2, float, float, int)> br = [&](Vec2 p, float a, float len, int d) { Vec2 e = p + fromAngle(a, len); line(c.s, p, e, std::max(0.5f, len * 0.05f), mix(c.sp.color, c.sp.color2, d * 0.3f), 0.95f);
        if (d < 3) { for (int k = 1; k <= 3; ++k) { Vec2 m = lerp(p, e, k * 0.28f); br(m, a + kPi / 3, len * 0.38f, d + 1); br(m, a - kPi / 3, len * 0.38f, d + 1); } } }; for (int i = 0; i < 6; ++i) br(o, i * kPi / 3, c.sp.size * 4.f, 0); disc(c.s, o, c.sp.size * 0.4f, c.sp.color2); });
  L("clima.sincelos", "Sincelos", "clima", "Na vida real: água escorre e congela formando cones de gelo mais grossos no topo, translúcidos, com brilho vertical.",
    8, Color{0.75f, 0.9f, 1, 1}, Color{1, 1, 1, 1}, 0.9f,
    [](NCtx& c, const PathPoint& p, int) { float h = c.sp.size * c.rng.range(2.f, 6.f), w = c.sp.size * c.rng.range(0.3f, 0.6f); fillPolyFlat(c.s, {p.p + Vec2{-w, 0}, p.p + Vec2{w, 0}, p.p + Vec2{0, h}}, c.sp.color, 0.8f); line(c.s, p.p + Vec2{-w * 0.3f, 1}, p.p + Vec2{-w * 0.1f, h * 0.7f}, 0.6f, c.sp.color2, 0.9f); },
    "line", "night", kNone);
}

// ------------------------------------------------------------------ pelos
void registerFurImpl(BrushLibrary& lib) {
  auto L = [&](const char* id, const char* nm, const char* desc, float size, Color c1, Color c2, float spacing, LiveBrush::Emit e, const char* demo = "wave", const char* bg = "paper", std::vector<ParamSpec> ps = kDens) {
    BrushInfo bi = info(id, nm, "pelos", desc, size, c1, c2, demo, bg);
    addLive(lib, bi, ps, [spacing](const StrokeParams& sp) { return sp.size * spacing; }, e);
  };
  L("pelos.pelo-curto", "Pelo curto", "Na vida real: pelagem é feita de fios finos que seguem a direção do crescimento, com subpelo escuro e pontas claras.",
    9, Color{0.45f, 0.3f, 0.18f, 1}, Color{0.8f, 0.6f, 0.4f, 1}, 0.12f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int i = 0; i < 6; ++i) { Vec2 a = p.p + p.dir.perp() * c.rng.sym() * c.sp.size * 1.2f; float len = c.sp.size * c.rng.range(1.f, 2.2f); Vec2 b = a + rotate(p.dir, c.rng.sym() * 0.5f) * len; taper(c.s, bez(a, (a + b) * 0.5f + p.dir.perp() * c.rng.sym() * 2.f, b, 5), 0.5f, 0.15f, mix(shade(c.sp.color, 0.7f), c.sp.color2, c.rng.uniform()), 0.9f); }
    });
  L("pelos.cabelo", "Cabelo (mechas)", "Na vida real: mechas fluem juntas, com raiz no couro, pontas que se soltam e uma faixa de brilho que acompanha a curvatura.",
    10, Color{0.15f, 0.1f, 0.08f, 1}, Color{0.7f, 0.55f, 0.4f, 1}, 0.2f,
    [](NCtx& c, const PathPoint& p, int) {
      for (int i = 0; i < 4; ++i) { Vec2 a = p.p + p.dir.perp() * (i - 1.5f) * c.sp.size * 0.35f; float len = c.sp.size * c.rng.range(3.f, 6.f); Vec2 b = a + p.dir * len + p.dir.perp() * c.rng.sym() * c.sp.size; Vec2 m = a + p.dir * len * 0.5f + p.dir.perp() * c.rng.sym() * c.sp.size * 1.4f;
        auto v = bez(a, m, b, 10); taper(c.s, v, c.sp.size * 0.2f, 0.2f, c.sp.color, 0.95f); std::vector<Vec2> hl(v.begin() + 2, v.begin() + 7); taper(c.s, hl, 0.5f, 0.2f, c.sp.color2, 0.35f); }
    });
  L("pelos.cilios", "Cílios", "Na vida real: cílios nascem da pálpebra em leque, curvam-se para cima e afinam na ponta; os do canto externo são mais longos.",
    5, Color{0.05f, 0.04f, 0.05f, 1}, Color{1, 1, 1, 1}, 0.45f,
    [](NCtx& c, const PathPoint& p, int) { Vec2 n = p.dir.perp() * -1.f; float len = c.sp.size * c.rng.range(2.5f, 4.f); Vec2 a = p.p, b = a + n * len + p.dir * len * 0.5f; taper(c.s, bez(a, a + n * len * 0.7f, b, 8), c.sp.size * 0.22f, 0.15f, c.sp.color); }, "wave", "white", kNone);
  L("pelos.escamas", "Escamas", "Na vida real: escamas de peixe/réptil são arcos sobrepostos em fileiras alternadas, mais claros na borda livre.",
    8, Color{0.2f, 0.55f, 0.6f, 1}, Color{0.8f, 1, 0.9f, 1}, 0.9f,
    [](NCtx& c, const PathPoint& p, int idx) {
      for (int k = 0; k < 3; ++k) { Vec2 q = p.p + Vec2{(idx & 1) * c.sp.size * 0.5f, k * c.sp.size * 0.75f}; ell(c.s, q, c.sp.size * 0.62f, c.sp.size * 0.62f, 0, shade(c.sp.color, 0.75f)); ell(c.s, q + Vec2{0, -c.sp.size * 0.06f}, c.sp.size * 0.52f, c.sp.size * 0.52f, 0, mix(c.sp.color, c.sp.color2, 0.2f * c.rng.uniform()), 1.f, 0.9f);
        for (int i = 0; i < 9; ++i) { float a = kPi * 0.1f + i * kPi * 0.8f / 8; disc(c.s, q + fromAngle(a, c.sp.size * 0.52f), 0.6f, c.sp.color2, 0.8f); } }
    }, "wave", "paper", kNone);
  addEnd(lib, [&] { BrushInfo bi = info("pelos.pena", "Pena", "pelos", "Na vida real: pena tem um eixo (raque) e barbas paralelas que se prendem por barbulas, com a ponta afilada e o lado curto/longo assimétrico. Modelo: o traço é a raque; centenas de barbas finas inclinadas para a ponta, com variação de cor e fendas.", 8, Color{0.85f, 0.85f, 0.9f, 1}, Color{0.3f, 0.4f, 0.6f, 1}, "arc", "paper"); return bi; }(), kNone,
    [](NCtx& c) { float L = c.path.length(); for (float s = 0; s < L; s += 1.4f) { PathPoint q = c.path.at(s); float t = s / L; float w = c.sp.size * 2.2f * std::sin(clampf(t * 1.1f + 0.05f) * kPi) ; for (int sd = -1; sd <= 1; sd += 2) { float lw = w * (sd < 0 ? 1.15f : 0.85f); Vec2 d = rotate(q.dir, sd * 0.9f); Vec2 e = q.p + d * lw; taper(c.s, {q.p, q.p + (e - q.p) * 0.5f + q.dir * lw * 0.25f, e}, 0.35f, 0.15f, mix(c.sp.color, c.sp.color2, t * 0.6f + c.rng.uniform() * 0.1f), 0.8f); } }
      std::vector<Vec2> v; for (float s = 0; s < L; s += 3.f) v.push_back(c.path.at(s).p); taper(c.s, v, c.sp.size * 0.12f, 0.2f, shade(c.sp.color, 0.55f)); });
  L("pelos.dente-de-leao", "Sementes de dente-de-leão", "Na vida real: cada semente tem um pára-quedas (pappus) de filamentos radiais e é soprada pelo vento.", 8,
    Color{1, 1, 1, 1}, Color{0.5f, 0.5f, 0.45f, 1}, 3.5f,
    [](NCtx& c, const PathPoint& p, int) { Vec2 q = p.p + c.rng.inDisc() * c.sp.size; float r = c.sp.size * c.rng.range(0.5f, 0.9f); line(c.s, q, q + Vec2{c.sp.wind * 4, r * 1.7f}, 0.5f, c.sp.color2); for (int i = 0; i < 14; ++i) { float a = -kPi + i * kPi / 13; line(c.s, q, q + fromAngle(a, r), 0.4f, c.sp.color, 0.9f); disc(c.s, q + fromAngle(a, r), 0.8f, c.sp.color, 0.9f, 0.4f); } },
    "wave", "sky", kNone);
}

}  // namespace

void registerPlants(BrushLibrary& lib) { registerPlantsImpl(lib); }
void registerTerrainWater(BrushLibrary& lib) { registerTerrainImpl(lib); registerWaterImpl(lib); }
void registerClimateFur(BrushLibrary& lib) { registerClimateImpl(lib); registerFurImpl(lib); }

}  // namespace nat
}  // namespace ab
