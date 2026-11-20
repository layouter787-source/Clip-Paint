/* AnimBrush C API - for JNI/Kotlin, Swift, C#, etc. The host implements the surface via callbacks.
 * Typical flow:
 *   ab_init("stamps.abpack");
 *   ab_brush* b = ab_brush_create("tinta.caneta-g");
 *   ab_stroke_params sp = ab_default_params(); sp.size = 8; sp.color[0..3] = ...
 *   ab_stroke_begin(b, &surface, &sp, &sample); ab_stroke_move(b, &sample)...; ab_stroke_end(b);
 */
#ifndef ANIMBRUSH_C_API_H
#define ANIMBRUSH_C_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ab_brush ab_brush;

/* Blend ids == ab::Blend order: 0 Normal 1 Multiply 2 Screen 3 Overlay 4 Darken 5 Lighten 6 Add 7 Erase 8 Smudge
 * 9 EraseSimilar 10 EraseDark 11 EraseLight 12 EraseEdge 13 Soften 14 Dissolve */
typedef struct {
  float x, y, radius, aspect, angle, hardness;
  float r, g, b;               /* straight colour 0..1 */
  float opacity, color_alpha;
  int blend, lock_alpha, mirror_x, invert_shape;
  const uint8_t* shape; int shape_w, shape_h;     /* 8-bit coverage mask spanning [-radius*aspect, +] x [-radius, +] ; NULL = round tip */
  const uint8_t* grain; int grain_w, grain_h;     /* tiled paper grain, may be NULL */
  float grain_depth, grain_scale, grain_bias, dissolve;
  uint32_t seed;
  float ref_r, ref_g, ref_b, tolerance, edge_limit;
} ab_dab;

typedef struct {
  void* user;
  int width, height;
  void (*draw_dab)(void* user, const ab_dab* dab);                                   /* required */
  void (*get_color)(void* user, float x, float y, float radius, float out_rgba[4]);  /* required (straight rgba) */
  /* optional (may be NULL): bulk read of premultiplied float rgba, w*h*4 floats */
  void (*read_patch)(void* user, int x0, int y0, int w, int h, float* out_premult_rgba);
  /* optional: composite premultiplied patch with blend mode (see ids) and opacity */
  void (*composite_patch)(void* user, int x0, int y0, int w, int h, const float* premult_rgba, int blend, float opacity);
} ab_surface;

typedef struct {
  float color[4], color2[4];
  float size, opacity;           /* size = radius in px */
  uint32_t seed; int frame; float time, wind, intensity; int lock_alpha;
} ab_stroke_params;

typedef struct { float x, y, pressure, tilt_x, tilt_y, dt, rotation; } ab_sample;

int  ab_init(const char* stamp_pack_path);                 /* 1 ok, 0 pack not found (procedural masks still work) */
int  ab_init_from_memory(const uint8_t* data, uint64_t size);
void ab_add_mask(const char* name, int w, int h, const uint8_t* coverage);  /* host-decoded PNG masks */
int  ab_brush_count(void);
const char* ab_brush_id(int index);
const char* ab_catalog_json(void);                         /* valid until next call */
ab_brush* ab_brush_create(const char* id);                 /* NULL if unknown */
void ab_brush_destroy(ab_brush*);
ab_stroke_params ab_default_params(void);
int  ab_brush_set_param(ab_brush*, const char* key, float value);
void ab_stroke_begin(ab_brush*, const ab_surface*, const ab_stroke_params*, const ab_sample*);
void ab_stroke_move(ab_brush*, const ab_sample*);
void ab_stroke_hold(ab_brush*, float dt);
void ab_stroke_end(ab_brush*);

#ifdef __cplusplus
}
#endif
#endif
