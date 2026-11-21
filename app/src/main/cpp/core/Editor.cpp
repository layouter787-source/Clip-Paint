#include "Editor.h"

#include <ctime>
#include <cstdio>
#include <unordered_set>

namespace ClipPaint {

namespace {

// Homografia que leva s[i] em d[i] (4 pontos). Retorna false se degenerada.
bool solveHomography(const Vec2 s[4], const Vec2 d[4], float H[9]) {
    double a[8][9];
    for (int i = 0; i < 4; ++i) {
        const double x = s[i].x, y = s[i].y, X = d[i].x, Y = d[i].y;
        const double r0[9] = {x, y, 1, 0, 0, 0, -x * X, -y * X, X};
        const double r1[9] = {0, 0, 0, x, y, 1, -x * Y, -y * Y, Y};
        std::memcpy(a[2 * i], r0, sizeof(r0));
        std::memcpy(a[2 * i + 1], r1, sizeof(r1));
    }
    for (int c = 0; c < 8; ++c) {
        int p = c;
        for (int r = c + 1; r < 8; ++r) if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
        if (std::fabs(a[p][c]) < 1e-10) return false;
        if (p != c) for (int k = 0; k < 9; ++k) std::swap(a[p][k], a[c][k]);
        for (int r = 0; r < 8; ++r) {
            if (r == c) continue;
            const double f = a[r][c] / a[c][c];
            for (int k = c; k < 9; ++k) a[r][k] -= f * a[c][k];
        }
    }
    for (int i = 0; i < 8; ++i) H[i] = (float)(a[i][8] / a[i][i]);
    H[8] = 1.0f;
    return true;
}

Vec2 norm(Vec2 v) {
    const float l = v.length();
    return l > 1e-6f ? v * (1.0f / l) : Vec2(1, 0);
}
float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

}  // namespace

Editor::Editor() {}

// -------------------------------------------------------------- pincel

const BrushEntry& Editor::entryOf(const BrushSetup& b) const {
    const auto& es = BrushLibrary::get().entries();
    static BrushEntry dummy;
    return (b.entry >= 0 && b.entry < (int)es.size()) ? es[b.entry] : dummy;
}

void Editor::selectBrush(bool eraserTool, int idx) {
    const auto& es = BrushLibrary::get().entries();
    if (idx < 0 || idx >= (int)es.size()) return;
    BrushSetup& b = brushFor(eraserTool);
    b.entry = idx;
    b.diameter = es[idx].defaultDiameter;
    b.hardness = -1.0f;
    b.params.clear();
    tool = eraserTool ? ToolId::Eraser : ToolId::Brush;
}

float Editor::sliderToSize(float t) { return std::max(1.0f, std::exp(clampf(t, 0.0f, 1.0f) * std::log(600.0f))); }
float Editor::sizeToSlider(float d) { return clampf(std::log(std::max(1.0f, d)) / std::log(600.0f), 0.0f, 1.0f); }

void Editor::setTool(ToolId t) {
    if (transforming && t != ToolId::Select) applyTransform();
    cancelCanvasAction();
    tool = t;
}

bool Editor::canDraw(std::string* why) const {
    const Layer& l = doc.activeLayer();
    if (l.locked) { if (why) *why = "Camada bloqueada"; return false; }
    if (!l.visible) { if (why) *why = "Camada oculta"; return false; }
    return true;
}

StrokeSetup Editor::makeSetup(bool mirror) const {
    const BrushSetup& b = tool == ToolId::Eraser ? brushErase_ : brushPaint_;
    StrokeSetup s;
    s.entry = &entryOf(b);
    s.color = color;
    if (s.entry->additive && s.entry->source == BrushSource::Anim && false) s.color = s.entry->suggested;
    s.diameter = b.diameter;
    s.opacity = b.opacity;
    s.hardness = b.hardness;
    s.smoothing = b.smoothing;
    s.eraseTool = tool == ToolId::Eraser;
    s.mirror = mirror;
    s.axis = doc.width() * 0.5f;
    s.seed = strokeCounter_ * 7919u + 13u;
    s.params = &b.params;
    return s;
}

// -------------------------------------------------------------- tela

void Editor::setScreen(int w, int h, float topInset, float bottomInset, float leftInset, float rightInset, float dp) {
    const Vec2 oldCenter((float)screenW_ * 0.5f, (float)screenH_ * 0.5f);
    screenW_ = w;
    screenH_ = h;
    dp_ = dp;
    insetL_ = leftInset;
    insetR_ = rightInset;
    (void)topInset;
    (void)bottomInset;
    view.offset = view.offset + (Vec2((float)w * 0.5f, (float)h * 0.5f) - oldCenter);
}

void Editor::fitView() {
    const float availW = (float)screenW_ - insetL_ - insetR_;
    const float availH = (float)screenH_ - marginTop_ - marginBottom_;
    const float dw = (float)doc.width(), dh = (float)doc.height();
    view.scale = std::max(0.02f, std::min(availW * 0.94f / dw, availH * 0.94f / dh));
    view.rot = 0.0f;
    const Vec2 c(insetL_ + availW * 0.5f, marginTop_ + availH * 0.5f);
    view.offset = Vec2(c.x - dw * 0.5f * view.scale, c.y - dh * 0.5f * view.scale);
}

// ------------------------------------------------------- entrada de canvas

void Editor::canvasDown(Vec2 screen, float pressure, double t, bool stylus) {
    (void)stylus;
    const Vec2 d = view.toDoc(screen);
    lastT_ = t;
    std::string why;
    switch (tool) {
        case ToolId::Brush:
        case ToolId::Eraser:
            if (!canDraw(&why)) { toast(why); return; }
            if (BrushLibrary::get().entries().empty()) { toast("Sem pincéis carregados"); return; }
            doc.pushUndo();
            strokeUndoPushed_ = true;
            strokeActive_ = true;
            lastDoc_ = d;
            lastPressure_ = pressure;
            lastMoveWall_ = nowSeconds();
            queue_.push_back({0, d, pressure, t});
            break;
        case ToolId::Fill:
            if (!canDraw(&why)) { toast(why); return; }
            if (d.x >= 0 && d.y >= 0 && d.x < doc.width() && d.y < doc.height()) {
                doc.pushUndo();
                if (doc.floodFill(doc.state.active, (int)d.x, (int)d.y, color, fillTolerance, fillAllLayers, fillGrow))
                    addRecentColor(color);
                else
                    doc.dropLastUndo();
            }
            break;
        case ToolId::Eyedropper:
            pickActive = true;
            pickScreen = screen;
            color = doc.pickColor((int)d.x, (int)d.y, true);
            color.a = 1.0f;
            break;
        case ToolId::Select:
            if (transforming) { transformDown(d); break; }
            if (selMethod == SelMethod::Auto) {
                if (d.x >= 0 && d.y >= 0 && d.x < doc.width() && d.y < doc.height()) {
                    doc.pushUndo();
                    doc.selectByColor((int)d.x, (int)d.y, selTolerance, false, selOp);
                }
                break;
            }
            selDragging = true;
            selStart = selCur = d;
            selPoints.clear();
            if (selMethod == SelMethod::Freehand) selPoints.push_back(d);
            break;
    }
}

void Editor::canvasMove(Vec2 screen, float pressure, double t) {
    const Vec2 d = view.toDoc(screen);
    switch (tool) {
        case ToolId::Brush:
        case ToolId::Eraser:
            if (strokeActive_) {
                queue_.push_back({1, d, pressure, t});
                lastDoc_ = d;
                lastPressure_ = pressure;
                lastT_ = t;
                lastMoveWall_ = nowSeconds();
            }
            break;
        case ToolId::Eyedropper:
            if (pickActive) {
                pickScreen = screen;
                color = doc.pickColor((int)d.x, (int)d.y, true);
                color.a = 1.0f;
            }
            break;
        case ToolId::Select:
            if (transforming) { transformDrag(d); break; }
            if (selDragging) {
                selCur = d;
                if (selMethod == SelMethod::Freehand &&
                    (selPoints.empty() || (d - selPoints.back()).length() * view.scale > 3.0f))
                    selPoints.push_back(d);
            }
            break;
        default:
            break;
    }
}

void Editor::canvasUp(Vec2 screen, double t) {
    switch (tool) {
        case ToolId::Brush:
        case ToolId::Eraser:
            if (strokeActive_) queue_.push_back({2, view.toDoc(screen), lastPressure_, t});
            break;
        case ToolId::Eyedropper:
            if (pickActive) { pickActive = false; addRecentColor(color); tool = returnTool; }
            break;
        case ToolId::Select:
            if (transforming) { dragHandle_ = Handle::None; break; }
            if (selDragging) finishSelectionDrag();
            break;
        default:
            break;
    }
}

void Editor::cancelCanvasAction() {
    if (strokeActive_) {
        queue_.clear();
        eng_[0].reset();
        eng_[1].reset();
        strokeActive_ = false;
        if (strokeUndoPushed_) { doc.restoreLastUndo(); strokeUndoPushed_ = false; }
    }
    selDragging = false;
    selPoints.clear();
    pickActive = false;
    dragHandle_ = Handle::None;
}

void Editor::finishSelectionDrag() {
    selDragging = false;
    const float minDrag = 4.0f / std::max(view.scale, 0.01f);
    const bool tiny = (selCur - selStart).length() < minDrag && selPoints.size() < 4;
    if (tiny) {
        doc.pushUndo();
        doc.selectNone();
    } else if (selMethod == SelMethod::Freehand) {
        if (selPoints.size() >= 3) { doc.pushUndo(); doc.selectPolygon(selPoints, selOp); }
    } else if (selMethod == SelMethod::Rect) {
        doc.pushUndo();
        doc.selectRect(selStart.x, selStart.y, selCur.x, selCur.y, selOp);
    } else {
        doc.pushUndo();
        doc.selectEllipse(selStart.x, selStart.y, selCur.x, selCur.y, selOp);
    }
    selPoints.clear();
}

void Editor::processSample(const Sample& s) {
    if (s.type == 0) {
        const StrokeSetup a = makeSetup(false);
        if (!a.entry || a.entry->id.empty()) { strokeActive_ = false; return; }
        eng_[0] = makeStrokeEngine(*a.entry);
        eng_[0]->begin(doc, doc.state.active, a, s.doc.x, s.doc.y, s.pressure, s.t);
        if (symmetry) {
            const StrokeSetup m = makeSetup(true);
            eng_[1] = makeStrokeEngine(*m.entry);
            eng_[1]->begin(doc, doc.state.active, m, s.doc.x, s.doc.y, s.pressure, s.t);
        }
        ++strokeCounter_;
    } else if (s.type == 1) {
        if (eng_[0]) eng_[0]->move(s.doc.x, s.doc.y, s.pressure, s.t);
        if (eng_[1]) eng_[1]->move(s.doc.x, s.doc.y, s.pressure, s.t);
    } else {
        if (eng_[0]) eng_[0]->end(s.doc.x, s.doc.y, s.t);
        if (eng_[1]) eng_[1]->end(s.doc.x, s.doc.y, s.t);
        eng_[0].reset();
        eng_[1].reset();
        strokeActive_ = false;
        strokeUndoPushed_ = false;
    }
}

void Editor::update(double now) {
    if (!queue_.empty()) {
        const double start = nowSeconds();
        size_t i = 0;
        while (i < queue_.size()) {
            processSample(queue_[i++]);
            if (nowSeconds() - start > 0.012) break;
        }
        queue_.erase(queue_.begin(), queue_.begin() + (long)i);
    } else if (strokeActive_ && eng_[0] && now - lastMoveWall_ > 0.035) {
        eng_[0]->hold(now);
        if (eng_[1]) eng_[1]->hold(now);
    }
}

// -------------------------------------------------------- transformacao

Vec2 Editor::sideMid(int i) const { return (quad[i] + quad[(i + 1) % 4]) * 0.5f; }

Vec2 Editor::rotateNode() const {
    const Vec2 c = (quad[0] + quad[1] + quad[2] + quad[3]) * 0.25f;
    const Vec2 tm = sideMid(0);
    return tm + norm(tm - c) * (46.0f * dp_ / std::max(view.scale, 0.001f));
}

void Editor::updateXform() {
    float H[9];
    if (solveHomography(quad0_, quad, H)) {
        std::memcpy(xform.H, H, sizeof(H));
        xform.active = true;
        xform.layerId = doc.floatingId;
    }
}

bool Editor::beginTransform(bool copy) {
    if (transforming) return true;
    std::string why;
    if (!canDraw(&why)) { toast(why); return false; }
    doc.pushUndo();
    if (!doc.beginFloating(copy)) {
        doc.dropLastUndo();
        toast(doc.hasSelection() ? "Nada para mover na selecao" : "Camada vazia");
        return false;
    }
    const RectI b = doc.floatBounds;
    quad0_[0] = quad[0] = Vec2((float)b.x0, (float)b.y0);
    quad0_[1] = quad[1] = Vec2((float)b.x1, (float)b.y0);
    quad0_[2] = quad[2] = Vec2((float)b.x1, (float)b.y1);
    quad0_[3] = quad[3] = Vec2((float)b.x0, (float)b.y1);
    transforming = true;
    tool = ToolId::Select;
    updateXform();
    return true;
}

void Editor::applyTransform() {
    if (!transforming) return;
    float H[9];
    std::memcpy(H, xform.H, sizeof(H));
    transforming = false;
    xform = LayerXform();
    dragHandle_ = Handle::None;
    doc.commitFloating(H);
}

void Editor::cancelTransform() {
    if (!transforming) return;
    transforming = false;
    xform = LayerXform();
    dragHandle_ = Handle::None;
    doc.floatingId = 0;
    doc.restoreLastUndo();
}

void Editor::transformTranslate(Vec2 d) {
    if (!transforming) return;
    for (int i = 0; i < 4; ++i) quad[i] = quad[i] + d;
    updateXform();
}

void Editor::transformPinch(Vec2 P, float s, float rot) {
    if (!transforming) return;
    const float c = std::cos(rot) * s, sn = std::sin(rot) * s;
    for (int i = 0; i < 4; ++i) {
        const Vec2 v = quad[i] - P;
        quad[i] = Vec2(P.x + c * v.x - sn * v.y, P.y + sn * v.x + c * v.y);
    }
    updateXform();
}

void Editor::flipTransform(bool horizontal) {
    if (!transforming) return;
    if (horizontal) { std::swap(quad[0], quad[1]); std::swap(quad[3], quad[2]); }
    else { std::swap(quad[0], quad[3]); std::swap(quad[1], quad[2]); }
    updateXform();
}

Handle Editor::hitHandle(Vec2 p) const {
    const float tol = 30.0f * dp_ / std::max(view.scale, 0.001f);
    if ((p - rotateNode()).length() < tol) return Handle::Rotate;
    for (int i = 0; i < 4; ++i) if ((p - quad[i]).length() < tol) return (Handle)((int)Handle::C0 + i);
    for (int i = 0; i < 4; ++i) if ((p - sideMid(i)).length() < tol) return (Handle)((int)Handle::S0 + i);
    return Handle::Body;
}

void Editor::transformDown(Vec2 p) {
    dragHandle_ = hitHandle(p);
    dragStart_ = p;
    dragLast_ = p;
    for (int i = 0; i < 4; ++i) dragQuad_[i] = quad[i];
    const Vec2 c = (quad[0] + quad[1] + quad[2] + quad[3]) * 0.25f;
    dragRotStart_ = std::atan2(p.y - c.y, p.x - c.x);
}

void Editor::transformDrag(Vec2 p) {
    if (dragHandle_ == Handle::None) return;
    const Vec2 delta = p - dragStart_;
    const Vec2* Q = dragQuad_;
    const Vec2 center = (Q[0] + Q[1] + Q[2] + Q[3]) * 0.25f;

    if (dragHandle_ == Handle::Body) {
        for (int i = 0; i < 4; ++i) quad[i] = Q[i] + delta;
    } else if (dragHandle_ == Handle::Rotate) {
        const float a = std::atan2(p.y - center.y, p.x - center.x) - dragRotStart_;
        const float c = std::cos(a), s = std::sin(a);
        for (int i = 0; i < 4; ++i) {
            const Vec2 v = Q[i] - center;
            quad[i] = Vec2(center.x + c * v.x - s * v.y, center.y + s * v.x + c * v.y);
        }
    } else if ((int)dragHandle_ >= (int)Handle::C0 && (int)dragHandle_ <= (int)Handle::C3) {
        const int i = (int)dragHandle_ - (int)Handle::C0;
        const Vec2 O = Q[(i + 2) % 4];
        if (tmode == TransformMode::Distort) {
            for (int k = 0; k < 4; ++k) quad[k] = Q[k];
            quad[i] = Q[i] + delta;
        } else if (tmode == TransformMode::Uniform) {
            const Vec2 v0 = Q[i] - O, v1 = Q[i] + delta - O;
            const float den = dot(v0, v0);
            if (den < 1e-6f) return;
            const float s = std::max(0.02f, dot(v1, v0) / den);
            for (int k = 0; k < 4; ++k) quad[k] = O + (Q[k] - O) * s;
        } else {
            const Vec2 u = Q[1] - Q[0], v = Q[3] - Q[0];
            const Vec2 uh = norm(u), vh = norm(v);
            const int sx = (i == 1 || i == 2) ? 1 : 0, sy = (i >= 2) ? 1 : 0;
            const Vec2 A = uh * (float)(2 * sx - 1), B = vh * (float)(2 * sy - 1);
            const Vec2 w = Q[i] + delta - O;
            const float det = A.x * B.y - A.y * B.x;
            if (std::fabs(det) < 1e-6f) return;
            const float a = (w.x * B.y - w.y * B.x) / det;
            const float b = (A.x * w.y - A.y * w.x) / det;
            const Vec2 nu = uh * a, nv = vh * b;
            const Vec2 P0 = O - nu * (float)(1 - sx) - nv * (float)(1 - sy);
            quad[0] = P0; quad[1] = P0 + nu; quad[3] = P0 + nv; quad[2] = P0 + nu + nv;
        }
    } else {  // lados
        const int i = (int)dragHandle_ - (int)Handle::S0;
        const int j = (i + 1) % 4;
        for (int k = 0; k < 4; ++k) quad[k] = Q[k];
        if (tmode == TransformMode::Distort) {
            const Vec2 t = norm(Q[j] - Q[i]);
            const Vec2 d = t * dot(delta, t);
            quad[i] = Q[i] + d;
            quad[j] = Q[j] + d;
        } else {
            const Vec2 uh = norm(Q[1] - Q[0]), vh = norm(Q[3] - Q[0]);
            Vec2 out = (i == 0) ? vh * -1.0f : (i == 1) ? uh : (i == 2) ? vh : uh * -1.0f;
            const Vec2 d = out * dot(delta, out);
            quad[i] = Q[i] + d;
            quad[j] = Q[j] + d;
        }
    }
    updateXform();
}

// ---------------------------------------------------------- documento

void Editor::undo() {
    cancelCanvasAction();
    if (transforming) { cancelTransform(); return; }
    if (!doc.undo()) toast("Nada para desfazer", 1.0);
}

void Editor::redo() {
    cancelCanvasAction();
    if (transforming) cancelTransform();
    if (!doc.redo()) toast("Nada para refazer", 1.0);
}

void Editor::addLayer() {
    doc.pushUndo();
    if (doc.addLayer() < 0) { doc.dropLastUndo(); toast("Limite de camadas atingido"); }
}
void Editor::duplicateLayer() {
    doc.pushUndo();
    if (doc.duplicateLayer(doc.state.active) < 0) { doc.dropLastUndo(); toast("Limite de camadas atingido"); }
}
void Editor::deleteLayer() { doc.pushUndo(); doc.removeLayer(doc.state.active); }
void Editor::mergeLayerDown() {
    doc.pushUndo();
    if (!doc.mergeDown(doc.state.active)) { doc.dropLastUndo(); toast("Não há camada abaixo"); }
}
void Editor::moveLayer(int dir) {
    const int to = doc.state.active + dir;
    if (to < 0 || to >= doc.layerCount()) return;
    doc.pushUndo();
    doc.moveLayer(doc.state.active, to);
}
void Editor::clearLayer() {
    if (doc.activeLayer().locked) { toast("Camada bloqueada"); return; }
    doc.pushUndo();
    doc.clearLayer(doc.state.active);
}
void Editor::beginLayerOpacityEdit() { doc.pushUndo(); opacityEditStarted_ = true; }

void Editor::layerProperty(int index, int what, float value) {
    if (index < 0 || index >= doc.layerCount()) return;
    if (what == 3) {
        doc.state.layers[index].opacity = clampf(value, 0.0f, 1.0f);
        doc.touch();
        return;
    }
    doc.pushUndo();
    Layer& l = doc.state.layers[index];
    switch (what) {
        case 0: l.visible = !l.visible; break;
        case 1: l.locked = !l.locked; break;
        case 2: l.alphaLock = !l.alphaLock; break;
        case 4: l.blend = (Blend)(((int)l.blend + 1) % (int)Blend::Count); break;
        default: break;
    }
    doc.touch();
}

void Editor::selectAllCmd() { doc.pushUndo(); doc.selectAll(); }
void Editor::deselectCmd() { if (doc.hasSelection()) { doc.pushUndo(); doc.selectNone(); } }
void Editor::invertSelectionCmd() { doc.pushUndo(); doc.invertSelection(); }
void Editor::featherSelectionCmd() {
    if (!doc.hasSelection()) { toast("Sem seleção"); return; }
    doc.pushUndo();
    doc.featherSelection(8);
}
void Editor::fillSelectionCmd() {
    if (!doc.hasSelection()) { toast("Sem seleção"); return; }
    std::string why;
    if (!canDraw(&why)) { toast(why); return; }
    doc.pushUndo();
    doc.fillSelection(doc.state.active, color);
    addRecentColor(color);
}
void Editor::clearSelectionCmd() {
    if (!doc.hasSelection()) { toast("Sem seleção"); return; }
    if (doc.activeLayer().locked) { toast("Camada bloqueada"); return; }
    doc.pushUndo();
    doc.clearSelectionPixels(doc.state.active);
}
void Editor::copyToNewLayerCmd() {
    if (!doc.hasSelection()) { toast("Sem seleção"); return; }
    doc.pushUndo();
    if (doc.layerViaCopy() < 0) { doc.dropLastUndo(); toast("Limite de camadas atingido"); return; }
    doc.activeLayer().name = "Da seleção";
    toast("Copiado para nova camada");
}
void Editor::setPaperColor(Color c) { doc.paper = c; doc.paper.a = 1.0f; doc.touch(); }
void Editor::togglePaper() { doc.paperVisible = !doc.paperVisible; doc.touch(); }

void Editor::addRecentColor(Color c) {
    for (size_t i = 0; i < recent.size(); ++i) {
        if (std::fabs(recent[i].r - c.r) + std::fabs(recent[i].g - c.g) + std::fabs(recent[i].b - c.b) < 0.02f) {
            recent.erase(recent.begin() + (long)i);
            break;
        }
    }
    c.a = 1.0f;
    recent.insert(recent.begin(), c);
    if (recent.size() > 14) recent.resize(14);
}

// ----------------------------------------------------- animacao / timeline

void Editor::resetAnimation() {
    animation.reset();
    animationTracks_.clear();
    animationCels_.clear();
    displayedAnimationFrame_ = -1;
    lastAnimationTime_ = 0.0;
    animationReady_ = false;
}

void Editor::syncAnimationTracks() {
    if (!animationReady_) {
        animation.reset();
        animationReady_ = true;
        displayedAnimationFrame_ = animation.currentFrame();
    }
    if (animation.frameCount() < 1) animation.setFrameCount(1);

    std::unordered_set<uint32_t> live;
    for (Layer& layer : doc.state.layers) {
        live.insert(layer.id);
        auto binding = animationTracks_.find(layer.id);
        uint64_t trackId = 0;
        if (binding == animationTracks_.end()) {
            trackId = animation.addTrack(layer.name);
            animationTracks_[layer.id] = trackId;
            for (int frame = 0; frame < animation.frameCount(); ++frame) {
                animation.insertFrame(trackId, frame);
                AnimationTrack* track = nullptr;
                for (auto& candidate : animation.tracks())
                    if (candidate.id == trackId) { track = &candidate; break; }
                if (!track || frame >= (int)track->frames.size()) continue;
                const int visibleFrame = displayedAnimationFrame_ >= 0 ? displayedAnimationFrame_ : animation.currentFrame();
                if (frame == visibleFrame) {
                    AnimationCel cel;
                    const bool hasPixels = std::any_of(layer.tiles.begin(), layer.tiles.end(),
                        [](const TilePtr& tile) { return (bool)tile; });
                    if (hasPixels) cel.tiles = layer.tiles;
                    if (hasPixels) animationCels_[track->frames[(size_t)frame].celId] = std::move(cel);
                }
            }
        } else {
            trackId = binding->second;
            AnimationTrack* track = nullptr;
            for (auto& candidate : animation.tracks())
                if (candidate.id == trackId) { track = &candidate; break; }
            if (!track) {
                animationTracks_.erase(binding);
                trackId = animation.addTrack(layer.name);
                animationTracks_[layer.id] = trackId;
                track = &animation.tracks().back();
            }
            track->name = layer.name;
            while ((int)track->frames.size() < animation.frameCount()) {
                const int at = (int)track->frames.size();
                animation.insertFrame(trackId, at);
            }
        }
    }

    bool removedTrack = false;
    for (auto it = animationTracks_.begin(); it != animationTracks_.end();) {
        if (live.find(it->first) == live.end()) {
            animation.removeTrack(it->second);
            it = animationTracks_.erase(it);
            removedTrack = true;
        } else {
            ++it;
        }
    }
    if (removedTrack) pruneAnimationCels();
    // Keep the visual track order aligned with the document layer stack.
    int order = 0;
    for (const Layer& layer : doc.state.layers) {
        const auto binding = animationTracks_.find(layer.id);
        if (binding != animationTracks_.end()) animation.moveTrack(binding->second, order++);
    }
    if (displayedAnimationFrame_ < 0) displayedAnimationFrame_ = animation.currentFrame();
}

void Editor::pruneAnimationCels() {
    std::unordered_set<uint64_t> referenced;
    for (const AnimationTrack& track : animation.tracks())
        for (const AnimationFrame& frame : track.frames)
            if (frame.celId != 0) referenced.insert(frame.celId);
    for (auto it = animationCels_.begin(); it != animationCels_.end();) {
        if (referenced.find(it->first) == referenced.end()) it = animationCels_.erase(it);
        else ++it;
    }
}

void Editor::storeAnimationFrame(int frame) {
    if (frame < 0 || frame >= animation.frameCount()) return;
    for (const Layer& layer : doc.state.layers) {
        const auto binding = animationTracks_.find(layer.id);
        if (binding == animationTracks_.end()) continue;
        for (const AnimationTrack& track : animation.tracks()) {
            if (track.id != binding->second || frame >= (int)track.frames.size()) continue;
            const uint64_t celId = track.frames[(size_t)frame].celId;
            if (celId != 0) {
                auto& tiles = animationCels_[celId].tiles;
                const bool hasPixels = std::any_of(layer.tiles.begin(), layer.tiles.end(),
                    [](const TilePtr& tile) { return (bool)tile; });
                if (hasPixels) tiles = layer.tiles;
                else tiles.clear();
            }
            break;
        }
    }
}

void Editor::applyAnimationFrame(int frame) {
    if (displayedAnimationFrame_ >= 0) storeAnimationFrame(displayedAnimationFrame_);
    syncAnimationTracks();
    if (frame < 0 || frame >= animation.frameCount()) return;
    const size_t tileCount = (size_t)doc.tilesX() * (size_t)doc.tilesY();
    for (Layer& layer : doc.state.layers) {
        const auto binding = animationTracks_.find(layer.id);
        if (binding == animationTracks_.end()) continue;
        const AnimationTrack* found = nullptr;
        for (const AnimationTrack& track : animation.tracks())
            if (track.id == binding->second) { found = &track; break; }
        if (!found || frame >= (int)found->frames.size()) {
            layer.tiles.assign(tileCount, nullptr);
            continue;
        }
        const uint64_t celId = found->frames[(size_t)frame].celId;
        const auto cel = animationCels_.find(celId);
        if (cel == animationCels_.end() || cel->second.tiles.empty()) layer.tiles.assign(tileCount, nullptr);
        else layer.tiles = cel->second.tiles;
        if (layer.tiles.size() != tileCount) layer.tiles.resize(tileCount);
    }
    displayedAnimationFrame_ = frame;
    doc.touch();
}

bool Editor::seekAnimation(int frame) {
    syncAnimationTracks();
    if (frame < 0 || frame >= animation.frameCount()) return false;
    if (frame == animation.currentFrame() && frame == displayedAnimationFrame_) return true;
    storeAnimationFrame(displayedAnimationFrame_);
    if (!animation.seek(frame) && animation.currentFrame() != frame) return false;
    displayedAnimationFrame_ = -1;
    applyAnimationFrame(frame);
    lastAnimationTime_ = 0.0;
    return true;
}

bool Editor::setAnimationFps(int fps) {
    syncAnimationTracks();
    return animation.setFps(fps);
}

bool Editor::setAnimationFrameCount(int count) {
    syncAnimationTracks();
    if (count < 1 || count > kMaxTimelineFrames) return false;
    storeAnimationFrame(displayedAnimationFrame_);
    if (!animation.setFrameCount(count)) return false;
    displayedAnimationFrame_ = -1;
    syncAnimationTracks();
    applyAnimationFrame(animation.currentFrame());
    return true;
}

bool Editor::insertAnimationFrame(bool duplicateCurrent) {
    syncAnimationTracks();
    const int oldCount = animation.frameCount();
    if (oldCount >= kMaxTimelineFrames) return false;
    storeAnimationFrame(displayedAnimationFrame_);
    const int insertion = animation.currentFrame() + 1;
    std::vector<uint64_t> trackIds;
    for (const AnimationTrack& track : animation.tracks()) trackIds.push_back(track.id);
    for (uint64_t id : trackIds) {
        const bool ok = duplicateCurrent
            ? animation.duplicateFrame(id, animation.currentFrame(), insertion)
            : animation.insertFrame(id, insertion) != 0;
        if (!ok) return false;
    }
    if (animation.frameCount() == oldCount && !animation.setFrameCount(oldCount + 1)) return false;
    return seekAnimation(std::min(insertion, animation.frameCount() - 1));
}

bool Editor::removeAnimationFrame() {
    syncAnimationTracks();
    if (animation.frameCount() <= 1) return false;
    const int removing = animation.currentFrame();
    storeAnimationFrame(displayedAnimationFrame_);
    std::vector<uint64_t> trackIds;
    for (const AnimationTrack& track : animation.tracks()) trackIds.push_back(track.id);
    for (uint64_t id : trackIds)
        if (!animation.removeFrame(id, removing)) return false;
    if (!animation.setFrameCount(animation.frameCount() - 1)) return false;
    const int target = std::min(removing, animation.frameCount() - 1);
    pruneAnimationCels();
    displayedAnimationFrame_ = -1;
    animation.seek(target);
    applyAnimationFrame(target);
    return true;
}

bool Editor::setAnimationPlaying(bool playing) {
    syncAnimationTracks();
    if (playing) storeAnimationFrame(displayedAnimationFrame_);
    lastAnimationTime_ = 0.0;
    return animation.setPlaying(playing);
}

int Editor::animationTrackCount() {
    syncAnimationTracks();
    return (int)animation.tracks().size();
}

std::string Editor::animationTrackName(int index) {
    syncAnimationTracks();
    if (index < 0 || index >= (int)animation.tracks().size()) return "";
    return animation.tracks()[(size_t)index].name;
}

void Editor::updateAnimation(double now) {
    syncAnimationTracks();
    if (!animation.playing()) {
        lastAnimationTime_ = 0.0;
        return;
    }
    if (lastAnimationTime_ <= 0.0) {
        lastAnimationTime_ = now;
        return;
    }
    const double delta = now - lastAnimationTime_;
    lastAnimationTime_ = now;
    if (animation.advance(delta)) applyAnimationFrame(animation.currentFrame());
}

bool Editor::saveAnimation(const std::string& path) {
    syncAnimationTracks();
    storeAnimationFrame(displayedAnimationFrame_);
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    auto put = [f](const auto& value) { return std::fwrite(&value, sizeof(value), 1, f) == 1; };
    auto raw = [f](const void* data, size_t size) { return size == 0 || std::fwrite(data, 1, size, f) == size; };
    const uint32_t magic = 0x314D4E41u; // ANM1
    const uint32_t width = (uint32_t)doc.width(), height = (uint32_t)doc.height();
    const uint32_t frameCount = (uint32_t)animation.frameCount();
    const uint32_t trackCount = (uint32_t)animation.tracks().size();
    uint32_t celCount = 0;
    for (const auto& entry : animationCels_) if (!entry.second.tiles.empty()) ++celCount;
    const int32_t fps = animation.fps(), current = animation.currentFrame();
    const OnionSkinSettings onion = animation.onionSkin();
    bool ok = put(magic) && put(width) && put(height) && put(fps) && put(frameCount) &&
              put(current) && put(trackCount) && put(celCount) &&
              put((uint8_t)(onion.enabled ? 1 : 0)) && put((int32_t)onion.previousFrames) &&
              put((int32_t)onion.nextFrames) && put(onion.previousOpacity) && put(onion.nextOpacity);
    for (size_t li = 0; ok && li < doc.state.layers.size(); ++li) {
        const Layer& layer = doc.state.layers[li];
        const auto binding = animationTracks_.find(layer.id);
        const AnimationTrack* track = nullptr;
        if (binding != animationTracks_.end())
            for (const auto& candidate : animation.tracks())
                if (candidate.id == binding->second) { track = &candidate; break; }
        if (!track) { ok = false; break; }
        const uint32_t layerIndex = (uint32_t)li;
        const uint16_t nameSize = (uint16_t)std::min<size_t>(track->name.size(), 200);
        const uint32_t frames = (uint32_t)track->frames.size();
        const uint32_t keys = (uint32_t)track->keys.size();
        ok = put(layerIndex) && put(nameSize) && raw(track->name.data(), nameSize) &&
             put((uint8_t)(track->visible ? 1 : 0)) && put((uint8_t)(track->locked ? 1 : 0)) &&
             put(track->opacity) && put(frames) && put(keys);
        for (const AnimationFrame& frame : track->frames) {
            const uint64_t celId = frame.celId;
            const int32_t exposure = frame.exposure;
            ok = ok && put(celId) && put(exposure) && put((uint8_t)(frame.enabled ? 1 : 0));
        }
        for (const AnimationKeyframe& key : track->keys) {
            const int32_t frame = key.frame;
            const uint8_t property = (uint8_t)key.property;
            const uint8_t interpolation = (uint8_t)key.interpolation;
            ok = ok && put(frame) && put(property) && put(key.value) && put(interpolation);
        }
    }
    for (const auto& entry : animationCels_) {
        if (!ok) break;
        if (entry.second.tiles.empty()) continue;
        const uint64_t celId = entry.first;
        uint32_t populated = 0;
        for (const TilePtr& tile : entry.second.tiles) if (tile) ++populated;
        ok = put(celId) && put(populated);
        for (size_t ti = 0; ok && ti < entry.second.tiles.size(); ++ti) {
            const TilePtr& tile = entry.second.tiles[ti];
            if (!tile) continue;
            const uint32_t index = (uint32_t)ti;
            ok = put(index) && raw(tile->px, sizeof(tile->px));
        }
    }
    std::fclose(f);
    if (!ok) { std::remove(tmp.c_str()); return false; }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool Editor::loadAnimation(const std::string& path) {
    resetAnimation();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { syncAnimationTracks(); return false; }
    auto get = [f](auto& value) { return std::fread(&value, sizeof(value), 1, f) == 1; };
    auto raw = [f](void* data, size_t size) { return size == 0 || std::fread(data, 1, size, f) == size; };
    uint32_t magic = 0, width = 0, height = 0, frameCount = 0, trackCount = 0, celCount = 0;
    int32_t fps = 24, current = 0, previous = 2, next = 1;
    uint8_t onionEnabled = 1;
    float previousOpacity = 0.22f, nextOpacity = 0.16f;
    bool ok = get(magic) && get(width) && get(height) && get(fps) && get(frameCount) &&
              get(current) && get(trackCount) && get(celCount) && get(onionEnabled) &&
              get(previous) && get(next) && get(previousOpacity) && get(nextOpacity);
    if (!ok || magic != 0x314D4E41u || width != (uint32_t)doc.width() ||
        height != (uint32_t)doc.height() || frameCount < 1 || frameCount > kMaxTimelineFrames ||
        trackCount > kMaxLayers || celCount > 100000 || fps < kMinTimelineFps ||
        fps > kMaxTimelineFps) {
        std::fclose(f);
        resetAnimation();
        syncAnimationTracks();
        return false;
    }
    animation.setFrameCount((int)frameCount);
    animation.setFps(fps);
    for (uint32_t ti = 0; ok && ti < trackCount; ++ti) {
        uint32_t layerIndex = 0, frames = 0, keys = 0;
        uint16_t nameSize = 0;
        uint8_t visible = 1, locked = 0;
        float opacity = 1.0f;
        ok = get(layerIndex) && get(nameSize) && nameSize <= 200;
        std::string name(nameSize, '\0');
        ok = ok && raw(name.data(), nameSize) && get(visible) && get(locked) &&
             get(opacity) && get(frames) && get(keys);
        if (!ok || frames > kMaxTimelineFrames || keys > 100000) { ok = false; break; }
        const uint64_t trackId = animation.addTrack(name);
        AnimationTrack* track = &animation.tracks().back();
        track->visible = visible != 0;
        track->locked = locked != 0;
        track->opacity = clampf(opacity, 0.0f, 1.0f);
        for (uint32_t fi = 0; ok && fi < frames; ++fi) {
            uint64_t celId = 0;
            int32_t exposure = 1;
            uint8_t enabled = 1;
            ok = get(celId) && get(exposure) && get(enabled);
            if (!ok || animation.insertFrame(trackId, (int)fi, celId) == 0) { ok = false; break; }
            track->frames[fi].exposure = clampi(exposure, 1, (int)frameCount);
            track->frames[fi].enabled = enabled != 0;
        }
        for (uint32_t ki = 0; ok && ki < keys; ++ki) {
            int32_t frame = 0;
            uint8_t property = 0, interpolation = 0;
            float value = 0;
            ok = get(frame) && get(property) && get(value) && get(interpolation);
            if (!ok || property > (uint8_t)AnimatedProperty::Visibility ||
                interpolation > (uint8_t)KeyInterpolation::EaseInOut) { ok = false; break; }
            AnimationKeyframe key;
            key.frame = frame;
            key.property = (AnimatedProperty)property;
            key.value = value;
            key.interpolation = (KeyInterpolation)interpolation;
            ok = animation.setKeyframe(trackId, key);
        }
        if (layerIndex < doc.state.layers.size())
            animationTracks_[doc.state.layers[layerIndex].id] = trackId;
    }
    const uint32_t expectedTiles = (uint32_t)((size_t)doc.tilesX() * (size_t)doc.tilesY());
    for (uint32_t ci = 0; ok && ci < celCount; ++ci) {
        uint64_t celId = 0;
        uint32_t populated = 0;
        ok = get(celId) && get(populated) && populated <= expectedTiles;
        if (!ok) break;
        AnimationCel cel;
        if (populated > 0) cel.tiles.assign(expectedTiles, nullptr);
        for (uint32_t k = 0; ok && k < populated; ++k) {
            uint32_t index = 0;
            ok = get(index) && index < expectedTiles;
            if (!ok) break;
            auto tile = std::make_shared<Tile>();
            ok = raw(tile->px, sizeof(tile->px));
            tile->version = nextTileVersion();
            if (ok) cel.tiles[index] = std::move(tile);
        }
        if (ok) animationCels_[celId] = std::move(cel);
    }
    std::fclose(f);
    if (!ok || animation.tracks().size() != trackCount) {
        resetAnimation();
        syncAnimationTracks();
        return false;
    }
    OnionSkinSettings& onion = animation.onionSkin();
    onion.enabled = onionEnabled != 0;
    onion.previousFrames = clampi(previous, 0, 12);
    onion.nextFrames = clampi(next, 0, 12);
    onion.previousOpacity = clampf(previousOpacity, 0.0f, 1.0f);
    onion.nextOpacity = clampf(nextOpacity, 0.0f, 1.0f);
    animation.seek(clampi(current, 0, (int)frameCount - 1));
    animationReady_ = true;
    displayedAnimationFrame_ = -1;
    applyAnimationFrame(animation.currentFrame());
    return true;
}

void Editor::toast(const std::string& s, double seconds) {
    toastText = s;
    toastUntil = nowSeconds() + seconds;
    ++toastSeq;
}

// ------------------------------------------------------------ arquivos

void Editor::setPaths(const std::string& internalDir, const std::string& externalDir) {
    internalDir_ = internalDir;
    externalDir_ = externalDir;
    if (!internalDir_.empty()) makeDirs(internalDir_);
    if (!externalDir_.empty()) makeDirs(externalDir_);
}

void Editor::newDocument(int w, int h, bool transparentPaper) {
    cancelCanvasAction();
    if (transforming) cancelTransform();
    doc.create(w, h);
    resetAnimation();
    doc.paperVisible = !transparentPaper;
    fitView();
    toast("Nova tela " + std::to_string(w) + "x" + std::to_string(h));
}

void Editor::initBrushesAndPrefs() {
    BrushLibrary& lib = BrushLibrary::get();
    brushPaint_.entry = lib.findDefault(false);
    brushErase_.entry = lib.findDefault(true);
    if (brushPaint_.entry >= 0) brushPaint_.diameter = lib.entries()[brushPaint_.entry].defaultDiameter;
    if (brushErase_.entry >= 0) brushErase_.diameter = lib.entries()[brushErase_.entry].defaultDiameter;
    loadPrefs();
}

bool Editor::exportPng() {
    const std::string dir = (externalDir_.empty() ? internalDir_ : externalDir_) + "/exports";
    makeDirs(dir);
    char name[64];
    const time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    std::strftime(name, sizeof(name), "ClipPaint_%Y%m%d_%H%M%S.png", &tmv);
    std::vector<uint8_t> px;
    doc.compositeAll(px, doc.paperVisible);
    const bool ok = writePng(dir + "/" + name, doc.width(), doc.height(), px.data(), dpi);
    toast(ok ? std::string("PNG salvo: exports/") + name : "Falha ao exportar PNG", 3.5);
    return ok;
}

void Editor::savePrefs() {
    std::string s;
    char buf[320];
    std::snprintf(buf, sizeof(buf), "color %.4f %.4f %.4f\n", color.r, color.g, color.b); s += buf;
    std::snprintf(buf, sizeof(buf), "finger %d\nsymmetry %d\n", fingerDraws ? 1 : 0, symmetry ? 1 : 0); s += buf;
    std::snprintf(buf, sizeof(buf), "fill %.3f %d %d\nseltol %.3f\n", fillTolerance, fillAllLayers ? 1 : 0, fillGrow, selTolerance); s += buf;
    for (int e = 0; e < 2; ++e) {
        const BrushSetup& b = e ? brushErase_ : brushPaint_;
        std::snprintf(buf, sizeof(buf), "brush %d %.3f %.3f %.3f %.3f ", e, b.diameter, b.opacity, b.smoothing, b.hardness);
        s += buf + entryOf(b).id + "\n";
    }
    for (const Color& r : recent) {
        std::snprintf(buf, sizeof(buf), "recent %.4f %.4f %.4f\n", r.r, r.g, r.b);
        s += buf;
    }
    writeTextFile(internalDir_ + "/prefs.txt", s);
}

void Editor::loadPrefs() {
    std::string s;
    if (internalDir_.empty() || !readTextFile(internalDir_ + "/prefs.txt", s)) return;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string::npos) nl = s.size();
        const std::string line = s.substr(pos, nl - pos);
        pos = nl + 1;
        float a, b, c, d;
        int i, j, k;
        char name[200];
        if (std::sscanf(line.c_str(), "color %f %f %f", &a, &b, &c) == 3) color = Color(a, b, c, 1.0f);
        else if (std::sscanf(line.c_str(), "finger %d", &i) == 1) fingerDraws = i != 0;
        else if (std::sscanf(line.c_str(), "symmetry %d", &i) == 1) symmetry = i != 0;
        else if (std::sscanf(line.c_str(), "seltol %f", &a) == 1) selTolerance = clampf(a, 0.0f, 1.0f);
        else if (std::sscanf(line.c_str(), "fill %f %d %d", &a, &j, &k) == 3) {
            fillTolerance = clampf(a, 0.0f, 1.0f); fillAllLayers = j != 0; fillGrow = clampi(k, 0, 6);
        } else if (std::sscanf(line.c_str(), "brush %d %f %f %f %f %199[^\n]", &i, &a, &b, &c, &d, name) == 6) {
            const int idx = BrushLibrary::get().findById(name);
            if (idx >= 0 && (i == 0 || i == 1)) {
                BrushSetup& bs = i ? brushErase_ : brushPaint_;
                bs.entry = idx;
                bs.diameter = clampf(a, 1.0f, 600.0f);
                bs.opacity = clampf(b, 0.0f, 1.0f);
                bs.smoothing = clampf(c, 0.0f, 1.0f);
                bs.hardness = d;
            }
        } else if (std::sscanf(line.c_str(), "recent %f %f %f", &a, &b, &c) == 3) {
            if (recent.size() < 14) recent.push_back(Color(a, b, c, 1.0f));
        }
    }
}

}  // namespace ClipPaint
