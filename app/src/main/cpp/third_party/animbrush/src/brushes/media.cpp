// Module "media": traditional media simulated with the DabBrush engine (graphite, charcoal, pastel, inks, markers,
// airbrush, blending) + one brush per zip mask of pen/pencil/chalk/air/gauze/oil (zip.*). Watercolour / bristle / spray
// brushes with custom behaviour live in media_custom.cpp.
#include "media_util.hpp"

namespace ab { namespace media {

void addDab(BrushLibrary& lib, BrushInfo info, DabBrushSpec spec, std::vector<PDef> pd, Tweak t) {
  BrushInfo copy = info;
  lib.add(std::move(info), [copy, spec, pd, t]() -> std::unique_ptr<Brush> { return std::make_unique<ParamDab>(copy, spec, pd, t); });
}

}}  // namespace ab::media

namespace ab {
using namespace media;

static void K(DabBrushSpec& s, Set st, In in, float a, float b) { s[st].link(in, Curve::linear(a, b)); }
static BrushInfo zipInfo(BrushInfo b, const char* mask) { b.stampName = mask; return b; }

void registerMediaBrushes(BrushLibrary& lib) {
  const Color graphite{0.17f, 0.17f, 0.2f, 1}, charcoal{0.07f, 0.07f, 0.07f, 1}, ink{0.03f, 0.03f, 0.06f, 1};
  const PDef texP{"textura", "Textura do papel", 1.f, 0.f, 1.f};
  auto texTweak = [](DabBrushSpec& s, const Brush& b) { s[Set::GrainDepth].base = b.param("textura"); };

  // =============================== LÁPIS ===============================
  auto pencil = [&](const char* id, const char* name, const char* real, const char* model, float size, float radLo, float hard,
                    float opHi, float grainD, float spacing, Color col) {
    DabBrushSpec s; s[Set::Hardness].set(hard); s[Set::Spacing].set(spacing);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(radLo, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(opHi * 0.2f, opHi));
    s[Set::GrainDepth].set(grainD); s.grainName = "@paper"; s.pressureOpacityGrainBias = true;
    s[Set::JitterPos].set(0.04f);
    BrushInfo b = mk(id, name, "lapis", real, model, size, col, 0.5f, 40);
    addDab(lib, b, s, {texP}, texTweak);
  };
  pencil("lapis.9h", "Lápis 9H (duro)", "grafite muito duro: risco fino, claro e quase uniforme, mal enche os vales do papel.",
         "ponta pequena e dura, opacidade baixa, dente do papel quase integral.", 3, 0.7f, 1.f, 0.35f, 1.f, 0.08f, Color{0.3f, 0.3f, 0.34f, 1});
  pencil("lapis.hb", "Lápis HB", "grafite médio: risco equilibrado; a pressão aprofunda o grafite nos vales do papel.",
         "pressão controla opacidade e o limiar do dente do papel (grainBias).", 4, 0.55f, 0.9f, 0.7f, 0.9f, 0.1f, graphite);
  pencil("lapis.2b", "Lápis 2B", "grafite macio: risco escuro, aveludado, mancha fácil.",
         "ponta mais larga e macia; menos dependência do papel.", 5, 0.5f, 0.75f, 0.85f, 0.65f, 0.1f, Color{0.12f, 0.12f, 0.15f, 1});
  pencil("lapis.6b", "Lápis 6B", "grafite muito macio: preto profundo, largo, quase pastoso.",
         "ponta larga, opacidade alta, pouco dente.", 7, 0.5f, 0.55f, 0.95f, 0.4f, 0.1f, Color{0.08f, 0.08f, 0.1f, 1});
  {  // mechanical pencil
    DabBrushSpec s; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.07f); s[Set::Radius].set(0.5f);
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.35f, 0.9f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(0.6f); s.pressureOpacityGrainBias = true;
    addDab(lib, mk("lapis.porta-minas", "Lapiseira", "lapis", "mina fina de diâmetro constante; traço uniforme e nítido, só a pressão muda o tom.",
                   "raio constante, opacidade pela pressão, dente leve.", 2.f, graphite, 0.5f, 10), s);
  }
  {  // tilt shading
    DabBrushSpec s; s[Set::Hardness].set(0.7f); s[Set::Spacing].set(0.12f);
    s[Set::Aspect].set(1).link(In::TiltDec, Curve::linear(0, 7.f));
    s[Set::AngleDeg].set(0).link(In::TiltAsc, Curve::linear(-180.f, 180.f));
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.6f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.1f, 0.6f)).link(In::TiltDec, Curve::linear(0, -0.15f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(1.f); s.pressureOpacityGrainBias = true;
    BrushInfo b = mk("lapis.sombreado-inclinado", "Lápis de sombrear (inclinado)",
                     "lapis", "deitar o lápis usa a lateral do grafite: faixa larga e macia, mais clara quanto mais deitado.",
                     "inclinação da caneta (tilt) alonga a pegada (aspect) e orienta pela direção da inclinação.", 6, graphite, 2, 50);
    addDab(lib, b, s, {texP}, texTweak);
  }
  {  // coloured pencil
    DabBrushSpec s; s[Set::Hardness].set(0.85f); s[Set::Spacing].set(0.08f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.65f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.12f, 0.75f));
    s.grainName = "@canvas"; s[Set::GrainDepth].set(0.8f); s.pressureOpacityGrainBias = true; s.grainScale = 0.9f;
    s[Set::HueShift].set(0).link(In::Random, Curve{{0, -0.004f}, {1, 0.004f}});
    s[Set::ValShift].set(0).link(In::Random, Curve{{0, -0.03f}, {1, 0.03f}});
    BrushInfo b = mk("lapis.colorido", "Lápis de cor", "lapis", "cera + pigmento: camadas sobrepostas ficam mais saturadas, o papel aparece nos vales, polir com pressão fecha o grão.",
                     "opacidade baixa acumulativa, grão de trama, pequena variação de matiz/valor por marca.", 5, Color{0.85f, 0.2f, 0.25f, 1}, 1, 40);
    addDab(lib, b, s, {texP}, texTweak);
  }
  {  // charcoal pencil
    DabBrushSpec s; s[Set::Hardness].set(0.7f); s[Set::Spacing].set(0.1f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.5f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.25f, 0.95f));
    s.grainName = "@rough"; s[Set::GrainDepth].set(0.9f); s.pressureOpacityGrainBias = true; s[Set::JitterPos].set(0.12f);
    s[Set::Dissolve].set(0.05f);
    addDab(lib, mk("lapis.carvao", "Lápis carvão", "lapis", "carvão prensado em madeira: preto fosco, grão áspero, falha ao ser arrastado leve.",
                   "grão grosso, jitter de posição e pequenos buracos.", 5, charcoal), s);
  }
  {  // sketch pencil hatching feel: lighter when fast
    DabBrushSpec s; s[Set::Hardness].set(0.9f); s[Set::Spacing].set(0.1f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.5f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.15f, 0.8f)).link(In::Speed1, Curve::linear(0, -0.35f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(1.f); s.pressureOpacityGrainBias = true; s[Set::JitterPos].set(0.1f);
    s[Set::SlowTracking].set(0);
    addDab(lib, mk("lapis.esboco", "Lápis de esboço", "lapis", "traços rápidos saem leves e soltos, devagar escurecem.",
                   "opacidade cai com a velocidade (speed1), leve tremor.", 4, Color{0.25f, 0.25f, 0.3f, 1}), s);
  }
  // zip pencils
  {
    DabBrushSpec s; s.stamp.maskName = "pencil_graphite"; s.stamp.rot = RotMode::Random; s[Set::Spacing].set(0.35f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.6f, 1.f)); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.2f, 0.7f));
    s[Set::Hardness].set(1.f); s.grainName = "@paper"; s[Set::GrainDepth].set(0.6f); s[Set::JitterPos].set(0.15f);
    addDab(lib, zipInfo(mk("zip.pencil_graphite", "Grafite texturizado", "lapis",
                           "ponta de grafite larga com granulação própria (a textura da ponta é irregular, nunca uma bolinha lisa).",
                           "carimbo de grafite girado ao acaso para não repetir o padrão, pressão modula opacidade.", 12, graphite, 3, 60), "pencil_graphite"), s);
  }
  {
    DabBrushSpec s; s.stamp.maskName = "pencil_charcoal"; s.stamp.rot = RotMode::DirectionPerp; s.stamp.rotJitterDeg = 8;
    s[Set::Spacing].set(0.25f); s[Set::Hardness].set(1.f); s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.5f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.2f, 0.9f)); s.grainName = "@fibers"; s[Set::GrainDepth].set(0.7f);
    addDab(lib, zipInfo(mk("zip.pencil_charcoal", "Carvão em cunha", "lapis",
                           "pedaço de carvão quebrado: ponta triangular facetada, deixa uma faixa de largura que depende de como se segura.",
                           "ponta triangular girada perpendicular à direção do traço, fibras do papel.", 12, charcoal, 3, 60), "pencil_charcoal"), s);
  }

  // =============================== CARVÃO / GIZ / PASTEL ===============================
  {
    DabBrushSpec s; s[Set::Hardness].set(0.55f); s[Set::Spacing].set(0.09f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.6f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.2f, 0.85f));
    s.grainName = "@rough"; s[Set::GrainDepth].set(1.f); s.pressureOpacityGrainBias = true;
    s[Set::JitterPos].set(0.18f); s[Set::Dissolve].set(0.12f); s[Set::Aspect].set(0.6f);
    s.stamp.rot = RotMode::Random;
    addDab(lib, mk("carvao.vinha", "Carvão de vinha", "carvao-giz", "galho de videira carbonizado: leve, quebradiço, deixa poeira e partículas soltas; é fácil de apagar e esfumar.",
                   "grão grosso, buracos aleatórios (dissolve) e deriva de posição.", 14, charcoal, 3, 80), s, {texP}, texTweak);
  }
  {
    DabBrushSpec s; s[Set::Hardness].set(0.8f); s[Set::Spacing].set(0.08f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.5f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.5f, 1.f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(0.55f); s.pressureOpacityGrainBias = true;
    addDab(lib, mk("carvao.comprimido", "Carvão comprimido", "carvao-giz", "carvão compactado com aglutinante: preto denso e profundo, poucos vazios.",
                   "alta opacidade, grão fraco.", 12, Color{0.02f, 0.02f, 0.02f, 1}, 3, 70), s);
  }
  {
    DabBrushSpec s; s[Set::Hardness].set(0.9f); s[Set::Spacing].set(0.1f); s[Set::Aspect].set(0.7f);
    s.stamp.rot = RotMode::DirectionPerp; s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.6f, 1.f));
    s[Set::Opacity].set(0.85f); s.grainName = "@rough"; s[Set::GrainDepth].set(0.9f); s[Set::Dissolve].set(0.05f);
    addDab(lib, mk("giz.bastao", "Giz de bastão", "carvao-giz", "giz calcário: pega só nos picos do papel, deixa risco opaco e seco.",
                   "pegada achatada perpendicular ao traço, grão forte.", 12, Color{0.95f, 0.95f, 0.9f, 1}, 3, 70), s);
  }
  {  // soft pastel
    DabBrushSpec s; s[Set::Hardness].set(0.65f); s[Set::Spacing].set(0.12f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.7f, 1.f)); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.2f, 0.85f));
    s.grainName = "@rough"; s[Set::GrainDepth].set(0.85f); s.pressureOpacityGrainBias = true; s[Set::Aspect].set(0.75f); s.stamp.rot = RotMode::DirectionPerp;
    s[Set::Smudge].set(0.08f); s[Set::SmudgeLength].set(0.6f); s[Set::SatShift].set(0).link(In::Random, Curve{{0, -0.04f}, {1, 0.04f}});
    addDab(lib, mk("pastel.macio", "Pastel seco macio", "carvao-giz", "pigmento quase puro: veludo opaco que se esfuma com o dedo e mistura com a camada de baixo.",
                   "pequena parcela de smudge mistura com o que já existe; grão grosso.", 16, Color{0.85f, 0.45f, 0.45f, 1}, 3, 90), s, {texP}, texTweak);
  }
  {
    DabBrushSpec s; s[Set::Hardness].set(0.95f); s[Set::Spacing].set(0.08f); s[Set::Aspect].set(0.35f); s.stamp.baseAngleDeg = 35;
    s[Set::Radius].set(0.8f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.3f, 0.95f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(0.7f); s.pressureOpacityGrainBias = true;
    addDab(lib, mk("pastel.duro", "Pastel seco duro (Conté)", "carvao-giz", "barra dura de pastel: ponta de quina permite linhas nítidas e hachuras; cobre menos.",
                   "pegada achatada com ângulo fixo (quina da barra).", 10, Color{0.5f, 0.3f, 0.2f, 1}, 2, 60), s);
  }
  {
    DabBrushSpec s; s[Set::Hardness].set(0.9f); s[Set::Spacing].set(0.1f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.6f, 1.f)); s[Set::Opacity].set(0.95f);
    s.grainName = "@canvas"; s[Set::GrainDepth].set(0.45f); s[Set::Smudge].set(0.3f); s[Set::SmudgeLength].set(0.85f);
    addDab(lib, mk("pastel.oleoso", "Pastel oleoso", "carvao-giz", "cera + óleo: nunca seca, arrasta e empasta quando se passa por cima.",
                   "smudge alto com memória longa; cobertura quase total; grão de trama.", 14, Color{0.9f, 0.5f, 0.15f, 1}, 3, 80), s);
  }
  {
    DabBrushSpec s; s[Set::Hardness].set(0.95f); s[Set::Spacing].set(0.1f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.65f, 1.f)); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.35f, 0.95f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(1.f); s.pressureOpacityGrainBias = true; s[Set::JitterPos].set(0.05f);
    addDab(lib, mk("giz.cera", "Giz de cera", "carvao-giz", "cera sobre papel rugoso: cobre os picos, deixa pontinhos brancos nos vales; pressão fecha o grão.",
                   "grão do papel com bias por pressão.", 9, Color{0.2f, 0.55f, 0.85f, 1}, 2, 60), s);
  }
  {
    DabBrushSpec s; s[Set::Hardness].set(0.75f); s[Set::Spacing].set(0.1f); s[Set::Aspect].set(0.8f); s.stamp.rot = RotMode::DirectionPerp;
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.55f, 1.f)); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.25f, 0.9f));
    s.grainName = "@paper"; s[Set::GrainDepth].set(0.8f); s.pressureOpacityGrainBias = true;
    addDab(lib, mk("giz.conte", "Conté sanguínea", "carvao-giz", "bastão de conté marrom-avermelhado: tom quente, risco firme e granulado.",
                   "pegada achatada girada; grão médio.", 11, Color{0.55f, 0.22f, 0.14f, 1}, 2, 60), s);
  }
  {  // blackboard chalk with dust
    DabBrushSpec s; s[Set::Hardness].set(0.95f); s[Set::Spacing].set(0.1f); s[Set::Aspect].set(0.8f);
    s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.7f, 1.f)); s[Set::Opacity].set(0.9f);
    s.grainName = "@rough"; s[Set::GrainDepth].set(0.9f); s[Set::Dissolve].set(0.08f); s[Set::JitterPos].set(0.06f);
    BrushInfo b = mk("giz.lousa", "Giz de lousa", "carvao-giz", "giz branco no quadro-negro: traço opaco, sem fim de tinta; falha e deixa poeira.",
                     "grão grosso e buracos; contraste forte sobre fundo escuro.", 8, Color{0.97f, 0.97f, 0.95f, 1}, 2, 50);
    b.demoBg = "dark";
    addDab(lib, b, s);
  }
  // zip chalks: each with distinct dynamics
  auto zipChalk = [&](const char* mask, const char* name, const char* real, const char* model, float size, RotMode rot, float rotJ,
                      float spacing, float opacity, float grainD, const char* grain, float aspect, float dissolve, float scatter, Color col, bool dark) {
    DabBrushSpec s; s.stamp.maskName = mask; s.stamp.rot = rot; s.stamp.rotJitterDeg = rotJ; s[Set::Spacing].set(spacing);
    s[Set::Hardness].set(1.f); s[Set::Aspect].set(aspect); s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.55f, 1.f));
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(opacity * 0.4f, opacity));
    if (grainD > 0) { s.grainName = grain; s[Set::GrainDepth].set(grainD); s.pressureOpacityGrainBias = true; }
    s[Set::Dissolve].set(dissolve); s.stamp.scatter = scatter; s[Set::JitterPos].set(0.05f);
    BrushInfo b = zipInfo(mk((std::string("zip.") + mask).c_str(), name, "carvao-giz", real, model, size, col, 3, 100), mask);
    if (dark) b.demoBg = "dark";
    addDab(lib, b, s);
  };
  zipChalk("chalk01", "Giz nuvem", "giz macio que deixa uma mancha irregular, de contorno felpudo.", "carimbo felpudo girado ao acaso, baixa opacidade acumulativa.", 20, RotMode::Random, 0, 0.3f, 0.35f, 0.4f, "@rough", 1, 0.02f, 0.1f, Color{0.9f, 0.9f, 0.95f, 1}, true);
  zipChalk("chalk02", "Giz pétalas", "giz que cai em pó formando manchas de lóbulos.", "carimbo lobulado com espalhamento (scatter) e giro aleatório.", 22, RotMode::Random, 0, 0.5f, 0.45f, 0.3f, "@paper", 1, 0.05f, 0.35f, Color{0.95f, 0.8f, 0.5f, 1}, true);
  zipChalk("chalk_charcoal", "Carvão em pó", "pó de carvão esfregado: nuvem manchada, escura no centro.", "carimbo irregular escuro, giro aleatório, dissolve para pó.", 22, RotMode::Random, 0, 0.35f, 0.5f, 0.5f, "@fibers", 1, 0.1f, 0.15f, charcoal, false);
  zipChalk("chalk_crayon", "Crayon texturizado", "crayon pressionado de lado: pegada lobulada com cera manchada.", "carimbo texturizado girado com a direção, grão do papel.", 14, RotMode::Direction, 20, 0.25f, 0.85f, 0.8f, "@paper", 1, 0.03f, 0.05f, Color{0.9f, 0.3f, 0.2f, 1}, false);
  zipChalk("chalk_hard", "Giz retangular duro", "giz de ponta retangular (de quadro-negro): lasca reta que deixa faixa larga áspera.", "ponta retangular que mantém a quina alinhada à direção.", 14, RotMode::Direction, 0, 0.12f, 0.9f, 0.9f, "@rough", 1, 0.04f, 0, Color{0.96f, 0.96f, 0.93f, 1}, true);
  zipChalk("chalk_pastel_hard", "Pastel retangular duro", "barra de pastel duro usada pela face: faixa retangular com tom que varia.", "ponta retangular orientada e grão médio.", 14, RotMode::Direction, 0, 0.15f, 0.75f, 0.7f, "@paper", 1, 0.02f, 0, Color{0.4f, 0.55f, 0.85f, 1}, false);
  zipChalk("chalk_pastel_soft", "Pastel macio redondo", "disco de pastel macio: pigmento granulado e translúcido, esfuma.", "carimbo circular granulado, giro aleatório, opacidade baixa que acumula.", 18, RotMode::Random, 0, 0.3f, 0.55f, 0.6f, "@rough", 1, 0, 0, Color{0.9f, 0.55f, 0.65f, 1}, false);

  // =============================== TINTA ===============================
  auto inkpen = [&](const char* id, const char* name, const char* real, const char* model, float size, std::function<void(DabBrushSpec&)> f,
                    Color col = Color{0.03f, 0.03f, 0.06f, 1}, std::vector<PDef> pd = {}, Tweak tw = nullptr, bool buildUp = true) {
    DabBrushSpec s; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.07f); s.buildUp = buildUp; f(s);
    BrushInfo b = mk(id, name, "tinta", real, model, size, col, 0.5f, 60);
    addDab(lib, b, s, pd, tw);
  };
  inkpen("tinta.tecnica", "Caneta técnica (nanquim)", "ponta tubular: traço de largura exata, sem variação, sem pressão.", "raio constante, hardness 1.", 2,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::SlowTracking].set(2.f); });
  inkpen("tinta.rapidografo", "Rapidógrafo", "tubo com agulha: linha perfeitamente uniforme, ligeiramente mais escura ao parar.", "raio constante, leve acúmulo de tinta com a lentidão.", 2.5f,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Opacity].set(1.f).link(In::Speed1, Curve::linear(0, -0.1f)); s[Set::SlowTracking].set(3.f); });
  inkpen("tinta.gpen", "G-pen (mangá)", "pena de metal flexível: pressão abre as pontas e engrossa; afina nas pontas do traço, bem nítida.", "raio pelo pressure (0.08→1) com leve afinamento pela velocidade; hardness 1.", 5,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.08f}, {0.3f, 0.35f}, {1, 1.f}}).link(In::Speed1, Curve::linear(0, -0.18f)); s[Set::SlowTracking].set(2.f); });
  inkpen("tinta.maru", "Maru-pen", "pena redonda de detalhe: linha mais fina e constante que a G-pen, ainda com pequena variação.", "faixa de raio estreita pela pressão.", 3,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.35f}, {1, 0.8f}}); });
  inkpen("tinta.mapping", "Mapping pen", "pena de mapa, muito fina e rígida: traço sutilmente nervoso.", "raio mínimo + pequeno tremor.", 2,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.3f}, {1, 0.55f}}); s[Set::JitterPos].set(0.05f); });
  inkpen("tinta.fonte", "Caneta-tinteiro", "a tinta acumula quando a mão pára e afina quando corre; a ponta de ouro dá uma leve variação.", "raio e opacidade dependem da velocidade (mais lento = mais largo e escuro).", 4,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.5f}, {1, 0.9f}}).link(In::Speed1, Curve::linear(0, -0.25f)); s[Set::Opacity].set(1.f).link(In::Speed1, Curve::linear(0, -0.25f)); s[Set::SlowTracking].set(2.f);},
         Color{0.05f, 0.08f, 0.3f, 1});
  inkpen("tinta.esferografica", "Esferográfica", "esfera rolante: tinta falha em momentos, escurece com pressão e deixa um fio contínuo.", "opacidade pela pressão, falhas aleatórias (dissolve), grão de papel leve.", 2.2f,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Opacity].set(0).link(In::Pressure, Curve{{0, 0.35f}, {1, 1.f}}); s[Set::Dissolve].set(0.06f); s.grainName = "@paper"; s[Set::GrainDepth].set(0.3f); s.pressureOpacityGrainBias = true; },
         Color{0.1f, 0.15f, 0.55f, 1});
  inkpen("tinta.gel", "Caneta de gel", "gel opaco e brilhante: cobre bem, traço uniforme e levemente espesso.", "raio constante, bordas ligeiramente macias.", 3,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.8f}, {1, 1.1f}}); s[Set::Hardness].set(0.95f); }, Color{0.7f, 0.1f, 0.4f, 1});
  inkpen("tinta.feltro", "Caneta de feltro", "ponta de feltro: traço macio, translúcido onde sobrepõe-se; não escurece dentro do mesmo traço.", "buildUp desligado (buffer de traço) + borda suave.", 4,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Hardness].set(0.85f); s[Set::Opacity].set(0.88f); }, Color{0.1f, 0.1f, 0.12f, 1}, {}, nullptr, false);
  inkpen("tinta.pincel-caneta", "Pincel-caneta (brush pen)", "ponta flexível: engrossa com a pressão e afina em velocidade com cauda fina.", "raio pela pressão (0.05→1.1) e redução forte pela velocidade.", 6,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.05f}, {0.5f, 0.55f}, {1, 1.15f}}).link(In::Speed1, Curve::linear(0, -0.55f)); s[Set::SlowTracking].set(3.f); });
  inkpen("tinta.caligrafia", "Caligrafia (pena chata)", "pena de ponta chata fixa a 45°: o traço é fino na direção do corte e grosso na perpendicular.", "pegada elíptica com ângulo fixo 45° (aspect 4): a espessura vem só da geometria.", 6,
         [](DabBrushSpec& s) { s[Set::Aspect].set(4.f); s[Set::Radius].set(0.4f); s.stamp.baseAngleDeg = -45.f; s[Set::Spacing].set(0.05f); });
  inkpen("tinta.cana", "Cana (reed pen)", "caniço cortado: linha áspera, borda irregular, absorção do papel.", "elipse inclinada com fibras de papel e tremor.", 5,
         [](DabBrushSpec& s) { s[Set::Aspect].set(2.5f); s[Set::Radius].set(0.5f); s.stamp.baseAngleDeg = -30.f; s.grainName = "@fibers"; s[Set::GrainDepth].set(0.55f); s[Set::JitterPos].set(0.07f); s[Set::Hardness].set(0.9f); },
         Color{0.15f, 0.1f, 0.06f, 1});
  inkpen("tinta.pena-tinteiro", "Pena de molhar (dip pen)", "mergulha-se a pena: a tinta acaba pouco a pouco e o traço desbota até ser preciso molhar de novo.", "opacidade e largura decrescem com a distância percorrida (Length); parâmetro 'capacidade'.", 4,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.3f}, {1, 1.f}}); s[Set::Opacity].set(1.f).link(In::Length, Curve::linear(0, -0.75f)); },
         Color{0.1f, 0.05f, 0.02f, 1}, {{"capacidade", "Capacidade de tinta", 1.f, 0.2f, 1.f}},
         [](DabBrushSpec& s, const Brush& b) { s[Set::Opacity].links[0].curve = Curve::linear(0, -0.85f / std::max(0.2f, b.param("capacidade"))); });
  inkpen("tinta.pena-quill", "Pena de ganso (quill)", "pena de ave cortada: engrossa pela pressão e pela inclinação, aveludada nos extremos.", "aspect pelo tilt, raio pela pressão e velocidade.", 5,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.2f}, {1, 1.f}}).link(In::Speed1, Curve::linear(0, -0.2f)); s[Set::Aspect].set(1.5f).link(In::TiltDec, Curve::linear(0, 3.f)); s.stamp.baseAngleDeg = -35.f; s[Set::JitterPos].set(0.02f); },
         Color{0.2f, 0.1f, 0.05f, 1});
  inkpen("tinta.lavagem", "Lavagem de nanquim", "nanquim diluído: lavis cinza translúcido, camadas somam-se aos poucos.", "dab macio de baixa opacidade com grão grosso, acumulativo.", 22,
         [](DabBrushSpec& s) { s[Set::Hardness].set(0.35f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.04f, 0.18f)); s[Set::Radius].set(1.f); s.grainName = "@rough"; s[Set::GrainDepth].set(0.35f); s[Set::Spacing].set(0.1f); s[Set::Hardness].set(0.3f); },
         Color{0.12f, 0.12f, 0.16f, 1});
  // zip pens
  auto zipPen = [&](const char* mask, const char* name, const char* real, const char* model, float size, std::function<void(DabBrushSpec&)> f, Color col = Color{0.03f, 0.03f, 0.06f, 1}) {
    DabBrushSpec s; s.stamp.maskName = mask; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.08f); f(s);
    addDab(lib, zipInfo(mk((std::string("zip.") + mask).c_str(), name, "tinta", real, model, size, col, 1, 80), mask), s);
  };
  zipPen("pen01", "Caneta redonda macia", "caneta de ponta redonda com borda ligeiramente suave.", "ponta redonda do pacote, raio pela pressão.", 5,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.25f}, {1, 1.f}}); });
  zipPen("pen02", "Caneta redonda dura", "caneta dura: borda definida, sem variação de opacidade.", "ponta de borda dura, raio constante com pequena reação à pressão.", 4,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.55f}, {1, 1.f}}); s[Set::SlowTracking].set(2.f); });
  zipPen("pen02_old", "Caneta gasta (serrilhada)", "caneta velha cujo contorno é irregular: o traço sai serrilhado e rústico.", "ponta de borda irregular e giro aleatório para quebrar a regularidade.", 4,
         [](DabBrushSpec& s) { s.stamp.rot = RotMode::Random; s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.4f}, {1, 1.f}}); s[Set::JitterPos].set(0.05f); });
  zipPen("pen_bleed", "Caneta que borra", "tinta que escorre nas fibras do papel: o contorno vaza com bordas manchadas e tom mais denso no centro.", "ponta manchada de borda irregular, opacidade alta e leve deriva; espaçamento maior para manter a mancha.", 7,
         [](DabBrushSpec& s) { s.stamp.rot = RotMode::Random; s[Set::Spacing].set(0.16f); s[Set::Opacity].set(0.55f); s.grainName = "@fibers"; s[Set::GrainDepth].set(0.5f); s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.5f}, {1, 1.1f}}).link(In::Speed1, Curve::linear(0, 0.15f)); });
  zipPen("pen_fade", "Marca-texto fosco", "caneta que desbota: marca translúcida cinzenta que se acumula por sobreposição.", "ponta cinza translúcida, opacidade baixa, acumulativa.", 8,
         [](DabBrushSpec& s) { s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.1f, 0.45f)); s[Set::Radius].set(1.f); s[Set::Spacing].set(0.1f); }, Color{0.25f, 0.28f, 0.35f, 1});

  // =============================== MARCADORES ===============================
  auto marker = [&](const char* id, const char* name, const char* real, const char* model, float size, Blend bl, bool buildUp,
                    std::function<void(DabBrushSpec&)> f, Color col) {
    DabBrushSpec s; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.08f); s.blend = bl; s.buildUp = buildUp; f(s);
    addDab(lib, mk(id, name, "marcador", real, model, size, col, 2, 80), s);
  };
  marker("marcador.chanfrado", "Marcador chanfrado", "ponta chata cortada em bisel: traço largo na direção do corte, fino na outra; cor translúcida que escurece ao repassar.",
         "elipse achatada com ângulo fixo, buffer de traço (sem escurecer no mesmo traço) e mistura Multiply.", 12, Blend::Multiply, false,
         [](DabBrushSpec& s) { s[Set::Aspect].set(3.f); s[Set::Radius].set(0.55f); s.stamp.baseAngleDeg = -45.f; s[Set::Opacity].set(0.8f); }, Color{0.95f, 0.4f, 0.1f, 1});
  marker("marcador.texto", "Marca-texto", "tinta fluorescente translúcida: realça sem esconder o que está embaixo; ponta chata larga.",
         "Multiply, ponta retangular larga, opacidade ~55%.", 14, Blend::Multiply, false,
         [](DabBrushSpec& s) { s[Set::Aspect].set(0.35f); s[Set::Radius].set(1.f); s[Set::Opacity].set(0.6f); s[Set::Hardness].set(1.f); s.stamp.baseAngleDeg = 0; }, Color{1.f, 0.95f, 0.1f, 1});
  marker("marcador.alcool", "Marcador a álcool (Copic)", "tinta de álcool: bordas levemente molhadas que se fundem; sobrepor faz camadas mais escuras e começa com um ponto mais carregado.",
         "buildUp desligado, borda suave, escurecimento no início do traço (opacidade pelo Stroke).", 12, Blend::Multiply, false,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Hardness].set(0.75f); s[Set::Opacity].set(0.7f).link(In::Stroke, Curve{{0, 0.25f}, {0.15f, 0.f}}); s[Set::Dissolve].set(0.01f); }, Color{0.2f, 0.65f, 0.75f, 1});
  marker("marcador.pincel", "Marcador pincel", "ponta flexível de pincel: engrossa com a pressão; traço opaco-translúcido.",
         "raio pela pressão, buffer de traço.", 10, Blend::Normal, false,
         [](DabBrushSpec& s) { s[Set::Radius].set(0).link(In::Pressure, Curve{{0, 0.25f}, {1, 1.1f}}); s[Set::Opacity].set(0.9f); s[Set::Hardness].set(0.9f); }, Color{0.7f, 0.15f, 0.5f, 1});
  marker("marcador.fino", "Fineliner", "caneta de ponta fina de feltro: linha precisa que não escurece ao cruzar o próprio traço.",
         "raio constante, buffer de traço.", 2, Blend::Normal, false, [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Hardness].set(1.f); s[Set::SlowTracking].set(2); }, Color{0.05f, 0.05f, 0.08f, 1});
  marker("marcador.tinta-opaca", "Marcador de tinta opaca", "marcador de tinta à base de pigmento: cobre totalmente, borda nítida, fosco.",
         "opacidade 1, borda dura, grão de tela leve.", 9, Blend::Normal, true,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Opacity].set(1.f); s.grainName = "@canvas"; s[Set::GrainDepth].set(0.15f); }, Color{0.95f, 0.95f, 0.9f, 1});
  marker("marcador.giz-liquido", "Marcador de giz líquido", "tinta de giz líquido: opaca, seca fosca com leve textura, borda arredondada.",
         "buffer de traço com grão grosso discreto.", 10, Blend::Normal, false,
         [](DabBrushSpec& s) { s[Set::Radius].set(1.f); s[Set::Hardness].set(0.95f); s[Set::Opacity].set(0.96f); s.grainName = "@rough"; s[Set::GrainDepth].set(0.5f); }, Color{0.98f, 0.3f, 0.5f, 1});

  // =============================== AEROGRAFO ===============================
  auto airb = [&](const char* id, const char* name, const char* real, const char* model, float size, std::function<void(DabBrushSpec&)> f,
                  Color col = Color{0.15f, 0.2f, 0.5f, 1}, const char* mask = nullptr, std::vector<PDef> pd = {}, Tweak tw = nullptr) {
    DabBrushSpec s; s[Set::Spacing].set(0.1f); f(s);
    BrushInfo b = mk(id, name, "aerografo", real, model, size, col, 2, 150);
    if (mask) { s.stamp.maskName = mask; b.stampName = mask; }
    addDab(lib, b, s, pd, tw);
  };
  PDef fluxo{"fluxo", "Fluxo de tinta", 1.f, 0.1f, 1.f};
  auto fluxoTw = [](DabBrushSpec& s, const Brush& b) { s[Set::Opacity].base *= b.param("fluxo"); for (auto& l : s[Set::Opacity].links) for (auto& p : l.curve.pts) p.y *= b.param("fluxo"); };
  airb("aerografo.suave", "Aerógrafo suave", "jato de ar com tinta fina: nuvem gaussiana; parado, a cor se acumula até saturar.", "hardness 0, opacidade baixa por marca, dabs por segundo mantêm o acúmulo com a mão parada.", 30,
       [](DabBrushSpec& s) { s[Set::Hardness].set(0.f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.02f, 0.18f)); s[Set::DabsPerSecond].set(30.f); s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.5f, 1.f)); },
       Color{0.15f, 0.2f, 0.5f, 1}, nullptr, {fluxo}, fluxoTw);
  airb("aerografo.duro", "Aerógrafo de borda definida", "bico próximo da superfície: pontos pequenos de borda quase nítida.", "hardness 0.6, opacidade moderada.", 18,
       [](DabBrushSpec& s) { s[Set::Hardness].set(0.55f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.05f, 0.3f)); s[Set::DabsPerSecond].set(25.f); s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.4f, 1.f)); },
       Color{0.8f, 0.15f, 0.15f, 1});
  airb("aerografo.granulado", "Aerógrafo granulado", "tinta salpicada em gotículas minúsculas: aparência sandy/grão, mais visível em tom claro.", "nuvem macia com 'dissolve' alto: pixels isolados em vez de névoa lisa.", 28,
       [](DabBrushSpec& s) { s[Set::Hardness].set(0.2f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.25f, 0.9f)); s[Set::Dissolve].set(0.55f); s[Set::DabsPerSecond].set(20.f); },
       Color{0.1f, 0.1f, 0.1f, 1});
  airb("aerografo.estencil", "Spray pontilhado (stipple)", "pontilhado de bico fino: pontos redondos distribuídos aleatoriamente com densidade gaussiana.", "muitas marcas pequenas espalhadas (count/scatter) em disco.", 24,
       [](DabBrushSpec& s) { s.stamp.count = 12; s.stamp.scatter = 8.f; s[Set::Radius].set(0.09f); s[Set::Hardness].set(0.9f); s[Set::Opacity].set(0.9f); s[Set::JitterRadius].set(0.8f); s[Set::Spacing].set(0.9f); s[Set::DabsPerSecond].set(15.f); },
       Color{0.05f, 0.05f, 0.05f, 1});
  airb("aerografo.respingo", "Respingo de aerógrafo (spatter)", "gotas grandes e pequenas lançadas ao acaso: pingos maiores perto do centro e satélites miúdos.", "dabs de raio muito variado espalhados; cada marca com opacidade quase total.", 26,
       [](DabBrushSpec& s) { s.stamp.count = 5; s.stamp.scatter = 7.f; s[Set::Radius].set(0.12f); s[Set::JitterRadius].set(1.6f); s[Set::Hardness].set(1.f); s[Set::Opacity].set(1.f); s[Set::Spacing].set(1.2f); s[Set::DabsPerSecond].set(10.f); },
       Color{0.05f, 0.05f, 0.05f, 1});
  airb("aerografo.poeira", "Poeira (speckle)", "grãos finíssimos como poeira mineral.", "espalhamento muito denso de marcas minúsculas.", 30,
       [](DabBrushSpec& s) { s.stamp.count = 28; s.stamp.scatter = 10.f; s[Set::Radius].set(0.035f); s[Set::Hardness].set(1.f); s[Set::Opacity].set(0.7f); s[Set::Spacing].set(1.0f); s[Set::DabsPerSecond].set(15.f); },
       Color{0.35f, 0.25f, 0.15f, 1});
  airb("aerografo.glitter", "Spray de glitter", "partículas de brilho metálico que refletem cores diferentes ao acaso.", "marcas pequenas duras com variação forte de matiz/valor.", 24,
       [](DabBrushSpec& s) { s.stamp.count = 6; s.stamp.scatter = 8.f; s[Set::Radius].set(0.08f); s[Set::Hardness].set(1.f); s[Set::Opacity].set(1.f); s[Set::Spacing].set(0.9f); s.stamp.hueJit = 0.45f; s.stamp.valJit = 0.3f; s.stamp.satJit = 0.3f; },
       Color{1.f, 0.8f, 0.3f, 1});

  // zip airs: radial masks from fine to flat, each with different dynamics
  airb("zip.air01", "Aerógrafo ponto fino", "bico de detalhe: nuvem pequena que acompanha a pressão no tamanho (retoque de realces).", "carimbo radial 'air01' pequeno, pressão → raio.", 12,
       [](DabBrushSpec& s) { s[Set::Hardness].set(1.f); s[Set::Opacity].set(0.3f); s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.25f, 1.f)); s[Set::DabsPerSecond].set(40.f); s[Set::Spacing].set(0.08f); }, Color{0.2f, 0.25f, 0.45f, 1}, "air01");
  airb("zip.air02", "Aerógrafo suave médio", "pulverização aberta: transição longa entre cor e papel.", "carimbo 'air02', pressão → opacidade (não o tamanho).", 20,
       [](DabBrushSpec& s) { s[Set::Hardness].set(1.f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.02f, 0.28f)); s[Set::Radius].set(1.f); s[Set::DabsPerSecond].set(30.f); }, Color{0.45f, 0.2f, 0.5f, 1}, "air02");
  airb("zip.air03", "Aerógrafo por velocidade", "mão rápida deposita menos tinta: o jato fica fraco ao varrer e forte ao parar.", "carimbo 'air03', opacidade cai com a velocidade (speed1).", 22,
       [](DabBrushSpec& s) { s[Set::Hardness].set(1.f); s[Set::Opacity].set(0.35f).link(In::Speed1, Curve::linear(0, -0.28f)); s[Set::DabsPerSecond].set(30.f); }, Color{0.1f, 0.5f, 0.6f, 1}, "air03");
  airb("zip.air04", "Aerógrafo cheio", "jato com núcleo mais concentrado e bordas curtas.", "carimbo 'air04' + variação aleatória de raio/posição.", 20,
       [](DabBrushSpec& s) { s[Set::Hardness].set(1.f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.05f, 0.4f)); s[Set::Radius].set(0).link(In::Pressure, Curve::linear(0.6f, 1.1f)); s[Set::JitterRadius].set(0.15f); s[Set::JitterPos].set(0.1f); s[Set::DabsPerSecond].set(25.f); }, Color{0.85f, 0.45f, 0.1f, 1}, "air04");
  airb("zip.air05", "Aerógrafo sobre papel", "o jato cobre o papel rugoso: pontinhos brancos permanecem nos vales.", "carimbo 'air05' com grão de papel.", 22,
       [](DabBrushSpec& s) { s[Set::Hardness].set(1.f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.1f, 0.5f)); s.grainName = "@paper"; s[Set::GrainDepth].set(0.8f); s[Set::DabsPerSecond].set(25.f); }, Color{0.1f, 0.45f, 0.25f, 1}, "air05");
  airb("zip.air06", "Aerógrafo opaco (chapado)", "tinta cobrindo quase totalmente com leve esfumado na borda; para fundos.", "carimbo 'air06' largo e opaco, esticado ao longo do movimento.", 26,
       [](DabBrushSpec& s) { s[Set::Hardness].set(1.f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.15f, 0.65f)); s[Set::Aspect].set(1.f); s[Set::Spacing].set(0.12f); }, Color{0.95f, 0.8f, 0.2f, 1}, "air06");
  airb("zip.air_particle", "Partículas finas", "areia soprada: grãos minúsculos espalhados com direção aleatória.", "carimbo de partículas girado ao acaso com deriva.", 24,
       [](DabBrushSpec& s) { s.stamp.rot = RotMode::Random; s[Set::Hardness].set(1.f); s[Set::Opacity].set(0.8f); s[Set::Spacing].set(0.35f); s[Set::JitterPos].set(0.4f); }, Color{0.15f, 0.1f, 0.05f, 1}, "air_particle");
  airb("zip.air_particle2", "Partículas grossas", "pedrinhas e sementes salpicadas.", "carimbo de pontos maiores, giro aleatório, escala variável.", 24,
       [](DabBrushSpec& s) { s.stamp.rot = RotMode::Random; s.stamp.scaleJitter = 0.3f; s[Set::Hardness].set(1.f); s[Set::Opacity].set(0.95f); s[Set::Spacing].set(0.5f); s[Set::JitterPos].set(0.5f); }, Color{0.25f, 0.18f, 0.1f, 1}, "air_particle2");
  {
    DabBrushSpec s; s.stamp.maskName = "gauze01"; s.stamp.rot = RotMode::Random; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.4f);
    s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.15f, 0.7f)); s.buildUp = false; s.blend = Blend::Multiply; s[Set::JitterPos].set(0.05f);
    addDab(lib, zipInfo(mk("zip.gauze01", "Carimbo de gaze", "aerografo", "tecido de gaze pressionado com tinta: deixa trama de fios e furos.", "carimbo de trama girado ao acaso, buffer de traço + Multiply para que a trama não sature.", 22, Color{0.25f, 0.2f, 0.4f, 1}, 4, 120), "gauze01"), s);
  }

  // =============================== MISTURA ===============================
  auto blend = [&](const char* id, const char* name, const char* real, const char* model, float size, std::function<void(DabBrushSpec&)> f, const char* mask = nullptr) {
    DabBrushSpec s; s[Set::Spacing].set(0.1f); s[Set::Hardness].set(0.5f); f(s);
    BrushInfo b = mk(id, name, "mistura", real, model, size, Color{0, 0, 0, 1}, 3, 120);
    b.demoBg = "fill";
    if (mask) { s.stamp.maskName = mask; b.stampName = mask; }
    addDab(lib, b, s);
  };
  blend("mistura.dedo", "Dedo (esfumar)", "o dedo arrasta a tinta úmida: cor sai do ponto de partida e deixa rastro que vai se esgotando.", "Smudge 0.95 com memória longa.", 20,
        [](DabBrushSpec& s) { s[Set::Smudge].set(0.95f); s[Set::SmudgeLength].set(0.88f); s[Set::Opacity].set(0).link(In::Pressure, Curve::linear(0.3f, 1.f)); s[Set::Hardness].set(0.45f); });
  blend("mistura.macio", "Esfumaço macio", "esfuminho de papel: mistura curta e delicada, preserva a estrutura.", "Smudge baixo-médio com memória curta, borda macia.", 18,
        [](DabBrushSpec& s) { s[Set::Smudge].set(0.55f); s[Set::SmudgeLength].set(0.4f); s[Set::Opacity].set(0.7f); s[Set::Hardness].set(0.1f); });
  blend("mistura.umido", "Mistura úmida", "tinta molhada recolhe a cor do que encontra e mistura com a cor do pincel.", "Smudge 0.7 + cor do pincel residual (colore o rastro).", 22,
        [](DabBrushSpec& s) { s[Set::Smudge].set(0.7f); s[Set::SmudgeLength].set(0.7f); s[Set::Opacity].set(0.85f); s[Set::Hardness].set(0.3f); });
  blend("mistura.desfocar", "Desfocar", "borrar sem arrastar: tudo se suaviza em vez de se deslocar.", "Blend::Soften (média local 5x5) sob a marca.", 20,
        [](DabBrushSpec& s) { s.blend = Blend::Soften; s[Set::Opacity].set(0.5f); s[Set::Hardness].set(0.4f); s[Set::Spacing].set(0.08f); });
  blend("mistura.misturador", "Misturador de cor", "espátula de mistura de cores: mistura os pigmentos de duas áreas sem pintar nada novo.", "Smudge puro com raio de amostragem grande.", 24,
        [](DabBrushSpec& s) { s[Set::Smudge].set(1.f); s[Set::SmudgeLength].set(0.5f); s[Set::SmudgeRadius].set(1.5f); s[Set::Opacity].set(0.55f); s[Set::Hardness].set(0.2f); });
  blend("mistura.pastel", "Esfumar pastel", "dedo sobre pastel: arrasta o pigmento, grão do papel se mantém.", "Smudge com grão do papel.", 20,
        [](DabBrushSpec& s) { s[Set::Smudge].set(0.85f); s[Set::SmudgeLength].set(0.8f); s[Set::Opacity].set(0.75f); s.grainName = "@rough"; s[Set::GrainDepth].set(0.6f); });
  blend("mistura.carvao", "Esfumar carvão", "a palma arrasta o carvão em faixas direcionais, deixando um véu cinza.", "Smudge com pegada alongada na direção do traço.", 22,
        [](DabBrushSpec& s) { s[Set::Smudge].set(0.9f); s[Set::SmudgeLength].set(0.92f); s[Set::Aspect].set(2.2f); s.stamp.rot = RotMode::Direction; s[Set::Opacity].set(0.6f); s[Set::Hardness].set(0.2f); });
  blend("mistura.liquify", "Arrastar (liquify)", "empurra a imagem como borracha de barro: desloca sem desbotar.", "Smudge 1.0 com memória quase infinita e pegada firme.", 24,
        [](DabBrushSpec& s) { s[Set::Smudge].set(1.f); s[Set::SmudgeLength].set(0.985f); s[Set::Opacity].set(0.95f); s[Set::Hardness].set(0.7f); s[Set::Spacing].set(0.06f); });
  blend("mistura.seco", "Mistura a seco", "pincel seco sobre pigmento seco: só raspa e mistura nos picos do papel.", "Smudge com grão do papel como máscara.", 20,
        [](DabBrushSpec& s) { s[Set::Smudge].set(0.8f); s[Set::SmudgeLength].set(0.5f); s[Set::Opacity].set(0.8f); s.grainName = "@paper"; s[Set::GrainDepth].set(1.f); s[Set::Hardness].set(0.7f); });

  registerCustom(lib);
}

}  // namespace ab
