#include "Gfx.h"

namespace ClipPaint {

namespace {

const char* kUiVs =
    "#version 300 es\n"
    "uniform vec2 uScreen;\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aLoc;\n"
    "layout(location=2) in vec2 aUV;\n"
    "layout(location=3) in vec4 aColor;\n"
    "layout(location=4) in vec4 aP;\n"
    "layout(location=5) in vec2 aQ;\n"
    "out vec2 vLoc; out vec2 vUV; out vec4 vColor; out vec4 vP; out vec2 vQ;\n"
    "void main(){\n"
    "  vec2 n = aPos / uScreen * 2.0 - 1.0;\n"
    "  gl_Position = vec4(n.x, -n.y, 0.0, 1.0);\n"
    "  vLoc = aLoc; vUV = aUV; vColor = aColor; vP = aP; vQ = aQ;\n"
    "}\n";

const char* kUiFs =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uTex;\n"
    "in vec2 vLoc; in vec2 vUV; in vec4 vColor; in vec4 vP; in vec2 vQ;\n"
    "out vec4 o;\n"
    "float sdRound(vec2 p, vec2 b, float r){\n"
    "  vec2 q = abs(p) - b + vec2(r);\n"
    "  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "vec3 hsv(float h, float s, float v){\n"
    "  vec3 p = abs(fract(h + vec3(1.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0);\n"
    "  return v * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), s);\n"
    "}\n"
    "void main(){\n"
    "  int mode = int(vP.w + 0.5);\n"
    "  vec4 c = vColor; float cov = 1.0;\n"
    "  if (mode == 7) { o = texture(uTex, vUV) * vColor; return; }\n"
    "  if (mode == 1) { cov = texture(uTex, vUV).r; }\n"
    "  else if (mode == 2) {\n"
    "    float d = length(vLoc);\n"
    "    cov = clamp(vP.x - d + 0.5, 0.0, 1.0) * clamp(d - vP.y + 0.5, 0.0, 1.0);\n"
    "    c = vec4(hsv(atan(vLoc.y, vLoc.x) / 6.2831853, 1.0, 1.0), vColor.a);\n"
    "  } else {\n"
    "    float d = sdRound(vLoc, vP.xy, vP.z);\n"
    "    cov = clamp(0.5 - d, 0.0, 1.0);\n"
    "    if (mode == 0 && vQ.x > 0.0) cov *= clamp(0.5 + d + vQ.x, 0.0, 1.0);\n"
    "    if (mode == 3) {\n"
    "      float s = clamp((vLoc.x / vP.x + 1.0) * 0.5, 0.0, 1.0);\n"
    "      float v = 1.0 - clamp((vLoc.y / vP.y + 1.0) * 0.5, 0.0, 1.0);\n"
    "      c = vec4(hsv(vQ.x, s, v), vColor.a);\n"
    "    } else if (mode == 4) {\n"
    "      vec2 g = floor(vLoc / vQ.x);\n"
    "      float m = mod(g.x + g.y, 2.0);\n"
    "      c = vec4(vec3(mix(0.74, 0.90, m)), vColor.a);\n"
    "    } else if (mode == 5) {\n"
    "      float h = clamp((vLoc.x / vP.x + 1.0) * 0.5, 0.0, 1.0);\n"
    "      c = vec4(hsv(h, 1.0, 1.0), vColor.a);\n"
    "    }\n"
    "  }\n"
    "  o = vec4(c.rgb * c.a, c.a) * cov;\n"
    "}\n";

// Fonte 5x7 classica (colunas; bit 0 = linha superior). ASCII 32..90.
const uint8_t kFont[91][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
    {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},{0x00,0x40,0x34,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},
    {0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x41,0x51,0x73},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
    {0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},
    {0x00,0x7F,0x41,0x41,0x00},{0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},
    {0x20,0x54,0x54,0x54,0x78},{0x7F,0x44,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},{0x38,0x44,0x44,0x44,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x18,0xA4,0xA4,0xA4,0x7C},{0x7F,0x04,0x04,0x04,0x78},
    {0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x40,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},
    {0x7C,0x04,0x78,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0xFC,0x18,0x24,0x24,0x18},
    {0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x24},{0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},
    {0x4C,0x90,0x90,0x90,0x7C},{0x44,0x64,0x54,0x4C,0x44},
};
constexpr int kGlyphs = 91;
constexpr int kCellW = 6;
constexpr int kVertFloats = 16;

int glyphIndex(unsigned char ch) {
    if (ch < 32 || ch > 122) ch = '?';
    return ch - 32;
}

// Letras acentuadas do Latim-1 (UTF-8 de 2 bytes 0xC3 xx): base + marca.
// accent: 1 agudo, 2 grave, 3 circunflexo, 4 til, 5 trema, 6 cedilha
bool latinBase(unsigned cp, char& base, int& accent) {
    static const struct { unsigned cp; char b; int a; } t[] = {
        {0xC0,'A',2},{0xC1,'A',1},{0xC2,'A',3},{0xC3,'A',4},{0xC4,'A',5},{0xC7,'C',6},{0xC8,'E',2},{0xC9,'E',1},
        {0xCA,'E',3},{0xCB,'E',5},{0xCC,'I',2},{0xCD,'I',1},{0xCE,'I',3},{0xD1,'N',4},{0xD2,'O',2},{0xD3,'O',1},
        {0xD4,'O',3},{0xD5,'O',4},{0xD6,'O',5},{0xD9,'U',2},{0xDA,'U',1},{0xDB,'U',3},{0xDC,'U',5},
        {0xE0,'a',2},{0xE1,'a',1},{0xE2,'a',3},{0xE3,'a',4},{0xE4,'a',5},{0xE7,'c',6},{0xE8,'e',2},{0xE9,'e',1},
        {0xEA,'e',3},{0xEB,'e',5},{0xEC,'i',2},{0xED,'i',1},{0xEE,'i',3},{0xEF,'i',5},{0xF1,'n',4},{0xF2,'o',2},
        {0xF3,'o',1},{0xF4,'o',3},{0xF5,'o',4},{0xF6,'o',5},{0xF9,'u',2},{0xFA,'u',1},{0xFB,'u',3},{0xFC,'u',5}};
    for (const auto& e : t) if (e.cp == cp) { base = e.b; accent = e.a; return true; }
    return false;
}

}  // namespace

bool UiRenderer::init() {
    prog_ = buildProgram(kUiVs, kUiFs, "ui");
    if (!prog_) return false;
    uScreen_ = glGetUniformLocation(prog_, "uScreen");
    uTex_ = glGetUniformLocation(prog_, "uTex");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = kVertFloats * sizeof(float);
    const int sizes[6] = {2, 2, 2, 4, 4, 2};
    size_t off = 0;
    for (int i = 0; i < 6; ++i) {
        glEnableVertexAttribArray((GLuint)i);
        glVertexAttribPointer((GLuint)i, sizes[i], GL_FLOAT, GL_FALSE, stride, (const void*)(off * sizeof(float)));
        off += (size_t)sizes[i];
    }
    glBindVertexArray(0);

    // Atlas da fonte (R8).
    const int W = kGlyphs * kCellW, H = 8;
    std::vector<uint8_t> atlas((size_t)W * H, 0);
    for (int g = 0; g < kGlyphs; ++g) {
        for (int c = 0; c < 5; ++c) {
            for (int r = 0; r < 7; ++r) {
                if (kFont[g][c] & (1 << r)) atlas[(size_t)r * W + g * kCellW + c] = 255;
            }
        }
    }
    glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    curTex_ = fontTex_;
    return true;
}

void UiRenderer::destroy() {
    if (fontTex_) glDeleteTextures(1, &fontTex_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (prog_) glDeleteProgram(prog_);
    fontTex_ = vbo_ = vao_ = prog_ = curTex_ = 0;
}

void UiRenderer::begin(int w, int h) {
    w_ = w;
    h_ = h;
    verts_.clear();
    clips_.clear();
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    curTex_ = fontTex_;
}

void UiRenderer::end() {
    flush();
    glDisable(GL_SCISSOR_TEST);
}

void UiRenderer::flush() {
    if (verts_.empty()) return;
    glUseProgram(prog_);
    glUniform2f(uScreen_, (float)w_, (float)h_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, curTex_);
    glUniform1i(uTex_, 0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts_.size() * sizeof(float)), verts_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts_.size() / kVertFloats));
    glBindVertexArray(0);
    verts_.clear();
}

void UiRenderer::pushClip(float x, float y, float w, float h) {
    flush();
    RectI r((int)x, (int)y, (int)(x + w), (int)(y + h));
    if (!clips_.empty()) r = r.intersect(clips_.back());
    clips_.push_back(r);
    glEnable(GL_SCISSOR_TEST);
    glScissor(r.x0, h_ - r.y1, std::max(0, r.width()), std::max(0, r.height()));
}

void UiRenderer::popClip() {
    flush();
    if (!clips_.empty()) clips_.pop_back();
    if (clips_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        const RectI& r = clips_.back();
        glScissor(r.x0, h_ - r.y1, std::max(0, r.width()), std::max(0, r.height()));
    }
}

void UiRenderer::quad(const Vec2 pos[4], const Vec2 loc[4], const Vec2 uv[4], Color c, float hw, float hh,
                      float rad, float mode, float q0, float q1) {
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; ++k) {
        const int i = idx[k];
        const float v[kVertFloats] = {pos[i].x, pos[i].y, loc[i].x, loc[i].y, uv[i].x, uv[i].y,
                                      c.r, c.g, c.b, c.a, hw, hh, rad, mode, q0, q1};
        verts_.insert(verts_.end(), v, v + kVertFloats);
    }
}

void UiRenderer::box(float x, float y, float w, float h, Color c, float rad, float mode, float q0, float q1) {
    const float hw = w * 0.5f, hh = h * 0.5f;
    const float cx = x + hw, cy = y + hh;
    const float ex = hw + 1.0f, ey = hh + 1.0f;
    const Vec2 pos[4] = {{cx - ex, cy - ey}, {cx + ex, cy - ey}, {cx + ex, cy + ey}, {cx - ex, cy + ey}};
    const Vec2 loc[4] = {{-ex, -ey}, {ex, -ey}, {ex, ey}, {-ex, ey}};
    const Vec2 uv[4] = {};
    quad(pos, loc, uv, c, hw, hh, std::min(rad, std::min(hw, hh)), mode, q0, q1);
}

void UiRenderer::rect(float x, float y, float w, float h, Color c, float radius) {
    box(x, y, w, h, c, radius, 0.0f, 0.0f, 0.0f);
}

void UiRenderer::rectOutline(float x, float y, float w, float h, Color c, float radius, float thick) {
    box(x, y, w, h, c, radius, 0.0f, thick, 0.0f);
}

void UiRenderer::circle(float cx, float cy, float r, Color c) {
    box(cx - r, cy - r, r * 2, r * 2, c, r, 0.0f, 0.0f, 0.0f);
}

void UiRenderer::ring(float cx, float cy, float r, float thick, Color c) {
    box(cx - r, cy - r, r * 2, r * 2, c, r, 0.0f, thick, 0.0f);
}

void UiRenderer::line(float x0, float y0, float x1, float y1, float width, Color c) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float ux = len > 1e-4f ? dx / len : 1.0f, uy = len > 1e-4f ? dy / len : 0.0f;
    const float nx = -uy, ny = ux;
    const float hw = len * 0.5f + width * 0.5f, hh = width * 0.5f;
    const float ex = hw + 1.0f, ey = hh + 1.0f;
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const Vec2 pos[4] = {{cx - ux * ex - nx * ey, cy - uy * ex - ny * ey},
                         {cx + ux * ex - nx * ey, cy + uy * ex - ny * ey},
                         {cx + ux * ex + nx * ey, cy + uy * ex + ny * ey},
                         {cx - ux * ex + nx * ey, cy - uy * ex + ny * ey}};
    const Vec2 loc[4] = {{-ex, -ey}, {ex, -ey}, {ex, ey}, {-ex, ey}};
    const Vec2 uv[4] = {};
    quad(pos, loc, uv, c, hw, hh, hh, 0.0f, 0.0f, 0.0f);
}

void UiRenderer::hueRing(float cx, float cy, float outer, float inner) {
    const float e = outer + 1.0f;
    const Vec2 pos[4] = {{cx - e, cy - e}, {cx + e, cy - e}, {cx + e, cy + e}, {cx - e, cy + e}};
    const Vec2 loc[4] = {{-e, -e}, {e, -e}, {e, e}, {-e, e}};
    const Vec2 uv[4] = {};
    quad(pos, loc, uv, Color(1, 1, 1, 1), outer, inner, 0.0f, 2.0f, 0.0f, 0.0f);
}

void UiRenderer::svSquare(float x, float y, float w, float h, float hue, float radius) {
    box(x, y, w, h, Color(1, 1, 1, 1), radius, 3.0f, hue, 0.0f);
}

void UiRenderer::hueBar(float x, float y, float w, float h) {
    box(x, y, w, h, Color(1, 1, 1, 1), h * 0.5f, 5.0f, 0.0f, 0.0f);
}

void UiRenderer::checker(float x, float y, float w, float h, float cell, float radius) {
    box(x, y, w, h, Color(1, 1, 1, 1), radius, 4.0f, std::max(2.0f, cell), 0.0f);
}

void UiRenderer::texture(GLuint tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                         Color tint) {
    flush();
    curTex_ = tex;
    const Vec2 pos[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const Vec2 loc[4] = {};
    const Vec2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    quad(pos, loc, uv, tint, 0.0f, 0.0f, 0.0f, 7.0f, 0.0f, 0.0f);
    flush();
    curTex_ = fontTex_;
}

float UiRenderer::textWidth(const std::string& s, float scale) const {
    int n = 0;
    for (unsigned char ch : s) if ((ch & 0xC0) != 0x80) ++n;
    return n == 0 ? 0.0f : (float)n * kCellW * scale - scale;
}

void UiRenderer::text(const std::string& s, float x, float y, float scale, Color c) {
    const float W = (float)(kGlyphs * kCellW);
    float px = std::floor(x), py = std::floor(y);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = (unsigned char)s[i];
        char base = (char)ch;
        int accent = 0;
        if (ch == 0xC3 && i + 1 < s.size()) {
            const unsigned cp = 0xC0 | ((unsigned char)s[i + 1] & 0x3F);
            if (!latinBase(cp, base, accent)) base = '?';
            ++i;
        } else if (ch >= 0x80) {
            // outras sequencias UTF-8: ignora os bytes de continuacao
            while (i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80) ++i;
            base = '?';
        }
        if (base != ' ') {
            const int g = glyphIndex((unsigned char)base);
            const float u0 = (float)(g * kCellW) / W, u1 = (float)(g * kCellW + 5) / W;
            const Vec2 pos[4] = {{px, py}, {px + 5 * scale, py}, {px + 5 * scale, py + 8 * scale}, {px, py + 8 * scale}};
            const Vec2 loc[4] = {};
            const Vec2 uv[4] = {{u0, 0.0f}, {u1, 0.0f}, {u1, 1.0f}, {u0, 1.0f}};
            quad(pos, loc, uv, c, 0, 0, 0, 1.0f, 0.0f, 0.0f);
            if (accent) {
                const bool upper = base >= 'A' && base <= 'Z';
                const int a0 = upper ? -2 : 0;
                static const int ac[7][6] = {  // pares (coluna, linha relativa) ate 3 pontos; -1 = fim
                    {0, 0, 0, 0, 0, 0}, {3, 0, 2, 1, -1, 0}, {1, 0, 2, 1, -1, 0}, {2, 0, 1, 1, 3, 1},
                    {0, 1, 1, 0, 2, 1}, {1, 0, 3, 0, -1, 0}, {2, 7, 1, 8, -1, 0}};
                for (int k = 0; k < 3; ++k) {
                    const int col = ac[accent][k * 2];
                    if (col < 0) break;
                    int row = ac[accent][k * 2 + 1];
                    if (accent != 6) row += a0;
                    rect(px + col * scale, py + row * scale, scale, scale, c, 0.0f);
                }
                if (accent == 4) rect(px + 3 * scale, py + a0 * scale, scale, scale, c, 0.0f);
            }
        }
        px += kCellW * scale;
    }
}

}  // namespace ClipPaint
