#include "BrushLibrary.h"

#include "../third_party/animbrush/include/animbrush/animbrush.hpp"

namespace ClipPaint {

BrushLibrary& BrushLibrary::get() {
    static BrushLibrary lib;
    return lib;
}

static std::vector<std::string> splitBar(const std::string& s) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        const size_t n = s.find('|', pos);
        out.push_back(s.substr(pos, n == std::string::npos ? std::string::npos : n - pos));
        if (n == std::string::npos) break;
        pos = n + 1;
    }
    return out;
}

void BrushLibrary::init(const Reader& reader) {
    reader_ = reader;
    entries_.clear();
    groups_[0].clear();
    groups_[1].clear();

    // --- MyPaint Brushes ---
    std::string index;
    if (reader_ && reader_("mypaint-brushes/index.txt", index)) {
        size_t pos = 0;
        std::string group;
        while (pos < index.size()) {
            size_t nl = index.find('\n', pos);
            if (nl == std::string::npos) nl = index.size();
            const std::string line = index.substr(pos, nl - pos);
            pos = nl + 1;
            if (line.size() < 3) continue;
            const std::vector<std::string> f = splitBar(line.substr(2));
            if (line[0] == 'G') {
                group = f[0];
                groups_[0].push_back(group);
            } else if (line[0] == 'B' && f.size() >= 5) {
                BrushEntry e;
                e.source = BrushSource::MyPaint;
                e.group = f[0];
                e.name = f[1];
                e.id = "mypaint-brushes/" + f[2];
                e.eraser = f[3].find('e') != std::string::npos;
                e.smudge = f[3].find('s') != std::string::npos;
                e.defaultDiameter = clampf((float)std::atof(f[4].c_str()), 1.0f, 400.0f);
                entries_.push_back(e);
            }
        }
    }

    // --- AnimBrush ---
    ab::BrushLibrary& alib = ab::BrushLibrary::instance();
    for (const std::string& cat : alib.categories()) groups_[1].push_back(cat);
    for (const ab::BrushEntry& be : alib.entries()) {
        BrushEntry e;
        e.source = BrushSource::Anim;
        e.id = be.info.id;
        e.name = be.info.name;
        e.group = be.info.category;
        e.description = be.info.description;
        e.eraser = be.info.isEraser;
        e.additive = be.info.additive;
        e.animated = be.info.animated;
        e.defaultDiameter = clampf(be.info.defaultSize * 2.0f, 2.0f, 600.0f);
        e.demo = be.info.demo;
        e.demoHold = be.info.demoHold;
        e.suggested = Color(be.info.suggestedColor.r, be.info.suggestedColor.g, be.info.suggestedColor.b, 1.0f);
        entries_.push_back(e);
    }
}

std::vector<int> BrushLibrary::inGroup(BrushSource s, const std::string& group, bool eraserOnly) const {
    std::vector<int> r;
    for (size_t i = 0; i < entries_.size(); ++i) {
        const BrushEntry& e = entries_[i];
        if (e.source != s || e.group != group) continue;
        if (eraserOnly && !e.eraser) continue;
        r.push_back((int)i);
    }
    return r;
}

int BrushLibrary::findById(const std::string& id) const {
    for (size_t i = 0; i < entries_.size(); ++i) if (entries_[i].id == id) return (int)i;
    return -1;
}

int BrushLibrary::findDefault(bool eraser) const {
    static const char* paint[] = {"mypaint-brushes/classic_pencil.myb", "mypaint-brushes/classic_brush.myb"};
    static const char* erase[] = {"mypaint-brushes/classic_eraser.myb", "mypaint-brushes/classic_ink_eraser.myb"};
    for (const char* id : eraser ? erase : paint) {
        const int i = findById(id);
        if (i >= 0) return i;
    }
    for (size_t i = 0; i < entries_.size(); ++i) if (entries_[i].eraser == eraser) return (int)i;
    return entries_.empty() ? -1 : 0;
}

bool BrushLibrary::readMyb(const BrushEntry& e, std::string& json) const {
    return reader_ && reader_(e.id, json);
}

}  // namespace ClipPaint
