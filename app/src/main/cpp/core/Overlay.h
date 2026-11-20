// Clip Paint - sobreposicoes desenhadas na GPU sobre o canvas (grade, simetria, selecao, nos de transformacao).
#pragma once

#include "Editor.h"

namespace ClipPaint {
void drawOverlays(UiRenderer& g, Editor& e, int screenW, int screenH, double now);
}
