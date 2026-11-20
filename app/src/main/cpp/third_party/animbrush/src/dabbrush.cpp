#include "../include/animbrush/dabbrush.hpp"

namespace ab {

DabBrushSpec::DabBrushSpec() {
  (*this)[Set::Opacity].set(1.f);
  (*this)[Set::Radius].set(1.f);
  (*this)[Set::Hardness].set(0.8f);
  (*this)[Set::Aspect].set(1.f);
  (*this)[Set::Spacing].set(0.15f);
  (*this)[Set::SmudgeLength].set(0.5f);
  (*this)[Set::SmudgeRadius].set(1.f);
}

DabBrush::DabBrush(BrushInfo info, DabBrushSpec spec) : spec_(std::move(spec)) { info_ = std::move(info); }

static inline float normSpeed(float v) { return 1.f - std::exp(-v / 900.f); }

void DabBrush::inputsFor(float* in) const {
  float dirDeg = std::atan2(dir_.y, dir_.x) * 180.f / kPi;
  while (dirDeg < 0) dirDeg += 180.f;
  while (dirDeg >= 180.f) dirDeg -= 180.f;
  float tdec = clampf(std::sqrt(tiltX_ * tiltX_ + tiltY_ * tiltY_));
  float tasc = std::atan2(tiltY_, tiltX_) / kPi * 0.5f + 0.5f;
  in[(int)In::Pressure] = pressure_;
  in[(int)In::Speed1] = normSpeed(speed1_);
  in[(int)In::Speed2] = normSpeed(speed2_);
  in[(int)In::Random] = 0.f;  // filled per dab
  in[(int)In::Stroke] = clampf(strokeTime_ / 1.5f);
  in[(int)In::Direction] = dirDeg / 180.f;
  in[(int)In::TiltDec] = tdec;
  in[(int)In::TiltAsc] = tasc;
  in[(int)In::Length] = clampf(strokeLen_ / 500.f);
}

float DabBrush::eval(Set s, const float* inputs) const {
  const Mapping& m = spec_.set[(int)s];
  float v = m.base;
  for (const auto& l : m.links) v += l.curve.eval(inputs[(int)l.input]);
  return v;
}

void DabBrush::begin(ISurface& s, const StrokeParams& sp, const InputSample& in) {
  sp_ = sp; rng_.reseed(sp.seed); active_ = true; target_ = &s;
  cur_ = last_ = filt_ = {in.x, in.y};
  pressure_ = in.pressure; tiltX_ = in.tiltX; tiltY_ = in.tiltY;
  distCarry_ = timeCarry_ = strokeLen_ = strokeTime_ = 0; speed1_ = speed2_ = 0;
  dir_ = {1, 0}; dabIndex_ = 0; smudgeInit_ = false; smudgeBucket_ = {0, 0, 0, 0};
  auto& bank = StampBank::instance();
  shape_ = spec_.stamp.maskName.empty() ? nullptr : bank.get(spec_.stamp.maskName);
  grain_ = spec_.grainName.empty() ? nullptr : bank.get(spec_.grainName);
  buffer_.reset();
  if (!spec_.buildUp) {
    buffer_ = std::make_unique<RasterSurface>(s.width(), s.height());
    base_ = s.readPatch({0, 0, s.width(), s.height()});
  }
  stepDirty_ = {};
  hasStepDirty_ = false;
  emitDab(s, cur_, pressure_, 0.f);
  flushBuffer(s);
}

void DabBrush::step(ISurface& s, const InputSample& in, bool) {
  float dt = std::max(1e-4f, in.dt);
  strokeTime_ += dt;
  pressure_ = in.pressure; tiltX_ = in.tiltX; tiltY_ = in.tiltY;
  cur_ = {in.x, in.y};
  float inputs[(int)In::Count_];
  inputsFor(inputs);
  float tr = std::max(0.f, eval(Set::SlowTracking, inputs));
  Vec2 prevFilt = filt_;
  if (tr > 0.01f) {
    float tau = tr * 0.004f;
    float k = 1.f - std::exp(-dt / tau);
    filt_ = filt_ + (cur_ - filt_) * k;
  } else {
    filt_ = cur_;
  }
  Vec2 seg = filt_ - prevFilt;
  float segLen = seg.len();
  float v = segLen / dt;
  speed1_ += (v - speed1_) * (1.f - std::exp(-dt / 0.04f));
  speed2_ += (v - speed2_) * (1.f - std::exp(-dt / 0.25f));
  if (segLen > 0.05f) dir_ = (dir_ * 0.6f + seg.norm() * 0.4f).norm();

  // time based dabs (airbrush)
  inputsFor(inputs);
  float dps = eval(Set::DabsPerSecond, inputs);
  if (dps > 0) {
    timeCarry_ += dt * dps;
    while (timeCarry_ >= 1.f) { timeCarry_ -= 1.f; emitDab(s, filt_, pressure_, 1.f / dps); }
  }

  // distance based dabs
  float prevP = pressure_;
  Vec2 a = last_, b = filt_;
  float L = dist(a, b);
  if (L > 1e-4f) {
    float radius0 = sp_.size * std::max(0.02f, eval(Set::Radius, inputs));
    float spacing = std::max(0.4f, std::max(0.03f, eval(Set::Spacing, inputs)) * radius0);
    float pos = spacing - distCarry_;
    while (pos <= L) {
      float t = pos / L;
      Vec2 p = lerp(a, b, t);
      emitDab(s, p, lerpf(prevP, pressure_, t), 0.f);
      inputsFor(inputs);
      radius0 = sp_.size * std::max(0.02f, eval(Set::Radius, inputs));
      spacing = std::max(0.4f, std::max(0.03f, eval(Set::Spacing, inputs)) * radius0);
      pos += spacing;
    }
    distCarry_ = L - (pos - spacing);
    if (distCarry_ < 0) distCarry_ = 0;
    strokeLen_ += L;
    last_ = b;
  }
}

void DabBrush::move(ISurface& s, const InputSample& in) {
  if (!active_) return;
  step(s, in, false);
  flushBuffer(s);
}

void DabBrush::hold(ISurface& s, float dt) {
  if (!active_) return;
  float inputs[(int)In::Count_];
  strokeTime_ += dt;
  inputsFor(inputs);
  float dps = eval(Set::DabsPerSecond, inputs);
  if (dps > 0) {
    timeCarry_ += dt * dps;
    while (timeCarry_ >= 1.f) { timeCarry_ -= 1.f; emitDab(s, filt_, pressure_, 1.f / dps); }
  }
  flushBuffer(s);
}

void DabBrush::end(ISurface& s) {
  (void)s;
  active_ = false;
  buffer_.reset();
  base_ = Patch();
}

void DabBrush::flushBuffer(ISurface& s) {
  if (spec_.buildUp || !buffer_ || !hasStepDirty_) { hasStepDirty_ = false; return; }
  IRect r = stepDirty_;
  r.x0 = std::max(0, r.x0); r.y0 = std::max(0, r.y0); r.x1 = std::min(s.width(), r.x1); r.y1 = std::min(s.height(), r.y1);
  if (!r.empty()) {
    // restore the pre-stroke pixels of the region, then composite the accumulated stroke buffer over it
    Patch orig(r.x0, r.y0, r.w(), r.h());
    for (int y = r.y0; y < r.y1; ++y)
      for (int x = r.x0; x < r.x1; ++x)
        std::copy(base_.at(x, y), base_.at(x, y) + 4, orig.at(x, y));
    s.compositePatch(orig, Blend::Smudge, 1.f);
    Patch layer = buffer_->readPatch(r);
    s.compositePatch(layer, spec_.blend, sp_.opacity * clampf(eval(Set::Opacity, bufInputs_)));
  }
  hasStepDirty_ = false;
}

void DabBrush::emitDab(ISurface& s, Vec2 pos, float pressure, float) {
  const StampOptions& st = spec_.stamp;
  float inputs[(int)In::Count_];
  pressure_ = pressure;
  inputsFor(inputs);
  int copies = std::max(1, st.count);
  for (int c = 0; c < copies; ++c) {
    inputs[(int)In::Random] = rng_.uniform();
    for (int i = 0; i < (int)In::Count_; ++i) bufInputs_[i] = inputs[i];

    float radius = sp_.size * std::max(0.02f, eval(Set::Radius, inputs));
    float jr = eval(Set::JitterRadius, inputs);
    if (jr > 0) radius *= std::max(0.2f, 1.f + rng_.gauss() * jr * 0.5f);
    if (st.scaleJitter > 0) radius *= 1.f + rng_.sym() * st.scaleJitter;
    radius = std::max(0.15f, radius);

    Vec2 p = pos;
    float jp = eval(Set::JitterPos, inputs);
    if (jp > 0) p += rng_.inDisc() * (jp * radius);
    if (st.scatter > 0) p += rng_.inDisc() * (st.scatter * radius);
    float obs = eval(Set::OffsetBySpeed, inputs);
    if (obs != 0) p += dir_ * (obs * normSpeed(speed1_));
    if (st.sideAlternate != 0) {
      float side = (dabIndex_ & 1) ? 1.f : -1.f;
      p += dir_.perp() * (side * st.sideAlternate * radius);
    }

    float dirAng = std::atan2(dir_.y, dir_.x);
    float ang = (st.baseAngleDeg + eval(Set::AngleDeg, inputs)) * kDeg2Rad;
    switch (st.rot) {
      case RotMode::Fixed: break;
      case RotMode::Random: ang += rng_.uniform() * kTau; break;
      case RotMode::Direction: ang += dirAng; break;
      case RotMode::DirectionPerp: ang += dirAng + kPi * 0.5f; break;
      case RotMode::Cycle: ang += dabIndex_ * st.cycleDeg * kDeg2Rad; break;
    }
    float follow = clampf(eval(Set::FollowDir, inputs));
    if (follow > 0) ang += dirAng * follow;
    if (st.rotJitterDeg > 0) ang += rng_.sym() * st.rotJitterDeg * kDeg2Rad;
    if (st.upright) ang = st.baseAngleDeg * kDeg2Rad;

    Dab d;
    d.x = p.x; d.y = p.y; d.radius = radius;
    d.aspect = std::max(0.05f, eval(Set::Aspect, inputs));
    d.angle = ang;
    d.hardness = clampf(eval(Set::Hardness, inputs));
    d.opacity = clampf(eval(Set::Opacity, inputs)) * (spec_.buildUp ? sp_.opacity : 1.f);
    d.blend = spec_.blend;
    d.lockAlpha = sp_.lockAlpha;
    d.shape = shape_;
    d.invertShape = st.invert;
    switch (st.mirror) {
      case MirrorMode::None: break;
      case MirrorMode::Random: d.mirrorX = rng_.chance(0.5f); break;
      case MirrorMode::Alternate: d.mirrorX = (dabIndex_ & 1) != 0; break;
    }
    d.seed = hash32(sp_.seed * 977u + (uint32_t)dabIndex_ * 31u + c);
    d.dissolve = clampf(eval(Set::Dissolve, inputs));

    // colour
    Color col = sp_.color;
    if (st.colorMode == ColorMode::Gradient) col = mix(sp_.color, sp_.color2, clampf(strokeLen_ / std::max(1.f, st.gradientLength)));
    else if (st.colorMode == ColorMode::Palette && !st.palette.empty())
      col = st.palette[rng_.irange(0, (int)st.palette.size() - 1)];
    float dh = eval(Set::HueShift, inputs) + (st.hueJit > 0 ? rng_.sym() * st.hueJit : 0.f);
    float ds = eval(Set::SatShift, inputs) + (st.satJit > 0 ? rng_.sym() * st.satJit : 0.f);
    float dv = eval(Set::ValShift, inputs) + (st.valJit > 0 ? rng_.sym() * st.valJit : 0.f);
    if (dh != 0 || ds != 0 || dv != 0) col = shiftHSV(col, dh, ds, dv);
    col.a = 1.f;
    d.color = col;
    d.refColor = spec_.refColor; d.tolerance = spec_.tolerance; d.edgeLimit = spec_.edgeLimit;

    // grain
    if (grain_) {
      d.grain = grain_;
      d.grainDepth = clampf(eval(Set::GrainDepth, inputs));
      d.grainScale = spec_.grainScale;
      d.grainBias = spec_.pressureOpacityGrainBias ? (pressure - 0.5f) * 0.9f : 0.f;
    }

    // smudge
    float sm = clampf(eval(Set::Smudge, inputs));
    if (sm > 0.001f && spec_.buildUp) {
      float srad = radius * std::max(0.1f, eval(Set::SmudgeRadius, inputs));
      Color sampled = s.getColor(p.x, p.y, srad);
      float L = clampf(eval(Set::SmudgeLength, inputs), 0.f, 0.995f);
      Color sp{sampled.r * sampled.a, sampled.g * sampled.a, sampled.b * sampled.a, sampled.a};
      if (!smudgeInit_) { smudgeBucket_ = sp; smudgeInit_ = true; }
      else {
        smudgeBucket_.r = lerpf(sp.r, smudgeBucket_.r, L); smudgeBucket_.g = lerpf(sp.g, smudgeBucket_.g, L);
        smudgeBucket_.b = lerpf(sp.b, smudgeBucket_.b, L); smudgeBucket_.a = lerpf(sp.a, smudgeBucket_.a, L);
      }
      // mix brush paint (alpha 1) with the bucket
      float pr = lerpf(col.r, smudgeBucket_.r, sm), pg = lerpf(col.g, smudgeBucket_.g, sm),
            pb = lerpf(col.b, smudgeBucket_.b, sm), pa = lerpf(1.f, smudgeBucket_.a, sm);
      if (spec_.blend == Blend::Smudge || sm > 0.f) {
        d.blend = Blend::Smudge;
        d.colorAlpha = pa;
        d.color = pa > 1e-4f ? Color{pr / pa, pg / pa, pb / pa, 1.f} : col;
      }
    }

    if (buffer_) {
      Dab bd = d; bd.blend = Blend::Normal; bd.opacity = d.opacity > 0 ? 1.f : 0.f;
      // the buffer keeps full coverage; the opacity is applied when compositing into the layer
      bd.lockAlpha = false;
      buffer_->drawDab(bd);
      float e = radius * std::max(1.f, d.aspect) * 1.5f + 3.f;
      IRect rr{(int)std::floor(p.x - e), (int)std::floor(p.y - e), (int)std::ceil(p.x + e), (int)std::ceil(p.y + e)};
      if (!hasStepDirty_) { stepDirty_ = rr; hasStepDirty_ = true; }
      else {
        stepDirty_.x0 = std::min(stepDirty_.x0, rr.x0); stepDirty_.y0 = std::min(stepDirty_.y0, rr.y0);
        stepDirty_.x1 = std::max(stepDirty_.x1, rr.x1); stepDirty_.y1 = std::max(stepDirty_.y1, rr.y1);
      }
    } else {
      s.drawDab(d);
    }
    ++dabIndex_;
  }
}

}  // namespace ab
