// AnimBrush - erasers module (categoria "borracha" / "borracha-especial").
#include <deque>

#include "../../include/animbrush/dabbrush.hpp"

namespace ab {
namespace {

using Spec = DabBrushSpec;

BrushInfo mk(const std::string& slug, const char* name, const char* cat, const std::string& real, const std::string& model,
             float size, float mn = 1.f, float mx = 150.f, std::vector<std::string> tags = {}) {
  BrushInfo i;
  i.id = "borracha." + slug; i.name = name; i.category = cat;
  i.description = "Na vida real: " + real + " Modelo: " + model;
  i.tags = std::move(tags); i.tags.push_back("borracha");
  i.isEraser = true; i.defaultSize = size; i.minSize = mn; i.maxSize = mx;
  i.suggestedColor = Color{0, 0, 0, 1};
  return i;
}

void pressureSize(Spec& s, float lo, float hi) { s[Set::Radius].set(lo).link(In::Pressure, Curve::linear(0.f, hi - lo)); }

// ---- erase field helper ----------------------------------------------------------------------
template <class F>
void eraseField(ISurface& s, IRect r, F f) {
  r.x0 = std::max(0, r.x0); r.y0 = std::max(0, r.y0); r.x1 = std::min(s.width(), r.x1); r.y1 = std::min(s.height(), r.y1);
  if (r.empty()) return;
  Patch p(r.x0, r.y0, r.w(), r.h());
  bool any = false;
  for (int y = r.y0; y < r.y1; ++y)
    for (int x = r.x0; x < r.x1; ++x) {
      float a = f((float)x + 0.5f, (float)y + 0.5f);
      if (a > 0.004f) { p.at(x, y)[3] = std::min(1.f, a); any = true; }
    }
  if (any) s.compositePatch(p, Blend::Erase, 1.f);
}

inline float softCov(float d, float hard) {  // d = distance/radius
  if (d >= 1.f) return 0.f;
  return d <= hard ? 1.f : 1.f - smoothstep(0.f, 1.f, (d - hard) / (1.f - hard));
}

// ---- base class: walks the path and calls stamp() at even spacing ---------------------------
class PathEraser : public Brush {
 public:
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    sp_ = sp; rng_.reseed(sp.seed); cur_ = last_ = prevP_ = {in.x, in.y}; dir_ = {1, 0}; carry_ = 0; len_ = 0; idx_ = 0;
    press_ = in.pressure;
    onBegin(s);
    stamp(s, cur_, radiusFor(press_), press_); ++idx_;
  }
  void move(ISurface& s, const InputSample& in) override {
    Vec2 p{in.x, in.y};
    Vec2 seg = p - last_; float L = seg.len();
    if (L < 1e-3f) return;
    Vec2 d = seg / L; dir_ = (dir_ * 0.6f + d * 0.4f).norm();
    float p0 = press_; press_ = in.pressure;
    float sp0 = std::max(0.6f, spacingFrac() * radiusFor(p0));
    float pos = sp0 - carry_;
    while (pos <= L) {
      float t = pos / L; Vec2 q = lerp(last_, p, t); float pr = lerpf(p0, press_, t);
      float r = radiusFor(pr);
      len_ += dist(prevP_, q);
      stamp(s, q, r, pr); prevP_ = q; ++idx_;
      pos += std::max(0.6f, spacingFrac() * r);
    }
    carry_ = std::max(0.f, L - (pos - std::max(0.6f, spacingFrac() * radiusFor(press_))));
    last_ = p; cur_ = p;
  }
  void end(ISurface& s) override { onEnd(s); }

 protected:
  virtual void onBegin(ISurface&) {}
  virtual void stamp(ISurface& s, Vec2 p, float radius, float pressure) = 0;
  virtual void onEnd(ISurface&) {}
  virtual float spacingFrac() const { return 0.15f; }
  virtual float radiusFor(float pr) const { return sp_.size * (0.4f + 0.6f * pr); }
  StrokeParams sp_; Rng rng_;
  Vec2 cur_, last_, prevP_, dir_{1, 0};
  float carry_ = 0, len_ = 0, press_ = 1; int idx_ = 0;
};

// ---- shield: erase clipped by a straight edge -------------------------------------------------
class ShieldEraser : public PathEraser {
 public:
  ShieldEraser() {
    addParam("angulo", "Angulo da regua (graus)", 75.f, 0.f, 180.f);
    addParam("posX", "Posicao X da regua (0-1)", 0.5f, 0.f, 1.f);
    addParam("posY", "Posicao Y da regua (0-1)", 0.5f, 0.f, 1.f);
  }
 protected:
  void onBegin(ISurface& s) override {
    float a = param("angulo") * kDeg2Rad;
    nx_ = -std::sin(a); ny_ = std::cos(a);
    lx_ = s.width() * param("posX"); ly_ = s.height() * param("posY");
    float sd = nx_ * (cur_.x - lx_) + ny_ * (cur_.y - ly_);
    side_ = sd >= 0 ? 1.f : -1.f;
  }
  void stamp(ISurface& s, Vec2 p, float r, float) override {
    IRect q{(int)(p.x - r - 2), (int)(p.y - r - 2), (int)(p.x + r + 3), (int)(p.y + r + 3)};
    eraseField(s, q, [&](float x, float y) {
      float d = std::hypot(x - p.x, y - p.y) / r;
      float c = softCov(d, 0.92f) * clampf((r * (1.f - d)) + 0.5f);
      float sd = side_ * (nx_ * (x - lx_) + ny_ * (y - ly_));
      return c * clampf(sd + 0.5f);
    });
  }
  float nx_ = 0, ny_ = 1, lx_ = 0, ly_ = 0, side_ = 1;
};

// ---- wet media lift: blotchy, never fully clean -----------------------------------------------
class WetLiftEraser : public PathEraser {
 public:
  WetLiftEraser() { addParam("mancha", "Tamanho das manchas", 0.05f, 0.01f, 0.2f); addParam("limpeza", "Limpeza maxima", 0.8f, 0.2f, 1.f); }
 protected:
  float spacingFrac() const override { return 0.3f; }
  void stamp(ISurface& s, Vec2 p, float r, float pr) override {
    float f = param("mancha"), mx = param("limpeza") * (0.55f + 0.45f * pr);
    uint32_t seed = sp_.seed;
    IRect q{(int)(p.x - r - 2), (int)(p.y - r - 2), (int)(p.x + r + 3), (int)(p.y + r + 3)};
    eraseField(s, q, [&](float x, float y) {
      float d = std::hypot(x - p.x, y - p.y) / r;
      if (d >= 1.f) return 0.f;
      // domain-warped fbm -> soft puddles; threshold shrinks toward the dab edge (uneven bloom edge)
      float wx = fbm(x * f * 2.f + 11.f, y * f * 2.f, 2, 2.f, 0.5f, seed + 5) * 6.f;
      float n = fbm(x * f + wx * 0.2f, y * f, 4, 2.f, 0.55f, seed);
      float th = 0.36f + 0.3f * d;
      return mx * smoothstep(th - 0.06f, th + 0.1f, n) * 0.5f * (1.f - d * d);
    });
  }
};

// ---- pixel eraser ----------------------------------------------------------------------------------
class PixelEraser : public PathEraser {
 public:
  PixelEraser() { addParam("pixel", "Tamanho do pixel", 6.f, 1.f, 32.f); }
 protected:
  float spacingFrac() const override { return 0.25f; }
  void stamp(ISurface& s, Vec2 p, float r, float) override {
    int c = std::max(1, (int)param("pixel"));
    int n = std::max(1, (int)std::lround(r * 2.f / c));
    int cx = (int)std::floor(p.x / c) - n / 2, cy = (int)std::floor(p.y / c) - n / 2;
    IRect q{cx * c, cy * c, (cx + n) * c, (cy + n) * c};
    q.x0 = std::max(0, q.x0); q.y0 = std::max(0, q.y0); q.x1 = std::min(s.width(), q.x1); q.y1 = std::min(s.height(), q.y1);
    if (q.empty()) return;
    Patch pt(q.x0, q.y0, q.w(), q.h());
    for (int y = q.y0; y < q.y1; ++y) for (int x = q.x0; x < q.x1; ++x) pt.at(x, y)[3] = 1.f;
    s.compositePatch(pt, Blend::Erase, 1.f);
  }
};

// ---- colour-range erasers (magic / continuous / background) --------------------------------------
class ColorRangeEraser : public PathEraser {
 public:
  enum Mode { Magic, Continuous, Background };
  explicit ColorRangeEraser(Mode m) : mode_(m) {
    addParam("tolerancia", "Tolerancia de cor", m == Background ? 0.22f : 0.2f, 0.02f, 0.8f);
    addParam("dureza", "Dureza da borda", 0.85f, 0.1f, 1.f);
  }
 protected:
  void onBegin(ISurface& s) override { sample(s, cur_, true); }
  void sample(ISurface& s, Vec2 p, bool force) {
    Color c = s.getColor(p.x, p.y, 2.5f);
    if (mode_ == Background && !force && (c.a < 0.1f || luma(c) < 0.22f)) return;  // protect line-art / holes
    if (c.a > 0.05f) { ref_ = c; valid_ = true; }
  }
  void stamp(ISurface& s, Vec2 p, float r, float) override {
    if (mode_ != Magic && idx_ > 0) sample(s, p, false);
    if (!valid_) return;
    Dab d; d.x = p.x; d.y = p.y; d.radius = r; d.hardness = param("dureza"); d.blend = Blend::EraseSimilar;
    d.refColor = ref_; d.tolerance = param("tolerancia"); d.opacity = 1.f;
    s.drawDab(d);
  }
  Mode mode_; Color ref_; bool valid_ = false;
};

// ---- bristle eraser: many fine parallel streaks -----------------------------------------------
class BristleEraser : public PathEraser {
 public:
  BristleEraser() { addParam("cerdas", "Numero de cerdas", 11.f, 3.f, 24.f); }
 protected:
  float spacingFrac() const override { return 0.3f; }
  void stamp(ISurface& s, Vec2 p, float r, float) override {
    int n = (int)param("cerdas");
    Vec2 perp = dir_.perp();
    if (!init_) { for (int i = 0; i < 24; ++i) { base_[i] = rng_.range(0.35f, 1.f); wid_[i] = rng_.range(0.6f, 1.5f); last_s_[i] = p; } init_ = true; }
    for (int i = 0; i < n; ++i) {
      float o = ((i + 0.5f) / n * 2.f - 1.f) * r * 0.95f;
      Vec2 q = p + perp * o;
      float op = base_[i] * (0.45f + 0.55f * (0.5f + 0.5f * noise1(len_ * 0.03f + i * 7.3f, sp_.seed)));
      Dab d; d.blend = Blend::Erase; d.hardness = 0.6f; d.opacity = op;
      float rw = std::max(0.6f, r * 0.9f / n * wid_[i]);
      Vec2 a = last_s_[i];
      if (dist(a, q) > r * 0.8f) a = q;  // big jump (direction flip): do not drag a streak across
      dabLine(s, a, q, rw, rw, d, 0.5f);
      last_s_[i] = q;
    }
  }
  bool init_ = false; float base_[24], wid_[24]; Vec2 last_s_[24];
};

// ---- scratch / razor ---------------------------------------------------------------------------------
class ScratchEraser : public PathEraser {
 public:
  ScratchEraser() { addParam("riscos", "Numero de riscos", 3.f, 1.f, 8.f); addParam("falhas", "Falhas (0-1)", 0.35f, 0.f, 0.9f); }
 protected:
  float spacingFrac() const override { return 0.35f; }
  void stamp(ISurface& s, Vec2 p, float r, float) override {
    int n = (int)param("riscos");
    Vec2 perp = dir_.perp();
    for (int j = 0; j < n; ++j) {
      float off = r * 0.9f * noise1(len_ * 0.012f + j * 13.1f, sp_.seed) + rng_.sym() * 0.35f;
      Vec2 q = p + perp * off;
      float alive = noise1(len_ * 0.045f + j * 5.7f, sp_.seed + 9);
      bool on = alive > (param("falhas") * 2.f - 1.f) * 0.8f - 0.2f;
      if (on && have_[j] && dist(prev_[j], q) < r * 0.7f) {
        Dab d; d.blend = Blend::Erase; d.hardness = 1.f; d.opacity = 1.f;
        float w = 0.55f + 0.4f * (0.5f + 0.5f * noise1(len_ * 0.1f + j, sp_.seed + 3));
        dabLine(s, prev_[j], q, w, w, d, 0.4f);
      }
      prev_[j] = q; have_[j] = on;
    }
  }
  Vec2 prev_[8]; bool have_[8] = {};
};

// ---- connected-region eraser ------------------------------------------------------------------------
class RegionEraser : public PathEraser {
 public:
  RegionEraser() { addParam("tolerancia", "Tolerancia de cor", 0.25f, 0.02f, 0.8f); addParam("janela", "Janela maxima (px)", 160.f, 30.f, 400.f); }
 protected:
  float spacingFrac() const override { return 4.0f; }
  float radiusFor(float) const override { return 3.f; }
  void stamp(ISurface& s, Vec2 p, float, float) override {
    Color c = s.getColor(p.x, p.y, 0.6f);
    if (c.a < 0.2f) return;
    grow(s, (int)p.x, (int)p.y, c);
  }
  void grow(ISurface& s, int sx, int sy, Color seed) {
    int R = (int)param("janela");
    Patch pt = s.readPatch({sx - R, sy - R, sx + R + 1, sy + R + 1});
    if (pt.w == 0 || !pt.contains(sx, sy)) return;
    std::vector<uint8_t> mark((size_t)pt.w * pt.h, 0);
    float tol = param("tolerancia");
    auto similar = [&](int x, int y) {
      const float* q = pt.at(x, y);
      if (q[3] < 0.05f) return false;
      float dr = q[0] / q[3] - seed.r, dg = q[1] / q[3] - seed.g, db = q[2] / q[3] - seed.b;
      return std::sqrt((dr * dr + dg * dg + db * db) / 3.f) <= tol;
    };
    std::deque<std::pair<int, int>> qu;
    qu.push_back({sx, sy}); mark[(size_t)(sy - pt.y0) * pt.w + (sx - pt.x0)] = 1;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!qu.empty()) {
      auto [x, y] = qu.front(); qu.pop_front();
      for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!pt.contains(nx, ny)) continue;
        uint8_t& m = mark[(size_t)(ny - pt.y0) * pt.w + (nx - pt.x0)];
        if (m || !similar(nx, ny)) continue;
        m = 1; qu.push_back({nx, ny});
      }
    }
    Patch e(pt.x0, pt.y0, pt.w, pt.h);   // erase the region and 1px of anti-aliased fringe around it
    for (int y = pt.y0; y < pt.y0 + pt.h; ++y)
      for (int x = pt.x0; x < pt.x0 + pt.w; ++x) {
        bool hit = mark[(size_t)(y - pt.y0) * pt.w + (x - pt.x0)] != 0;
        if (!hit) {
          for (int k = 0; k < 4 && !hit; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (pt.contains(nx, ny) && mark[(size_t)(ny - pt.y0) * pt.w + (nx - pt.x0)]) hit = true;
          }
          if (hit) e.at(x, y)[3] = 0.6f * (1.f - pt.at(x, y)[3] * 0.0f);
        } else e.at(x, y)[3] = 1.f;
      }
    s.compositePatch(e, Blend::Erase, 1.f);
  }
};

// ---- shape erasers: rect / ellipse / line / lasso (applied on release) ------------------------
class ShapeEraser : public Brush {
 public:
  enum Mode { Rect, Ellipse, Line, Lasso };
  explicit ShapeEraser(Mode m) : mode_(m) {
    if (m == Line) addParam("largura", "Largura da linha", 10.f, 1.f, 120.f);
    addParam("suave", "Suavidade da borda (px)", 1.f, 0.5f, 30.f);
  }
  void begin(ISurface&, const StrokeParams& sp, const InputSample& in) override { sp_ = sp; pts_.clear(); pts_.push_back({in.x, in.y}); }
  void move(ISurface&, const InputSample& in) override {
    Vec2 p{in.x, in.y};
    if (mode_ == Lasso) { if (dist(pts_.back(), p) >= 3.f) pts_.push_back(p); }
    else if (pts_.size() == 1) pts_.push_back(p); else pts_[1] = p;
  }
  void end(ISurface& s) override {
    if (pts_.size() < 2) return;
    float soft = param("suave");
    if (mode_ == Lasso) { lasso(s, soft); return; }
    Vec2 a = pts_.front(), b = pts_.back();
    if (mode_ == Rect) {
      float x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x), y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
      eraseField(s, {(int)x0 - 2, (int)y0 - 2, (int)x1 + 3, (int)y1 + 3}, [&](float x, float y) {
        float cx = clampf((std::min(x + 0.5f, x1) - std::max(x - 0.5f, x0))), cy = clampf((std::min(y + 0.5f, y1) - std::max(y - 0.5f, y0)));
        (void)soft; return cx * cy;
      });
    } else if (mode_ == Ellipse) {
      Vec2 c = (a + b) * 0.5f; float ra = std::max(1.f, std::fabs(b.x - a.x) * 0.5f), rb = std::max(1.f, std::fabs(b.y - a.y) * 0.5f);
      eraseField(s, {(int)(c.x - ra) - 3, (int)(c.y - rb) - 3, (int)(c.x + ra) + 4, (int)(c.y + rb) + 4}, [&](float x, float y) {
        float d = std::hypot((x - c.x) / ra, (y - c.y) / rb);
        return clampf((1.f - d) * std::min(ra, rb) / soft + 0.5f);
      });
    } else {  // Line
      float w = param("largura") * 0.5f; Vec2 ab = b - a; float L2 = std::max(1e-4f, ab.len2());
      float m = w + soft + 3.f;
      eraseField(s, {(int)(std::min(a.x, b.x) - m), (int)(std::min(a.y, b.y) - m), (int)(std::max(a.x, b.x) + m), (int)(std::max(a.y, b.y) + m)}, [&](float x, float y) {
        float t = clampf(dot(Vec2{x, y} - a, ab) / L2);
        float d = dist(Vec2{x, y}, a + ab * t);
        return clampf((w - d) / soft + 0.5f);
      });
    }
  }
 private:
  void lasso(ISurface& s, float) {
    if (pts_.size() < 3) return;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (auto& p : pts_) { minx = std::min(minx, p.x); miny = std::min(miny, p.y); maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y); }
    int x0 = std::max(0, (int)minx - 1), y0 = std::max(0, (int)miny - 1), x1 = std::min(s.width(), (int)maxx + 2), y1 = std::min(s.height(), (int)maxy + 2);
    if (x1 <= x0 || y1 <= y0) return;
    Patch p(x0, y0, x1 - x0, y1 - y0);
    const int SS = 4; size_t n = pts_.size();
    std::vector<float> xs;
    for (int y = y0; y < y1; ++y) {
      for (int sy = 0; sy < SS; ++sy) {
        float yy = y + (sy + 0.5f) / SS; xs.clear();
        for (size_t i = 0; i < n; ++i) {
          Vec2 u = pts_[i], v = pts_[(i + 1) % n];
          if ((u.y <= yy) != (v.y <= yy)) xs.push_back(u.x + (yy - u.y) / (v.y - u.y) * (v.x - u.x));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
          float xa = std::max((float)x0, xs[k]), xb = std::min((float)x1, xs[k + 1]);
          if (xb <= xa) continue;
          for (int x = (int)xa; x <= (int)xb && x < x1; ++x) {
            float ov = std::min(xb, x + 1.f) - std::max(xa, (float)x);
            if (ov > 0) p.at(x, y)[3] += ov / SS;
          }
        }
      }
    }
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) { float& a = p.at(x, y)[3]; a = std::min(1.f, a); }
    s.compositePatch(p, Blend::Erase, 1.f);
  }
  Mode mode_; StrokeParams sp_; std::vector<Vec2> pts_;
};

// ---- multi-layer composite: several DabBrushes driven by the same input (halo, mirror, smudge+erase)
class MultiEraser : public Brush {
 public:
  struct Member { std::unique_ptr<DabBrush> b; float scale; bool mirror; bool recolor; };
  MultiEraser() {}
  void add(DabBrush* b, float scale = 1.f, bool mirror = false) { members_.push_back({std::unique_ptr<DabBrush>(b), scale, mirror, false}); if (mirror && params_.empty()) addParam("eixo", "Eixo de simetria (0-1)", 0.5f, 0.f, 1.f); }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    for (auto& m : members_) { StrokeParams q = sp; q.size = sp.size * m.scale; q.seed = sp.seed + (m.mirror ? 99u : 0u); m.b->begin(s, q, tf(m, s, in)); }
  }
  void move(ISurface& s, const InputSample& in) override { for (auto& m : members_) m.b->move(s, tf(m, s, in)); }
  void end(ISurface& s) override { for (auto& m : members_) m.b->end(s); }
  void hold(ISurface& s, float dt) override { for (auto& m : members_) m.b->hold(s, dt); }
 private:
  InputSample tf(const Member& m, ISurface& s, InputSample in) const {
    if (m.mirror) in.x = 2.f * s.width() * param("eixo") - in.x;
    return in;
  }
  std::vector<Member> members_;
};

// ---- erase-to-colour (paper layers): paints a flat colour instead of transparency ----------
class PaperEraser : public Brush {
 public:
  PaperEraser(BrushInfo i, Spec sp) : inner_(std::move(i), std::move(sp)) {
    addParam("r", "Papel R", 1.f, 0.f, 1.f); addParam("g", "Papel G", 1.f, 0.f, 1.f); addParam("b", "Papel B", 1.f, 0.f, 1.f);
  }
  void begin(ISurface& s, const StrokeParams& sp, const InputSample& in) override {
    StrokeParams q = sp; q.color = Color{param("r"), param("g"), param("b"), 1.f}; inner_.begin(s, q, in);
  }
  void move(ISurface& s, const InputSample& in) override { inner_.move(s, in); }
  void end(ISurface& s) override { inner_.end(s); }
  void hold(ISurface& s, float dt) override { inner_.hold(s, dt); }
 private:
  DabBrush inner_;
};

// ================================================================================================
void reg(BrushLibrary& lib, BrushInfo i, Spec s) { lib.addDab(std::move(i), std::move(s)); }
template <class T, class... A>
void regCustom(BrushLibrary& lib, BrushInfo i, A... a) {
  lib.add(std::move(i), [=]() -> std::unique_ptr<Brush> { return std::make_unique<T>(a...); });
}
std::unique_ptr<Brush> noop() { return nullptr; }

Spec eraseSpec(float hard = 1.f, float spacing = 0.12f) {
  Spec s; s.blend = Blend::Erase; s[Set::Hardness].set(hard); s[Set::Spacing].set(spacing); s[Set::Opacity].set(1.f); return s;
}
Spec stampSpec(const char* mask, float spacing, float opacity = 1.f) {
  Spec s = eraseSpec(1.f, spacing); s.stamp.maskName = mask; s[Set::Opacity].set(opacity); return s;
}

}  // namespace

void registerEraserBrushes(BrushLibrary& lib) {
  const char* C = "borracha";
  const char* CE = "borracha-especial";
  (void)noop;

  // ------------------------------------------------------------------ basicas
  { Spec s = eraseSpec(1.f, 0.08f); pressureSize(s, 0.3f, 1.f);
    reg(lib, mk("redonda-dura", "Borracha redonda dura", C, "uma borracha de vinil nova apaga com borda nitida e limpa, e o tamanho do contato muda com a pressao.", "dab circular de dureza 1 com anti-aliasing de 1px; raio ligado a pressao.", 14), s); }
  { Spec s = eraseSpec(0.f, 0.1f); pressureSize(s, 0.5f, 1.f);
    reg(lib, mk("redonda-macia", "Borracha redonda macia", C, "o miolo de uma borracha macia apaga por completo, mas a borda perde forca pouco a pouco porque o contato e menos firme.", "dab gaussiano (dureza 0) sem acumulo de borda: overlaps aprofundam o apagado.", 22, 2, 200), s); }
  { Spec s = eraseSpec(0.9f, 0.1f); s[Set::Opacity].set(0.15f).link(In::Pressure, Curve::linear(0.f, 0.85f)); s[Set::Radius].set(0.9f);
    reg(lib, mk("pressao-opacidade", "Borracha por pressao", C, "quanto mais forte voce aperta, mais pigmento sai; leve toque so clareia.", "tamanho fixo; opacidade do apagado vai de 15% a 100% com a pressao.", 16), s); }
  { Spec s = eraseSpec(1.f, 0.15f); s.buildUp = false; s[Set::Opacity].set(0.5f);
    reg(lib, mk("translucida-uniforme", "Borracha translucida 50%", C, "uma passada de borracha leve remove metade do pigmento, e passar de novo no mesmo ponto na mesma pincelada nao remove mais.", "dabs acumulados em buffer de traco (sem sobreposicao) e compostos uma unica vez a 50%; so uma nova pincelada apaga mais.", 18), s); }
  { Spec s = eraseSpec(0.85f, 0.1f); s[Set::SlowTracking].set(28.f); s[Set::Radius].set(0.9f);
    reg(lib, mk("estabilizada", "Borracha estabilizada", C, "uma borracha guiada por uma mao firme faz cortes limpos sem tremor.", "suavizacao exponencial da posicao do ponteiro (28px) antes de gerar os dabs.", 12), s); }
  { Spec s = eraseSpec(1.f, 0.1f); s[Set::Radius].set(0.35f).link(In::Speed1, Curve::linear(0.f, 1.3f)).link(In::Pressure, Curve::linear(0.f, 0.4f));
    reg(lib, mk("varredura", "Borracha de varredura", C, "um movimento rapido de braco limpa uma faixa larga; devagar apaga so um fio.", "raio cresce com a velocidade do traco (Speed1) e um pouco com a pressao.", 16), s); }
  { Spec s = eraseSpec(1.f, 0.1f); s[Set::Aspect].set(1.f).link(In::TiltDec, Curve::linear(0.f, 3.f)); s[Set::AngleDeg].set(0.f).link(In::TiltAsc, Curve::linear(0.f, 360.f));
    reg(lib, mk("inclinada", "Borracha inclinada (tilt)", C, "inclinar a borracha deita o corpo dela sobre o papel e alonga a area de contato na direcao da inclinacao.", "elipse cujo eixo maior cresce com a declinacao da caneta e aponta para o azimute da inclinacao.", 12), s); }
  { Spec s = eraseSpec(0.8f, 0.3f); s[Set::Opacity].set(0.08f); s[Set::DabsPerSecond].set(45.f); s[Set::Spacing].set(0.5f); s[Set::Hardness].set(0.f);
    BrushInfo i = mk("aerografo", "Borracha aerografo", C, "um jato de ar abrasivo remove pigmento aos poucos, mais ainda se voce fica parado.", "dabs gaussianos de baixa opacidade emitidos por tempo (45/s) alem da distancia; hold() continua apagando parado.", 26, 4, 200);
    i.demoHold = 0.8f; reg(lib, i, s); }
  { Spec s = eraseSpec(0.4f, 0.4f); s[Set::Opacity].set(0.55f); s[Set::DabsPerSecond].set(70.f); s[Set::Dissolve].set(0.55f); s[Set::JitterPos].set(0.8f); s[Set::Radius].set(1.f);
    s.stamp.maskName = "air_particle"; s.stamp.rot = RotMode::Random;
    BrushInfo i = mk("spray-granulado", "Borracha spray granulado", C, "um spray de grao fino respinga particulas; cada gotinha abre um buraquinho isolado.", "mascara air_particle girada ao acaso + dissolve de 55% + jitter, emitida por tempo.", 24, 4, 200);
    i.stampName = "air_particle"; i.demoHold = 0.6f; reg(lib, i, s); }

  // ------------------------------------------------------------------ papelaria
  { Spec s = stampSpec("flat_rect", 0.2f, 0.65f); s[Set::Aspect].set(1.2f); s.stamp.baseAngleDeg = 25.f; s.grainName = "@paper"; s[Set::GrainDepth].set(0.8f); s.pressureOpacityGrainBias = true;
    BrushInfo i = mk("lapis-vinil", "Borracha de lapis (vinil)", C, "o vinil retangular tem aresta reta; ela raspa o grafite pela textura do papel e deixa um fantasma claro nos vales.", "mascara flat_rect inclinada 25 graus + grao de papel gating o apagado; pressao empurra o apagado para dentro dos vales.", 12);
    i.stampName = "flat_rect"; reg(lib, i, s); }
  { Spec s = eraseSpec(0.3f, 0.3f); s[Set::Opacity].set(0.14f); s.grainName = "@rough"; s[Set::GrainDepth].set(0.35f); s[Set::Dissolve].set(0.0f);
    reg(lib, mk("maleavel", "Borracha maleavel (kneaded)", C, "a massinha cinza levanta o pigmento aos poucos, nunca tira tudo de uma vez; ao pressionar varias vezes, vai clareando.", "opacidade de 14% por dab com acumulo, dureza baixa e grao suave: cada passagem clareia mais.", 20), s); }
  { Spec s = stampSpec("circle_rough", 0.5f, 0.75f); s.stamp.rot = RotMode::Random; s.stamp.scaleJitter = 0.25f; s[Set::Dissolve].set(0.18f); s[Set::JitterPos].set(0.3f);
    BrushInfo i = mk("borracha-goma", "Borracha de goma esfarelando", C, "a borracha-goma se desfaz em migalhas ao esfregar, deixando a area apagada irregular.", "mascara circle_rough com rotacao e escala aleatorias, dissolve e jitter de posicao.", 16);
    i.stampName = "circle_rough"; reg(lib, i, s); }
  { Spec s = stampSpec("flat_rect", 0.1f); s[Set::Aspect].set(2.8f); s.stamp.baseAngleDeg = 40.f; s[Set::Radius].set(0.45f).link(In::Pressure, Curve::linear(0.f, 0.15f));
    BrushInfo i = mk("precisao-caneta", "Borracha de precisao (stick)", C, "a borracha em formato de caneta tem ponta retangular fina para corrigir detalhes pequenos.", "mascara retangular alongada (aspecto 2.8) fixa a 40 graus; raio pequeno.", 6, 1, 40);
    i.stampName = "flat_rect"; reg(lib, i, s); }
  { Spec s = eraseSpec(1.f, 0.25f); s[Set::JitterPos].set(0.35f); s[Set::Radius].set(0.55f); s[Set::Opacity].set(0.9f); s[Set::DabsPerSecond].set(90.f); s[Set::Spacing].set(0.2f);
    BrushInfo i = mk("eletrica", "Borracha eletrica", C, "a borracha eletrica gira e vibra, apagando rapido numa area pequena com cantos irregulares.", "dabs duros de raio pequeno com vibracao aleatoria (jitter 35%) e emissao temporal de 90 dabs/s.", 8, 2, 60);
    i.demoHold = 0.3f; reg(lib, i, s); }
  { Spec s = stampSpec("flat_rect", 0.12f); s[Set::Aspect].set(3.5f); s.stamp.baseAngleDeg = 45.f; s[Set::Radius].set(1.f).link(In::Pressure, Curve::linear(0.f, 0.2f));
    BrushInfo i = mk("cinzel-45", "Borracha cinzel fixa (45 graus)", C, "uma borracha larga e achatada usada de lado apaga faixas retas com aresta diagonal.", "retangulo largo (aspecto 3.5) com angulo fixo de 45 graus, independente da direcao do traco.", 12);
    i.stampName = "flat_rect"; reg(lib, i, s); }
  { Spec s = stampSpec("flat_rect", 0.15f); s[Set::Aspect].set(3.f); s.stamp.rot = RotMode::DirectionPerp; s[Set::Radius].set(0.9f);
    BrushInfo i = mk("cinzel-direcional", "Borracha cinzel direcional", C, "ao varrer com a borracha de lado, a aresta fica sempre perpendicular ao movimento.", "retangulo largo que gira para ficar perpendicular a direcao do traco.", 12);
    i.stampName = "flat_rect"; reg(lib, i, s); }

  // ------------------------------------------------------------------ midias
  { Spec s = stampSpec("chalk_pastel_soft", 0.2f, 0.55f); s[Set::Hardness].set(1.f); s.stamp.rot = RotMode::Random; s.grainName = "@fibers"; s[Set::GrainDepth].set(1.f); s[Set::Dissolve].set(0.1f);
    BrushInfo i = mk("feltro-giz", "Apagador de feltro (lousa)", C, "o apagador de feltro nao limpa a lousa por completo: espalha po de giz e deixa uma nuvem de fibras.", "mascara chalk_pastel_soft girada ao acaso, grao de fibras e dissolve: apaga so parte do giz.", 22, 4, 150);
    i.stampName = "chalk_pastel_soft"; reg(lib, i, s); }
  { Spec s = eraseSpec(0.88f, 0.1f); s[Set::Opacity].set(0.95f); s.grainName = "@paper"; s[Set::GrainDepth].set(0.9f); s[Set::Dissolve].set(0.12f); s[Set::JitterPos].set(0.08f);
    s.stamp.maskName = "pen_fade"; s[Set::Hardness].set(0.9f);
    BrushInfo i = mk("tinta-abrasiva", "Borracha de tinta (abrasiva)", C, "a borracha de tinta tem abrasivo que lixa a superficie do papel: o corte e aspero e raspado.", "dab quase duro com grao de papel de alto contraste, dissolve e jitter: borda serrilhada.", 8, 2, 80);
    i.stampName = "pen_fade"; reg(lib, i, s); }
  { Spec s = stampSpec("water_bleed", 0.3f, 0.3f); s.stamp.rot = RotMode::Random; s[Set::JitterPos].set(0.2f); s[Set::Radius].set(1.1f);
    BrushInfo i = mk("aquarela-clarear", "Borracha de aquarela (clarear)", C, "tirar aquarela com pano umido levanta o pigmento de forma desigual, com manchas e bordas de flor.", "mascara water_bleed aleatoria com baixa opacidade acumulativa: clareia com bordas organicas.", 26, 6, 160);
    i.stampName = "water_bleed"; reg(lib, i, s); }
  { Spec s = stampSpec("pencil_charcoal", 0.25f, 0.4f); s.stamp.rot = RotMode::Direction; s.grainName = "@fibers"; s[Set::GrainDepth].set(0.8f);
    BrushInfo i = mk("carvao-levantar", "Borracha de carvao (levantar)", C, "para levantar carvao, passa-se a borracha macia em dabs: o po sai pela fibra do papel e deixa reflexos.", "mascara pencil_charcoal orientada pela direcao + grao de fibras, opacidade 40%.", 20, 4, 150);
    i.stampName = "pencil_charcoal"; reg(lib, i, s); }
  { Spec s = stampSpec("gauze01", 0.25f, 0.5f); s.stamp.rot = RotMode::Random;
    BrushInfo i = mk("gaze-tecido", "Borracha de gaze (esfregar)", C, "esfregar com gaze deixa a marca da trama do tecido na area apagada.", "mascara gauze01 girada ao acaso: apaga seguindo a trama.", 22, 6, 150);
    i.stampName = "gauze01"; reg(lib, i, s); }
  { Spec s = eraseSpec(1.f, 0.4f); s.grainName = "@canvas"; s[Set::GrainDepth].set(1.f); s[Set::Opacity].set(0.85f);
    reg(lib, mk("tela-trama", "Borracha de trama de tela", C, "numa tela de pintura a borracha so alcanca os fios altos da trama; os vales ficam com tinta.", "grao @canvas (trama) com profundidade total: apaga apenas as cristas da trama.", 16, 4, 120), s); }
  { Spec s = eraseSpec(0.6f, 0.3f); s[Set::Opacity].set(0.4f); s.stamp.maskName = "chalk01"; s.stamp.rot = RotMode::Random; s.stamp.scaleJitter = 0.2f;
    BrushInfo i = mk("pastel-esfumada", "Borracha de pastel (nuvem)", C, "o pastel seco sai em nuvens de po quando se esfrega com os dedos ou um chumaco.", "mascara chalk01 aleatoria com 40% de opacidade acumulativa.", 24, 6, 160);
    i.stampName = "chalk01"; reg(lib, i, s); }

  // ------------------------------------------------------------------ texturas
  { Spec s = stampSpec("chalk_hard", 0.35f, 0.9f); s.stamp.rot = RotMode::Direction; s[Set::Aspect].set(1.f);
    BrushInfo i = mk("textura-giz-duro", "Borracha textura giz duro", CE, "uma lixa fina deixa a area com a rugosidade do abrasivo.", "mascara chalk_hard retangular rugosa alinhada ao traco.", 14);
    i.stampName = "chalk_hard"; reg(lib, i, s); }
  { Spec s = stampSpec("circle_rough", 0.35f, 0.9f); s.stamp.rot = RotMode::Random;
    BrushInfo i = mk("textura-circulo-aspero", "Borracha circulo aspero", CE, "pressionar uma esponja natural deixa um circulo de bordas porosas.", "mascara circle_rough com rotacao aleatoria.", 16);
    i.stampName = "circle_rough"; reg(lib, i, s); }
  { Spec s = stampSpec("points03", 0.5f, 1.f); s.stamp.rot = RotMode::Random; s[Set::JitterPos].set(0.2f);
    BrushInfo i = mk("pontilhada", "Borracha pontilhada", CE, "carimbar com um rolo de espinhos fura o pigmento em milhares de pontos.", "mascara points03 (poeira de pontos) rotacionada ao acaso.", 18);
    i.stampName = "points03"; reg(lib, i, s); }
  { Spec s = stampSpec("sputtering02", 0.4f, 1.f); s.stamp.rot = RotMode::Random; s.stamp.count = 2; s.stamp.scatter = 0.5f; s.stamp.scaleJitter = 0.3f;
    BrushInfo i = mk("respingo", "Borracha de respingos", CE, "respingos de solvente comem o pigmento em gotinhas de tamanhos diferentes.", "mascara sputtering02, 2 copias espalhadas por dab com escala aleatoria.", 20);
    i.stampName = "sputtering02"; reg(lib, i, s); }
  { Spec s = stampSpec("sputtering01", 0.5f, 1.f); s.stamp.rot = RotMode::Random; s.stamp.count = 2; s.stamp.scatter = 0.6f;
    BrushInfo i = mk("respingo-grosso", "Borracha respingo grosso", CE, "gotas grandes de agua sanitaria abrem furos redondos isolados.", "mascara sputtering01 (gotas grandes) com 2 copias espalhadas.", 22);
    i.stampName = "sputtering01"; reg(lib, i, s); }
  { Spec s = stampSpec("fur01", 0.4f, 1.f); s.stamp.rot = RotMode::Direction; s[Set::Aspect].set(1.f); s.stamp.mirror = MirrorMode::Random;
    BrushInfo i = mk("pelo", "Borracha de pelo", CE, "um pincel seco de pelo arrasta e abre mechas irregulares na tinta.", "mascara fur01 orientada pela direcao e espelhada ao acaso.", 22);
    i.stampName = "fur01"; reg(lib, i, s); }
  { Spec s = stampSpec("net_lace", 0.9f, 1.f); s.stamp.rot = RotMode::Direction;
    BrushInfo i = mk("renda", "Borracha de renda", CE, "apagar atraves de uma renda como estencil deixa o desenho do bordado vazado.", "mascara net_lace usada como estencil de apagado, alinhada a direcao.", 24);
    i.stampName = "net_lace"; reg(lib, i, s); }
  { Spec s = stampSpec("net2", 0.9f, 1.f); s.stamp.baseAngleDeg = 0.f;
    BrushInfo i = mk("grade", "Borracha de grade", CE, "apagar por uma tela metalica deixa uma grade quadriculada de furos.", "mascara net2 (grade) com angulo fixo.", 22);
    i.stampName = "net2"; reg(lib, i, s); }
  { Spec s = stampSpec("stamp_star", 1.1f, 1.f); s.stamp.rot = RotMode::Random; s.stamp.scaleJitter = 0.2f;
    BrushInfo i = mk("estrela", "Borracha de estrela", CE, "um vazador de papel em forma de estrela corta buracos com o desenho dele.", "mascara stamp_star carimbada com rotacao aleatoria.", 14);
    i.stampName = "stamp_star"; reg(lib, i, s); }
  { Spec s = stampSpec("stamp_leaf1", 1.0f, 1.f); s.stamp.rot = RotMode::DirectionPerp; s.stamp.mirror = MirrorMode::Alternate; s.stamp.scaleJitter = 0.2f;
    BrushInfo i = mk("folha", "Borracha de folha", CE, "um vazador em forma de folha deixa buracos com contorno de folha.", "mascara stamp_leaf1 que acompanha a direcao e alterna o espelho.", 14);
    i.stampName = "stamp_leaf1"; reg(lib, i, s); }
  { Spec s = stampSpec("stamp_heart", 1.2f, 1.f); s.stamp.baseAngleDeg = 0.f; s.stamp.upright = true; s.stamp.invert = false;
    BrushInfo i = mk("coracao", "Borracha de coracao", CE, "um vazador de coracao abre furos em forma de coracao, sempre em pe.", "mascara stamp_heart sem rotacao.", 14);
    i.stampName = "stamp_heart"; reg(lib, i, s); }
  { Spec s = stampSpec("stamp_snow1", 1.1f, 1.f); s.stamp.rot = RotMode::Random; s.stamp.scaleJitter = 0.3f;
    BrushInfo i = mk("floco-de-neve", "Borracha floco de neve", CE, "um vazador de floco de neve corta cristais de seis pontas.", "mascara stamp_snow1 com rotacao e escala aleatorias.", 16);
    i.stampName = "stamp_snow1"; reg(lib, i, s); }
  { Spec s = stampSpec("stamp_drop", 1.0f, 1.f); s.stamp.rot = RotMode::DirectionPerp; s.stamp.baseAngleDeg = 180.f;
    BrushInfo i = mk("gota", "Borracha de gota", CE, "gotas de solvente caem e comem a tinta no formato de pingos.", "mascara stamp_drop orientada pela direcao do traco.", 12);
    i.stampName = "stamp_drop"; reg(lib, i, s); }

  // ------------------------------------------------------------------ analise de cor/borda
  { Spec s = eraseSpec(0.9f, 0.1f); s.blend = Blend::Dissolve; s[Set::Opacity].set(0.6f); s[Set::Hardness].set(0.2f); s[Set::Spacing].set(0.25f);
    reg(lib, mk("dissolver", "Borracha dissolver", CE, "um acido fraco corroi uma imagem em pontos aleatorios ate sumir.", "Blend::Dissolve: cada pixel e removido se um ruido hash for menor que a cobertura do dab: derrete em pontilhado.", 24, 4, 150), s); }
  { Spec s = eraseSpec(1.f, 0.2f); s.blend = Blend::EraseEdge; s.edgeLimit = 0.8f;
    reg(lib, mk("halo-bordas", "Borracha de halo (bordas)", CE, "a franja clara em volta de uma selecao feita com varinha-magica precisa ser tirada sem tocar o miolo.", "Blend::EraseEdge: so apaga pixels semi-transparentes (alpha < limite).", 24, 4, 150), s); }
  { Spec s = eraseSpec(1.f, 0.12f); s.blend = Blend::EraseDark; s.tolerance = 0.3f;
    reg(lib, mk("so-linha", "Borracha de linha (tinta escura)", CE, "apagar so o contorno preto de um desenho sem estragar as cores e como usar um solvente seletivo.", "Blend::EraseDark: o peso do apagado vem da escuridao do pixel; cores claras sobrevivem.", 14), s); }
  { Spec s = eraseSpec(1.f, 0.12f); s.blend = Blend::EraseLight; s.tolerance = 0.3f;
    reg(lib, mk("so-claro", "Borracha de areas claras", CE, "remover apenas o branco de um scan sem afetar o desenho escuro.", "Blend::EraseLight: apaga pixels claros (luminancia alta) e preserva os escuros.", 18), s); }
  regCustom<ColorRangeEraser>(lib, mk("cor-similar", "Borracha de cor similar (magica)", CE, "um corretor que remove so uma cor: toca-se uma vez e ele passa a apagar tudo que se parece com ela.", "amostra a cor sob o primeiro dab e usa Blend::EraseSimilar com tolerancia ajustavel.", 16), ColorRangeEraser::Magic);
  regCustom<ColorRangeEraser>(lib, mk("cor-continua", "Borracha de cor continua", CE, "uma borracha que a cada passo olha de novo o que esta sob a ponta e remove cores parecidas com ela.", "reamostra a cor no centro a cada dab e aplica EraseSimilar: acompanha gradientes.", 16), ColorRangeEraser::Continuous);
  regCustom<ColorRangeEraser>(lib, mk("fundo", "Borracha de fundo", CE, "apagar o fundo de uma foto sem comer o contorno: o centro define o fundo e as linhas escuras sao protegidas.", "reamostra o centro como cor de fundo, ignora amostras escuras ou transparentes e aplica EraseSimilar.", 20), ColorRangeEraser::Background);
  { Spec s; s.blend = Blend::Normal; s[Set::Hardness].set(1.f); s[Set::Spacing].set(0.1f); pressureSize(s, 0.4f, 1.f);
    BrushInfo i = mk("para-cor-papel", "Borracha de cor de papel", CE, "o corretivo liquido nao remove tinta, cobre com a cor do papel.", "pinta uma cor chapada (parametros R/G/B, padrao branco) em vez de transparencia: ideal para camadas opacas.", 16);
    i.isEraser = true;
    lib.add(i, [i, s]() -> std::unique_ptr<Brush> { return std::make_unique<PaperEraser>(i, s); }); }
  { Spec s = eraseSpec(1.f, 0.12f); s.blend = Blend::Soften; s[Set::Opacity].set(0.7f); s[Set::Hardness].set(0.5f);
    reg(lib, mk("suavizar-bordas", "Borracha suavizar (feather)", CE, "um pano limpo passado de leve sobre uma borda dura a esfuma.", "Blend::Soften: mistura cada pixel com a media 5x5 local dentro do dab.", 18), s); }

  // ------------------------------------------------------------------ composicoes (varios dabs)
  { BrushInfo i = mk("arrastar-apagar", "Borracha arrasto (smudge-erase)", CE, "esfregar com o dedo puxa a tinta pelo papel e ao mesmo tempo a leva embora.", "arrasta pixels (Smudge de balde longo) e apaga 15% do alpha por dab, entao a mancha vai sumindo.", 18);
    lib.add(i, []() -> std::unique_ptr<Brush> {
      auto m = std::make_unique<MultiEraser>();
      Spec a; a[Set::Hardness].set(0.5f); a[Set::Smudge].set(1.f); a[Set::SmudgeLength].set(0.92f); a[Set::Opacity].set(0.85f); a[Set::Spacing].set(0.12f);
      Spec b = eraseSpec(0.6f, 0.12f); b[Set::Opacity].set(0.15f);
      BrushInfo ia; ia.id = "borracha.arrastar-apagar.a"; BrushInfo ib = ia;
      m->add(new DabBrush(ia, a)); m->add(new DabBrush(ib, b)); return m; }); }
  { BrushInfo i = mk("halo-sombra", "Borracha com halo (sombra suave)", CE, "uma ponta apaga o centro por completo e ao redor deixa um anel de sombra que desvanece.", "dois dabs por passo: nucleo duro a 55% do raio + anel gaussiano a 135% com 35% de opacidade.", 14);
    lib.add(i, []() -> std::unique_ptr<Brush> {
      auto m = std::make_unique<MultiEraser>();
      Spec a = eraseSpec(1.f, 0.1f); Spec b = eraseSpec(0.f, 0.15f); b[Set::Opacity].set(0.35f);
      BrushInfo ia; m->add(new DabBrush(ia, a), 0.55f); m->add(new DabBrush(ia, b), 1.35f); return m; }); }
  { BrushInfo i = mk("simetrica", "Borracha simetrica (espelho)", CE, "ao desenhar simetria, cada corte numa metade se repete na outra.", "segundo DabBrush alimentado com a posicao refletida no eixo vertical (parametro eixo).", 14);
    lib.add(i, []() -> std::unique_ptr<Brush> {
      auto m = std::make_unique<MultiEraser>();
      Spec a = eraseSpec(0.95f, 0.1f); pressureSize(a, 0.4f, 1.f);
      BrushInfo ia; m->add(new DabBrush(ia, a)); m->add(new DabBrush(ia, a), 1.f, true); return m; }); }

  // ------------------------------------------------------------------ custom (comportamento proprio)
  regCustom<ShieldEraser>(lib, mk("regua-protetora", "Borracha com regua (shield)", CE, "o gabarito de metal protege o desenho: a borracha passa por cima e so apaga de um lado da aresta.", "a area apagada e recortada por uma reta (angulo/posicao ajustaveis) com anti-aliasing: o lado onde o traco comecou apaga, o outro e protegido.", 16));
  regCustom<WetLiftEraser>(lib, mk("aquarela-lavar", "Borracha de aquarela (lavar)", CE, "ao lavar a aquarela, a agua leva o pigmento em poças irregulares e nunca deixa o papel perfeitamente limpo.", "campo de ruido fbm com warp: manchas suaves cuja borda e retraida em direcao a borda do dab; limpeza maxima 80%.", 28, 6, 160));
  regCustom<PixelEraser>(lib, mk("pixel", "Borracha de pixel (quadrada)", CE, "no pixel art apaga-se pixel a pixel, sem bordas suaves.", "apaga celulas quadradas alinhadas a grade do tamanho de pixel escolhido, sem anti-aliasing.", 8, 1, 60));
  regCustom<BristleEraser>(lib, mk("cerdas", "Borracha de cerdas (pincel seco)", CE, "um pincel seco embebido em solvente arrasta fios finos paralelos, cada um com carga diferente.", "N linhas de apagado finas, paralelas a direcao, com opacidade e largura individuais moduladas por ruido 1D ao longo do traco.", 18, 4, 100));
  regCustom<ScratchEraser>(lib, mk("riscar", "Borracha de riscos (lamina)", CE, "uma lamina ou agulha raspa linhas finissimas na tinta seca, com falhas quando levanta.", "poucos riscos duros de 1px acompanhando a direcao, com deriva lateral por ruido e interrupcoes.", 16, 4, 80));
  regCustom<RegionEraser>(lib, mk("regiao-conectada", "Borracha de regiao (balde de apagar)", CE, "apagar uma forma inteira de uma vez, como puxar uma pecinha de papel colorido do colagem.", "flood-fill 4-conectado em janela limitada a partir do pixel sob a ponta: apaga a regiao de cor parecida e uma franja de 1px.", 10, 2, 40));
  regCustom<ShapeEraser>(lib, mk("retangulo", "Borracha retangular", CE, "um estilete e uma regua cortam um retangulo perfeito de papel.", "apaga o retangulo definido pelo ponto inicial e final do gesto ao soltar.", 10), ShapeEraser::Rect);
  regCustom<ShapeEraser>(lib, mk("elipse", "Borracha elipse", CE, "um compasso de corte remove um disco ou oval.", "apaga a elipse inscrita no retangulo do gesto, com borda anti-aliased.", 10), ShapeEraser::Ellipse);
  regCustom<ShapeEraser>(lib, mk("linha-reta", "Borracha de linha reta", CE, "um estilete numa regua faz um corte reto de uma ponta a outra.", "apaga um capsulo de largura ajustavel entre o primeiro e o ultimo ponto.", 10), ShapeEraser::Line);
  regCustom<ShapeEraser>(lib, mk("laco", "Borracha de laco (lasso)", CE, "recortar a mao livre com uma tesoura em volta de uma forma.", "o gesto vira um poligono fechado; ao soltar apaga o interior por scanline 4x com cobertura exata.", 10), ShapeEraser::Lasso);

  // ------------------------------------------------------------------ padroes
  { Spec s = eraseSpec(1.f, 1.6f); s[Set::Radius].set(0.5f);
    reg(lib, mk("tracejada-pontos", "Borracha pontilhada (pontos)", CE, "uma roda de picotar fura papel em pontos regulares.", "dabs duros espacados 1.6 raios: linha de furos redondos.", 10, 2, 60), s); }
  { Spec s = eraseSpec(1.f, 1.5f); s[Set::Aspect].set(2.4f); s.stamp.rot = RotMode::Direction; s[Set::Radius].set(0.45f);
    reg(lib, mk("tracejada-traco", "Borracha tracejada", CE, "um cortador de pizza com serrilha recorta tracos curtos alinhados.", "elipses alongadas orientadas pela direcao com espacamento maior que o comprimento.", 10, 2, 60), s); }
  { Spec s = eraseSpec(1.f, 0.4f); s.grainName = "@dots"; s[Set::GrainDepth].set(1.f); s[Set::Opacity].set(1.f);
    reg(lib, mk("retícula", "Borracha de reticula (halftone)", CE, "apagar por uma tela de pontos deixa a imagem como impressao de jornal.", "grao @dots (pontos regulares) gating do apagado: sobra uma trama de pontos.", 22, 6, 150), s); }

  // ------------------------------------------------------------------ afilada / fade
  { Spec s = eraseSpec(0.9f, 0.1f); s[Set::Opacity].set(1.f).link(In::Length, Curve{{0.f, 0.f}, {0.7f, -1.f}}); s[Set::Radius].set(1.f);
    reg(lib, mk("fade", "Borracha fade (desvanecer)", C, "uma borracha que perde a forca: no inicio apaga tudo e vai deixando cada vez mais pigmento ate parar de apagar.", "a opacidade cai linearmente de 100% a 0% ao longo de 350px de traco (input Length).", 16), s); }
  { Spec s = eraseSpec(1.f, 0.08f); s[Set::Radius].set(0.15f).link(In::Length, Curve{{0.f, 0.f}, {0.08f, 0.9f}}).link(In::Pressure, Curve::linear(0.f, 0.35f));
    reg(lib, mk("afilada", "Borracha afilada (taper)", C, "uma ponta macia comeca fina, engrossa e termina fina como pincelada.", "raio sobe nos primeiros 40px de traco e depende da pressao no fim.", 16), s); }
  { Spec s = eraseSpec(0.9f, 0.15f); s.stamp.maskName = "oil_hair"; s.stamp.rot = RotMode::Direction; s[Set::Opacity].set(0.8f);
    BrushInfo i = mk("pincel-oleo-seco", "Borracha de pincel oleo", C, "um pincel duro de oleo raspa a tinta e deixa sulcos das cerdas.", "mascara oil_hair orientada pela direcao do traco.", 18);
    i.stampName = "oil_hair"; reg(lib, i, s); }
  { Spec s = eraseSpec(1.f, 0.5f); s.stamp.maskName = "chalk_crayon"; s.stamp.rot = RotMode::Random; s[Set::Opacity].set(0.8f); s.stamp.scaleJitter = 0.2f;
    BrushInfo i = mk("giz-cera", "Borracha giz de cera", C, "raspar giz de cera com uma unha deixa lascas irregulares.", "mascara chalk_crayon girada ao acaso.", 18);
    i.stampName = "chalk_crayon"; reg(lib, i, s); }
  { Spec s = eraseSpec(0.7f, 0.9f); s.stamp.count = 6; s.stamp.scatter = 1.1f; s.stamp.scaleJitter = 0.9f; s[Set::Hardness].set(0.7f); s[Set::Opacity].set(0.9f); s[Set::JitterRadius].set(0.5f);
    reg(lib, mk("migalhas", "Borracha de migalhas", CE, "pedacinhos de borracha esfarelando espalham buracos de tamanhos diferentes.", "6 dabs por passo espalhados em 1.1 raio com escala variando 90%.", 14), s); }
}

}  // namespace ab
