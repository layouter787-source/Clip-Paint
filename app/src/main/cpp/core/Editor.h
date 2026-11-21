// Clip Paint - estado do editor: ferramentas, pincel (libmypaint / AnimBrush), selecao com
// transformacao (mover, escalar, girar, perspectiva), fila de amostras, arquivos e preferencias.
#pragma once

#include "../animation/Timeline.h"
#include "../brush/StrokeEngine.h"

#include <unordered_map>
#include "../graphics/Gfx.h"
#include "../io/IO.h"
#include "../layers/Document.h"

namespace ClipPaint {

enum class ToolId { Brush, Eraser, Fill, Select, Eyedropper };
enum class SelMethod { Freehand, Rect, Ellipse, Auto };
enum class TransformMode { Free, Uniform, Distort };
enum class Handle { None, Body, C0, C1, C2, C3, S0, S1, S2, S3, Rotate };

class Editor {
public:
    Editor();

    Document doc;
    AnimationTimeline animation;
    ViewState view;
    ToolId tool = ToolId::Brush;
    Color color{0.08f, 0.09f, 0.12f, 1.0f};
    std::vector<Color> recent;

    bool fingerDraws = true;
    bool symmetry = false;
    bool showGrid = false;

    float fillTolerance = 0.12f;
    bool fillAllLayers = false;
    int fillGrow = 2;

    // --- selecao ---
    SelMethod selMethod = SelMethod::Freehand;
    SelOp selOp = SelOp::Replace;
    float selTolerance = 0.18f;
    std::vector<Vec2> selPoints;
    Vec2 selStart, selCur;
    bool selDragging = false;

    // --- sessao de transformacao (conteudo flutuante) ---
    bool transforming = false;
    TransformMode tmode = TransformMode::Free;
    Vec2 quad[4];            // TL, TR, BR, BL em coordenadas do documento
    LayerXform xform;
    Vec2 rotateNode() const;  // posicao (doc) do no de rotacao
    Vec2 sideMid(int i) const;

    ToolId returnTool = ToolId::Brush;  // ferramenta a retomar depois do conta-gotas
    bool pickActive = false;
    Vec2 pickScreen;

    std::string toastText;
    double toastUntil = 0;
    uint32_t toastSeq = 0;

    // --- inicializacao / arquivos ---
    void setPaths(const std::string& internalDir, const std::string& externalDir);
    void initBrushesAndPrefs();   // escolhe pincéis padrao e le prefs.txt
    void savePrefs();
    int dpi = 0;                  // resolucao do projeto (metadado, vai no PNG)
    double ringUntil = 0;         // mostra o anel de tamanho do pincel no canvas
    void showSizeRing() { ringUntil = nowSeconds() + 0.9; }
    void newDocument(int w, int h, bool transparentPaper);
    bool exportPng();

    // --- tela ---
    void setScreen(int w, int h, float topInset, float bottomInset, float leftInset, float rightInset, float dp);
    void fitView();
    void setSafeMargins(float top, float bottom) { marginTop_ = top; marginBottom_ = bottom; }
    int screenW() const { return screenW_; }
    int screenH() const { return screenH_; }
    float dp() const { return dp_; }

    // --- pincel ---
    BrushSetup& currentBrush() { return tool == ToolId::Eraser ? brushErase_ : brushPaint_; }
    BrushSetup& brushFor(bool eraser) { return eraser ? brushErase_ : brushPaint_; }
    const BrushEntry& entryOf(const BrushSetup& b) const;
    void selectBrush(bool eraserTool, int entryIndex);
    static float sliderToSize(float t);
    static float sizeToSlider(float d);

    // --- ferramentas ---
    void setTool(ToolId t);
    bool canDraw(std::string* why) const;

    // --- entrada no canvas (coordenadas de tela) ---
    void canvasDown(Vec2 screen, float pressure, double t, bool stylus);
    void canvasMove(Vec2 screen, float pressure, double t);
    void canvasUp(Vec2 screen, double t);
    void cancelCanvasAction();
    bool canvasBusy() const { return strokeActive_ || selDragging || pickActive || dragHandle_ != Handle::None; }
    void update(double now);
    bool hasPendingWork() const { return !queue_.empty(); }

    // --- transformacao ---
    bool beginTransform(bool copy);   // levanta a selecao (ou a camada) como conteudo flutuante
    void applyTransform();
    void cancelTransform();
    void transformPinch(Vec2 pivotDoc, float scaleFactor, float rotDelta);
    void transformTranslate(Vec2 deltaDoc);
    void setTransformMode(TransformMode m) { tmode = m; }
    void flipTransform(bool horizontal);
    Handle hitHandle(Vec2 docPt) const;

    // --- comandos de documento ---
    void undo();
    void redo();
    void addLayer();
    void duplicateLayer();
    void deleteLayer();
    void mergeLayerDown();
    void moveLayer(int dir);
    void clearLayer();
    void layerProperty(int index, int what, float value);  // 0 vis,1 lock,2 alphalock,3 opacity,4 blend
    void beginLayerOpacityEdit();
    void selectAllCmd();
    void deselectCmd();
    void invertSelectionCmd();
    void featherSelectionCmd();
    void fillSelectionCmd();
    void clearSelectionCmd();
    void copyToNewLayerCmd();
    void setPaperColor(Color c);
    void togglePaper();
    void addRecentColor(Color c);

    // --- animacao raster integrada ao documento/camadas ---
    void resetAnimation();
    bool loadAnimation(const std::string& path);
    bool saveAnimation(const std::string& path);
    bool seekAnimation(int frame);
    bool setAnimationFps(int fps);
    bool setAnimationFrameCount(int count);
    bool insertAnimationFrame(bool duplicateCurrent);
    bool removeAnimationFrame();
    bool setAnimationPlaying(bool playing);
    int animationTrackCount();
    std::string animationTrackName(int index);
    void updateAnimation(double now);

    void toast(const std::string& s, double seconds = 2.0);

private:
    struct Sample {
        int type;  // 0 inicio, 1 mover, 2 fim
        Vec2 doc;
        float pressure;
        double t;
    };
    void processSample(const Sample& s);
    void loadPrefs();
    void finishSelectionDrag();
    void updateXform();
    void transformDown(Vec2 docPt);
    void transformDrag(Vec2 docPt);
    StrokeSetup makeSetup(bool mirror) const;

    BrushSetup brushPaint_, brushErase_;
    std::unique_ptr<StrokeEngine> eng_[2];
    std::vector<Sample> queue_;
    bool strokeActive_ = false;
    bool strokeUndoPushed_ = false;
    float lastPressure_ = 1.0f;
    Vec2 lastDoc_;
    double lastT_ = 0, lastMoveWall_ = 0;
    uint32_t strokeCounter_ = 1;

    Handle dragHandle_ = Handle::None;
    Vec2 dragStart_, dragLast_;
    Vec2 dragQuad_[4];
    float dragRotStart_ = 0;
    bool opacityEditStarted_ = false;

    struct AnimationCel {
        std::vector<TilePtr> tiles;
    };
    std::unordered_map<uint32_t, uint64_t> animationTracks_;
    std::unordered_map<uint64_t, AnimationCel> animationCels_;
    int displayedAnimationFrame_ = -1;
    double lastAnimationTime_ = 0.0;
    bool animationReady_ = false;
    void syncAnimationTracks();
    void storeAnimationFrame(int frame);
    void applyAnimationFrame(int frame);
    void pruneAnimationCels();

    std::string internalDir_, externalDir_;
    int screenW_ = 1080, screenH_ = 1920;
    float dp_ = 2.75f;
    float marginTop_ = 0, marginBottom_ = 0;
    float insetL_ = 0, insetR_ = 0;
    Vec2 quad0_[4];
};

}  // namespace ClipPaint
