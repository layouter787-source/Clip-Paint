// AnimBrush - DabBrush: a libmypaint-style parametric dab engine (+ stamp features).
//
// Every setting = base value + sum of curves over dynamic inputs (pressure, speed, random,
// stroke progress, direction, tilt...). The same engine drives pencils, pens, airbrushes,
// smudge/oil brushes, markers, textured chalk, and stamp brushes (leaves, flowers, footprints).
#pragma once
#include "brush.hpp"
#include "library.hpp"

namespace ab {

enum class In : int { Pressure, Speed1, Speed2, Random, Stroke, Direction, TiltDec, TiltAsc, Length, Count_ };
enum class Set : int {
  Opacity,        // 0..1 (multiplied by StrokeParams.opacity)
  Radius,         // multiplier of StrokeParams.size (1 = same). Never < 0.02
  Hardness,       // 0..1
  Aspect,         // ellipse ratio (>=0.05). 1 = round
  AngleDeg,       // added to base angle (degrees)
  Spacing,        // dab distance as fraction of radius (0.1 = dense, 1 = one dab per radius). Clamped >= 0.03
  DabsPerSecond,  // extra time-based dabs (airbrush) - applies while held still too
  JitterPos,      // random positional offset, fraction of radius
  JitterRadius,   // random radius variation (0..1)
  Smudge,         // 0..1 how much canvas colour the dab picks up
  SmudgeLength,   // 0..1 memory of the smudge bucket (1 = long streaks)
  SmudgeRadius,   // radius multiplier of the colour sampling area
  SlowTracking,   // px of pointer smoothing (lag)
  GrainDepth,     // 0..1 paper grain influence
  Dissolve,       // 0..1 random pixel drop
  HueShift,       // per-dab, -0.5..0.5
  SatShift,       // per-dab, additive
  ValShift,       // per-dab, additive
  OffsetBySpeed,  // px of displacement along stroke direction * speed
  FollowDir,      // 0..1: how much the dab angle follows the stroke direction
  Count_
};

struct Mapping {
  float base = 0;
  struct Link { In input; Curve curve; };
  std::vector<Link> links;
  Mapping& set(float b) { base = b; return *this; }
  Mapping& link(In in, Curve c) { links.push_back({in, std::move(c)}); return *this; }
};

enum class RotMode : uint8_t { Fixed, Random, Direction, DirectionPerp, Cycle };
enum class MirrorMode : uint8_t { None, Random, Alternate };
enum class ColorMode : uint8_t { Solid, Gradient, Palette };

struct StampOptions {
  std::string maskName;            // name inside the StampBank ("" = procedural round tip)
  RotMode rot = RotMode::Fixed;
  float baseAngleDeg = 0;
  float cycleDeg = 30;             // RotMode::Cycle step per dab
  float rotJitterDeg = 0;          // random +/- degrees
  MirrorMode mirror = MirrorMode::None;
  float sideAlternate = 0;         // lateral offset (fraction of radius) alternating left/right (footprints)
  float scatter = 0;               // random spread radius, in multiples of radius
  int count = 1;                   // copies per spacing step (scatter bursts)
  float scaleJitter = 0;           // +/- fraction
  float hueJit = 0, satJit = 0, valJit = 0;  // per-dab colour jitter
  ColorMode colorMode = ColorMode::Solid;
  std::vector<Color> palette;      // for ColorMode::Palette
  float gradientLength = 600;      // px for ColorMode::Gradient (color -> color2)
  bool invert = false;             // use mask inverted
  bool upright = false;            // keep angle fixed regardless of rotation settings (crown, tree...)
  bool normalizeAlpha = false;     // stamp painted with constant alpha (no soft build-up inside a stroke)
};

struct DabBrushSpec {
  Mapping set[(int)Set::Count_];
  StampOptions stamp;
  Blend blend = Blend::Normal;
  std::string grainName;           // paper grain texture from the StampBank ("" none; "@paper", "@canvas", "@rough" are procedural)
  float grainScale = 1;
  bool anchoredGrain = true;       // grain fixed to canvas (paper) vs. moving with the brush
  bool buildUp = true;             // false = dabs inside one stroke do not darken each other (marker): uses stroke buffer
  bool pressureOpacityGrainBias = false;
  Color refColor{0, 0, 0, 1};      // eraser similar etc.
  float tolerance = 0.25f;
  float edgeLimit = 0.6f;
  DabBrushSpec();
  Mapping& operator[](Set s) { return set[(int)s]; }
  const Mapping& operator[](Set s) const { return set[(int)s]; }
};

class DabBrush : public Brush {
 public:
  explicit DabBrush(BrushInfo info, DabBrushSpec spec);
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override;
  void move(ISurface& s, const InputSample& in) override;
  void end(ISurface& s) override;
  void hold(ISurface& s, float dt) override;
  DabBrushSpec& spec() { return spec_; }

 private:
  float eval(Set s, const float* inputs) const;
  void inputsFor(float* out) const;
  void emitDab(ISurface& s, Vec2 pos, float pressure, float dtDab);
  void step(ISurface& s, const InputSample& in, bool first);
  void flushBuffer(ISurface& s);

  DabBrushSpec spec_;
  StrokeParams sp_;
  Rng rng_;
  // stroke state
  bool active_ = false;
  Vec2 cur_{}, last_{}, filt_{};      // raw pointer, last emitted position, slow-tracked pointer
  float pressure_ = 0, tiltX_ = 0, tiltY_ = 0;
  float distCarry_ = 0, timeCarry_ = 0;
  float strokeLen_ = 0, strokeTime_ = 0;
  float speed1_ = 0, speed2_ = 0;
  Vec2 dir_{1, 0};
  int dabIndex_ = 0;
  Color smudgeBucket_{0, 0, 0, 0};
  bool smudgeInit_ = false;
  const Mask* shape_ = nullptr;
  const Mask* grain_ = nullptr;
  // stroke buffer for non-buildup brushes
  std::unique_ptr<RasterSurface> buffer_;
  Patch base_;                        // pre-stroke snapshot (non build-up brushes)
  IRect stepDirty_{};
  bool hasStepDirty_ = false;
  float bufInputs_[(int)In::Count_] = {};
  ISurface* target_ = nullptr;
};

}  // namespace ab
