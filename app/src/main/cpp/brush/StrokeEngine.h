// Clip Paint - interface unica de "traco": libmypaint ou AnimBrush atras da mesma API.
#pragma once

#include "BrushLibrary.h"

namespace ClipPaint {

struct StrokeSetup {
    const BrushEntry* entry = nullptr;
    Color color{0, 0, 0, 1};
    float diameter = 10.0f;
    float opacity = 1.0f;
    float hardness = -1.0f;
    float smoothing = 0.0f;
    bool eraseTool = false;      // ferramenta Borracha
    bool mirror = false;         // simetria (espelha X em torno de `axis`)
    float axis = 0.0f;
    uint32_t seed = 1;
    const std::map<std::string, float>* params = nullptr;
};

class StrokeEngine {
public:
    virtual ~StrokeEngine() = default;
    virtual void begin(Document& doc, int layer, const StrokeSetup& s, float x, float y, float pressure, double t) = 0;
    virtual void move(float x, float y, float pressure, double t) = 0;
    virtual void hold(double t) = 0;                    // caneta parada (aerografo, fogo...)
    virtual void end(float x, float y, double t) = 0;
    virtual bool active() const = 0;
};

std::unique_ptr<StrokeEngine> makeStrokeEngine(const BrushEntry& e);

}  // namespace ClipPaint
