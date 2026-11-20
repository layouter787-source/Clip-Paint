// Clip Paint - modelo do documento: camadas raster em tiles (copy-on-write),
// selecao, historico por snapshots e operacoes de pixel.
#pragma once

#include "../core/Common.h"

namespace ClipPaint {

constexpr int TILE = 64;
constexpr int kMaxLayers = 32;

// Tile RGBA8 com alfa pre-multiplicado. `version` muda sempre que o conteudo muda.
struct Tile {
    uint8_t px[TILE * TILE * 4];
    uint32_t version = 0;
};
using TilePtr = std::shared_ptr<Tile>;

enum class Blend : uint8_t { Normal, Multiply, Screen, Overlay, Darken, Lighten, Add, Count };
const char* blendName(Blend b);

struct Layer {
    uint32_t id = 0;
    std::string name;
    bool visible = true;
    bool locked = false;
    bool alphaLock = false;
    float opacity = 1.0f;
    Blend blend = Blend::Normal;
    std::vector<TilePtr> tiles;  // tilesX * tilesY; nullptr = transparente
};

using Mask = std::vector<uint8_t>;  // largura*altura, 0..255

struct DocState {
    std::vector<Layer> layers;  // 0 = fundo, ultimo = topo
    int active = 0;
    std::shared_ptr<const Mask> selection;  // nullptr = sem selecao
};

// Parametros de um "dab" (carimbo) do pincel.
struct DabParams {
    float x = 0, y = 0;
    float radius = 5;
    float hardness = 0.8f;
    float aspect = 1.0f;     // >= 1
    float angleDeg = 0.0f;
    float opaque = 1.0f;     // 0..1 (ja inclui opacidade do pincel)
    float r = 0, g = 0, b = 0;  // cor RETA (nao pre-multiplicada), 0..1
    float colorA = 1.0f;     // "alpha_eraser" do libmypaint: 1 = pinta, 0 = apaga, entre = mistura (smudge)
    float lockAlpha = 0.0f;  // 0..1: so recolore pixels existentes
};

enum class SelOp { Replace, Add, Subtract };

class Document {
public:
    Document();

    void create(int w, int h);
    Layer makeLayer(const std::string& name);  // camada vazia com id novo (nao insere)
    int width() const { return w_; }
    int height() const { return h_; }
    int tilesX() const { return tx_; }
    int tilesY() const { return ty_; }

    DocState state;
    Color paper{1.0f, 1.0f, 1.0f, 1.0f};
    bool paperVisible = true;

    // Incrementa quando propriedades/estrutura mudam (obriga recomposicao total).
    uint32_t structRev = 1;
    void touch();

    Layer& activeLayer() { return state.layers[state.active]; }
    const Layer& activeLayer() const { return state.layers[state.active]; }
    int layerCount() const { return (int)state.layers.size(); }
    bool setActive(int i);

    // --- acesso a tiles ---
    const Tile* readTile(const Layer& l, int tx, int ty) const {
        return l.tiles[(size_t)ty * tx_ + tx].get();
    }
    // Aloca / clona (copy-on-write) e incrementa a versao.
    Tile* writableTile(Layer& l, int tx, int ty);

    // --- camadas ---
    int addLayer(const std::string& name = "");
    int duplicateLayer(int i);
    bool removeLayer(int i);
    bool moveLayer(int from, int to);
    bool mergeDown(int i);
    void clearLayer(int i);
    void flipLayerHorizontal(int i);
    void flipLayerVertical(int i);

    // --- pixels ---
    // Desenha um dab na camada; devolve a area afetada.
    RectI drawDab(Layer& l, const DabParams& d, const Mask* sel);
    // Cor media (pre-multiplicada) numa area circular da camada.
    void sampleLayer(const Layer& l, float cx, float cy, float radius, float out[4]) const;
    void getPixel(const Layer& l, int x, int y, uint8_t out[4]) const;  // pre-multiplicado
    // Compoe todo o documento (alfa reto, RGBA8). withPaper: inclui a cor do papel.
    void compositeAll(std::vector<uint8_t>& out, bool withPaper) const;
    Color pickColor(int x, int y, bool allLayers) const;  // alfa reto

    // --- ferramentas ---
    bool floodFill(int layerIdx, int sx, int sy, const Color& c, float tolerance,
                   bool sampleAllLayers, int grow);
    void fillSelection(int layerIdx, const Color& c);
    void clearSelectionPixels(int layerIdx);
    int layerViaCopy();  // copia a selecao para uma nova camada

    // --- selecao ---
    void selectAll();
    void selectNone();
    void invertSelection();
    void featherSelection(int radius);
    void selectPolygon(const std::vector<Vec2>& poly, SelOp op);
    void selectRect(float x0, float y0, float x1, float y1, SelOp op);
    void selectEllipse(float x0, float y0, float x1, float y1, SelOp op);
    bool hasSelection() const { return (bool)state.selection; }
    const Mask* selectionMask() const { return state.selection.get(); }

    // --- transformacao ---
    // Homografia 3x3 (linha a linha) doc->doc. O afim x'=a*x+c*y+e, y'=b*x+d*y+f vira {a,c,e,b,d,f,0,0,1}.
    bool transformLayerH(int idx, const float H[9]);
    bool transformLayer(int idx, const float m[6]);  // afim {a,b,c,d,e,f}
    bool layerBounds(const Layer& l, RectI& out) const;

    // Conteudo flutuante (mover / escalar / perspectiva da selecao ou da camada inteira).
    uint32_t floatingId = 0;
    RectI floatBounds;
    bool hasFloating() const { return floatingId != 0; }
    bool beginFloating(bool copy);                // levanta os pixels selecionados numa camada flutuante
    void commitFloating(const float H[9]);        // aplica a homografia e funde na camada de origem
    void selectByColor(int sx, int sy, float tol, bool allLayers, SelOp op);
    std::vector<uint8_t> floodMask(int layerIdx, int sx, int sy, float tol, bool allLayers, int grow) const;

    // --- historico ---
    void pushUndo();     // chamar ANTES de modificar
    void dropLastUndo(); // descarta o ultimo snapshot (acao cancelada)
    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void restoreLastUndo();  // volta ao ultimo snapshot e o descarta (cancelar traco)

private:
    struct Snapshot {
        DocState st;
        int cost = 0;
    };
    int w_ = 0, h_ = 0, tx_ = 0, ty_ = 0;
    uint32_t nextLayerId_ = 1;
    std::vector<Snapshot> undo_, redo_;
    void trimHistory();
};

uint32_t nextTileVersion();

}  // namespace ClipPaint
