// Clip Paint - catalogo de pincéis: MyPaint Brushes 2.0.2 (via libmypaint 1.6.1) e AnimBrush.
#pragma once

#include <functional>
#include <map>

#include "../layers/Document.h"

namespace ClipPaint {

enum class BrushSource { MyPaint = 0, Anim = 1 };

struct BrushEntry {
    BrushSource source = BrushSource::MyPaint;
    std::string id;        // MyPaint: caminho do .myb nos assets; Anim: id (ex.: "ink.gpen")
    std::string name;
    std::string group;     // grupo (MyPaint) ou categoria (Anim)
    std::string description;
    bool eraser = false;
    bool smudge = false;
    bool additive = false;     // Anim: pinta luz (Blend::Add)
    bool animated = false;
    float defaultDiameter = 10.0f;
    Color suggested{0.1f, 0.1f, 0.12f, 1.0f};
    std::string demo = "wave";   // pre-visualizacao: wave, up, line, dot, circle, arc, zigzag
    float demoHold = 0.0f;
    float minDiameter = 1.0f, maxDiameter = 600.0f;
};

// Ajustes do usuario para a ferramenta (pincel ou borracha).
struct BrushSetup {
    int entry = 0;               // indice em BrushLibrary::entries()
    float diameter = 10.0f;
    float opacity = 1.0f;
    float hardness = -1.0f;      // <0 = do pincel
    float smoothing = 0.0f;
    std::map<std::string, float> params;  // sliders proprios do AnimBrush
};

class BrushLibrary {
public:
    using Reader = std::function<bool(const std::string& assetPath, std::string& out)>;
    static BrushLibrary& get();

    // Le mypaint-brushes/index.txt e registra os pincéis do AnimBrush.
    void init(const Reader& reader);
    bool loaded() const { return !entries_.empty(); }
    const std::vector<BrushEntry>& entries() const { return entries_; }
    // Grupos por origem, na ordem de exibicao.
    const std::vector<std::string>& groups(BrushSource s) const { return groups_[(int)s]; }
    std::vector<int> inGroup(BrushSource s, const std::string& group, bool eraserOnly) const;
    int findDefault(bool eraser) const;
    int findById(const std::string& id) const;
    bool readMyb(const BrushEntry& e, std::string& json) const;

private:
    std::vector<BrushEntry> entries_;
    std::vector<std::string> groups_[2];
    Reader reader_;
};

}  // namespace ClipPaint
