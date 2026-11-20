// Clip Paint - exportacao PNG e formato de projeto (.cpd).
#pragma once

#include "../layers/Document.h"

namespace ClipPaint {

// PNG RGBA 8 bits (alfa reto), sem dependencias externas (deflate "stored").
bool writePng(const std::string& path, int w, int h, const uint8_t* rgba, int dpi = 0);

// Projeto nativo: camadas, propriedades e tiles comprimidos (RLE).
bool saveProject(const Document& doc, const std::string& path);
bool loadProject(Document& doc, const std::string& path);

bool fileExists(const std::string& path);
bool makeDirs(const std::string& path);
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);

}  // namespace ClipPaint
