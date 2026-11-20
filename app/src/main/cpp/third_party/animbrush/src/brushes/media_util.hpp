// Private helpers for the "media" module.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "../../include/animbrush/dabbrush.hpp"

namespace ab { namespace media {

struct PDef { const char* key; const char* label; float def, mn, mx; };
using Tweak = std::function<void(DabBrushSpec&, const Brush&)>;

// DabBrush with UI params that can tweak the spec at begin().
class ParamDab : public DabBrush {
 public:
  ParamDab(BrushInfo i, DabBrushSpec s, std::vector<PDef> pd, Tweak t)
      : DabBrush(std::move(i), s), base_(s), tweak_(std::move(t)) {
    for (auto& p : pd) addParam(p.key, p.label, p.def, p.mn, p.mx);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    if (tweak_) { DabBrushSpec c = base_; tweak_(c, *this); spec() = c; }
    DabBrush::begin(s, sp, in);
  }
 private:
  DabBrushSpec base_;
  Tweak tweak_;
};

inline BrushInfo mk(const char* id, const char* name, const char* cat, const std::string& real, const std::string& model,
                    float size, Color col = Color{0, 0, 0, 1}, float mn = 1.f, float mx = 120.f) {
  BrushInfo b;
  b.id = id; b.name = name; b.category = cat;
  b.description = "Na vida real: " + real + " Modelo: " + model;
  b.defaultSize = size; b.minSize = mn; b.maxSize = mx; b.suggestedColor = col;
  b.needsPressure = true;
  return b;
}

void addDab(BrushLibrary& lib, BrushInfo info, DabBrushSpec spec, std::vector<PDef> pd = {}, Tweak t = nullptr);

// implemented in media_custom.cpp
void registerCustom(BrushLibrary& lib);

}}  // namespace ab::media
