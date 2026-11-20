#include "Gfx.h"

#include <android/log.h>

#define RLOGI(...) __android_log_print(ANDROID_LOG_INFO, "ClipPaint-Renderer", __VA_ARGS__)
#define RLOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ClipPaint-Renderer", __VA_ARGS__)

namespace ClipPaint {

namespace {

const char* kQuadVs =
    "#version 300 es\n"
    "layout(location=0) in vec4 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "out vec2 vUV;\n"
    "void main(){ gl_Position = aPos; vUV = aUV; }\n";

const char* kComposeFs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uLayer;\n"
    "uniform sampler2D uBack;\n"
    "uniform float uOpacity;\n"
    "uniform int uMode;\n"
    "uniform vec2 uTexSize;\n"
    "in vec2 vUV;\n"
    "out vec4 o;\n"
    "vec3 bl(int m, vec3 cb, vec3 cs){\n"
    "  if (m == 1) return cb * cs;\n"
    "  if (m == 2) return cb + cs - cb * cs;\n"
    "  if (m == 3) return mix(2.0*cb*cs, 1.0 - 2.0*(1.0-cb)*(1.0-cs), step(0.5, cb));\n"
    "  if (m == 4) return min(cb, cs);\n"
    "  if (m == 5) return max(cb, cs);\n"
    "  if (m == 6) return min(vec3(1.0), cb + cs);\n"
    "  return cs;\n"
    "}\n"
    "void main(){\n"
    "  vec4 s = texture(uLayer, vUV) * uOpacity;\n"
    "  if (uMode == 0) { o = s; return; }\n"
    "  vec4 b = texture(uBack, gl_FragCoord.xy / uTexSize);\n"
    "  vec3 Cb = b.a > 0.0 ? b.rgb / b.a : vec3(0.0);\n"
    "  vec3 Cs = s.a > 0.0 ? s.rgb / s.a : vec3(0.0);\n"
    "  vec3 B = bl(uMode, Cb, Cs);\n"
    "  vec3 rgb = s.rgb * (1.0 - b.a) + b.rgb * (1.0 - s.a) + s.a * b.a * B;\n"
    "  o = vec4(rgb, s.a + b.a - s.a * b.a);\n"
    "}\n";

const char* kScreenFs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uTex;\n"
    "in vec2 vUV;\n"
    "out vec4 o;\n"
    "void main(){ o = texture(uTex, vUV); }\n";

const char* kFlatFs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform vec4 uColor;\n"
    "uniform float uCell;\n"
    "in vec2 vUV;\n"
    "out vec4 o;\n"
    "void main(){\n"
    "  vec4 c = uColor;\n"
    "  if (uCell > 0.0) {\n"
    "    float m = mod(floor(gl_FragCoord.x / uCell) + floor(gl_FragCoord.y / uCell), 2.0);\n"
    "    c = vec4(vec3(mix(0.74, 0.90, m)), 1.0);\n"
    "  }\n"
    "  o = vec4(c.rgb * c.a, c.a);\n"
    "}\n";

const char* kSelFs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uMask;\n"
    "uniform vec2 uPix;\n"
    "uniform float uTime;\n"
    "in vec2 vUV;\n"
    "out vec4 o;\n"
    "void main(){\n"
    "  float m = texture(uMask, vUV).r;\n"
    "  float e = abs(texture(uMask, vUV + vec2(uPix.x, 0.0)).r - texture(uMask, vUV - vec2(uPix.x, 0.0)).r)\n"
    "          + abs(texture(uMask, vUV + vec2(0.0, uPix.y)).r - texture(uMask, vUV - vec2(0.0, uPix.y)).r);\n"
    "  e = clamp(e * 2.0, 0.0, 1.0);\n"
    "  float ants = step(0.5, fract((gl_FragCoord.x + gl_FragCoord.y) / 12.0 - uTime * 0.8));\n"
    "  float dim = (1.0 - m) * 0.30 * (1.0 - e);\n"
    "  o = vec4(vec3(ants) * e, e + dim);\n"
    "}\n";

Vec2 toNdcPx(Vec2 p, int w, int h) { return {p.x / (float)w * 2.0f - 1.0f, 1.0f - p.y / (float)h * 2.0f}; }

}  // namespace

// ------------------------------------------------------------------ ciclo

bool Renderer::createPrograms() {
    progCompose_ = buildProgram(kQuadVs, kComposeFs, "compose");
    progScreen_ = buildProgram(kQuadVs, kScreenFs, "screen");
    progFlat_ = buildProgram(kQuadVs, kFlatFs, "flat");
    progSel_ = buildProgram(kQuadVs, kSelFs, "selection");
    if (!progCompose_ || !progScreen_ || !progFlat_ || !progSel_) return false;
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)(4 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

bool Renderer::initialize(ANativeWindow* window) {
    if (!egl_.initialize(window)) return false;
    width_ = egl_.width();
    height_ = egl_.height();
    if (!createPrograms() || !ui.init()) {
        destroy();
        return false;
    }
    layers_.clear();
    compTex_ = compFbo_ = scratchTex_ = tmpFbo_ = selTex_ = 0;
    texW_ = texH_ = 0;
    lastStructRev_ = 0;
    docToken_ = 0;
    RLOGI("Renderer ready: %d x %d", width_, height_);
    return true;
}

void Renderer::destroy() {
    if (egl_.ready()) {
        for (auto& kv : layers_) if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
        if (compTex_) glDeleteTextures(1, &compTex_);
        if (scratchTex_) glDeleteTextures(1, &scratchTex_);
        if (selTex_) glDeleteTextures(1, &selTex_);
        if (compFbo_) glDeleteFramebuffers(1, &compFbo_);
        if (tmpFbo_) glDeleteFramebuffers(1, &tmpFbo_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        for (GLuint p : {progCompose_, progScreen_, progFlat_, progSel_}) if (p) glDeleteProgram(p);
        ui.destroy();
    }
    layers_.clear();
    compTex_ = compFbo_ = scratchTex_ = tmpFbo_ = selTex_ = 0;
    vbo_ = vao_ = 0;
    progCompose_ = progScreen_ = progFlat_ = progSel_ = 0;
    texW_ = texH_ = 0;
    egl_.destroy();
    width_ = height_ = 0;
}

bool Renderer::pollSize() {
    int w = width_, h = height_;
    if (!egl_.querySize(w, h)) return false;
    if (w == width_ && h == height_) return false;
    width_ = w;
    height_ = h;
    return true;
}

void Renderer::beginFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.168f, 0.176f, 0.199f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::endFrame() {
    ui.end();
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) RLOGE("GL error: 0x%x", err);
    egl_.swapBuffers();
}

// ------------------------------------------------------------- recursos

void Renderer::ensureCompositor(const Document& doc) {
    const int tw = doc.tilesX() * TILE, th = doc.tilesY() * TILE;
    if (compTex_ && tw == texW_ && th == texH_) return;
    for (auto& kv : layers_) if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
    layers_.clear();
    if (compTex_) glDeleteTextures(1, &compTex_);
    if (scratchTex_) glDeleteTextures(1, &scratchTex_);
    if (selTex_) glDeleteTextures(1, &selTex_);
    if (compFbo_) glDeleteFramebuffers(1, &compFbo_);
    if (!tmpFbo_) glGenFramebuffers(1, &tmpFbo_);
    texW_ = tw;
    texH_ = th;
    auto mk = [&](GLuint& t, GLenum internal, GLenum fmt) {
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, tw, th, 0, fmt, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    mk(compTex_, GL_RGBA8, GL_RGBA);
    mk(scratchTex_, GL_RGBA8, GL_RGBA);
    mk(selTex_, GL_R8, GL_RED);
    glGenFramebuffers(1, &compFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, compFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, compTex_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) RLOGE("composite FBO incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    lastStructRev_ = 0;
    docToken_ = 0;
}

Renderer::LayerGpu& Renderer::gpuLayer(const Document& doc, uint32_t id) {
    auto it = layers_.find(id);
    if (it != layers_.end()) return it->second;
    LayerGpu g;
    glGenTextures(1, &g.tex);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texW_, texH_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, tmpFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tex, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g.ver.assign((size_t)doc.tilesX() * doc.tilesY(), 0);
    return layers_.emplace(id, std::move(g)).first->second;
}

void Renderer::uploadTile(const Document& doc, const Layer& l, LayerGpu& g, int tx, int ty) {
    static const std::vector<uint8_t> zeros((size_t)TILE * TILE * 4, 0);
    const Tile* t = doc.readTile(l, tx, ty);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, tx * TILE, ty * TILE, TILE, TILE, GL_RGBA, GL_UNSIGNED_BYTE,
                    t ? (const void*)t->px : (const void*)zeros.data());
}

// ------------------------------------------------------------ composicao

void Renderer::drawTexQuad(GLuint prog, const Vec2 p[4], const float uv[8], const float* w) {
    // Ordem de tira: 0,1,3,2. Posicao em clip-space com w por vertice => textura corrigida em perspectiva.
    static const int order[4] = {0, 1, 3, 2};
    float v[24];
    for (int k = 0; k < 4; ++k) {
        const int i = order[k];
        const float ww = w ? w[i] : 1.0f;
        v[k * 6 + 0] = p[i].x * ww;
        v[k * 6 + 1] = p[i].y * ww;
        v[k * 6 + 2] = 0.0f;
        v[k * 6 + 3] = ww;
        v[k * 6 + 4] = uv[i * 2];
        v[k * 6 + 5] = uv[i * 2 + 1];
    }
    glUseProgram(prog);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void Renderer::recomposite(const Document& doc, RectI r, const LayerXform& xf) {
    r = r.intersect(RectI(0, 0, texW_, texH_));
    if (r.empty()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, compFbo_);
    glViewport(0, 0, texW_, texH_);
    glEnable(GL_SCISSOR_TEST);
    glScissor(r.x0, r.y0, r.width(), r.height());
    if (doc.paperVisible) {
        glClearColor(doc.paper.r * doc.paper.a, doc.paper.g * doc.paper.a, doc.paper.b * doc.paper.a, doc.paper.a);
    } else {
        glClearColor(0, 0, 0, 0);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(progCompose_);
    glUniform1i(glGetUniformLocation(progCompose_, "uLayer"), 0);
    glUniform1i(glGetUniformLocation(progCompose_, "uBack"), 1);
    glUniform2f(glGetUniformLocation(progCompose_, "uTexSize"), (float)texW_, (float)texH_);
    const GLint uOpacity = glGetUniformLocation(progCompose_, "uOpacity");
    const GLint uMode = glGetUniformLocation(progCompose_, "uMode");

    const float W = (float)texW_, H = (float)texH_;
    for (const Layer& l : doc.state.layers) {
        if (!l.visible || l.opacity <= 0.0f) continue;
        auto it = layers_.find(l.id);
        if (it == layers_.end()) continue;
        const int mode = (int)l.blend;
        if (mode != 0) {
            glDisable(GL_BLEND);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, scratchTex_);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, r.x0, r.y0, r.x0, r.y0, r.width(), r.height());
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, it->second.tex);
        glUniform1f(uOpacity, l.opacity);
        glUniform1i(uMode, mode);

        Vec2 p[4];
        float uv[8];
        float wv[4] = {1, 1, 1, 1};
        const float* wp = nullptr;
        if (xf.active && l.id == xf.layerId) {
            const float cx[4] = {0, W, W, 0}, cy[4] = {0, 0, H, H};
            const float* h = xf.H;
            bool bad = false;
            for (int i = 0; i < 4; ++i) {
                const float X = h[0] * cx[i] + h[1] * cy[i] + h[2];
                const float Y = h[3] * cx[i] + h[4] * cy[i] + h[5];
                const float w = h[6] * cx[i] + h[7] * cy[i] + h[8];
                if (w < 1e-4f) { bad = true; break; }
                p[i] = {X / w / W * 2.0f - 1.0f, Y / w / H * 2.0f - 1.0f};
                wv[i] = w;
            }
            if (bad) continue;
            wp = wv;
            const float full[8] = {0, 0, 1, 0, 1, 1, 0, 1};
            std::memcpy(uv, full, sizeof(full));
        } else {
            const float x0 = (float)r.x0, x1 = (float)r.x1, y0 = (float)r.y0, y1 = (float)r.y1;
            p[0] = {x0 / W * 2 - 1, y0 / H * 2 - 1}; p[1] = {x1 / W * 2 - 1, y0 / H * 2 - 1};
            p[2] = {x1 / W * 2 - 1, y1 / H * 2 - 1}; p[3] = {x0 / W * 2 - 1, y1 / H * 2 - 1};
            const float u[8] = {x0 / W, y0 / H, x1 / W, y0 / H, x1 / W, y1 / H, x0 / W, y1 / H};
            std::memcpy(uv, u, sizeof(u));
        }
        drawTexQuad(progCompose_, p, uv, wp);
    }
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    mipsStale_ = true;
}

void Renderer::syncDocument(const Document& doc, const LayerXform& xf) {
    if (!egl_.ready()) return;
    ensureCompositor(doc);
    RectI changed;
    for (const Layer& l : doc.state.layers) {
        LayerGpu& g = gpuLayer(doc, l.id);
        for (int ty = 0; ty < doc.tilesY(); ++ty) {
            for (int tx = 0; tx < doc.tilesX(); ++tx) {
                const size_t idx = (size_t)ty * doc.tilesX() + tx;
                const Tile* t = doc.readTile(l, tx, ty);
                const uint32_t ver = t ? t->version : 0;
                if (ver == g.ver[idx]) continue;
                uploadTile(doc, l, g, tx, ty);
                g.ver[idx] = ver;
                changed.unite(RectI(tx * TILE, ty * TILE, tx * TILE + TILE, ty * TILE + TILE));
            }
        }
    }
    // Libera texturas de camadas removidas (mantem algumas para desfazer).
    if (layers_.size() > doc.state.layers.size() + 6) {
        for (auto it = layers_.begin(); it != layers_.end();) {
            bool used = false;
            for (const Layer& l : doc.state.layers) if (l.id == it->first) { used = true; break; }
            if (used) { ++it; continue; }
            glDeleteTextures(1, &it->second.tex);
            it = layers_.erase(it);
        }
    }
    const bool full = doc.structRev != lastStructRev_ || xf.active || lastXformActive_;
    if (full) {
        recomposite(doc, RectI(0, 0, texW_, texH_), xf);
    } else if (!changed.empty()) {
        recomposite(doc, changed, xf);
    }
    lastStructRev_ = doc.structRev;
    lastXformActive_ = xf.active;
}

// -------------------------------------------------------------- desenho

void Renderer::drawCanvas(const Document& doc, const ViewState& view, float timeSec, bool showSelection,
                          float uiScale) {
    if (!egl_.ready() || !compTex_) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    const float dw = (float)doc.width(), dh = (float)doc.height();
    const Vec2 sc[4] = {view.toScreen({0, 0}), view.toScreen({dw, 0}), view.toScreen({dw, dh}), view.toScreen({0, dh})};
    Vec2 ndc[4];
    for (int i = 0; i < 4; ++i) ndc[i] = toNdcPx(sc[i], width_, height_);
    const float uvs[8] = {0, 0, dw / texW_, 0, dw / texW_, dh / texH_, 0, dh / texH_};

    // Sombra suave da folha.
    glUseProgram(progFlat_);
    glUniform1f(glGetUniformLocation(progFlat_, "uCell"), 0.0f);
    const Vec2 center((sc[0].x + sc[2].x) * 0.5f, (sc[0].y + sc[2].y) * 0.5f);
    for (int i = 4; i >= 1; --i) {
        const float grow = i * 3.0f * uiScale;
        Vec2 q[4];
        for (int k = 0; k < 4; ++k) {
            Vec2 d = sc[k] - center;
            const float len = std::max(1.0f, d.length());
            q[k] = toNdcPx(sc[k] + d * (grow / len), width_, height_);
        }
        glUniform4f(glGetUniformLocation(progFlat_, "uColor"), 0, 0, 0, 0.10f);
        drawTexQuad(progFlat_, q, uvs);
    }
    if (!doc.paperVisible) {
        glUseProgram(progFlat_);
        glUniform1f(glGetUniformLocation(progFlat_, "uCell"), std::max(6.0f, 8.0f * uiScale));
        glUniform4f(glGetUniformLocation(progFlat_, "uColor"), 1, 1, 1, 1);
        drawTexQuad(progFlat_, ndc, uvs);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, compTex_);
    if (view.scale < 1.0f) {
        if (mipsStale_) {
            glGenerateMipmap(GL_TEXTURE_2D);
            mipsStale_ = false;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, view.scale >= 4.0f ? GL_NEAREST : GL_LINEAR);
    glUseProgram(progScreen_);
    glUniform1i(glGetUniformLocation(progScreen_, "uTex"), 0);
    drawTexQuad(progScreen_, ndc, uvs);

    if (showSelection && doc.hasSelection()) {
        if (docToken_ != doc.structRev) {
            std::vector<uint8_t> padded((size_t)texW_ * texH_, 0);
            const Mask& m = *doc.selectionMask();
            for (int y = 0; y < doc.height(); ++y) {
                std::memcpy(&padded[(size_t)y * texW_], &m[(size_t)y * doc.width()], (size_t)doc.width());
            }
            glBindTexture(GL_TEXTURE_2D, selTex_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW_, texH_, GL_RED, GL_UNSIGNED_BYTE, padded.data());
            docToken_ = doc.structRev;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, selTex_);
        glUseProgram(progSel_);
        glUniform1i(glGetUniformLocation(progSel_, "uMask"), 0);
        const float step = std::max(1.0f, 1.5f / std::max(view.scale, 0.01f));
        glUniform2f(glGetUniformLocation(progSel_, "uPix"), step / texW_, step / texH_);
        glUniform1f(glGetUniformLocation(progSel_, "uTime"), timeSec);
        drawTexQuad(progSel_, ndc, uvs);
    }
    glDisable(GL_BLEND);
}

GLuint Renderer::uploadTexture(int w, int h, const uint8_t* px) {
    if (!egl_.ready()) return 0;
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void Renderer::drawLayerThumb(const Document& doc, uint32_t layerId, float x, float y, float w, float h) {
    auto it = layers_.find(layerId);
    if (it == layers_.end() || texW_ <= 0) return;
    ui.checker(x, y, w, h, std::max(4.0f, w / 8.0f), 3.0f);
    const float ar = (float)doc.width() / (float)doc.height();
    float fw = w, fh = w / ar;
    if (fh > h) { fh = h; fw = h * ar; }
    ui.texture(it->second.tex, x + (w - fw) * 0.5f, y + (h - fh) * 0.5f, fw, fh, 0.0f, 0.0f,
               (float)doc.width() / texW_, (float)doc.height() / texH_);
}

}  // namespace ClipPaint
