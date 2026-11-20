#include "decorfx_util.hpp"

namespace ab {
namespace dfx {

static inline Color C1(const Ctx& c) { return c.sp->color; }
static inline Color C2(const Ctx& c) { return c.sp->color2; }
static inline float W(const Ctx& c) { return c.size * (0.45f + 0.55f * c.pr); }  // pressure-scaled radius

// draw the stroke piece between the previous and the current element, shifted sideways by `off`
static void stepLine(Ctx& c, float off, float r0, float r1, Color col, float op = 1.f) {
  float s0 = std::max(0.f, c.arc - c.step);
  seg(*c.s, P(c, s0, off), P(c, c.arc, off), r0, r1, col, op);
}
// sagging string between two points (quadratic), returns points
static std::vector<Vec2> sagCurve(Vec2 a, Vec2 b, float sag, int n = 14) {
  std::vector<Vec2> v;
  Vec2 m = (a + b) * 0.5f + Vec2{0, sag};
  for (int i = 0; i <= n; ++i) {
    float t = (float)i / n;
    v.push_back(a * ((1 - t) * (1 - t)) + m * (2 * t * (1 - t)) + b * (t * t));
  }
  return v;
}
// polyline of local-unit pattern at an arc position
static void localPoly(Ctx& c, Vec2 o, Vec2 d, Vec2 n, const std::vector<Vec2>& pat, float unit, float r, Color col) {
  std::vector<Vec2> v;
  for (auto& q : pat) v.push_back(local(o, d, n, q.x * unit, q.y * unit));
  poly(*c.s, v, r, r, col);
}

void registerDecor(BrushLibrary& lib) {
  static const Color ink{0.08f, 0.08f, 0.12f, 1}, white{1, 1, 1, 1};

  // ---------------- linhas ritmadas ----------------
  addArc(lib, mkInfo("decor.pontilhada", "Linha pontilhada", "decor",
      "Na vida real: pontos de caneta/furos de picote com ritmo constante. Modelo: discos redondos a intervalo fixo em comprimento de arco; raio segue a pressão.", 4, ink, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.8f; };
           a.emit = [](Ctx& c) { disc(*c.s, c.p, W(c) * 0.9f, C1(c), 0.95f); }; return a; });

  addArc(lib, mkInfo("decor.tracejada", "Linha tracejada", "decor",
      "Na vida real: tracejado de planta/mapa com traço e vão regulares que seguem a curva. Modelo: traços como trechos da trajetória (não retas), ritmo traço:vão ajustável.", 3, ink, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 6.f; };
           a.emit = [](Ctx& c) { float len = c.step * 0.62f; Vec2 q = P(c, c.arc);
             for (float t = 0; t <= len; t += 1.5f) seg(*c.s, P(c, c.arc + t), P(c, c.arc + std::min(len, t + 1.5f)), W(c) * 0.8f, W(c) * 0.8f, C1(c)); (void)q; };
           return a; });

  addArc(lib, mkInfo("decor.traco-ponto", "Traço e ponto", "decor",
      "Na vida real: linha de eixo/centro de desenho técnico (traço longo, vão, ponto, vão). Modelo: período de 4 símbolos sobre o comprimento de arco.", 3, ink, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 9.f; };
           a.emit = [](Ctx& c) { float L = c.step; float r = W(c) * 0.75f;
             for (float t = 0; t <= L * 0.5f; t += 1.5f) seg(*c.s, P(c, c.arc + t), P(c, c.arc + t + 1.5f), r, r, C1(c));
             disc(*c.s, P(c, c.arc + L * 0.72f), r * 1.1f, C1(c)); }; return a; });

  addArc(lib, mkInfo("decor.trilho-duplo", "Linha dupla", "decor",
      "Na vida real: filete duplo de gráfica/estrada. Modelo: duas trajetórias paralelas deslocadas pela normal local.", 2, ink, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1.5f; };
           a.emit = [](Ctx& c) { float d = c.size * 1.6f; stepLine(c, d, W(c) * 0.5f, W(c) * 0.5f, C1(c)); stepLine(c, -d, W(c) * 0.5f, W(c) * 0.5f, C1(c)); };
           return a; });

  addArc(lib, mkInfo("decor.trilho-triplo", "Linha tripla (grossa-fina-grossa)", "decor",
      "Na vida real: filete de moldura clássico com linha grossa central entre duas finas. Modelo: três trajetórias paralelas de espessuras diferentes.", 3, ink, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1.5f; };
           a.emit = [](Ctx& c) { float d = c.size * 2.4f; stepLine(c, 0, W(c) * 0.9f, W(c) * 0.9f, C1(c));
             stepLine(c, d, W(c) * 0.3f, W(c) * 0.3f, C1(c)); stepLine(c, -d, W(c) * 0.3f, W(c) * 0.3f, C1(c)); };
           return a; });

  addArc(lib, mkInfo("decor.ondulada", "Linha ondulada", "decor",
      "Na vida real: onda senoidal de ornamento/água em desenho. Modelo: deslocamento senoidal da normal em função do arco; amplitude e comprimento de onda são parâmetros.", 3, Color{0.1f, 0.35f, 0.7f, 1}, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1.5f; };
           a.emit = [](Ctx& c) { float A = c.P("amp") , lam = c.P("onda");
             float s0 = std::max(0.f, c.arc - c.step);
             Vec2 a0 = P(c, s0, std::sin(s0 / lam * kTau) * A), b0 = P(c, c.arc, std::sin(c.arc / lam * kTau) * A);
             seg(*c.s, a0, b0, W(c) * 0.7f, W(c) * 0.7f, C1(c)); }; return a; },
      {{"amp", "Amplitude", 9, 1, 40}, {"onda", "Comprimento de onda", 36, 8, 120}});

  addArc(lib, mkInfo("decor.zigue-zague", "Dentes / ticks", "decor",
      "Na vida real: serrilha e marcas de régua. Modelo: riscos perpendiculares alternando comprimento (longo/curto) a intervalo fixo.", 2, ink, white),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 3.2f; };
           a.emit = [](Ctx& c) { float L = (c.idx % 4 == 0 ? 3.2f : 1.8f) * c.size;
             seg(*c.s, c.p - c.nrm * L * 0.5f, c.p + c.nrm * L * 0.5f, W(c) * 0.35f, W(c) * 0.35f, C1(c)); stepLine(c, 0, W(c) * 0.3f, W(c) * 0.3f, C1(c)); };
           return a; });

  // ---------------- corrente ----------------
  addArc(lib, mkInfo("decor.corrente", "Corrente (elos 3D)", "decor",
      "Na vida real: elos de metal se alternam: um de frente (anel oval) e o seguinte de perfil (barra fina), entrelaçados. Modelo: elos pares = anel elíptico com brilho; ímpares = barra arredondada de perfil; luz superior-esquerda.", 7, Color{0.62f, 0.64f, 0.68f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 1.55f; };
           a.emit = [](Ctx& c) { float r = W(c); float ang = c.dir.angle(); Color m = C1(c);
             if (c.idx % 2 == 0) {
               auto ring = circlePts(c.p, r * 1.55f, r * 0.95f, ang, 28);
               poly(*c.s, ring, r * 0.34f, r * 0.34f, shade(m, 0.45f));
               poly(*c.s, ring, r * 0.24f, r * 0.24f, m);
               auto hl = circlePts(c.p - Vec2{0.3f, 0.5f} * r * 0.25f, r * 1.5f, r * 0.9f, ang, 10, kPi * 0.95f, kPi * 1.55f);
               poly(*c.s, hl, r * 0.1f, r * 0.1f, tint(m, white, 0.75f), 0.9f);
             } else {
               seg(*c.s, c.p - c.dir * r * 1.15f, c.p + c.dir * r * 1.15f, r * 0.5f, r * 0.5f, shade(m, 0.5f));
               seg(*c.s, c.p - c.dir * r * 1.0f, c.p + c.dir * r * 1.0f, r * 0.34f, r * 0.34f, m);
               seg(*c.s, c.p - c.dir * r * 0.8f - c.nrm * r * 0.1f, c.p + c.dir * r * 0.5f - c.nrm * r * 0.1f, r * 0.1f, r * 0.1f, tint(m, white, 0.8f));
             } };
           return a; });

  addArc(lib, mkInfo("decor.corda", "Corda trançada (3 fios)", "decor",
      "Na vida real: três fios torcidos em hélice; cada fio passa na frente e atrás conforme a fase da torção e fica mais claro quando está na frente. Modelo: 3 hélices com fase 120° em função do arco, desenhadas em ordem de profundidade com sombreamento.", 6, Color{0.78f, 0.6f, 0.34f, 1}, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1.f; };
           a.emit = [](Ctx& c) { float R = W(c); float tw = c.P("torcao"); float s0 = std::max(0.f, c.arc - c.step);
             struct S { float depth; int k; } order[3];
             for (int k = 0; k < 3; ++k) order[k] = {std::cos(c.arc / tw * kTau + k * kTau / 3), k};
             std::sort(order, order + 3, [](const S& a, const S& b) { return a.depth < b.depth; });
             for (auto& o : order) { int k = o.k; float ph1 = c.arc / tw * kTau + k * kTau / 3, ph0 = s0 / tw * kTau + k * kTau / 3;
               float off1 = std::sin(ph1) * R * 0.75f, off0 = std::sin(ph0) * R * 0.75f; float lit = 0.62f + 0.38f * o.depth;
               Color col = shade(C1(c), lit);
               seg(*c.s, P(c, s0, off0), P(c, c.arc, off1), R * 0.62f, R * 0.62f, shade(col, 0.55f));
               seg(*c.s, P(c, s0, off0 - 0.2f * R), P(c, c.arc, off1 - 0.2f * R), R * 0.42f, R * 0.42f, col); } };
           return a; },
      {{"torcao", "Passo da torção", 20, 8, 60}});

  addArc(lib, mkInfo("decor.arame-farpado", "Arame farpado", "decor",
      "Na vida real: dois fios de aço torcidos entre si, com farpas de 4 pontas amarradas a cada ~12 cm. Modelo: dois fios senoidais em contrafase + farpas em X afiadas em intervalo fixo, com brilho metálico.", 3, Color{0.45f, 0.47f, 0.5f, 1}, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1.f; };
           a.emit = [](Ctx& c) { float R = c.size * 0.9f, lam = c.size * 9.f; float s0 = std::max(0.f, c.arc - c.step);
             for (int k = 0; k < 2; ++k) { float sg = k ? -1.f : 1.f;
               seg(*c.s, P(c, s0, sg * std::sin(s0 / lam * kTau) * R), P(c, c.arc, sg * std::sin(c.arc / lam * kTau) * R), c.size * 0.3f, c.size * 0.3f, shade(C1(c), 0.6f));
               seg(*c.s, P(c, s0, sg * std::sin(s0 / lam * kTau) * R - 0.4f), P(c, c.arc, sg * std::sin(c.arc / lam * kTau) * R - 0.4f), c.size * 0.14f, c.size * 0.14f, tint(C1(c), white, 0.6f)); }
             if (std::fmod(c.arc, c.size * 18.f) < 1.0f) { float L = c.size * 2.6f; Vec2 cp = c.p;
               for (int k = 0; k < 2; ++k) { float an = (k ? 1.f : -1.f) * 0.9f; Vec2 d = rotate(c.nrm, an);
                 seg(*c.s, cp - d * L * 0.5f, cp + d * L * 0.5f, c.size * 0.28f, c.size * 0.04f, shade(C1(c), 0.7f));
                 seg(*c.s, cp, cp - d * L * 0.5f, c.size * 0.04f, c.size * 0.28f, shade(C1(c), 0.7f)); } } };
           return a; });

  addArc(lib, mkInfo("decor.costura", "Costura (ponto corrido)", "decor",
      "Na vida real: linha de pontos de agulha: trechos de fio por cima do tecido com furinhos e pequena sombra nas pontas. Modelo: traços sobre a trajetória com brilho de fio, sombra deslocada e furos escuros nas extremidades.", 3, Color{0.85f, 0.2f, 0.2f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 4.4f; };
           a.emit = [](Ctx& c) { float L = c.step * 0.68f; float r = c.size * 0.55f;
             std::vector<Vec2> pts; for (float t = 0; t <= L; t += 1.5f) pts.push_back(P(c, c.arc + t, 0));
             std::vector<Vec2> sh; for (auto& p : pts) sh.push_back(p + Vec2{0.8f, 1.2f});
             poly(*c.s, sh, r, r, Color{0, 0, 0, 1}, 0.28f);
             poly(*c.s, pts, r, r, shade(C1(c), 0.75f)); poly(*c.s, pts, r * 0.55f, r * 0.55f, C1(c));
             for (auto& p : pts) { (void)p; }
             disc(*c.s, pts.front() - D(c, c.arc) * r * 0.9f, r * 0.45f, Color{0, 0, 0, 1}, 0.8f, 0.4f);
             disc(*c.s, pts.back() + D(c, c.arc + L) * r * 0.9f, r * 0.45f, Color{0, 0, 0, 1}, 0.8f, 0.4f); };
           return a; });

  addArc(lib, mkInfo("decor.ponto-cruz", "Ponto cruz", "decor",
      "Na vida real: bordado em pontos de cruz, cada ponto são duas diagonais. Modelo: X com fios de brilho diferente (a diagonal superior é mais clara), alinhado ao grid local do traçado.", 3, Color{0.15f, 0.45f, 0.8f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.7f; };
           a.emit = [](Ctx& c) { float h = c.size * 1.0f; Vec2 d = c.dir, n = c.nrm;
             seg(*c.s, c.p - d * h - n * h, c.p + d * h + n * h, c.size * 0.45f, c.size * 0.45f, shade(C1(c), 0.7f));
             seg(*c.s, c.p - d * h + n * h, c.p + d * h - n * h, c.size * 0.45f, c.size * 0.45f, C1(c));
             seg(*c.s, c.p - d * h + n * h * 0.9f, c.p + d * h - n * h * 1.1f, c.size * 0.14f, c.size * 0.14f, tint(C1(c), white, 0.5f)); };
           return a; });

  addArc(lib, mkInfo("decor.ziper", "Zíper", "decor",
      "Na vida real: dentes metálicos de duas fitas que se encaixam alternados, com o cursor (puxador) no ponto onde fecha. Modelo: dentes retangulares alternados esquerda/direita em meio passo, e puxador desenhado no fim do traço.", 4, Color{0.75f, 0.77f, 0.8f, 1}, Color{0.2f, 0.2f, 0.25f, 1}),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 1.1f; };
           a.emit = [](Ctx& c) { float w = c.size * 1.2f; float side = (c.idx % 2) ? 1.f : -1.f;
             Vec2 p = c.p; Vec2 a0 = p + c.nrm * side * 0.2f * w, b0 = p + c.nrm * side * w;
             seg(*c.s, a0, b0, c.size * 0.42f, c.size * 0.42f, shade(C1(c), 0.55f)); seg(*c.s, a0, b0, c.size * 0.28f, c.size * 0.28f, C1(c));
             disc(*c.s, b0, c.size * 0.42f, shade(C1(c), 0.8f)); };
           a.finish = [](Ctx& c) { Vec2 p = c.p; Color col = C2(c);
             std::vector<Vec2> q = {p - c.dir * c.size * 1.4f - c.nrm * c.size * 1.7f, p + c.dir * c.size * 1.4f - c.nrm * c.size * 1.7f,
                                    p + c.dir * c.size * 1.0f + c.nrm * c.size * 1.7f, p - c.dir * c.size * 1.0f + c.nrm * c.size * 1.7f};
             fillPoly(*c.s, q, col); disc(*c.s, p + c.dir * c.size * 2.4f, c.size * 0.9f, col); disc(*c.s, p + c.dir * c.size * 2.4f, c.size * 0.4f, white); };
           return a; });

  addArc(lib, mkInfo("decor.fita", "Fita torcida", "decor",
      "Na vida real: uma fita de cetim torce ao longo do caminho: mostra o lado de frente, afina quando fica de perfil e revela o verso (outra cor). Modelo: largura = |cos(fase)|; lado frente/verso troca de cor e sombreamento no sinal do cosseno.", 9, Color{0.85f, 0.12f, 0.25f, 1}, Color{0.55f, 0.05f, 0.15f, 1}),
      [] { ArcSpec a; a.spacing = [](float) { return 2.f; };
           a.emit = [](Ctx& c) { float tw = c.size * 11.f; float s0 = std::max(0.f, c.arc - c.step);
             float ph0 = s0 / tw * kTau, ph1 = c.arc / tw * kTau; float w0 = std::cos(ph0) * c.size * 1.6f, w1 = std::cos(ph1) * c.size * 1.6f;
             Color col = (w1 + w0 >= 0) ? C1(c) : C2(c); float sh = 0.7f + 0.3f * std::fabs(std::cos(ph1));
             std::vector<Vec2> q = {P(c, s0, w0), P(c, c.arc, w1), P(c, c.arc, -w1), P(c, s0, -w0)};
             fillPoly(*c.s, q, shade(col, sh)); seg(*c.s, P(c, s0, w0), P(c, c.arc, w1), 0.5f, 0.5f, shade(col, 0.55f)); seg(*c.s, P(c, s0, -w0), P(c, c.arc, -w1), 0.5f, 0.5f, shade(col, 0.55f)); };
           return a; });

  addArc(lib, mkInfo("decor.renda", "Barra de renda", "decor",
      "Na vida real: renda de bordado tem uma linha base e festões (arcos) com olhais (furinhos) no centro de cada arco. Modelo: linha base + semicírculos repetidos + anel e ponto em cada festão.", 4, Color{0.98f, 0.96f, 0.9f, 1}, Color{0.3f, 0.2f, 0.2f, 1}),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 4.4f; };
           a.emit = [](Ctx& c) { float L = c.step; Vec2 o = P(c, c.arc), e = P(c, c.arc + L); Vec2 m = (o + e) * 0.5f; float r = L * 0.5f; Vec2 n = D(c, c.arc + L * 0.5f).perp();
             std::vector<Vec2> arc; for (int i = 0; i <= 16; ++i) { float t = kPi * i / 16.f; arc.push_back(m + (o - m) * std::cos(t) + n * (std::sin(t) * r)); }
             poly(*c.s, arc, c.size * 0.28f, c.size * 0.28f, shade(C1(c), 0.5f)); poly(*c.s, arc, c.size * 0.18f, c.size * 0.18f, C1(c));
             Vec2 ec = m + n * r * 0.45f; poly(*c.s, circlePts(ec, r * 0.28f, r * 0.28f, 0, 12), c.size * 0.14f, c.size * 0.14f, shade(C1(c), 0.55f)); disc(*c.s, ec, r * 0.09f, shade(C1(c), 0.55f));
             seg(*c.s, o, e, c.size * 0.2f, c.size * 0.2f, C1(c)); };
           return a; });

  addArc(lib, mkInfo("decor.perolas", "Fio de pérolas", "decor",
      "Na vida real: esferas nacaradas enfiadas num fio, com brilho especular e reflexo de borda. Modelo: esfera sombreada (base escura, corpo, brilho e ponto especular) + fio entre elas; pequena variação de tamanho.", 7, Color{0.97f, 0.95f, 0.92f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.05f; };
           a.emit = [](Ctx& c) { float r = W(c) * (0.92f + 0.12f * c.rnd()); seg(*c.s, c.prev, c.p, 0.6f, 0.6f, Color{0.35f, 0.3f, 0.3f, 1}, 0.8f);
             Color pc = tint(C1(c), Color{0.9f, 0.8f, 0.95f, 1}, 0.15f * c.rnd(1)); sphere(*c.s, c.p, r, pc, {-0.5f, -0.6f}, 0.95f); };
           return a; });

  addArc(lib, mkInfo("decor.gemas", "Fio de gemas", "decor",
      "Na vida real: pedras lapidadas refletem a luz em facetas de claridades diferentes. Modelo: hexágono com facetas internas claras/escuras, cor da pedra rotacionada em matiz por pedra.", 7, Color{0.85f, 0.1f, 0.3f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.4f; };
           a.emit = [](Ctx& c) { float r = W(c); Color g = hsv(std::fmod(c.rnd() * 0.9f + c.idx * 0.13f, 1.f), 0.8f, 0.9f);
             std::vector<Vec2> hex, in; for (int i = 0; i < 6; ++i) { hex.push_back(c.p + fromAngle(i * kPi / 3 + kPi / 6, r)); in.push_back(c.p + fromAngle(i * kPi / 3 + kPi / 6, r * 0.5f)); }
             fillPoly(*c.s, hex, shade(g, 0.55f));
             for (int i = 0; i < 6; ++i) { std::vector<Vec2> f = {hex[i], hex[(i + 1) % 6], in[(i + 1) % 6], in[i]}; fillPoly(*c.s, f, shade(g, 0.6f + 0.55f * (0.5f + 0.5f * std::cos(i * kPi / 3 + 0.9f)))); }
             fillPoly(*c.s, in, tint(g, white, 0.35f)); poly(*c.s, hex, 0.5f, 0.5f, shade(g, 0.4f), 1.f, Blend::Normal, true);
             star4(*c.s, c.p - Vec2{r * 0.3f, r * 0.3f}, r * 0.45f, white, 0.9f); };
           return a; });

  addArc(lib, mkInfo("decor.bandeirinhas", "Bandeirinhas de festa", "decor",
      "Na vida real: bandeirolas penduradas num barbante que faz uma curva (catenária) entre dois pontos de fixação. Modelo: curva quadrática com flecha proporcional ao vão entre fixações; triângulos coloridos pendem de cada curva.", 10, Color{0.3f, 0.2f, 0.15f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 7.f; };
           a.emit = [](Ctx& c) { if (c.isFirst || c.idx == 0) return; Vec2 a0 = c.prev, b0 = c.p; auto cur = sagCurve(a0, b0, dist(a0, b0) * 0.22f);
             poly(*c.s, cur, 0.8f, 0.8f, C1(c));
             int n = 3; for (int k = 0; k < n; ++k) { float t = (k + 0.7f) / (n + 0.4f); Vec2 pt = cur[(int)(t * 14)]; float w = c.size * 0.9f;
               Color col = paletteColor(c.idx * 3 + k); std::vector<Vec2> tri = {pt - Vec2{w, 0}, pt + Vec2{w, 0}, pt + Vec2{0, w * 2.0f}};
               fillPoly(*c.s, tri, col); poly(*c.s, tri, 0.4f, 0.4f, shade(col, 0.6f), 1.f, Blend::Normal, true); } };
           return a; });

  addArc(lib, mkInfo("decor.luzes-natal", "Cordão de luzes", "decor",
      "Na vida real: lâmpadas penduradas num fio com curva, cada uma emitindo halo de luz colorida. Modelo: fio em curva + bulbo (soquete, vidro) + halo aditivo; cor cíclica; brilho oscila com o quadro.", 8, Color{0.1f, 0.1f, 0.1f, 1}, white, "wave", "dark"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 6.f; };
           a.emit = [](Ctx& c) { Color wire{0.55f, 0.55f, 0.6f, 1}; if (c.idx > 0) { auto cur = sagCurve(c.prev, c.p, dist(c.prev, c.p) * 0.2f); poly(*c.s, cur, 0.7f, 0.7f, wire);
               for (int k = 1; k <= 2; ++k) { Vec2 b = cur[k * 14 / 3]; Color col = paletteColor(c.idx * 2 + k); float fl = 0.75f + 0.25f * std::sin(c.frame * 1.7f + c.idx * 2.f + k);
                 seg(*c.s, b, b + Vec2{0, c.size * 0.5f}, c.size * 0.18f, c.size * 0.18f, Color{0.3f, 0.3f, 0.3f, 1});
                 glow(*c.s, b + Vec2{0, c.size * 1.1f}, c.size * 3.2f, col, 0.55f * fl); disc(*c.s, b + Vec2{0, c.size * 1.1f}, c.size * 0.65f, tint(col, white, 0.5f), 0.7f);
                 disc(*c.s, b + Vec2{0, c.size * 1.1f}, c.size * 0.28f, white, 0.5f, 0.9f); } } };
           return a; });

  addArc(lib, mkInfo("decor.filme", "Rolo de filme", "decor",
      "Na vida real: filme 35 mm tem faixas escuras, furos de perfuração nas bordas e quadros claros ao centro. Modelo: faixa escura seguindo a trajetória, furos retangulares claros nos dois lados a passo fixo e divisões de quadro.", 8, Color{0.1f, 0.1f, 0.12f, 1}, Color{0.85f, 0.85f, 0.8f, 1}),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.4f; };
           a.emit = [](Ctx& c) { float s0 = std::max(0.f, c.arc - c.step); float w = c.size * 2.2f;
             std::vector<Vec2> q = {P(c, s0, w), P(c, c.arc + 0.5f, w), P(c, c.arc + 0.5f, -w), P(c, s0, -w)}; fillPoly(*c.s, q, C1(c));
             float hole = c.size * 0.55f; for (int sd = -1; sd <= 1; sd += 2) { Vec2 h = P(c, c.arc, sd * w * 0.82f); std::vector<Vec2> r = {h - c.dir * hole * 0.6f - c.nrm * hole * 0.45f, h + c.dir * hole * 0.6f - c.nrm * hole * 0.45f, h + c.dir * hole * 0.6f + c.nrm * hole * 0.45f, h - c.dir * hole * 0.6f + c.nrm * hole * 0.45f}; fillPoly(*c.s, r, C2(c)); }
             if (c.idx % 4 == 0) seg(*c.s, P(c, c.arc, w * 0.6f), P(c, c.arc, -w * 0.6f), 0.8f, 0.8f, shade(C2(c), 0.4f)); };
           return a; });

  addArc(lib, mkInfo("decor.trilho-trem", "Trilho de trem", "decor",
      "Na vida real: dois trilhos de aço paralelos sobre dormentes de madeira regularmente espaçados. Modelo: dormentes (retângulos de madeira, com variação de tom) e dois trilhos metálicos com brilho.", 4, Color{0.5f, 0.5f, 0.55f, 1}, Color{0.4f, 0.27f, 0.15f, 1}),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.f; };
           a.emit = [](Ctx& c) { float g = c.size * 2.2f; float L = g * 1.7f; Color wood = shade(C2(c), 0.8f + 0.4f * c.rnd());
             std::vector<Vec2> q = {c.p - c.nrm * L - c.dir * c.size * 0.45f, c.p + c.nrm * L - c.dir * c.size * 0.45f, c.p + c.nrm * L + c.dir * c.size * 0.45f, c.p - c.nrm * L + c.dir * c.size * 0.45f};
             fillPoly(*c.s, q, wood); poly(*c.s, q, 0.4f, 0.4f, shade(wood, 0.6f), 1.f, Blend::Normal, true);
             for (int sd = -1; sd <= 1; sd += 2) { Vec2 a1 = P(c, std::max(0.f, c.arc - c.step), sd * g), b1 = P(c, c.arc, sd * g);
               seg(*c.s, a1, b1, c.size * 0.38f, c.size * 0.38f, shade(C1(c), 0.55f)); seg(*c.s, a1 - c.nrm * sd * 0.5f, b1 - c.nrm * sd * 0.5f, c.size * 0.2f, c.size * 0.2f, tint(C1(c), white, 0.5f)); } };
           return a; });

  addArc(lib, mkInfo("decor.tijolos", "Caminho de tijolos", "decor",
      "Na vida real: tijolos assentados em fiadas com juntas desencontradas (meio tijolo de defasagem) e argamassa clara entre eles. Modelo: duas fiadas de tijolos retangulares com cor variando por tijolo e junta de argamassa.", 9, Color{0.7f, 0.28f, 0.18f, 1}, Color{0.82f, 0.8f, 0.72f, 1}),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.2f; };
           a.emit = [](Ctx& c) { float h = c.size * 0.95f; float Lb = c.step * 0.92f;
             for (int row = -1; row <= 1; row += 2) { float off = row * h; float shift = (row > 0) ? c.step * 0.5f : 0.f; float s0 = c.arc + shift - c.step * 0.5f;
               Vec2 a0 = P(c, s0, off), b0 = P(c, s0 + Lb, off); Vec2 nn = D(c, s0).perp();
               std::vector<Vec2> q = {a0 - nn * h * 0.46f, b0 - nn * h * 0.46f, b0 + nn * h * 0.46f, a0 + nn * h * 0.46f};
               fillPoly(*c.s, q, C2(c)); for (auto& v : q) { (void)v; }
               std::vector<Vec2> q2; for (int k = 0; k < 4; ++k) { q2.push_back(lerp((a0 + b0) * 0.5f + nn * (k < 2 ? -h * 0.4f : h * 0.4f), (k == 0 || k == 3) ? a0 : b0, 0.92f)); }
               q2 = {a0 - nn * h * 0.4f + (b0 - a0) * 0.04f, b0 - nn * h * 0.4f - (b0 - a0) * 0.04f, b0 + nn * h * 0.4f - (b0 - a0) * 0.04f, a0 + nn * h * 0.4f + (b0 - a0) * 0.04f};
               fillPoly(*c.s, q2, shade(tint(C1(c), hsv(0.05f, 0.5f, 0.6f), 0.4f * c.rnd(row + 5)), 0.85f + 0.3f * c.rnd(row + 9))); } };
           return a; });

  addArc(lib, mkInfo("decor.cerca", "Cerca de madeira", "decor",
      "Na vida real: estacas verticais de topo pontudo presas por duas travessas horizontais. Modelo: estacas com alturas ligeiramente irregulares e sombra lateral; travessas contínuas na altura de 30% e 70%.", 6, Color{0.62f, 0.45f, 0.28f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 2.1f; };
           a.emit = [](Ctx& c) { float w = c.size * 0.8f, h = c.size * (4.2f + 0.8f * c.rnd()); Vec2 b0 = c.p;
             if (c.idx > 0) for (float f : {0.3f, 0.68f}) { seg(*c.s, c.prev - Vec2{0, h * f}, c.p - Vec2{0, h * f}, c.size * 0.35f, c.size * 0.35f, shade(C1(c), 0.8f)); }
             Color col = shade(C1(c), 0.9f + 0.2f * c.rnd(2));
             std::vector<Vec2> q = {b0 + Vec2{-w, 0}, b0 + Vec2{-w, -h}, b0 + Vec2{0, -h - w * 0.9f}, b0 + Vec2{w, -h}, b0 + Vec2{w, 0}};
             fillPoly(*c.s, q, col); std::vector<Vec2> sh = {b0 + Vec2{w * 0.3f, 0}, b0 + Vec2{w * 0.3f, -h - w * 0.15f}, b0 + Vec2{w, -h}, b0 + Vec2{w, 0}}; fillPoly(*c.s, sh, shade(col, 0.72f));
             seg(*c.s, b0 + Vec2{-w * 0.5f, -h * 0.9f}, b0 + Vec2{-w * 0.5f, -h * 0.1f}, 0.5f, 0.5f, shade(col, 0.8f)); };
           return a; });

  addArc(lib, mkInfo("decor.bambu", "Bambu", "decor",
      "Na vida real: colmo cilíndrico verde dividido em gomos por nós salientes; de alguns nós saem ramos com folhas finas. Modelo: tubo sombreado seguindo o traço, anéis nos nós a intervalos e folhas lanceoladas.", 6, Color{0.4f, 0.62f, 0.25f, 1}, Color{0.25f, 0.5f, 0.2f, 1}),
      [] { ArcSpec a; a.spacing = [](float) { return 2.f; };
           a.emit = [](Ctx& c) { float r = c.size; float s0 = std::max(0.f, c.arc - c.step);
             seg(*c.s, P(c, s0), P(c, c.arc), r, r, shade(C1(c), 0.65f)); seg(*c.s, P(c, s0, -r * 0.18f), P(c, c.arc, -r * 0.18f), r * 0.65f, r * 0.65f, C1(c)); seg(*c.s, P(c, s0, -r * 0.4f), P(c, c.arc, -r * 0.4f), r * 0.14f, r * 0.14f, tint(C1(c), white, 0.55f), 0.8f);
             float seg_len = c.size * 8.f; float m = std::fmod(c.arc, seg_len);
             if (m < 2.f) { seg(*c.s, c.p - c.nrm * r * 1.15f, c.p + c.nrm * r * 1.15f, r * 0.24f, r * 0.24f, shade(C1(c), 0.55f)); seg(*c.s, c.p - c.nrm * r, c.p + c.nrm * r, r * 0.1f, r * 0.1f, tint(C1(c), white, 0.3f));
               if (c.rnd((int)(c.arc / seg_len)) > 0.4f) { int sgn = c.rnd((int)(c.arc / seg_len) + 3) > 0.5f ? 1 : -1; Vec2 st = c.p + c.nrm * r * (float)sgn;
                 for (int k = 0; k < 3; ++k) { Vec2 d = rotate(c.nrm * (float)sgn, (k - 1) * 0.5f - 0.3f * c.dir.x) ; std::vector<Vec2> lf = {st, st + d * r * 2.f + rotate(d, 1.57f) * r * 0.5f, st + d * r * 5.f, st + d * r * 2.f - rotate(d, 1.57f) * r * 0.5f}; fillPoly(*c.s, lf, C2(c)); } } } };
           return a; });

  addArc(lib, mkInfo("decor.meandro-grego", "Meandro grego", "decor",
      "Na vida real: friso grego em chave contínua (meandro), geometria ortogonal repetida de ritmo exato. Modelo: padrão de chave em unidades locais ancorado no comprimento de arco, ligado ao próximo módulo.", 3, Color{0.75f, 0.58f, 0.15f, 1}, white),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 8.f; };
           a.emit = [](Ctx& c) { float u = c.step; Vec2 o = P(c, c.arc), d = D(c, c.arc), n = d.perp();
             std::vector<Vec2> pat = {{0, 0.5f}, {0, -0.5f}, {0.75f, -0.5f}, {0.75f, 0.25f}, {0.25f, 0.25f}, {0.25f, -0.25f}, {0.5f, -0.25f}, {0.5f, 0.0f}, {0.5f, 0.0f}};
             std::vector<Vec2> v; for (auto& q : pat) v.push_back(local(o, d, n, q.x * u, q.y * u * 0.9f)); poly(*c.s, v, c.size * 0.35f, c.size * 0.35f, C1(c));
             std::vector<Vec2> conn = {local(o, d, n, 0, 0.5f * u * 0.9f), local(o, d, n, u, 0.5f * u * 0.9f)}; poly(*c.s, conn, c.size * 0.35f, c.size * 0.35f, C1(c)); };
           return a; });

  addArc(lib, mkInfo("decor.voluta", "Voluta / floreio", "decor",
      "Na vida real: ornamento barroco com espirais (volutas) alternadas dos dois lados do eixo e folhinhas. Modelo: espiral logarítmica a cada passo, lado alternado, com ponto-folha no centro.", 3, Color{0.35f, 0.18f, 0.1f, 1}, Color{0.2f, 0.45f, 0.2f, 1}),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float s) { return s * 6.5f; };
           a.emit = [](Ctx& c) { float sd = (c.idx % 2) ? 1.f : -1.f; float R = c.size * 3.f; Vec2 ctr = P(c, c.arc + c.step * 0.5f, sd * R * 0.95f); Vec2 start = P(c, c.arc, 0);
             std::vector<Vec2> sp; float a0 = std::atan2(start.y - ctr.y, start.x - ctr.x);
             for (int i = 0; i <= 40; ++i) { float t = i / 40.f; float r = R * 0.95f * (1.f - 0.82f * t); sp.push_back(ctr + fromAngle(a0 + sd * -1.f * t * 5.2f, r)); }
             poly(*c.s, sp, c.size * 0.45f, c.size * 0.15f, C1(c)); disc(*c.s, sp.back(), c.size * 0.45f, C1(c));
             seg(*c.s, P(c, std::max(0.f, c.arc - c.step * 0.5f)), P(c, c.arc + c.step * 0.5f), c.size * 0.3f, c.size * 0.3f, C1(c));
             Vec2 lp = ctr + (start - ctr) * 0.3f; std::vector<Vec2> lf = {lp, lp + c.nrm * sd * c.size * 1.2f + c.dir * c.size * 1.3f, lp + c.nrm * sd * c.size * 2.4f, lp + c.nrm * sd * c.size * 1.2f - c.dir * c.size * 0.2f}; fillPoly(*c.s, lf, C2(c)); };
           return a; });

  addArc(lib, mkInfo("decor.seta", "Seta", "decor",
      "Na vida real: seta de sinalização com haste e ponta triangular alinhada à última direção do movimento. Modelo: haste desenhada ao longo do traço; ponta calculada no fim a partir da tangente final.", 3, ink, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1.5f; };
           a.emit = [](Ctx& c) { stepLine(c, 0, W(c) * 0.8f, W(c) * 0.8f, C1(c)); };
           a.finish = [](Ctx& c) { Vec2 d = D(c, c.arc), n = d.perp(); float L = c.size * 5.f; Vec2 t = c.p + d * L * 0.5f;
             std::vector<Vec2> head = {t, c.p - d * L * 0.5f + n * L * 0.55f, c.p - d * L * 0.2f, c.p - d * L * 0.5f - n * L * 0.55f}; fillPoly(*c.s, head, C1(c)); };
           return a; });

  addArc(lib, mkInfo("decor.chave-curva", "Chave { } entre extremos", "decor",
      "Na vida real: chave agrupa itens: dois quartos de círculo, ponta central e hastes. Modelo: calculada no fim entre início e fim do traço, abre para o lado do arrasto.", 3, ink, white),
      [] { ArcSpec a; a.spacing = [](float) { return 1e9f; };
           a.finish = [](Ctx& c) { Vec2 A = c.first, B = c.p; float L = dist(A, B); if (L < 8) return; Vec2 d = (B - A) / L, n = d.perp(); float w = std::min(c.size * 3.f, L * 0.2f);
             std::vector<Vec2> v; auto q = [&](float u, float vv) { return A + d * u + n * vv; };
             for (int i = 0; i <= 12; ++i) { float t = i / 12.f * kPi * 0.5f; v.push_back(q(L * 0.5f - w * 1.2f * std::sin(t) - 0, 0)); (void)t; }
             v.clear(); for (int i = 0; i <= 14; ++i) { float t = i / 14.f * kPi / 2; v.push_back(q(w * 2 * (1 - std::cos(t)) * 0 + (L * 0.5f - w) * (float)i / 14.f * 0 + std::sin(t) * 0, 0)); }
             v.clear();
             for (int i = 0; i <= 12; ++i) { float t = i / 12.f * kPi * 0.5f; v.push_back(q(w * (1 - std::cos(t)) , w * std::sin(t) * -1.f * 0 + 0)); }
             v.clear();
             std::vector<Vec2> p1; for (int i = 0; i <= 12; ++i) { float t = i / 12.f * kPi * 0.5f; p1.push_back(q(w * std::sin(t) * 0.f + (1 - std::cos(t)) * 0.f + 0.f, 0.f)); }
             // brace: A→(arc)→mid-start→mid tip→mid-end→(arc)→B
             Vec2 ctr = (A + B) * 0.5f; std::vector<Vec2> br;
             for (int i = 0; i <= 10; ++i) { float t = i / 10.f * kPi * 0.5f; br.push_back(q(w * (1 - std::cos(t)) * 0.0f + w * std::sin(t) * 0.0f + 0, 0)); }
             br.clear();
             br.push_back(A);
             for (int i = 1; i <= 10; ++i) { float t = i / 10.f * kPi * 0.5f; br.push_back(q(w * std::sin(t), w - w * std::cos(t) )); }
             for (int i = 1; i <= 10; ++i) { float t = i / 10.f * kPi * 0.5f; br.push_back(q(L * 0.5f - w * 1.0f + w * (1 - std::cos(t)) * 0.0f + w * (float)i / 10.f * 0.0f + (L * 0.5f - 2 * w - 0) * 0.0f, w * 2 + w * 0.f * t)); }
             br.clear(); br.push_back(A);
             for (int i = 0; i <= 8; ++i) { float t = i / 8.f * kPi * 0.5f; br.push_back(q(w * 0.2f * i / 8.f + 0.f, 0.f) ); (void)t; }
             (void)ctr; (void)p1;
             // clean construction
             br.clear();
             auto arcPts = [&](Vec2 cc, float a0, float a1, std::vector<Vec2>& out) { for (int i = 0; i <= 10; ++i) out.push_back(cc + d * (std::cos(a0 + (a1 - a0) * i / 10.f) * w) + n * (std::sin(a0 + (a1 - a0) * i / 10.f) * w)); };
             Vec2 m = ctr; arcPts(A + d * w + n * w * 0.f, kPi, kPi * 0.5f, br);   // lower-left corner curve going inward
             arcPts(m - d * w + n * w * 2.f, -kPi * 0.5f, 0, br);
             arcPts(m + d * w + n * w * 2.f, kPi, kPi * 1.5f, br);
             arcPts(B - d * w + n * w * 0.f, kPi * 0.5f, 0, br);
             poly(*c.s, br, c.size * 0.4f, c.size * 0.4f, C1(c)); };
           return a; });

  // ---------------- hachuras / texturas ----------------
  addArc(lib, mkInfo("decor.hachura", "Hachura", "decor",
      "Na vida real: desenhista sombreia com riscos paralelos curtos; mais pressão = traços mais próximos/longos e a mão sempre tremula um pouco. Modelo: riscos num ângulo fixo (45°), comprimento e opacidade por pressão, tremor aleatório controlado.", 2, ink, white, "wave", "paper"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.7f; };
           a.emit = [](Ctx& c) { float ang = c.P("angulo") * kDeg2Rad + c.rng->sym() * 0.08f; float L = c.size * (3.f + 6.f * c.pr) * (0.8f + 0.4f * c.rng->uniform());
             Vec2 d = fromAngle(ang); Vec2 o = c.p + c.nrm * (c.rng->sym() * c.size * 0.4f); seg(*c.s, o - d * L * 0.5f, o + d * L * 0.5f, c.size * 0.12f + 0.2f, c.size * 0.05f + 0.1f, C1(c), 0.55f + 0.4f * c.pr); };
           return a; },
      {{"angulo", "Ângulo", -50, -90, 90}});

  addArc(lib, mkInfo("decor.hachura-cruzada", "Hachura cruzada", "decor",
      "Na vida real: ao pressionar mais o artista cruza uma segunda camada de riscos, escurecendo. Modelo: camada 1 sempre; camada 2 (ângulo oposto) só aparece acima de ~45% de pressão; terceira acima de 80%.", 2, ink, white, "wave", "paper"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.9f; };
           a.emit = [](Ctx& c) { int layers = 1 + (c.pr > 0.45f) + (c.pr > 0.8f); float L = c.size * 5.f;
             for (int k = 0; k < layers; ++k) { float ang = (k == 0 ? -50.f : (k == 1 ? 40.f : 90.f)) * kDeg2Rad + c.rng->sym() * 0.07f; Vec2 d = fromAngle(ang); Vec2 o = c.p + c.nrm * (c.rng->sym() * c.size * 0.5f);
               seg(*c.s, o - d * L * 0.5f, o + d * L * 0.5f, c.size * 0.1f + 0.2f, c.size * 0.05f + 0.1f, C1(c), 0.75f); } };
           return a; });

  addArc(lib, mkInfo("decor.hachura-contorno", "Hachura de contorno", "decor",
      "Na vida real: hachura que acompanha a forma (curva de nível) para sugerir volume. Modelo: riscos perpendiculares à direção local do traço, levemente curvados para o mesmo lado.", 2, ink, white, "wave", "paper"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.5f; };
           a.emit = [](Ctx& c) { float L = c.size * (3.f + 5.f * c.pr); std::vector<Vec2> v; for (int i = -4; i <= 4; ++i) { float t = i / 4.f; v.push_back(c.p + c.nrm * (t * L * 0.5f) + c.dir * (t * t * L * 0.12f)); }
             poly(*c.s, v, c.size * 0.1f + 0.2f, c.size * 0.06f + 0.1f, C1(c), 0.8f); };
           return a; });

  addArc(lib, mkInfo("decor.pontilhismo", "Pontilhismo (estípula)", "decor",
      "Na vida real: nanquim em pontos; a densidade de pontos cria os tons, e pontos não se sobrepõem muito (distribuição azarada de Poisson). Modelo: N pontos por passo em disco de raio ~2.5r, com N crescendo com a pressão e rejeição de pontos muito próximos dos últimos.", 3, ink, white, "wave", "paper"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.5f; };
           a.emit = [](Ctx& c) { int n = 1 + (int)(c.pr * 4.f); for (int k = 0; k < n; ++k) { Vec2 p = c.p + c.rng->inDisc() * (c.size * 2.8f);
               bool ok = true; for (size_t j = 0; j + 1 < c.st->size(); j += 2) { float dx = (*c.st)[j] - p.x, dy = (*c.st)[j + 1] - p.y; if (dx * dx + dy * dy < sqr(c.size * 0.55f)) { ok = false; break; } }
               if (!ok) continue; disc(*c.s, p, c.size * (0.16f + 0.22f * c.rng->uniform()), C1(c), 0.9f); c.st->push_back(p.x); c.st->push_back(p.y); if (c.st->size() > 160) c.st->erase(c.st->begin(), c.st->begin() + 2); } };
           return a; });

  // halftone helper used by decor + manga: canvas-anchored rotated grid
  struct Halftone { std::map<long long, float> done; };
  addArc(lib, mkInfo("decor.retícula-meio-tom", "Retícula de meio-tom", "decor",
      "Na vida real: impressão offset usa pontos de tamanho variável numa grade rotacionada a 45°; o tom vem da área do ponto. Modelo: grade ancorada na tela (pontos de strokes diferentes alinham), raio do ponto = pressão×falloff do pincel; reaplicar só aumenta o ponto.", 14, ink, white, "wave", "white"),
      [] { auto h = std::make_shared<Halftone>(); ArcSpec a; a.spacing = [](float s) { return s * 0.3f; }; a.begin = [h](Ctx&) { h->done.clear(); };
           a.emit = [h](Ctx& c) { float pitch = c.P("passo"); float ang = 45.f * kDeg2Rad; float ca = std::cos(ang), sa = std::sin(ang); float R = c.size;
             int span = (int)(R / pitch) + 2; float u0 = c.p.x * ca + c.p.y * sa, v0 = -c.p.x * sa + c.p.y * ca; int iu = (int)std::round(u0 / pitch), iv = (int)std::round(v0 / pitch);
             for (int j = -span; j <= span; ++j) for (int i = -span; i <= span; ++i) { int gu = iu + i, gv = iv + j; float u = gu * pitch, v = gv * pitch; Vec2 g{u * ca - v * sa, u * sa + v * ca};
               float d = dist(g, c.p); if (d > R) continue; float fall = 1.f - smoothstep(0.35f, 1.f, d / R); float rad = pitch * 0.72f * std::sqrt(c.pr * fall + 0.0001f); if (rad < 0.35f) continue;
               long long key = (long long)gu * 100003LL + gv; float& dn = h->done[key]; if (rad <= dn + 0.05f) continue; dn = rad; disc(*c.s, g, rad, C1(c), 1.f); } };
           return a; },
      {{"passo", "Passo da grade (px)", 6, 3, 20}});

  addArc(lib, mkInfo("decor.areia", "Granulado de areia", "decor",
      "Na vida real: grãos minerais de tamanhos e tons ligeiramente diferentes espalhados sem padrão. Modelo: grãos minúsculos aleatórios em disco, tom variando em torno da cor base (claro/escuro).", 12, Color{0.78f, 0.66f, 0.45f, 1}, white, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.4f; };
           a.emit = [](Ctx& c) { for (int k = 0; k < 7; ++k) { Vec2 p = c.p + c.rng->inDisc() * c.size; float t = c.rng->uniform(); Color col = shiftHSV(C1(c), c.rng->sym() * 0.02f, c.rng->sym() * 0.1f, (t - 0.5f) * 0.35f); disc(*c.s, p, 0.45f + 0.7f * c.rng->uniform() * c.rng->uniform(), col, 0.9f, 0.9f); } };
           return a; });

  addArc(lib, mkInfo("decor.riscos", "Riscos e arranhões", "decor",
      "Na vida real: arranhões em superfície são linhas finas, quase retas, de comprimento aleatório, uns mais claros (expõem o material) e outros escuros. Modelo: riscos longos finos com ligeira curvatura, direção alinhada ao movimento com desvio, tom claro ou escuro.", 1, Color{0.9f, 0.9f, 0.9f, 1}, Color{0.1f, 0.1f, 0.1f, 1}, "wave", "paper"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 5.f; };
           a.emit = [](Ctx& c) { float L = c.size * (6.f + 24.f * c.rng->uniform()); float ang = c.dir.angle() + c.rng->sym() * 0.35f; Vec2 d = fromAngle(ang); Vec2 o = c.p + c.nrm * (c.rng->sym() * c.size * 4.f);
             Color col = c.rng->chance(0.5f) ? C1(c) : C2(c); std::vector<Vec2> v; float bend = c.rng->sym() * 0.15f; for (int i = 0; i <= 8; ++i) { float t = i / 8.f - 0.5f; v.push_back(o + d * (t * L) + d.perp() * (t * t * L * bend)); }
             poly(*c.s, v, 0.15f + 0.4f * c.rng->uniform(), 0.05f, col, 0.35f + 0.5f * c.rng->uniform()); };
           return a; });

  addArc(lib, mkInfo("decor.vidro-quebrado", "Vidro trincado", "decor",
      "Na vida real: o impacto cria trincas radiais que partem do ponto e trincas concêntricas poligonais que unem as radiais; algumas radiais se bifurcam. Modelo: 9-14 radiais com comprimento variável e bifurcações, anéis poligonais irregulares ligando raios vizinhos, brilho branco nas arestas.", 2, Color{0.92f, 0.96f, 1.f, 1}, Color{0.1f, 0.1f, 0.15f, 1}, "dot", "night"),
      [] { ArcSpec a; a.emitAtBegin = true; a.spacing = [](float) { return 1e9f; };
           a.emit = [](Ctx& c) { int n = 10 + (int)(c.rng->uniform() * 5); float Rmax = c.size * 14.f; std::vector<float> ang(n), len(n);
             for (int i = 0; i < n; ++i) { ang[i] = (i + c.rng->sym() * 0.3f) * kTau / n; len[i] = Rmax * (0.5f + 0.5f * c.rng->uniform()); }
             for (int i = 0; i < n; ++i) { Vec2 p = c.p; std::vector<Vec2> v{p}; float steps = len[i] / (c.size * 1.6f); float an = ang[i];
               for (int s = 1; s <= (int)steps; ++s) { an += c.rng->sym() * 0.08f; p += fromAngle(an, c.size * 1.6f); v.push_back(p);
                 if (c.rng->chance(0.12f)) { std::vector<Vec2> br{p}; Vec2 q = p; float ab = an + (c.rng->chance(0.5f) ? 0.5f : -0.5f); for (int t = 0; t < 4; ++t) { ab += c.rng->sym() * 0.2f; q += fromAngle(ab, c.size * 1.5f); br.push_back(q); } poly(*c.s, br, c.size * 0.2f, c.size * 0.04f, C1(c), 0.8f); } }
               poly(*c.s, v, c.size * 0.3f, c.size * 0.04f, C1(c), 0.9f); poly(*c.s, v, c.size * 0.1f, c.size * 0.02f, Color{1, 1, 1, 1}, 0.9f); }
             for (float rr = Rmax * 0.18f; rr < Rmax * 0.95f; rr *= 1.55f) { std::vector<Vec2> ring; for (int i = 0; i < n; ++i) { float r2 = rr * (0.85f + 0.3f * c.rng->uniform()); ring.push_back(c.p + fromAngle(ang[i], r2)); }
               poly(*c.s, ring, c.size * 0.18f, c.size * 0.18f, C1(c), 0.8f, Blend::Normal, true); }
             disc(*c.s, c.p, c.size * 1.8f, C2(c), 0.5f, 0.9f); disc(*c.s, c.p, c.size * 0.8f, C1(c), 0.9f); };
           return a; });

  addArc(lib, mkInfo("decor.bolhas-sabao", "Bolhas de sabão (aglomerado)", "decor",
      "Na vida real: filme fino de sabão é quase transparente, com contorno irisado e dois reflexos (janela e ponto) em lados opostos. Modelo: disco quase transparente, anel com matiz variando em volta (interferência), brilho principal e reflexo secundário, tamanhos variados.", 8, Color{0.6f, 0.85f, 1.f, 1}, white, "wave", "dark"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.1f; };
           a.emit = [](Ctx& c) { if (c.rng->uniform() > 0.55f) return; float r = c.size * (0.35f + 1.5f * c.rng->uniform() * c.rng->uniform()); Vec2 p = c.p + c.rng->inDisc() * c.size * 2.5f;
             disc(*c.s, p, r, Color{0.7f, 0.85f, 1.f, 1}, 0.8f, 0.12f);
             for (int i = 0; i < 18; ++i) { float a0 = i * kTau / 18, a1 = (i + 1.2f) * kTau / 18; Color col = hsv(std::fmod(i / 18.f + c.rng->uniform() * 0.2f, 1.f), 0.55f, 1.f); poly(*c.s, circlePts(p, r, r, 0, 3, a0, a1), r * 0.07f, r * 0.07f, col, 0.55f, Blend::Add); }
             Vec2 hl = p + Vec2{-0.45f, -0.5f} * r; ellipseDab(*c.s, hl, r * 0.35f, r * 0.17f, -0.7f, white, 0.6f, 0.85f); disc(*c.s, p + Vec2{0.5f, 0.45f} * r, r * 0.09f, white, 0.5f, 0.6f); };
           return a; });

  addArc(lib, mkInfo("decor.brilho-glitter", "Glitter", "decor",
      "Na vida real: lantejoulas refletem a luz em lampejos pontuais; cada partícula brilha em momento/cor diferente. Modelo: estrelas de 4 pontas de tamanhos variados com matiz aleatório; brilho depende do quadro (cintilação).", 6, white, white, "wave", "dark"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.9f; };
           a.emit = [](Ctx& c) { if (c.rng->uniform() > 0.6f) return; Vec2 p = c.p + c.rng->inDisc() * c.size * 2.2f; float tw = 0.55f + 0.45f * std::sin(c.frame * 2.3f + c.rnd() * 20.f); float r = c.size * (0.3f + 1.4f * c.rng->uniform() * c.rng->uniform()) * tw;
             Color col = tint(hsv(c.rng->uniform(), 0.4f, 1.f), white, 0.35f); glow(*c.s, p, r * 1.3f, col, 0.45f); star4(*c.s, p, r * 1.6f, col, 1.f, Blend::Add, c.rng->uniform() * 0.4f); disc(*c.s, p, r * 0.22f, white, 0.8f, 1.f, Blend::Add); };
           return a; });
  lib.find("decor.brilho-glitter");

  addArc(lib, mkInfo("decor.confete", "Confete", "decor",
      "Na vida real: papelzinhos coloridos giram em 3D ao cair; vistos de lado parecem finos, de frente mostram a área toda. Modelo: retângulos com largura = |cos(rotação 3D)|, cor da paleta, ângulo e fase aleatórios.", 6, white, white, "wave", "white"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 0.8f; };
           a.emit = [](Ctx& c) { if (c.rng->uniform() > 0.55f) return; Vec2 p = c.p + c.rng->inDisc() * c.size * 3.f; float rot = c.rng->uniform() * kTau, flip = std::fabs(std::cos(c.rng->uniform() * kPi)); Color col = paletteColor(c.rng->irange(0, 7));
             float w = c.size * 0.9f, h = c.size * 0.45f * (0.3f + 0.7f * flip); std::vector<Vec2> q; for (Vec2 v : {Vec2{-w, -h}, Vec2{w, -h}, Vec2{w, h}, Vec2{-w, h}}) q.push_back(p + rotate(v, rot));
             fillPoly(*c.s, q, shade(col, 0.75f + 0.35f * flip)); };
           return a; });

  addArc(lib, mkInfo("decor.estrelas-espalhadas", "Estrelas espalhadas", "decor",
      "Na vida real: estrelinhas de 5 pontas de tamanhos diversos como adesivos. Modelo: polígono estrelado regular com rotação aleatória, pequeno brilho e cores quentes.", 7, Color{1.f, 0.85f, 0.2f, 1}, white, "wave", "dark"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.6f; };
           a.emit = [](Ctx& c) { Vec2 p = c.p + c.rng->inDisc() * c.size * 1.5f; float r = c.size * (0.5f + c.rng->uniform()); float rot = c.rng->uniform() * kTau; std::vector<Vec2> v;
             for (int i = 0; i < 10; ++i) v.push_back(p + fromAngle(rot + i * kPi / 5, (i & 1) ? r * 0.42f : r)); Color col = shiftHSV(C1(c), c.rng->sym() * 0.05f, 0, c.rng->sym() * 0.1f); fillPoly(*c.s, v, shade(col, 0.8f)); std::vector<Vec2> in; for (auto& q : v) in.push_back(p + (q - p) * 0.72f); fillPoly(*c.s, in, tint(col, white, 0.25f)); };
           return a; });

  addArc(lib, mkInfo("decor.constelacao", "Constelação", "decor",
      "Na vida real: estrelas ligadas por linhas finas formam figuras; as linhas ligam estrelas próximas, não todas. Modelo: estrelas (com brilho) ao longo do traço; cada nova estrela liga-se por fio tênue às 2 anteriores se estiverem a menos de ~9 raios.", 4, Color{0.85f, 0.92f, 1.f, 1}, white, "wave", "night"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 5.f; };
           a.emit = [](Ctx& c) { Vec2 p = c.p + c.nrm * (c.rng->sym() * c.size * 2.f); float r = c.size * (0.35f + 0.8f * c.rng->uniform());
             int n = (int)c.st->size() / 2; for (int k = std::max(0, n - 2); k < n; ++k) { Vec2 q{(*c.st)[k * 2], (*c.st)[k * 2 + 1]}; if (dist(p, q) < c.size * 9.f) seg(*c.s, q, p, 0.35f, 0.35f, C1(c), 0.55f, Blend::Add); }
             c.st->push_back(p.x); c.st->push_back(p.y); glow(*c.s, p, r * 3.f, C1(c), 0.3f); star4(*c.s, p, r * 2.2f, C1(c), 0.9f, Blend::Add); disc(*c.s, p, r * 0.5f, white, 0.8f, 1.f); };
           return a; });

  addArc(lib, mkInfo("decor.rastro-brilhante", "Rastro de brilho", "decor",
      "Na vida real: varinha mágica deixa faíscas que flutuam e se apagam; as primeiras ficam e as novas surgem na ponta. Modelo: glints de 4 pontas com deslocamento aleatório da trajetória e tamanho decrescente com o frame de vida.", 6, Color{1, 0.9f, 0.5f, 1}, white, "wave", "dark"),
      [] { ArcSpec a; a.spacing = [](float s) { return s * 1.3f; };
           a.emit = [](Ctx& c) { for (int k = 0; k < 2; ++k) { Vec2 p = c.p + c.rng->inDisc() * c.size * 1.8f + Vec2{0, c.rng->uniform() * c.size * 2.f}; float r = c.size * (0.2f + 0.9f * c.rng->uniform() * c.rng->uniform());
               glow(*c.s, p, r * 2.2f, C1(c), 0.4f); star4(*c.s, p, r * 1.8f, tint(C1(c), white, 0.5f), 1.f, Blend::Add, c.rng->uniform()); } };
           return a; });
}

}  // namespace dfx
}  // namespace ab
