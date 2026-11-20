// Clip Paint - gera a pre-visualizacao de um pincel renderizando um traco real com o motor.
#pragma once

#include "../brush/StrokeEngine.h"

namespace ClipPaint {

// Devolve RGBA8 pre-multiplicado (w*h*4). false se o pincel nao puder ser renderizado.
bool renderBrushPreview(const BrushEntry& e, int w, int h, std::vector<uint8_t>& premultRgba);

}  // namespace ClipPaint
