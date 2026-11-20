// AnimBrush - surface abstraction + reference CPU rasterizer.
//
// The brush engines never touch pixels directly: they emit "dabs" to an ISurface,
// exactly like libmypaint's MyPaintSurface. The host app can implement ISurface on top
// of its own tiles / GPU textures, or use RasterSurface (reference CPU implementation).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core.hpp"

namespace ab {

// 8-bit coverage mask ("stamp" / tip shape / grain texture). 255 = full ink.
struct Mask {
  std::string name;
  int w = 0, h = 0;
  std::vector<uint8_t> a;
  bool empty() const { return a.empty(); }
  float at(int x, int y) const { return a[(size_t)y * w + x] * (1.f / 255.f); }
  // bilinear, u,v in 0..1 over the mask; returns 0 outside.
  float sample(float u, float v) const;
  // tiled bilinear (u,v any real) - used for paper grain
  float sampleTiled(float u, float v) const;
};

enum class Blend : uint8_t {
  Normal,      // source-over
  Multiply,    // W3C multiply (markers, shadows)
  Screen,      // W3C screen
  Overlay,
  Darken,
  Lighten,
  Add,         // light: rgb += c*a  (alpha union).   Glow / fire / lightning
  Erase,       // dst *= (1 - a)
  Smudge,      // premult lerp of dst toward (color * colorAlpha) with weight a
  EraseSimilar,// erase only pixels whose color is close to refColor (tolerance)
  EraseDark,   // erase weighted by darkness (kills ink lines, keeps light fill)
  EraseLight,  // erase weighted by lightness
  EraseEdge,   // erase only semi-transparent edge pixels (alpha < edgeLimit)
  Soften,      // local blur of premult rgba inside the dab (softens edges / anti-alias eraser)
  Dissolve,    // Erase with per-pixel hashed threshold (stipple / dissolving)
};

struct Dab {
  float x = 0, y = 0;     // centre in surface pixels
  float radius = 5;       // semi-axis perpendicular to `angle` (px)
  float aspect = 1;       // semi-axis along `angle` = radius * aspect (aspect >= 0.05)
  float angle = 0;        // radians, rotation of the ellipse
  float hardness = 0.8f;  // 0 = fully soft (gaussian-like) .. 1 = hard edge (procedural round tip)
  Color color{0, 0, 0, 1};
  float opacity = 1;      // per-dab alpha at full coverage (0..1)
  Blend blend = Blend::Normal;
  bool lockAlpha = false; // only recolour existing pixels (Normal/Multiply/...)
  float colorAlpha = 1;   // alpha of `color` for Blend::Smudge
  // Tip shape. If null: procedural round/elliptic tip with `hardness`.
  const Mask* shape = nullptr;
  bool mirrorX = false;   // flip the shape horizontally (before rotation)
  bool invertShape = false;
  // Paper grain: multiplies coverage by lerp(1, grain(x,y), grainDepth).
  const Mask* grain = nullptr;
  float grainDepth = 0;
  float grainScale = 1;   // 1 = 1 mask pixel per canvas pixel
  float grainBias = 0;    // shifts grain threshold (-1..1): pressure pushes ink into valleys
  float dissolve = 0;     // 0..1 fraction of pixels randomly dropped (spray/stipple); also Blend::Dissolve strength
  uint32_t seed = 0;
  // Params for the special erase / soften blends
  Color refColor{0, 0, 0, 1};
  float tolerance = 0.25f;    // EraseSimilar
  float edgeLimit = 0.6f;     // EraseEdge
  // Optional soft clip against a straight line (e.g. flat tips): unused when 0
};

// Premultiplied float RGBA image patch (for effects that need read-back / bulk composite).
struct Patch {
  int x0 = 0, y0 = 0, w = 0, h = 0;
  std::vector<float> px;  // w*h*4 premultiplied rgba
  Patch() = default;
  Patch(int x, int y, int w_, int h_) : x0(x), y0(y), w(w_), h(h_), px((size_t)w_ * h_ * 4, 0.f) {}
  float* at(int x, int y) { return &px[((size_t)(y - y0) * w + (x - x0)) * 4]; }
  const float* at(int x, int y) const { return &px[((size_t)(y - y0) * w + (x - x0)) * 4]; }
  bool contains(int x, int y) const { return x >= x0 && y >= y0 && x < x0 + w && y < y0 + h; }
};

class ISurface {
 public:
  virtual ~ISurface() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  // Composite one dab.
  virtual void drawDab(const Dab& d) = 0;
  // Average colour (straight rgba) under a disc (used for smudge/colour pick). radius in px.
  virtual Color getColor(float x, float y, float radius) const = 0;
  // Bulk read (premultiplied) of the surface. Rect is clipped to the surface.
  virtual Patch readPatch(IRect r) const = 0;
  // Bulk composite of a premultiplied patch with the given blend mode (Normal/Multiply/Screen/Add/Erase... ) and opacity.
  // For Erase the patch alpha is used as erase amount.
  virtual void compositePatch(const Patch& p, Blend mode, float opacity) = 0;
};

// Reference implementation: float premultiplied RGBA, row-major.
class RasterSurface : public ISurface {
 public:
  RasterSurface(int w, int h);
  int width() const override { return w_; }
  int height() const override { return h_; }
  void drawDab(const Dab& d) override;
  Color getColor(float x, float y, float radius) const override;
  Patch readPatch(IRect r) const override;
  void compositePatch(const Patch& p, Blend mode, float opacity) override;

  void clear(Color c = Color{0, 0, 0, 0});
  float* pixel(int x, int y) { return &px_[((size_t)y * w_ + x) * 4]; }
  const float* pixel(int x, int y) const { return &px_[((size_t)y * w_ + x) * 4]; }
  // Flatten over a background colour -> 8-bit RGBA (straight, alpha=255 if bg opaque).
  std::vector<uint8_t> toRGBA8(Color bg = Color{1, 1, 1, 1}) const;
  // Checkerboard flatten (for viewing transparency).
  std::vector<uint8_t> toRGBA8Checker(int cell = 8) const;
  // Blit helper: composite another surface/patch with Normal blend.
  void drawPatchNormal(const Patch& p) { compositePatch(p, Blend::Normal, 1.f); }

 private:
  int w_, h_;
  std::vector<float> px_;
};

// Blend a straight source colour (cs, sa=coverage*alpha) into a premultiplied float pixel.
void blendPixel(float* dst, Color cs, float sa, Blend mode, bool lockAlpha);

}  // namespace ab
