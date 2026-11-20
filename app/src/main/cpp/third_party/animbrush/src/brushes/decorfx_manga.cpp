// decorfx part 2: manga, efeitos-anim, pixel.
#include "decorfx_util.hpp"

namespace ab {
namespace dfx {
namespace {

float Wd(const Ctx& c) { return c.size * lerpf(0.35f, 1.f, c.pr); }
Color C1(const Ctx& c) { Color k = c.sp->color; k.a = 1; return k; }
Color C2(const Ctx& c) { Color k = c.sp->color2; k.a = 1; return k; }
const Color kInk{0.05f, 0.05f, 0.08f, 1}, kWhite{1, 1, 1, 1};

// tapered line a->b : radius 0 at ends if both=true, else 0 at b
void taperLine(ISurface& s, Vec2 a, Vec2 b, float w, Color c, bool both = false, float op = 1.f) {
  const int n = std::max(2, (int)(dist(a, b) / 2.f));
  for (int i = 0; i < n; ++i) {
    float t0 = i / (float)n, t1 = (i + 1) / (float)n;
    float f0 = both ? std::sin(t0 * kPi) : 1.f - t0, f1 = both ? std::sin(t1 * kPi) : 1.f - t1;
    seg(s, lerp(a, b, t0), lerp(a, b, t1), std::max(0.2f, w * f0), std::max(0.2f, w * f1), c, op);
  }
}
std::vector<Vec2> spiky(Vec2 o, float rIn, float rOut, int n, uint32_t seed, float jit) {
  std::vector<Vec2> v; Rng r(seed);
  for (int i = 0; i < n * 2; ++i) {
    float a = i / (float)(n * 2) * kTau;
    float rr = (i & 1) ? rIn * r.range(1.f - jit, 1.f + jit) : rOut * r.range(1.f - jit, 1.f + jit);
    v.push_back(o + fromAngle(a, rr));
  }
  return v;
}
Vec2 center(const Ctx& c) { return (c.bbMin + c.bbMax) * 0.5f; }

void registerMangaPart(BrushLibrary& lib) {
  addArc(lib, mkInfo("manga.linhas-velocidade", "Linhas de velocidade", "manga",
      "Na vida real (mangá): traços paralelos à direção do movimento indicam velocidade; ficam finos nas pontas e variam de comprimento. Modelo: 3 linhas afinadas por passo, deslocadas pela normal, comprimento cresce com a velocidade da caneta.",
      3, kInk, kWhite, "line", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 2.2f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); for (int i = 0; i < 3; ++i) { float off = r.sym() * c.size * 5.f; float L = c.size * r.range(6.f, 14.f) * (0.6f + std::min(1.5f, c.speed / 600.f));
             Vec2 a0 = c.p + c.nrm * off; taperLine(*c.s, a0, a0 - c.dir * L, Wd(c) * r.range(0.3f, 0.7f), C1(c)); } };
           return a; });

  addArc(lib, mkInfo("manga.linhas-foco", "Linhas de concentração", "manga",
      "Na vida real: linhas radiais convergindo no ponto de foco (choque, surpresa); o centro fica vazio e as linhas variam em espessura. Modelo: ao soltar, ~110 linhas radiais a partir do ponto inicial, vazio central proporcional ao arrasto.",
      3, kInk, kWhite, "dot", "white"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { Vec2 o = c.first; float R0 = std::max(c.size * 6.f, c.maxDist * 0.9f); float R1 = std::max(c.s->width(), c.s->height()) * 0.8f; Rng r(c.sp->seed);
             for (int i = 0; i < 110; ++i) { float ang = i / 110.f * kTau + r.sym() * 0.02f; float r0 = R0 * r.range(1.f, 1.5f), r1 = R1 * r.range(0.6f, 1.f);
               taperLine(*c.s, o + fromAngle(ang, r1), o + fromAngle(ang, r0), c.size * r.range(0.25f, 0.9f), C1(c)); } };
           a.emitAtBegin = false; return a; });

  addArc(lib, mkInfo("manga.explosao", "Explosão de impacto", "manga",
      "Na vida real: balão de impacto com pontas irregulares ao redor de um centro claro. Modelo: ao soltar, polígono estrela de raio alternado aleatório do tamanho do arrasto, preenchimento e contorno.",
      3, kInk, kWhite, "circle", "white"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; }; a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { Vec2 o = center(c); float R = std::max(c.size * 5.f, (c.bbMax - c.bbMin).len() * 0.5f);
             auto v = spiky(o, R * 0.62f, R * 1.15f, 14, c.sp->seed, 0.22f); fillPoly(*c.s, v, C2(c)); poly(*c.s, v, c.size * 0.45f, c.size * 0.45f, C1(c), 1.f, Blend::Normal, true); };
           return a; });

  addArc(lib, mkInfo("manga.nuvem-pensamento", "Balão de pensamento", "manga",
      "Na vida real: balão de pensamento tem contorno em arcos de nuvem e bolinhas decrescentes levando ao personagem. Modelo: ao soltar, elipse da caixa do traço com festões (arcos) + 3 bolhas decrescentes.",
      3, kInk, kWhite, "circle", "white"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; }; a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { Vec2 o = center(c); float rx = std::max(c.size * 5.f, (c.bbMax.x - c.bbMin.x) * 0.5f), ry = std::max(c.size * 3.5f, (c.bbMax.y - c.bbMin.y) * 0.5f);
             int n = 14; for (int i = 0; i < n; ++i) { float ang = i / (float)n * kTau; Vec2 q = o + Vec2{std::cos(ang) * rx, std::sin(ang) * ry}; disc(*c.s, q, rx * 0.2f, C1(c)); }
             for (int i = 0; i < n; ++i) { float ang = i / (float)n * kTau; Vec2 q = o + Vec2{std::cos(ang) * rx, std::sin(ang) * ry}; disc(*c.s, q, rx * 0.2f - c.size * 0.4f, C2(c)); }
             ellipseDab(*c.s, o, rx, ry, 0, C2(c), 1.f, 1.f);
             for (int i = 0; i < 3; ++i) { Vec2 q = o + Vec2{-rx * (0.5f + 0.25f * i), ry * (1.4f + 0.45f * i)}; float r = rx * (0.12f - 0.03f * i); disc(*c.s, q, r + c.size * 0.4f, C1(c)); disc(*c.s, q, r, C2(c)); } };
           return a; });

  addArc(lib, mkInfo("manga.gota-suor", "Gotas de suor / lágrima", "manga",
      "Na vida real (convenção do mangá): gotas em forma de lágrima voando do rosto marcam nervosismo. Modelo: a cada passo, gota (ponta + base redonda) lançada em arco para fora, com brilho.",
      6, Color{0.25f, 0.55f, 0.95f, 1}, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 5.f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); float ang = -kPi * 0.5f + r.sym() * 0.9f; Vec2 d = fromAngle(ang); Vec2 b = c.p + d * c.size * 2.5f; float s = c.size * r.range(0.8f, 1.2f);
             Vec2 n = d.perp(); std::vector<Vec2> v; v.push_back(b + d * s * 1.8f);
             for (int i = 0; i <= 14; ++i) { float t = kPi * (0.15f + 0.7f * i / 14.f) ; v.push_back(b - d * s * 0.2f + (n * std::sin(t - kPi * 0.5f * 0.f) * 0.f)); }
             v.clear(); v.push_back(b + d * s * 1.9f);
             for (int i = -7; i <= 7; ++i) { float t = i / 7.f * kPi * 0.78f; v.push_back(b + d * (-std::cos(t) * s * 0.9f + s * 0.1f) + n * (std::sin(t) * s * 0.9f)); }
             fillPoly(*c.s, v, C1(c)); poly(*c.s, v, 0.7f, 0.7f, shade(C1(c), 0.55f), 1.f, Blend::Normal, true);
             disc(*c.s, b - n * s * 0.3f - d * s * 0.2f, s * 0.22f, kWhite, 0.8f); };
           return a; });

  addArc(lib, mkInfo("manga.veia-raiva", "Veia de raiva", "manga",
      "Na vida real (convenção do mangá): quatro arcos em cruz = veia saltada de irritação. Modelo: no ponto inicial, quatro pares de arcos em L girados 90° com preenchimento vermelho e contorno.",
      9, Color{0.85f, 0.1f, 0.15f, 1}, kInk, "dot", "white"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx& c) { float s = c.size; for (int k = 0; k < 4; ++k) { float rot = k * kPi * 0.5f + 0.4f; std::vector<Vec2> v;
               for (int i = 0; i <= 10; ++i) { float t = i / 10.f * kPi * 0.5f; v.push_back(c.p + rotate(Vec2{std::cos(t) * s * 1.7f, std::sin(t) * s * 1.7f}, rot) + rotate(Vec2{s * 0.35f, s * 0.35f}, rot)); }
               for (int i = 10; i >= 0; --i) { float t = i / 10.f * kPi * 0.5f; v.push_back(c.p + rotate(Vec2{std::cos(t) * s * 0.85f, std::sin(t) * s * 0.85f}, rot) + rotate(Vec2{s * 0.35f, s * 0.35f}, rot)); }
               fillPoly(*c.s, v, C1(c)); poly(*c.s, v, 0.9f, 0.9f, C2(c), 1.f, Blend::Normal, true); } };
           return a; });

  addArc(lib, mkInfo("manga.rubor", "Rubor (hachuras)", "manga",
      "Na vida real (mangá): rubor nas bochechas é feito por poucas hachuras diagonais, não por cor chapada. Modelo: pequenos feixes de 3 a 4 traços diagonais finos a cada passo, rosados.",
      3, Color{0.95f, 0.35f, 0.45f, 1}, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 3.2f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); for (int i = 0; i < 4; ++i) { Vec2 q = c.p + Vec2{(i - 1.5f) * c.size * 0.9f, 0}; taperLine(*c.s, q, q + Vec2{c.size * 1.3f, -c.size * 2.2f} * r.range(0.8f, 1.2f), Wd(c) * 0.5f, C1(c)); } };
           return a; });

  addArc(lib, mkInfo("manga.linhas-tristeza", "Linhas de tristeza", "manga",
      "Na vida real (mangá): faixas verticais caindo sobre o rosto/cenário = depressão, 'cara azul'. Modelo: traços verticais de comprimentos diferentes que descem da linha do traço e afinam.",
      3, kInk, kWhite, "line", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.8f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); float L = c.size * r.range(5.f, 20.f); taperLine(*c.s, c.p, c.p + Vec2{r.sym() * 0.6f, L}, Wd(c) * 0.6f, C1(c)); };
           return a; });

  // ---- screentones, anchored to the canvas grid so overlapping strokes line up ----
  addArc(lib, mkInfo("manga.tom-pontos", "Retícula (tom de pontos)", "manga",
      "Na vida real: a retícula/screentone é uma folha de pontos a 45° cuja densidade dá o cinza; pontos maiores = mais escuro. Modelo: grade a 45° fixa na tela (passo ~size*0.55), ponto de raio proporcional à pressão e à distância ao traço.",
      9, kInk, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.9f; };
           a.emit = [](Ctx& c) { float pitch = std::max(3.f, c.size * 0.55f); float R = c.size * 1.4f; float u0 = std::floor((c.p.x - R) / pitch) * pitch, v0 = std::floor((c.p.y - R) / pitch) * pitch;
             for (float y = v0; y < c.p.y + R; y += pitch) for (float x = u0; x < c.p.x + R; x += pitch) { int ix = (int)std::round(x / pitch), iy = (int)std::round(y / pitch);
               Vec2 g{x + ((iy & 1) ? pitch * 0.5f : 0.f), y}; float d = dist(g, c.p); if (d > R) continue; float rr = pitch * 0.5f * (1.f - d / R * 0.7f) * (0.35f + 0.65f * c.pr); (void)ix;
               disc(*c.s, g, std::max(0.3f, rr), C1(c)); } };
           return a; });

  addArc(lib, mkInfo("manga.tom-linhas", "Retícula de linhas", "manga",
      "Na vida real: tom de linhas paralelas finas (cinza mecânico). Modelo: linhas a 45° fixas na tela, espaçadas por size*0.5, desenhadas só dentro do raio do pincel.",
      9, kInk, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.8f; };
           a.emit = [](Ctx& c) { float pitch = std::max(3.f, c.size * 0.5f); float R = c.size * 1.3f; Vec2 dir = fromAngle(-kPi * 0.25f); Vec2 n = dir.perp();
             float k0 = std::floor(dot(c.p - n * 0.f, n) / pitch - R / pitch) * pitch;
             for (float k = k0; k < k0 + 2 * R + pitch; k += pitch) { float off = k - dot(c.p, n); if (std::fabs(off) > R) continue; float half = std::sqrt(std::max(0.f, R * R - off * off));
               Vec2 mid = c.p + n * off; seg(*c.s, mid - dir * half, mid + dir * half, 0.6f, 0.6f, C1(c)); } };
           return a; });

  addArc(lib, mkInfo("manga.tom-degrade", "Retícula em degradê", "manga",
      "Na vida real: tom degradê (bokashi) = pontos que crescem de forma contínua. Modelo: retícula a 45° em que o raio do ponto varia com a distância percorrida (fino no começo, grosso no fim).",
      10, kInk, kWhite, "line", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.9f; };
           a.emit = [](Ctx& c) { float pitch = std::max(3.f, c.size * 0.45f); float R = c.size * 1.3f; float t = clampf(c.arc / 300.f);
             float u0 = std::floor((c.p.x - R) / pitch) * pitch, v0 = std::floor((c.p.y - R) / pitch) * pitch;
             for (float y = v0; y < c.p.y + R; y += pitch) for (float x = u0; x < c.p.x + R; x += pitch) { int iy = (int)std::round(y / pitch); Vec2 g{x + ((iy & 1) ? pitch * 0.5f : 0.f), y};
               if (dist(g, c.p) > R) continue; disc(*c.s, g, std::max(0.25f, pitch * 0.5f * t * 1.1f), C1(c)); } };
           return a; });

  addArc(lib, mkInfo("manga.hachura-cruzada", "Hachura cruzada (kakeami)", "manga",
      "Na vida real: sombra por cruzamento de hachuras; cada camada de traços em outro ângulo escurece mais. Modelo: dois feixes de traços finos a +45° e -45° com tremor de mão; o segundo só aparece com pressão alta.",
      8, kInk, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.55f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); float L = c.size * 1.6f; auto hatch = [&](float ang) { Vec2 d = fromAngle(ang), o = c.p + c.nrm * r.sym() * c.size * 0.4f; seg(*c.s, o - d * L * 0.5f + Vec2{r.sym(), r.sym()}, o + d * L * 0.5f, 0.55f, 0.55f, C1(c)); };
             hatch(-kPi * 0.25f + r.sym() * 0.08f); if (c.pr > 0.6f) hatch(kPi * 0.25f + r.sym() * 0.08f); };
           return a; });

  addArc(lib, mkInfo("manga.balao-fala", "Balão de fala", "manga",
      "Na vida real: balão de fala é uma elipse levemente irregular com rabicho apontando ao falante. Modelo: ao soltar, elipse da caixa do traço (preenchida de branco + contorno) e rabicho triangular na direção do último movimento.",
      3, kInk, kWhite, "circle", "paper"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; }; a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { Vec2 o = center(c); float rx = std::max(c.size * 6.f, (c.bbMax.x - c.bbMin.x) * 0.5f), ry = std::max(c.size * 4.f, (c.bbMax.y - c.bbMin.y) * 0.5f);
             Vec2 tip = o + Vec2{-rx * 0.5f, ry * 1.7f}; std::vector<Vec2> tail{o + Vec2{-rx * 0.45f, ry * 0.7f}, tip, o + Vec2{-rx * 0.05f, ry * 0.95f}};
             auto pts = circlePts(o, rx, ry, 0, 40); fillPoly(*c.s, pts, C2(c)); fillPoly(*c.s, tail, C2(c)); poly(*c.s, pts, c.size * 0.35f, c.size * 0.35f, C1(c), 1.f, Blend::Normal, true);
             seg(*c.s, tail[0], tip, c.size * 0.35f, c.size * 0.35f, C1(c)); seg(*c.s, tip, tail[2], c.size * 0.35f, c.size * 0.35f, C1(c)); };
           return a; });

  addArc(lib, mkInfo("manga.vento-redemoinho", "Redemoinho de vento", "manga",
      "Na vida real (mangá): vento é desenhado como espirais abertas que terminam em voltinhas. Modelo: a cada passo, curva em espiral logarítmica afinada que sai da trajetória.",
      3, kInk, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 6.f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); Vec2 o = c.p + c.nrm * r.sym() * c.size * 3.f; float rot = c.dir.angle(); std::vector<Vec2> v;
             for (int i = 0; i <= 28; ++i) { float t = i / 28.f; float ang = t * kPi * 3.2f; float rr = c.size * (0.4f + 2.6f * (1.f - t)) * (t < 0.5f ? 1.f : 1.f); v.push_back(o + rotate(Vec2{c.size * 5.f * t - 0.f + std::cos(ang) * rr * 0.4f, std::sin(ang) * rr * 0.5f}, rot)); }
             for (size_t i = 1; i < v.size(); ++i) seg(*c.s, v[i - 1], v[i], Wd(c) * (1.f - i / 30.f) * 0.6f + 0.3f, Wd(c) * (1.f - (i + 1) / 30.f) * 0.6f + 0.3f, C1(c)); };
           return a; });
}

// ------------------------------------------------------------------------------------------------
void registerAnimPart(BrushLibrary& lib) {
  addArc(lib, mkInfo("efeitos-anim.rastro-movimento", "Rastro de movimento", "efeitos-anim",
      "Na vida real: o obturador aberto grava varios instantes: o objeto deixa um borrão que some atrás. Modelo: ecos translúcidos do disco do pincel espaçados ao longo do caminho, alongados na direção do movimento (smear).",
      16, Color{0.25f, 0.5f, 1, 1}, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.6f; };
           a.emit = [](Ctx& c) { float len = c.size * (1.2f + std::min(2.f, c.speed / 500.f)); Dab d; d.color = C1(c); d.hardness = 0.4f; d.opacity = 0.2f; d.x = c.p.x - c.dir.x * len * 0.5f; d.y = c.p.y - c.dir.y * len * 0.5f;
             d.radius = Wd(c) * 0.9f; d.aspect = 1.f + len / std::max(1.f, Wd(c)); d.angle = c.dir.angle(); c.s->drawDab(d); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.fantasma", "Fantasma (onion ghost)", "efeitos-anim",
      "Na vida real (animação): 'eco' translúcido das poses anteriores mostra o arco do movimento. Modelo: discos-anel translúcidos a intervalos regulares, cada vez mais fracos e menores no passado.",
      14, Color{0.3f, 0.6f, 1, 1}, kWhite, "wave", "white"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 3.2f; };
           a.emit = [](Ctx& c) { float f = clampf(1.f - c.idx * 0.04f); disc(*c.s, c.p, Wd(c) * (0.6f + 0.4f * f), C1(c), 0.9f, 0.25f * f + 0.08f); disc(*c.s, c.p, Wd(c) * (0.6f + 0.4f * f) * 0.7f, C1(c), 0.9f, 0.0f); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.estrela-impacto", "Estrela de impacto", "efeitos-anim",
      "Na vida real (cartoon): estrela irregular amarela com contorno marca o instante do golpe. Modelo: no ponto inicial, polígono estrela de raios aleatórios que treme a cada frame (frame altera a semente).",
      22, Color{1, 0.85f, 0.15f, 1}, Color{0.85f, 0.1f, 0.1f, 1}, "dot", "white"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx& c) { auto v = spiky(c.p, c.size * 1.2f, c.size * 2.4f, 9, c.sp->seed + c.frame * 17u, 0.25f); fillPoly(*c.s, v, C1(c)); poly(*c.s, v, c.size * 0.12f + 1.f, c.size * 0.12f + 1.f, C2(c), 1.f, Blend::Normal, true);
             auto in = spiky(c.p, c.size * 0.55f, c.size * 1.1f, 9, c.sp->seed + c.frame * 17u + 3, 0.2f); fillPoly(*c.s, in, kWhite, 0.8f); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.onda-choque", "Onda de choque", "efeitos-anim",
      "Na vida real: uma explosão/impacto cria uma frente de pressão circular que se expande e enfraquece. Modelo: três anéis elípticos (perspectiva) cujo raio cresce com o número do frame e cuja espessura/alpha caem; use um frame por quadro da animação.",
      20, kWhite, Color{0.5f, 0.7f, 1, 1}, "dot", "night"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx& c) { float t = std::fmod((float)c.frame, 12.f) / 12.f; for (int k = 0; k < 3; ++k) { float rr = c.size * (1.f + (t + k * 0.28f) * 5.f); float f = clampf(1.f - (t + k * 0.28f)); auto pts = circlePts(c.p, rr, rr * 0.45f, 0, 56);
               poly(*c.s, pts, c.size * 0.28f * f + 0.5f, c.size * 0.28f * f + 0.5f, C1(c), 0.75f * f, Blend::Add, true); glow(*c.s, c.p, rr * 1.1f, C2(c), 0.05f * f); } };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.poeira-passo", "Nuvenzinha de poeira", "efeitos-anim",
      "Na vida real (animação): ao pisar/frear sobe uma nuvenzinha que incha e some em 4 a 6 quadros. Modelo: aglomerado de 4 a 5 círculos com contorno, tamanho crescente conforme frame, cor creme e miolo mais claro.",
      11, Color{0.82f, 0.72f, 0.58f, 1}, Color{0.45f, 0.35f, 0.28f, 1}, "wave", "sky"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 5.f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); float g = 0.7f + 0.3f * std::min(5, c.frame % 6) / 5.f; for (int pass = 0; pass < 2; ++pass) for (int i = 0; i < 5; ++i) { Rng q(c.seed(i)); Vec2 o = c.p + Vec2{(q.sym()) * c.size * 1.8f * g, -std::fabs(q.sym()) * c.size * 1.2f * g}; float rr = c.size * q.range(0.6f, 1.1f) * g;
             if (pass == 0) disc(*c.s, o, rr + 1.6f, C2(c)); else { disc(*c.s, o, rr, C1(c)); disc(*c.s, o + Vec2{-rr * 0.2f, -rr * 0.25f}, rr * 0.5f, shade(C1(c), 1.12f), 0.5f, 0.6f); } } (void)r; };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.corte-espada", "Corte de espada", "efeitos-anim",
      "Na vida real (anime): um golpe de espada deixa um arco em meia-lua, afinado nas pontas e quase branco no fio. Modelo: ao soltar, crescente entre início e fim (arco da curvatura do gesto) com núcleo branco e brilho aditivo.",
      7, kWhite, Color{0.5f, 0.8f, 1, 1}, "arc", "night"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; }; a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { std::vector<Vec2> outer, inner; int n = 40; float L = c.path->length(); for (int i = 0; i <= n; ++i) { float t = i / (float)n; PathPoint q = c.path->at(L * t); float w = std::sin(t * kPi); outer.push_back(q.p + q.dir.perp() * c.size * 0.1f); inner.push_back(q.p - q.dir.perp() * c.size * 1.6f * w); }
             std::vector<Vec2> poly2 = outer; for (int i = n; i >= 0; --i) poly2.push_back(inner[i]);
             for (int g = 3; g >= 1; --g) { std::vector<Vec2> big; for (auto& p2 : poly2) big.push_back(p2); poly(*c.s, outer, c.size * 0.9f * g * 0.5f, c.size * 0.9f * g * 0.5f, C2(c), 0.06f, Blend::Add); }
             fillPoly(*c.s, poly2, C2(c), 0.7f, Blend::Add); fillPoly(*c.s, poly2, C1(c), 0.9f, Blend::Add); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.linhas-vento", "Linhas de vento", "efeitos-anim",
      "Na vida real (animação): rajada = filetes curvos que terminam numa voltinha. Modelo: fio afinado seguindo o traço com onda lenta e laço no final de cada filete.",
      3, Color{0.45f, 0.55f, 0.7f, 1}, kWhite, "wave", "sky"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 7.f; };
           a.emit = [](Ctx& c) { Rng r(c.seed()); Vec2 o = c.p + c.nrm * r.sym() * c.size * 4.f; float L = c.size * r.range(8.f, 16.f); std::vector<Vec2> v;
             for (float t = 0; t < L; t += 2.f) v.push_back(o - c.dir * t + c.nrm * std::sin(t * 0.25f + c.frame * 0.7f) * c.size * 0.4f);
             Vec2 e = v.back(); for (int i = 0; i < 14; ++i) { float a2 = i / 14.f * kTau * 0.9f; v.push_back(e + Vec2{std::sin(a2), 1.f - std::cos(a2)} * c.size * 1.1f); }
             for (size_t i = 1; i < v.size(); ++i) { float f = 1.f - (float)i / v.size() * 0.6f; seg(*c.s, v[i - 1], v[i], Wd(c) * 0.35f * f, Wd(c) * 0.35f * f, C1(c), 0.8f); } };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.smear", "Smear (quadro borrado)", "efeitos-anim",
      "Na vida real (animação 2D/sakuga): entre poses extremas o objeto vira um borrão alongado. Modelo: elipse muito esticada na direção do movimento, com opacidade em degradê e bordas multiplicadas.",
      20, Color{0.9f, 0.35f, 0.25f, 1}, kWhite, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.45f; };
           a.emit = [](Ctx& c) { float len = c.size * (2.5f + std::min(5.f, c.speed / 250.f)); for (int k = 0; k < 3; ++k) { Dab d; d.color = C1(c); d.hardness = 0.1f + 0.3f * k; d.opacity = 0.09f + 0.05f * k; d.x = c.p.x - c.dir.x * len * 0.35f; d.y = c.p.y - c.dir.y * len * 0.35f;
               d.radius = Wd(c) * (1.f - k * 0.25f); d.aspect = len / std::max(1.f, d.radius); d.angle = c.dir.angle(); c.s->drawDab(d); } };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.carga-energia", "Carga de energia", "efeitos-anim",
      "Na vida real (anime): partículas de luz convergem ao personagem enquanto ele carrega. Modelo: ao soltar, 60 partículas em raios que vão da periferia ao ponto final, com traços afinando e brilho aditivo mais forte perto do centro.",
      6, Color{0.4f, 0.8f, 1, 1}, kWhite, "dot", "night"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; }; a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { Rng r(c.sp->seed + c.frame * 11u); Vec2 o = c.p; float R = c.size * 14.f; glow(*c.s, o, R * 0.35f, C1(c), 0.55f);
             for (int i = 0; i < 60; ++i) { float ang = r.uniform() * kTau; float r0 = R * r.range(0.45f, 1.f), r1 = r0 - R * r.range(0.1f, 0.25f); Vec2 p0 = o + fromAngle(ang, r0), p1 = o + fromAngle(ang, std::max(c.size, r1));
               for (int k = 0; k < 8; ++k) { float t = k / 8.f; seg(*c.s, lerp(p0, p1, t), lerp(p0, p1, t + 0.125f), 0.3f + t * 1.3f, 0.3f + (t + 0.125f) * 1.3f, mix(C1(c), C2(c), t), 0.5f, Blend::Add); } } };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.circulo-magico", "Círculo mágico", "efeitos-anim",
      "Na vida real (fantasia): círculo de invocação com anéis concêntricos, polígono inscrito e runas nas bordas, que gira. Modelo: ao soltar, centro = caixa do traço; anéis, hexagrama e marcas de runa, ângulo = frame*3°; brilho aditivo.",
      4, Color{0.55f, 0.35f, 1, 1}, Color{0.8f, 0.95f, 1, 1}, "circle", "night"),
      [] { ArcSpec a; a.spacing = [](float) { return 1e6f; }; a.emit = [](Ctx&) {};
           a.finish = [](Ctx& c) { Vec2 o = center(c); float R = std::max(c.size * 8.f, std::max(c.bbMax.x - c.bbMin.x, c.bbMax.y - c.bbMin.y) * 0.5f); float rot = c.frame * 3.f * kDeg2Rad; float w = c.size * 0.3f + 0.6f;
             auto ring = [&](float rr, float ww, Color col) { auto p = circlePts(o, rr, rr, 0, 90); poly(*c.s, p, ww, ww, col, 0.9f, Blend::Add, true); glow(*c.s, o, rr, col, 0.0f); };
             ring(R, w * 1.4f, C1(c)); ring(R * 0.92f, w * 0.6f, C2(c)); ring(R * 0.62f, w, C1(c)); ring(R * 0.25f, w * 0.7f, C2(c));
             for (int t = 0; t < 2; ++t) { std::vector<Vec2> tri; for (int i = 0; i < 3; ++i) tri.push_back(o + fromAngle(rot + t * kPi / 3.f + i * kTau / 3.f, R * 0.92f)); poly(*c.s, tri, w * 0.8f, w * 0.8f, C2(c), 0.9f, Blend::Add, true); }
             for (int i = 0; i < 24; ++i) { float a2 = -rot * 0.6f + i * kTau / 24.f; Vec2 q = o + fromAngle(a2, R * 0.77f); Rng rr(i * 7 + 1); for (int k = 0; k < 2; ++k) seg(*c.s, q + fromAngle(a2 + rr.sym(), 0), q + fromAngle(a2 + rr.range(-1.5f, 1.5f) + k, R * 0.07f), w * 0.5f, w * 0.5f, C1(c), 0.9f, Blend::Add); }
             glow(*c.s, o, R * 1.15f, C1(c), 0.12f); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.faiscas-impacto", "Faíscas de impacto", "efeitos-anim",
      "Na vida real: dois materiais se chocando soltam faíscas em raios, mais longas e brancas no centro. Modelo: no ponto inicial, 18 a 26 traços radiais curtos de comprimento aleatório (muda por frame) + flash aditivo e estrela de quatro pontas.",
      6, Color{1, 0.85f, 0.35f, 1}, kWhite, "dot", "night"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx& c) { Rng r(c.sp->seed + c.frame * 31u); int n = r.irange(18, 26); glow(*c.s, c.p, c.size * 4.f, C1(c), 0.5f);
             for (int i = 0; i < n; ++i) { float ang = r.uniform() * kTau; float L = c.size * r.range(2.f, 7.f); float r0 = c.size * r.range(0.4f, 1.5f); Vec2 a0 = c.p + fromAngle(ang, r0), b0 = c.p + fromAngle(ang, r0 + L);
               for (int k = 0; k < 6; ++k) { float t = k / 6.f; seg(*c.s, lerp(a0, b0, t), lerp(a0, b0, t + 1.f / 6.f), (1.f - t) * 1.4f + 0.2f, (1.f - t - 1.f / 6.f) * 1.4f + 0.2f, mix(C2(c), C1(c), t), 0.9f, Blend::Add); } }
             star4(*c.s, c.p, c.size * 2.4f, C2(c), 0.95f, Blend::Add); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.brilho", "Brilho cintilante (shine)", "efeitos-anim",
      "Na vida real (animação): brilho de metal/lente aparece como estrela de 4 pontas que pulsa. Modelo: estrelas de quatro pontas ao longo do traço, tamanho pulsa com o frame (fase por estrela), núcleo branco com halo.",
      10, Color{1, 1, 0.85f, 1}, Color{1, 0.9f, 0.4f, 1}, "wave", "night"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 2.2f; };
           a.emit = [](Ctx& c) { float ph = c.rnd() * 6.28f; float pulse = 0.55f + 0.45f * std::sin(c.frame * 0.9f + ph); Vec2 q = c.p + c.nrm * (c.rnd(1) - 0.5f) * c.size * 2.f;
             star4(*c.s, q, c.size * (0.6f + c.rnd(2)) * pulse * 1.6f, C1(c), 0.95f, Blend::Add, c.rnd(3) * 0.4f); glow(*c.s, q, c.size * pulse, C2(c), 0.25f); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.flash-golpe", "Flash de golpe", "efeitos-anim",
      "Na vida real (cartoon): no instante do impacto a tela 'estoura' em branco com raios. Modelo: no ponto inicial, 12 raios afilados longos e curtos alternados, círculo branco e halo, com pontas que variam por frame.",
      12, kWhite, Color{1, 0.9f, 0.4f, 1}, "dot", "night"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx& c) { Rng r(c.sp->seed + c.frame * 5u); glow(*c.s, c.p, c.size * 5.f, C2(c), 0.5f); for (int i = 0; i < 12; ++i) { float ang = i / 12.f * kTau + r.sym() * 0.08f; float L = c.size * ((i & 1) ? r.range(2.5f, 4.f) : r.range(4.5f, 7.f));
               Vec2 p0 = c.p + fromAngle(ang, c.size * 0.5f), p1 = c.p + fromAngle(ang, L); for (int k = 0; k < 6; ++k) { float t = k / 6.f; seg(*c.s, lerp(p0, p1, t), lerp(p0, p1, t + 1.f / 6.f), c.size * 0.35f * (1.f - t) + 0.2f, c.size * 0.35f * (1.f - t - 0.17f) + 0.2f, C1(c), 1.f, Blend::Add); } }
             disc(*c.s, c.p, c.size * 0.9f, C1(c), 0.9f, 1.f, Blend::Add); };
           return a; });

  addArc(lib, mkInfo("efeitos-anim.gotas-voando", "Gotas voando", "efeitos-anim",
      "Na vida real: gotas lançadas descrevem parábolas (movimento balístico). Modelo: no ponto inicial, 7 gotas com velocidades iniciais aleatórias e a posição do frame calculada pela parábola; cada frame avança no tempo.",
      5, Color{0.3f, 0.6f, 1, 1}, kWhite, "dot", "white"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e6f; };
           a.emit = [](Ctx& c) { Rng r(c.sp->seed); float t = 0.3f + (c.frame % 10) * 0.12f; for (int i = 0; i < 7; ++i) { float ang = -kPi * 0.5f + r.sym() * 1.1f, v = c.size * r.range(6.f, 12.f);
               Vec2 p = c.p + Vec2{std::cos(ang) * v * t, std::sin(ang) * v * t + 0.5f * c.size * 18.f * t * t}; float rr = c.size * r.range(0.4f, 0.8f) * (1.f - t * 0.25f);
               disc(*c.s, p, rr, C1(c), 0.9f); disc(*c.s, p - Vec2{rr * 0.3f, rr * 0.3f}, rr * 0.3f, kWhite, 0.8f, 0.8f); } };
           return a; });
}

// ------------------------------------------------------------------------------------------------
void registerPixelPart(BrushLibrary& lib) {
  auto snap = [](float v, int b) { return (int)std::floor(v / b) * b; };
  addArc(lib, mkInfo("pixel.caneta", "Caneta pixel", "pixel",
      "Na vida real (pixel art): cada pixel é um quadrado de cor chapada na grade; linhas são escadas sem suavização. Modelo: o traço é amostrado a cada 1 px e cada amostra pinta um bloco alinhado à grade (sem antialiasing).",
      2, kInk, kWhite, "wave", "white"),
      [snap] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1.f; };
           a.emit = [snap](Ctx& c) { int b = std::max(1, (int)std::round(c.size)); fillRectPx(*c.s, snap(c.p.x, b), snap(c.p.y, b), b, b, C1(c)); }; return a; });

  struct Pat { const char* id; const char* name; const char* desc; int kind; };
  static const Pat pats[] = {
      {"pixel.dither-bayer2", "Dither Bayer 2x2", "Na vida real (pixel art clássico): dither ordenado simula tons com 5 níveis usando matriz 2x2. Modelo: pixel só é pintado onde o limiar da matriz < densidade (pressão); padrão fixo na tela.", 2},
      {"pixel.dither-bayer4", "Dither Bayer 4x4", "Na vida real: matriz de Bayer 4x4 dá 17 níveis de cinza com textura em cruzeta fina. Modelo: limiar ordenado 4x4 ancorado na tela.", 4},
      {"pixel.dither-bayer8", "Dither Bayer 8x8", "Na vida real: Bayer 8x8 dá degradê quase contínuo (65 níveis) com padrão discreto. Modelo: limiar 8x8 gerado recursivamente.", 8},
      {"pixel.dither-xadrez", "Dither xadrez", "Na vida real: meio-tom 50% em tabuleiro, usado em consoles antigos para simular transparência. Modelo: pinta pixels com (x+y) par.", -1},
      {"pixel.dither-linhas", "Dither de linhas", "Na vida real: linhas horizontais alternadas (televisão/CRT) como cinza 50%. Modelo: pinta linhas y par.", -2}};
  for (const Pat& p : pats) {
    int kind = p.kind;
    addArc(lib, mkInfo(p.id, p.name, "pixel", p.desc, 8, kInk, kWhite, "wave", "white"),
        [kind] { ArcSpec a; a.spacing = [](float s) { return s * 0.7f; };
             a.emit = [kind](Ctx& c) { int R = (int)(c.size * 1.2f); float dens = 0.15f + 0.85f * c.pr; int n = kind > 0 ? kind : 2;
               auto bayer = [](int x, int y, int n2) { int v = 0; int m = n2; int xx = x, yy = y; int bits = 0; while ((1 << bits) < n2) ++bits; for (int b = 0; b < bits; ++b) { int bx = (xx >> b) & 1, by = (yy >> b) & 1; v = (v << 2) | ((bx ^ by) << 1) | by; } (void)m; return (float)v / (float)(n2 * n2); };
               for (int y = -R; y <= R; ++y) for (int x = -R; x <= R; ++x) { int px = (int)std::floor(c.p.x) + x, py = (int)std::floor(c.p.y) + y; if (x * x + y * y > R * R) continue;
                 bool on; if (kind > 0) on = bayer(px & (n - 1), py & (n - 1), n) < dens; else if (kind == -1) on = ((px + py) & 1) == 0; else on = (py & 1) == 0;
                 if (on) fillRectPx(*c.s, px, py, 1, 1, C1(c)); } };
             return a; });
  }

  addArc(lib, mkInfo("pixel.sombra-luz", "Luz e sombra pixel", "pixel",
      "Na vida real (pixel art): sombreamento por rampa de cores, luz no canto superior esquerdo e sombra no inferior direito, em blocos. Modelo: cada bloco clareia (Overlay) no topo-esquerda e escurece embaixo-direita, sem suavização.",
      3, Color{0.5f, 0.5f, 0.5f, 1}, kWhite, "wave", "white"),
      [snap] { ArcSpec a; a.spacing = [](float) { return 1.f; };
           a.emit = [snap](Ctx& c) { int b = std::max(1, (int)std::round(c.size)); int x = snap(c.p.x, b), y = snap(c.p.y, b); fillRectPx(*c.s, x - b, y - b, b, b, kWhite, 0.35f, Blend::Overlay); fillRectPx(*c.s, x + b, y + b, b, b, kInk, 0.35f, Blend::Overlay); }; return a; });
}

}  // namespace

void registerManga(BrushLibrary& lib) { registerMangaPart(lib); }
void registerAnim(BrushLibrary& lib) { registerAnimPart(lib); }
void registerPixel(BrushLibrary& lib) { registerPixelPart(lib); }

}  // namespace dfx
}  // namespace ab
