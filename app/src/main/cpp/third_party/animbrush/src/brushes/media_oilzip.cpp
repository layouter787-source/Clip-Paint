// media: the three oil texture tips of the zip (bristle streak lines + rough dabs).
#include "../../include/animbrush/dabbrush.hpp"
namespace ab {
void registerMediaOilZip(BrushLibrary& lib) {
  auto mk = [](const char* mask, const char* name, const char* desc, float size, Color c) {
    BrushInfo i; i.id = std::string("zip.") + mask; i.name = name; i.category = "oleo-acrilica"; i.description = desc; i.stampName = mask;
    i.defaultSize = size; i.minSize = 3; i.maxSize = 80; i.needsPressure = true; i.suggestedColor = c; i.demoBg = "paper"; return i;
  };
  {  // hard bristle lines
    DabBrushSpec s; s.stamp.maskName = "oil_line_hard"; s.stamp.rot = RotMode::Direction; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.1f);
    s[Set::Aspect].set(1.f); s[Set::Opacity].set(0.0f).link(In::Pressure, Curve::linear(0.55f, 1.f));
    s[Set::Radius].set(0.f).link(In::Pressure, Curve::linear(0.55f, 1.f));
    s[Set::Smudge].set(0.18f).link(In::Speed1, Curve::linear(0.f, 0.15f)); s[Set::SmudgeLength].set(0.8f);
    s.stamp.valJit = 0.04f;
    lib.addDab(mk("oil_line_hard", "Óleo: cerdas duras (linhas)",
      "Na vida real: pincel de cerdas duras (porco) arrasta a tinta espessa deixando sulcos paralelos nitidos na direcao do golpe. Modelo: mascara de linhas onduladas girada para seguir a direcao do traco, borda dura, pressao muda tamanho e carga, mistura leve com a tinta do suporte (smudge).", 20, Color{0.75f, 0.2f, 0.15f, 1}), s);
  }
  {  // soft bristle lines
    DabBrushSpec s; s.stamp.maskName = "oil_line_soft"; s.stamp.rot = RotMode::Direction; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.1f);
    s[Set::Opacity].set(0.0f).link(In::Pressure, Curve::linear(0.4f, 0.9f));
    s[Set::Radius].set(0.f).link(In::Pressure, Curve::linear(0.5f, 1.f));
    s[Set::Smudge].set(0.4f); s[Set::SmudgeLength].set(0.9f); s[Set::SmudgeRadius].set(0.8f);
    lib.addDab(mk("oil_line_soft", "Óleo: cerdas macias (mistura)",
      "Na vida real: pincel macio (marta/sintetico) puxa tinta umida e a mistura, deixando veios suaves e degrades de cor. Modelo: mascara de linhas suaves seguindo a direcao, com forte captura de cor do suporte e memoria longa do balde de tinta.", 22, Color{0.2f, 0.4f, 0.75f, 1}), s);
  }
  {  // rough dabs
    DabBrushSpec s; s.stamp.maskName = "oil_rough"; s.stamp.rot = RotMode::Random; s.stamp.scaleJitter = 0.3f; s.stamp.hueJit = 0.02f; s.stamp.valJit = 0.08f;
    s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.55f); s[Set::JitterPos].set(0.35f);
    s[Set::Opacity].set(0.0f).link(In::Pressure, Curve::linear(0.5f, 0.95f)); s[Set::Radius].set(0.f).link(In::Pressure, Curve::linear(0.6f, 1.f));
    lib.addDab(mk("oil_rough", "Óleo: pinceladas ásperas (dab)",
      "Na vida real: toques curtos de pincel carregado (pontilhado impressionista) deixam manchas de tinta macias de formatos irregulares e variacao de tom. Modelo: carimbo de manchas soltas com rotacao e escala aleatorias, espacamento largo, leve variacao de matiz/valor por toque.", 18, Color{0.9f, 0.65f, 0.15f, 1}), s);
  }
}
}  // namespace ab
