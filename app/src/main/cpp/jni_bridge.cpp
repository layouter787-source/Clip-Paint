// Clip Paint - ponte JNI: expoe o motor nativo para o Kotlin (object com.clippaint.Native).
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include "core/Engine.h"
#include "ui/BrushPreview.h"

using namespace ClipPaint;

#define JF(ret, name) extern "C" JNIEXPORT ret JNICALL Java_com_clippaint_Native_##name
#define LOCK() Engine& E = Engine::get(); std::lock_guard<std::mutex> lockGuard(E.mtx)

namespace {

std::mutex g_previewMutex;
jobject g_assetRef = nullptr;

std::string toStd(JNIEnv* env, jstring s) {
    if (!s) return std::string();
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string r = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return r;
}

jstring toJ(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }

jint packArgb(const Color& c) {
    auto q = [](float v) { return (uint32_t)clampi((int)(v * 255.0f + 0.5f), 0, 255); };
    return (jint)((0xFFu << 24) | (q(c.r) << 16) | (q(c.g) << 8) | q(c.b));
}

Color unpackArgb(jint v) {
    const uint32_t u = (uint32_t)v;
    return Color(((u >> 16) & 255) / 255.0f, ((u >> 8) & 255) / 255.0f, (u & 255) / 255.0f, 1.0f);
}

// RGBA pre-multiplicado (bytes) -> ARGB reto (int), formato do Bitmap.setPixels.
jint straightArgb(const uint8_t* p) {
    const int a = p[3];
    int r = p[0], g = p[1], b = p[2];
    if (a > 0 && a < 255) {
        r = std::min(255, r * 255 / a);
        g = std::min(255, g * 255 / a);
        b = std::min(255, b * 255 / a);
    }
    return (jint)(((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
}

jintArray toIntArray(JNIEnv* env, const std::vector<jint>& v) {
    jintArray a = env->NewIntArray((jsize)v.size());
    if (a && !v.empty()) env->SetIntArrayRegion(a, 0, (jsize)v.size(), v.data());
    return a;
}

jobjectArray toStringArray(JNIEnv* env, const std::vector<std::string>& v) {
    jclass cls = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray((jsize)v.size(), cls, nullptr);
    for (size_t i = 0; i < v.size(); ++i) {
        jstring s = toJ(env, v[i]);
        env->SetObjectArrayElement(arr, (jsize)i, s);
        env->DeleteLocalRef(s);
    }
    return arr;
}

}  // namespace

// ------------------------------------------------------------- ciclo de vida

JF(void, initEngine)(JNIEnv* env, jclass, jobject assets, jstring files, jstring ext, jfloat dp) {
    if (!g_assetRef) g_assetRef = env->NewGlobalRef(assets);
    Engine::get().init(AAssetManager_fromJava(env, g_assetRef), toStd(env, files), toStd(env, ext), dp);
}

JF(jboolean, openProject)(JNIEnv* env, jclass, jstring path, jint w, jint h, jboolean transparent, jint dpi) {
    return Engine::get().openProject(toStd(env, path), w, h, transparent != 0, dpi) ? JNI_TRUE : JNI_FALSE;
}

JF(jboolean, saveProject)(JNIEnv* env, jclass, jstring path) {
    return Engine::get().saveProject(toStd(env, path)) ? JNI_TRUE : JNI_FALSE;
}

JF(jboolean, exportPng)(JNIEnv* env, jclass, jstring path) {
    return Engine::get().exportPng(toStd(env, path)) ? JNI_TRUE : JNI_FALSE;
}

JF(jboolean, saveThumbnail)(JNIEnv* env, jclass, jstring path, jint maxSize) {
    return Engine::get().saveThumbnail(toStd(env, path), maxSize) ? JNI_TRUE : JNI_FALSE;
}

JF(void, surfaceCreated)(JNIEnv* env, jclass, jobject surface) {
    ANativeWindow* w = ANativeWindow_fromSurface(env, surface);
    if (w) Engine::get().surfaceCreated(w);
}

JF(void, surfaceDestroyed)(JNIEnv*, jclass) { Engine::get().surfaceDestroyed(); }

JF(void, requestRender)(JNIEnv*, jclass) { Engine::get().requestRender(12); }

JF(void, setMargins)(JNIEnv*, jclass, jfloat top, jfloat bottom) { Engine::get().setMargins(top, bottom); }

JF(void, fitView)(JNIEnv*, jclass) {
    {
        LOCK();
        E.ed.fitView();
    }
    Engine::get().requestRender();
}

JF(void, resetRotation)(JNIEnv*, jclass) {
    {
        LOCK();
        const Vec2 c((float)E.ed.screenW() * 0.5f, (float)E.ed.screenH() * 0.5f);
        const Vec2 d = E.ed.view.toDoc(c);
        E.ed.view.rot = 0;
        E.ed.view.offset = c - d * E.ed.view.scale;
    }
    Engine::get().requestRender();
}

JF(jint, docWidth)(JNIEnv*, jclass) { LOCK(); return E.ed.doc.width(); }
JF(jint, docHeight)(JNIEnv*, jclass) { LOCK(); return E.ed.doc.height(); }
JF(jfloat, getZoom)(JNIEnv*, jclass) { LOCK(); return E.ed.view.scale; }

// Layout: [acao, indice, nPonteiros, tempo] + por ponteiro [id, tipo, x, y, pressao, nHist] + nHist*[x, y, pressao, tempo]
JF(void, touch)(JNIEnv* env, jclass, jdoubleArray data) {
    const jsize n = env->GetArrayLength(data);
    jdouble* d = env->GetDoubleArrayElements(data, nullptr);
    if (!d) return;
    TouchEvent ev;
    if (n >= 4) {
        ev.action = (int)d[0];
        ev.actionIndex = (int)d[1];
        const int np = (int)d[2];
        ev.time = d[3];
        jsize pos = 4;
        for (int i = 0; i < np && pos + 6 <= n; ++i) {
            TouchPointer p;
            p.id = (int)d[pos];
            p.toolType = (int)d[pos + 1];
            p.x = (float)d[pos + 2];
            p.y = (float)d[pos + 3];
            p.pressure = (float)d[pos + 4];
            const int nh = (int)d[pos + 5];
            pos += 6;
            for (int h = 0; h < nh && pos + 4 <= n; ++h, pos += 4)
                p.history.push_back({(float)d[pos], (float)d[pos + 1], (float)d[pos + 2], d[pos + 3]});
            ev.pointers.push_back(std::move(p));
        }
    }
    env->ReleaseDoubleArrayElements(data, d, 2 /* JNI_ABORT */);
    Engine::get().touch(ev);
}

// ------------------------------------------------------------------- estado

// [podeDesfazer, podeRefazer, ferramenta, transformando, temSelecao, cor, camadaAtiva, nCamadas, revisao, toastSeq]
JF(jintArray, state)(JNIEnv* env, jclass) {
    LOCK();
    std::vector<jint> v = {E.ed.doc.canUndo() ? 1 : 0, E.ed.doc.canRedo() ? 1 : 0, (jint)E.ed.tool,
                           E.ed.transforming ? 1 : 0, E.ed.doc.hasSelection() ? 1 : 0, packArgb(E.ed.color),
                           E.ed.doc.state.active, E.ed.doc.layerCount(), (jint)(E.ed.doc.structRev & 0x7fffffff),
                           (jint)E.ed.toastSeq};
    return toIntArray(env, v);
}

JF(jstring, toastText)(JNIEnv* env, jclass) { LOCK(); return toJ(env, E.ed.toastText); }

JF(void, undo)(JNIEnv*, jclass) { { LOCK(); E.ed.undo(); } Engine::get().requestRender(); }
JF(void, redo)(JNIEnv*, jclass) { { LOCK(); E.ed.redo(); } Engine::get().requestRender(); }

// ---------------------------------------------------------------- ferramentas

JF(void, setTool)(JNIEnv*, jclass, jint t) {
    {
        LOCK();
        const ToolId id = (ToolId)clampi(t, 0, 4);
        if (id == ToolId::Eyedropper && E.ed.tool != ToolId::Eyedropper) E.ed.returnTool = E.ed.tool;
        E.ed.setTool(id);
    }
    Engine::get().requestRender();
}

JF(jint, getTool)(JNIEnv*, jclass) { LOCK(); return (jint)E.ed.tool; }
JF(void, setColor)(JNIEnv*, jclass, jint argb) { LOCK(); E.ed.color = unpackArgb(argb); }
JF(jint, getColor)(JNIEnv*, jclass) { LOCK(); return packArgb(E.ed.color); }
JF(void, addRecent)(JNIEnv*, jclass, jint argb) { LOCK(); E.ed.addRecentColor(unpackArgb(argb)); }

JF(jintArray, recents)(JNIEnv* env, jclass) {
    LOCK();
    std::vector<jint> v;
    for (const Color& c : E.ed.recent) v.push_back(packArgb(c));
    return toIntArray(env, v);
}

JF(void, setFlag)(JNIEnv*, jclass, jint which, jboolean on) {
    {
        LOCK();
        const bool b = on != 0;
        switch (which) {
            case 0: E.ed.symmetry = b; break;
            case 1: E.ed.showGrid = b; break;
            case 2: E.ed.fingerDraws = b; break;
            case 3: if (E.ed.doc.paperVisible != b) E.ed.togglePaper(); break;
            case 4: E.ed.fillAllLayers = b; break;
            default: break;
        }
    }
    Engine::get().requestRender();
}

JF(jboolean, getFlag)(JNIEnv*, jclass, jint which) {
    LOCK();
    switch (which) {
        case 0: return E.ed.symmetry;
        case 1: return E.ed.showGrid;
        case 2: return E.ed.fingerDraws;
        case 3: return E.ed.doc.paperVisible;
        case 4: return E.ed.fillAllLayers;
        default: return JNI_FALSE;
    }
}

JF(void, setPaperColor)(JNIEnv*, jclass, jint argb) { { LOCK(); E.ed.setPaperColor(unpackArgb(argb)); } Engine::get().requestRender(); }

// ------------------------------------------------------------------ pincéis

JF(jobjectArray, brushGroups)(JNIEnv* env, jclass, jint source) {
    return toStringArray(env, BrushLibrary::get().groups(source == 0 ? BrushSource::MyPaint : BrushSource::Anim));
}

JF(jintArray, brushesIn)(JNIEnv* env, jclass, jint source, jstring group, jboolean eraserOnly) {
    const std::vector<int> ids = BrushLibrary::get().inGroup(source == 0 ? BrushSource::MyPaint : BrushSource::Anim,
                                                             toStd(env, group), eraserOnly != 0);
    return toIntArray(env, std::vector<jint>(ids.begin(), ids.end()));
}

static const BrushEntry* entryAt(jint i) {
    const auto& es = BrushLibrary::get().entries();
    return (i >= 0 && i < (jint)es.size()) ? &es[(size_t)i] : nullptr;
}

JF(jstring, brushName)(JNIEnv* env, jclass, jint i) { const BrushEntry* e = entryAt(i); return toJ(env, e ? e->name : ""); }
JF(jstring, brushGroup)(JNIEnv* env, jclass, jint i) { const BrushEntry* e = entryAt(i); return toJ(env, e ? e->group : ""); }
JF(jstring, brushDescription)(JNIEnv* env, jclass, jint i) {
    const BrushEntry* e = entryAt(i);
    return toJ(env, e ? e->description : "");
}
JF(jint, brushFlags)(JNIEnv*, jclass, jint i) {  // bit0 eraser, bit1 smudge, bit2 light(add), bit3 animated, bit4 anim source
    const BrushEntry* e = entryAt(i);
    if (!e) return 0;
    return (e->eraser ? 1 : 0) | (e->smudge ? 2 : 0) | (e->additive ? 4 : 0) | (e->animated ? 8 : 0) |
           (e->source == BrushSource::Anim ? 16 : 0);
}

JF(jintArray, brushPreview)(JNIEnv* env, jclass, jint i, jint w, jint h) {
    const BrushEntry* e = entryAt(i);
    if (!e) return nullptr;
    std::vector<uint8_t> px;
    {
        std::lock_guard<std::mutex> g(g_previewMutex);
        if (!renderBrushPreview(*e, w, h, px)) return nullptr;
    }
    std::vector<jint> out((size_t)w * h);
    for (size_t k = 0; k < out.size(); ++k) out[k] = straightArgb(&px[k * 4]);
    return toIntArray(env, out);
}

JF(void, selectBrush)(JNIEnv*, jclass, jboolean eraser, jint idx) {
    LOCK();
    E.ed.selectBrush(eraser != 0, idx);
}

JF(jint, currentBrush)(JNIEnv*, jclass, jboolean eraser) { LOCK(); return E.ed.brushFor(eraser != 0).entry; }

// which: 0 diametro, 1 opacidade, 2 dureza (-1 = do pincel), 3 suavizacao
JF(jfloat, getBrushValue)(JNIEnv*, jclass, jboolean eraser, jint which) {
    LOCK();
    const BrushSetup& b = E.ed.brushFor(eraser != 0);
    switch (which) {
        case 0: return b.diameter;
        case 1: return b.opacity;
        case 2: return b.hardness;
        default: return b.smoothing;
    }
}

JF(void, setBrushValue)(JNIEnv*, jclass, jboolean eraser, jint which, jfloat v) {
    {
        LOCK();
        BrushSetup& b = E.ed.brushFor(eraser != 0);
        switch (which) {
            case 0: b.diameter = clampf(v, 1.0f, 600.0f); E.ed.showSizeRing(); break;
            case 1: b.opacity = clampf(v, 0.0f, 1.0f); break;
            case 2: b.hardness = v < 0 ? -1.0f : clampf(v, 0.0f, 1.0f); break;
            default: b.smoothing = clampf(v, 0.0f, 1.0f); break;
        }
    }
    Engine::get().requestRender();
}

// 0 tolerancia, 1 expandir (px), 2 tolerancia da selecao automatica
JF(jfloat, getFill)(JNIEnv*, jclass, jint which) {
    LOCK();
    return which == 0 ? E.ed.fillTolerance : (which == 1 ? (jfloat)E.ed.fillGrow : E.ed.selTolerance);
}

JF(void, setFill)(JNIEnv*, jclass, jint which, jfloat v) {
    LOCK();
    if (which == 0) E.ed.fillTolerance = clampf(v, 0.0f, 1.0f);
    else if (which == 1) E.ed.fillGrow = clampi((int)(v + 0.5f), 0, 8);
    else E.ed.selTolerance = clampf(v, 0.0f, 1.0f);
}

// ------------------------------------------------------------------ selecao

JF(void, setSelMethod)(JNIEnv*, jclass, jint m) { LOCK(); E.ed.selMethod = (SelMethod)clampi(m, 0, 3); }
JF(jint, getSelMethod)(JNIEnv*, jclass) { LOCK(); return (jint)E.ed.selMethod; }
JF(void, setSelOp)(JNIEnv*, jclass, jint m) { LOCK(); E.ed.selOp = (SelOp)clampi(m, 0, 2); }
JF(jint, getSelOp)(JNIEnv*, jclass) { LOCK(); return (jint)E.ed.selOp; }

// 0 tudo, 1 inverter, 2 suavizar, 3 pintar, 4 limpar, 5 copiar p/ camada, 6 desmarcar
JF(void, selAction)(JNIEnv*, jclass, jint a) {
    {
        LOCK();
        switch (a) {
            case 0: E.ed.selectAllCmd(); break;
            case 1: E.ed.invertSelectionCmd(); break;
            case 2: E.ed.featherSelectionCmd(); break;
            case 3: E.ed.fillSelectionCmd(); break;
            case 4: E.ed.clearSelectionCmd(); break;
            case 5: E.ed.copyToNewLayerCmd(); break;
            default: E.ed.deselectCmd(); break;
        }
    }
    Engine::get().requestRender();
}

JF(jboolean, beginTransform)(JNIEnv*, jclass, jboolean copy) {
    bool ok;
    {
        LOCK();
        ok = E.ed.beginTransform(copy != 0);
    }
    Engine::get().requestRender();
    return ok ? JNI_TRUE : JNI_FALSE;
}

JF(void, applyTransform)(JNIEnv*, jclass) { { LOCK(); E.ed.applyTransform(); } Engine::get().requestRender(); }
JF(void, cancelTransform)(JNIEnv*, jclass) { { LOCK(); E.ed.cancelTransform(); } Engine::get().requestRender(); }
JF(void, setTransformMode)(JNIEnv*, jclass, jint m) { LOCK(); E.ed.setTransformMode((TransformMode)clampi(m, 0, 2)); }
JF(jint, getTransformMode)(JNIEnv*, jclass) { LOCK(); return (jint)E.ed.tmode; }
JF(void, flipTransform)(JNIEnv*, jclass, jboolean h) { { LOCK(); E.ed.flipTransform(h != 0); } Engine::get().requestRender(); }

// ------------------------------------------------------------------ camadas

JF(jint, layerCount)(JNIEnv*, jclass) { LOCK(); return E.ed.doc.layerCount(); }
JF(jint, activeLayer)(JNIEnv*, jclass) { LOCK(); return E.ed.doc.state.active; }

JF(jstring, layerName)(JNIEnv* env, jclass, jint i) {
    LOCK();
    return toJ(env, (i >= 0 && i < E.ed.doc.layerCount()) ? E.ed.doc.state.layers[(size_t)i].name : "");
}

// bit0 visivel, bit1 bloqueada, bit2 bloqueio de alfa
JF(jint, layerFlags)(JNIEnv*, jclass, jint i) {
    LOCK();
    if (i < 0 || i >= E.ed.doc.layerCount()) return 0;
    const Layer& l = E.ed.doc.state.layers[(size_t)i];
    return (l.visible ? 1 : 0) | (l.locked ? 2 : 0) | (l.alphaLock ? 4 : 0);
}

JF(jfloat, layerOpacity)(JNIEnv*, jclass, jint i) {
    LOCK();
    return (i >= 0 && i < E.ed.doc.layerCount()) ? E.ed.doc.state.layers[(size_t)i].opacity : 1.0f;
}

JF(jstring, layerBlend)(JNIEnv* env, jclass, jint i) {
    LOCK();
    return toJ(env, (i >= 0 && i < E.ed.doc.layerCount()) ? blendName(E.ed.doc.state.layers[(size_t)i].blend) : "");
}

JF(jintArray, layerThumb)(JNIEnv* env, jclass, jint i, jint w, jint h) {
    LOCK();
    const Document& d = E.ed.doc;
    if (i < 0 || i >= d.layerCount() || w <= 0 || h <= 0) return nullptr;
    const Layer& l = d.state.layers[(size_t)i];
    const float ar = (float)d.width() / (float)d.height();
    int tw = w, th = h;
    if ((float)w / (float)h > ar) tw = std::max(1, (int)(h * ar)); else th = std::max(1, (int)(w / ar));
    std::vector<jint> out((size_t)w * h, 0);
    const int ox = (w - tw) / 2, oy = (h - th) / 2;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            float acc[4] = {0, 0, 0, 0};
            for (int sy = 0; sy < 3; ++sy) {
                for (int sx = 0; sx < 3; ++sx) {
                    const int px = (int)((x + (sx + 0.5f) / 3.0f) * d.width() / tw);
                    const int py = (int)((y + (sy + 0.5f) / 3.0f) * d.height() / th);
                    uint8_t c[4];
                    d.getPixel(l, clampi(px, 0, d.width() - 1), clampi(py, 0, d.height() - 1), c);
                    for (int k = 0; k < 4; ++k) acc[k] += c[k];
                }
            }
            uint8_t p[4];
            for (int k = 0; k < 4; ++k) p[k] = (uint8_t)(acc[k] / 9.0f + 0.5f);
            out[(size_t)(y + oy) * w + (x + ox)] = straightArgb(p);
        }
    }
    return toIntArray(env, out);
}

// op: 0 nova, 1 duplicar, 2 apagar, 3 mesclar p/ baixo, 4 subir, 5 descer, 6 limpar, 7 selecionar(i),
//     8 visivel(i), 9 bloquear(i), 10 bloquear alfa(i), 11 modo de mistura(i), 12 opacidade(i, f), 13 iniciar edicao de opacidade
JF(void, layerOp)(JNIEnv*, jclass, jint op, jint i, jfloat f) {
    {
        LOCK();
        Editor& e = E.ed;
        switch (op) {
            case 0: e.addLayer(); break;
            case 1: e.duplicateLayer(); break;
            case 2: e.deleteLayer(); break;
            case 3: e.mergeLayerDown(); break;
            case 4: e.moveLayer(+1); break;
            case 5: e.moveLayer(-1); break;
            case 6: e.clearLayer(); break;
            case 7: e.doc.setActive(i); break;
            case 8: e.layerProperty(i, 0, 0); break;
            case 9: e.layerProperty(i, 1, 0); break;
            case 10: e.layerProperty(i, 2, 0); break;
            case 11: e.layerProperty(i, 4, 0); break;
            case 12: e.layerProperty(i, 3, f); break;
            default: e.beginLayerOpacityEdit(); break;
        }
    }
    Engine::get().requestRender();
}
