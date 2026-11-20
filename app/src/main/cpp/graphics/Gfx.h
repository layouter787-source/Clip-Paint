// Clip Paint - camada grafica (EGL, renderizador de UI em lote, compositor do canvas).
#pragma once

#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <unordered_map>

#include "../core/Common.h"
#include "../layers/Document.h"

namespace ClipPaint {

class EGLContextManager {
public:
    bool initialize(ANativeWindow* window);
    void destroy();
    bool swapBuffers();
    bool querySize(int& w, int& h) const;
    int width() const { return width_; }
    int height() const { return height_; }
    bool ready() const {
        return display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE && context_ != EGL_NO_CONTEXT;
    }

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    int width_ = 0, height_ = 0;
};

GLuint buildProgram(const char* vs, const char* fs, const char* label);

// Transformacao do canvas: tela = offset + R(rot) * (doc * scale)
struct ViewState {
    float scale = 1.0f;
    float rot = 0.0f;
    Vec2 offset;
    Vec2 toScreen(Vec2 d) const {
        const float c = std::cos(rot), s = std::sin(rot);
        return {offset.x + (d.x * c - d.y * s) * scale, offset.y + (d.x * s + d.y * c) * scale};
    }
    Vec2 toDoc(Vec2 p) const {
        const float c = std::cos(-rot), s = std::sin(-rot);
        const float x = (p.x - offset.x) / scale, y = (p.y - offset.y) / scale;
        return {x * c - y * s, x * s + y * c};
    }
};

// Renderizador 2D em lote: retangulos arredondados, circulos, linhas, texto bitmap,
// roda de matiz, quadrado S/V e texturas.
class UiRenderer {
public:
    bool init();
    void destroy();
    void begin(int w, int h);
    void end();

    void rect(float x, float y, float w, float h, Color c, float radius = 0.0f);
    void rectOutline(float x, float y, float w, float h, Color c, float radius, float thick);
    void circle(float cx, float cy, float r, Color c);
    void ring(float cx, float cy, float r, float thick, Color c);
    void line(float x0, float y0, float x1, float y1, float width, Color c);
    void hueRing(float cx, float cy, float outer, float inner);
    void svSquare(float x, float y, float w, float h, float hue, float radius);
    void hueBar(float x, float y, float w, float h);
    void checker(float x, float y, float w, float h, float cell, float radius);
    void texture(GLuint tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                 Color tint = Color(1, 1, 1, 1));

    // texto: `scale` = pixels por ponto da fonte 5x7
    void text(const std::string& s, float x, float y, float scale, Color c);
    float textWidth(const std::string& s, float scale) const;
    float textHeight(float scale) const { return 7.0f * scale; }

    void pushClip(float x, float y, float w, float h);
    void popClip();
    void flush();

private:
    void quad(const Vec2 pos[4], const Vec2 loc[4], const Vec2 uv[4], Color c, float hw, float hh, float rad,
              float mode, float q0, float q1);
    void box(float x, float y, float w, float h, Color c, float rad, float mode, float q0, float q1);

    GLuint prog_ = 0, vao_ = 0, vbo_ = 0, fontTex_ = 0, curTex_ = 0;
    GLint uScreen_ = -1, uTex_ = -1;
    int w_ = 0, h_ = 0;
    std::vector<float> verts_;
    std::vector<RectI> clips_;
};

struct LayerXform {
    bool active = false;
    uint32_t layerId = 0;
    float H[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // homografia doc->doc (linha a linha)
};

class Renderer {
public:
    bool initialize(ANativeWindow* window);
    void destroy();
    bool ready() const { return egl_.ready(); }
    bool pollSize();  // true se o tamanho mudou
    int width() const { return width_; }
    int height() const { return height_; }

    void beginFrame();
    void endFrame();

    void syncDocument(const Document& doc, const LayerXform& xf);
    void drawCanvas(const Document& doc, const ViewState& view, float timeSec, bool showSelection,
                    float uiScale);
    // Textura RGBA8 pre-multiplicada (ex.: pre-visualizacao de pincel). Retorna 0 se falhar.
    GLuint uploadTexture(int w, int h, const uint8_t* rgbaPremult);
    // Miniatura de camada (usa a textura da propria camada).
    void drawLayerThumb(const Document& doc, uint32_t layerId, float x, float y, float w, float h);

    UiRenderer ui;

private:
    struct LayerGpu {
        GLuint tex = 0;
        std::vector<uint32_t> ver;
    };
    bool createPrograms();
    void ensureCompositor(const Document& doc);
    LayerGpu& gpuLayer(const Document& doc, uint32_t id);
    void uploadTile(const Document& doc, const Layer& l, LayerGpu& g, int tx, int ty);
    void recomposite(const Document& doc, RectI r, const LayerXform& xf);
    void drawTexQuad(GLuint prog, const Vec2 p[4], const float uv[8], const float* w = nullptr);

    EGLContextManager egl_;
    int width_ = 0, height_ = 0;

    GLuint progCompose_ = 0, progScreen_ = 0, progSel_ = 0, progFlat_ = 0;
    GLuint vao_ = 0, vbo_ = 0;
    std::unordered_map<uint32_t, LayerGpu> layers_;

    GLuint compTex_ = 0, compFbo_ = 0, scratchTex_ = 0, tmpFbo_ = 0, selTex_ = 0;
    int texW_ = 0, texH_ = 0;
    uint32_t lastStructRev_ = 0;
    bool lastXformActive_ = false;
    bool mipsStale_ = true;
    const Mask* selUploaded_ = nullptr;
    uint32_t docToken_ = 0;
};

}  // namespace ClipPaint
