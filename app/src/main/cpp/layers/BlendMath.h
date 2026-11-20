// Clip Paint - formulas de mistura (identicas as do shader de composicao).
#pragma once

#include "Document.h"

namespace ClipPaint {

inline float blendChannel(Blend m, float cb, float cs) {
    switch (m) {
        case Blend::Multiply: return cb * cs;
        case Blend::Screen: return cb + cs - cb * cs;
        case Blend::Overlay:
            return cb <= 0.5f ? 2.0f * cb * cs : 1.0f - 2.0f * (1.0f - cb) * (1.0f - cs);
        case Blend::Darken: return std::min(cb, cs);
        case Blend::Lighten: return std::max(cb, cs);
        case Blend::Add: return std::min(1.0f, cb + cs);
        default: return cs;
    }
}

// cb = fundo pre-multiplicado (modificado in-place), cs = fonte pre-multiplicada
// (ja multiplicada pela opacidade da camada).
inline void blendPremult(float cb[4], const float cs[4], Blend m) {
    const float as = cs[3];
    if (as <= 0.0f) return;
    const float ab = cb[3];
    if (m == Blend::Normal || ab <= 0.0f) {
        const float k = 1.0f - as;
        for (int i = 0; i < 4; ++i) cb[i] = cs[i] + cb[i] * k;
        return;
    }
    float out[4];
    for (int i = 0; i < 3; ++i) {
        const float Cb = cb[i] / ab;
        const float Cs = cs[i] / as;
        const float B = blendChannel(m, Cb, Cs);
        out[i] = cs[i] * (1.0f - ab) + cb[i] * (1.0f - as) + as * ab * B;
    }
    out[3] = as + ab - as * ab;
    for (int i = 0; i < 4; ++i) cb[i] = out[i];
}

}  // namespace ClipPaint
