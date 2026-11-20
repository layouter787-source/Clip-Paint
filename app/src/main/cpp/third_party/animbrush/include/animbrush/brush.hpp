// AnimBrush - Brush interface, stroke input, parameters and path helpers.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core.hpp"
#include "surface.hpp"

namespace ab {

struct InputSample {
  float x = 0, y = 0;
  float pressure = 1.f;   // 0..1  (mouse / finger: use 1 or a fixed value)
  float tiltX = 0;        // -1..1 (stylus tilt, 0 = perpendicular)
  float tiltY = 0;        // -1..1
  float dt = 1.f / 120;   // seconds since previous sample (needed for speed / time based effects)
  float rotation = 0;     // stylus barrel rotation, radians (optional)
};

// Everything the host sets once per stroke.
struct StrokeParams {
  Color color{0, 0, 0, 1};
  Color color2{1, 1, 1, 1};   // secondary colour (gradients, two-tone brushes, glow halo override...)
  float size = 12.f;          // base RADIUS in px (the brush maps pressure etc. around this)
  float opacity = 1.f;        // global flow/opacity multiplier 0..1
  uint32_t seed = 1;          // deterministic randomness: same seed + same input = same stroke
  int frame = 0;              // animation frame index: brushes marked `animated` vary with it
  float time = 0;             // seconds on the animation timeline (wind, flicker)
  float wind = 0;             // -1..1  horizontal wind (trees, grass, hair, fire, smoke, snow...)
  float intensity = 1.f;      // generic 0..2 "amount" (glow strength, density, number of branches...)
  bool lockAlpha = false;     // only paint over existing pixels
};

// Editable parameter exposed to the host UI (sliders).
struct ParamDef {
  std::string key, label;
  float def = 0, min = 0, max = 1;
  float value = 0;
};

struct BrushInfo {
  std::string id;           // "ink.gpen"
  std::string name;         // Portuguese display name
  std::string category;     // "tinta", "luz", "natureza", ...
  std::string description;  // PT: how the real thing behaves + how it is modelled
  std::vector<std::string> tags;
  bool isEraser = false;
  bool animated = false;    // result depends on StrokeParams.frame/time (flicker, wind...)
  bool needsPressure = false;
  bool additive = false;    // paints light (Blend::Add) -> best on transparent layers over dark bg
  float defaultSize = 12;   // radius px
  float minSize = 1, maxSize = 200;
  std::string stampName;    // source texture (when derived from the PNG pack)
  // Hints for the catalogue renderer / host preview thumbnails:
  Color suggestedColor{0, 0, 0, 1};    // sensible default colour (tree = brown, fire = orange, ...)
  Color suggestedColor2{1, 1, 1, 1};   // secondary colour (leaves, halo, gradient end...)
  std::string demo = "wave";   // "wave" S-curve, "up" bottom->top, "line", "dot" (hold), "circle", "arc", "zigzag"
  std::string demoBg = "";     // "" auto, "white","dark","paper","sky","night","fill" (erasers: pre-painted pattern)
  float demoHold = 0;          // seconds to call hold() after the stroke ends (airbrush/fire/etc.)
};

class Brush {
 public:
  virtual ~Brush() = default;

  const BrushInfo& info() const { return info_; }
  BrushInfo& mutableInfo() { return info_; }

  // ---- stroke lifecycle ----
  // begin() receives the first sample (it should already deposit ink for it).
  virtual void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) = 0;
  virtual void move(ISurface& s, const InputSample& in) = 0;
  virtual void end(ISurface& s) = 0;
  // Called while the pen is held still (airbrush build-up, fire emitters, drips...). Default: nothing.
  virtual void hold(ISurface& s, float dt) { (void)s; (void)dt; }

  // ---- UI parameters ----
  const std::vector<ParamDef>& params() const { return params_; }
  bool setParam(const std::string& key, float v) {
    for (auto& p : params_) if (p.key == key) { p.value = clampf(v, p.min, p.max); return true; }
    return false;
  }
  float param(const std::string& key) const {
    for (auto& p : params_) if (p.key == key) return p.value;
    return 0.f;
  }

 protected:
  void addParam(const std::string& key, const std::string& label, float def, float mn, float mx) {
    ParamDef p; p.key = key; p.label = label; p.def = def; p.min = mn; p.max = mx; p.value = def;
    params_.push_back(p);
  }
  BrushInfo info_;
  std::vector<ParamDef> params_;
};

// ---------------------------------------------------------------------------------------
// Helpers used by many brush implementations
// ---------------------------------------------------------------------------------------

// Accumulates a stroke as a polyline with arc length, smoothed direction and speed.
struct PathPoint {
  Vec2 p;
  float pressure = 1;
  float t = 0;       // seconds since stroke start
  float d = 0;       // arc length from start (px)
  Vec2 dir{1, 0};    // smoothed unit tangent
  float speed = 0;   // px / s (smoothed)
  float tiltX = 0, tiltY = 0;
};

class PathTracker {
 public:
  void reset() { pts_.clear(); t_ = 0; }
  // Adds a sample. Returns the new point (always added; samples closer than minDist are merged).
  const PathPoint& add(const InputSample& in, float minDist = 0.5f);
  const std::vector<PathPoint>& points() const { return pts_; }
  size_t size() const { return pts_.size(); }
  const PathPoint& back() const { return pts_.back(); }
  float length() const { return pts_.empty() ? 0.f : pts_.back().d; }
  // Position + interpolated attributes at arc-length s (clamped).
  PathPoint at(float s) const;
  // Resample at uniform spacing (px). Includes first point; last partial step dropped.
  std::vector<PathPoint> resample(float spacing) const;

 private:
  std::vector<PathPoint> pts_;
  float t_ = 0;
};

// Draw a line of round dabs from a to b (radius r0->r1) with spacing = max(0.5, spacingFrac * radius).
// `carry` holds the left-over distance between calls so dabs are evenly spaced across segments.
void dabLine(ISurface& s, Vec2 a, Vec2 b, float r0, float r1, const Dab& proto, float spacingFrac, float* carry = nullptr);

// Single round dab convenience.
void dabRound(ISurface& s, Vec2 p, float radius, const Dab& proto);

// Polyline of per-point radii/opacity: each segment drawn with dabLine.
struct PolyPt { Vec2 p; float r; float a = 1.f; };
void dabPolyline(ISurface& s, const std::vector<PolyPt>& pts, const Dab& proto, float spacingFrac = 0.25f);

// Bounding rect of a dab-ish circle helper
inline IRect circleRect(Vec2 c, float r, int w, int h) {
  IRect q;
  q.x0 = std::max(0, (int)std::floor(c.x - r - 1)); q.y0 = std::max(0, (int)std::floor(c.y - r - 1));
  q.x1 = std::min(w, (int)std::ceil(c.x + r + 1));  q.y1 = std::min(h, (int)std::ceil(c.y + r + 1));
  return q;
}

}  // namespace ab
