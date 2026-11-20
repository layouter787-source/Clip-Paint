# AnimBrush — biblioteca de pincéis e borrachas (estilo libmypaint) para apps de animação 2D

C++17, **sem dependências**. ~500 pincéis/borrachas, cada um com comportamento próprio baseado em como a coisa funciona no mundo real
(raio = canal de descarga com ramificações e brilho, não uma linha torta; árvore = crescimento recursivo; fogo = partículas com empuxo e cor de corpo negro...).

## Como funciona (mesma ideia do libmypaint)
O motor **não toca em pixels**: ele emite *dabs* (carimbos) para uma `ISurface` que **o seu app implementa** (tiles/GPU) — ou use a `RasterSurface` de referência (CPU, float premultiplicado).

```cpp
#include <animbrush/animbrush.hpp>
ab::StampBank::instance().loadPack("assets/stamps.abpack");    // texturas do zip (165 máscaras)
auto brush = ab::BrushLibrary::instance().create("raios.relampago");
ab::RasterSurface canvas(1080, 1920);
ab::StrokeParams sp; sp.color = {0.7f,0.8f,1,1}; sp.size = 6; sp.seed = 7; sp.frame = currentFrame;
brush->begin(canvas, sp, firstSample);
for (auto& s : samples) brush->move(canvas, s);   // x,y,pressure,tilt,dt
brush->hold(canvas, dt);                           // opcional: caneta parada (aerógrafo, fogo)
brush->end(canvas);
```
- `BrushLibrary::catalogJson()` devolve a lista para montar a UI (nome, categoria, descrição, parâmetros/sliders).
- `include/animbrush/c_api.h`: API C para JNI/Kotlin (superfície por callbacks).
- Determinístico: mesma `seed` + mesma entrada = mesmo traço. Pincéis `animated` variam com `frame`/`time`/`wind` (relâmpago refaz a cada quadro, fogo tremula, árvore balança).
- Borrachas = pincéis com `isEraser` (modos de blend: apagar, cor similar, só linha, só bordas, dissolver, suavizar, região conectada...).

## Build
```
cmake -S . -B build && cmake --build build -j
build/render_catalog saida/ - assets/stamps.abpack     # renderiza 1 PNG por pincel + catalog.json
```
`tools/pack_stamps.py <zip_extraído> assets` normaliza os PNGs (polaridade/contraste) e gera `stamps.abpack`.

## Documentos
- `docs/CATALOGO.md` — todos os pincéis, com "na vida real" + "modelo".
- `docs/INTEGRACAO.md` — como ligar a `ISurface` ao seu canvas/GPU e ao Kotlin.

## Estrutura
`include/animbrush/` API · `src/` núcleo (rasterizador, DabBrush, registro) · `src/brushes/` módulos (mídias, natureza, luz, carimbos, borrachas, decor/mangá/FX) · `assets/` texturas normalizadas · `demo/` renderizador de catálogo.
