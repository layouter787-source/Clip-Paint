#include "Document.h"
#include "BlendMath.h"

#include <chrono>

namespace ClipPaint {

static uint32_t g_tileVersion = 1;
uint32_t nextTileVersion() { return ++g_tileVersion; }

double nowSeconds() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

const char* blendName(Blend b) {
    switch (b) {
        case Blend::Normal: return "Normal";
        case Blend::Multiply: return "Multiplicar";
        case Blend::Screen: return "Tela";
        case Blend::Overlay: return "Sobrepor";
        case Blend::Darken: return "Escurecer";
        case Blend::Lighten: return "Clarear";
        case Blend::Add: return "Adicionar";
        default: return "?";
    }
}

static uint32_t g_structRev = 1;
void Document::touch() { structRev = ++g_structRev; }

Document::Document() { create(2048, 1536); }

Layer Document::makeLayer(const std::string& name) {
    Layer l;
    l.id = nextLayerId_++;
    l.name = name.empty() ? ("Camada " + std::to_string(l.id)) : name;
    l.tiles.assign((size_t)tx_ * ty_, nullptr);
    return l;
}

void Document::create(int w, int h) {
    w_ = clampi(w, 16, 8192);
    h_ = clampi(h, 16, 8192);
    tx_ = (w_ + TILE - 1) / TILE;
    ty_ = (h_ + TILE - 1) / TILE;
    undo_.clear();
    redo_.clear();
    state = DocState();
    nextLayerId_ = 1;
    state.layers.push_back(makeLayer(""));
    state.active = 0;
    paper = Color(1, 1, 1, 1);
    paperVisible = true;
    touch();
}

bool Document::setActive(int i) {
    if (i < 0 || i >= layerCount()) return false;
    state.active = i;
    return true;
}

Tile* Document::writableTile(Layer& l, int tx, int ty) {
    TilePtr& p = l.tiles[(size_t)ty * tx_ + tx];
    if (!p) {
        p = std::make_shared<Tile>();
        std::memset(p->px, 0, sizeof(p->px));
    } else if (p.use_count() > 1) {
        p = std::make_shared<Tile>(*p);
        if (!undo_.empty()) undo_.back().cost++;
    }
    p->version = nextTileVersion();
    return p.get();
}

// ---------------------------------------------------------------- camadas

int Document::addLayer(const std::string& name) {
    if (layerCount() >= kMaxLayers) return -1;
    const int idx = state.active + 1;
    state.layers.insert(state.layers.begin() + idx, makeLayer(name));
    state.active = idx;
    touch();
    return idx;
}

int Document::duplicateLayer(int i) {
    if (i < 0 || i >= layerCount() || layerCount() >= kMaxLayers) return -1;
    Layer copy = state.layers[i];
    copy.id = nextLayerId_++;
    copy.name = copy.name + " copia";
    state.layers.insert(state.layers.begin() + i + 1, std::move(copy));
    state.active = i + 1;
    touch();
    return i + 1;
}

bool Document::removeLayer(int i) {
    if (i < 0 || i >= layerCount()) return false;
    if (layerCount() == 1) {
        clearLayer(0);
        return true;
    }
    state.layers.erase(state.layers.begin() + i);
    if (state.active > i) --state.active;
    state.active = clampi(state.active, 0, layerCount() - 1);
    touch();
    return true;
}

bool Document::moveLayer(int from, int to) {
    if (from < 0 || from >= layerCount() || to < 0 || to >= layerCount() || from == to) return false;
    Layer l = std::move(state.layers[from]);
    state.layers.erase(state.layers.begin() + from);
    state.layers.insert(state.layers.begin() + to, std::move(l));
    state.active = to;
    touch();
    return true;
}

void Document::clearLayer(int i) {
    if (i < 0 || i >= layerCount()) return;
    Layer& l = state.layers[i];
    if (l.locked) return;
    std::fill(l.tiles.begin(), l.tiles.end(), nullptr);
    touch();
}

bool Document::mergeDown(int i) {
    if (i <= 0 || i >= layerCount()) return false;
    Layer& top = state.layers[i];
    Layer& bot = state.layers[i - 1];
    if (bot.locked) return false;
    if (top.visible) {
        for (int ty = 0; ty < ty_; ++ty) {
            for (int tx = 0; tx < tx_; ++tx) {
                const Tile* st = readTile(top, tx, ty);
                if (!st) continue;
                Tile* dt = writableTile(bot, tx, ty);
                for (int k = 0; k < TILE * TILE; ++k) {
                    float cb[4], cs[4];
                    for (int c = 0; c < 4; ++c) {
                        cb[c] = dt->px[k * 4 + c] / 255.0f;
                        cs[c] = st->px[k * 4 + c] / 255.0f * top.opacity;
                    }
                    blendPremult(cb, cs, top.blend);
                    for (int c = 0; c < 4; ++c) {
                        dt->px[k * 4 + c] = (uint8_t)clampi((int)(cb[c] * 255.0f + 0.5f), 0, 255);
                    }
                }
            }
        }
    }
    state.layers.erase(state.layers.begin() + i);
    state.active = i - 1;
    touch();
    return true;
}

void Document::flipLayerHorizontal(int i) {
    const float m[6] = {-1, 0, 0, 1, (float)w_, 0};
    transformLayer(i, m);
}

void Document::flipLayerVertical(int i) {
    const float m[6] = {1, 0, 0, -1, 0, (float)h_};
    transformLayer(i, m);
}

// --------------------------------------------------------------- historico

void Document::trimHistory() {
    int total = 0;
    for (const auto& s : undo_) total += s.cost;
    while (!undo_.empty() && (undo_.size() > 100 || total > 8000)) {
        total -= undo_.front().cost;
        undo_.erase(undo_.begin());
    }
}

void Document::pushUndo() {
    Snapshot s;
    s.st = state;
    undo_.push_back(std::move(s));
    redo_.clear();
    trimHistory();
}

void Document::dropLastUndo() {
    if (!undo_.empty()) undo_.pop_back();
}

void Document::restoreLastUndo() {
    if (undo_.empty()) return;
    state = undo_.back().st;
    undo_.pop_back();
    touch();
}

bool Document::undo() {
    if (undo_.empty()) return false;
    Snapshot cur;
    cur.st = state;
    cur.cost = undo_.back().cost;
    redo_.push_back(std::move(cur));
    state = undo_.back().st;
    undo_.pop_back();
    state.active = clampi(state.active, 0, layerCount() - 1);
    touch();
    return true;
}

bool Document::redo() {
    if (redo_.empty()) return false;
    Snapshot cur;
    cur.st = state;
    cur.cost = redo_.back().cost;
    undo_.push_back(std::move(cur));
    state = redo_.back().st;
    redo_.pop_back();
    state.active = clampi(state.active, 0, layerCount() - 1);
    touch();
    return true;
}

}  // namespace ClipPaint
