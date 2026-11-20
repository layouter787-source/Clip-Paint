// AnimBrush - brush registry (the "library" the host app browses) + stamp texture bank.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "brush.hpp"

namespace ab {

// ---- Stamp / texture bank ---------------------------------------------------------------
// Holds 8-bit coverage masks by name. Loaded from the single-file pack produced by
// tools/pack_stamps.py ("stamps.abpack"), or filled by the host from decoded PNGs.
// Names starting with '@' are procedural built-ins (always available): @paper @canvas @rough @noise @fibers @dots @linen
class StampBank {
 public:
  static StampBank& instance();
  bool loadPack(const std::string& path);                       // returns false on failure
  bool loadPackFromMemory(const uint8_t* data, size_t size);
  void add(Mask m);                                              // host-supplied (e.g. from Android Bitmap)
  const Mask* get(const std::string& name);                      // nullptr if unknown
  std::vector<std::string> names() const;
  size_t size() const { return masks_.size(); }
  void clear() { masks_.clear(); }

 private:
  StampBank();
  void makeBuiltins();
  std::map<std::string, std::unique_ptr<Mask>> masks_;
};

// ---- Registry ---------------------------------------------------------------------------
using BrushFactory = std::function<std::unique_ptr<Brush>()>;

struct BrushEntry {
  BrushInfo info;
  BrushFactory make;
};

class BrushLibrary {
 public:
  static BrushLibrary& instance();   // lazily calls registerAllBrushes() once

  void add(BrushInfo info, BrushFactory f);
  // Convenience: register a DabBrush preset.
  void addDab(BrushInfo info, struct DabBrushSpec spec);

  const std::vector<BrushEntry>& entries() const { return entries_; }
  std::vector<const BrushEntry*> byCategory(const std::string& cat) const;
  std::vector<std::string> categories() const;
  const BrushEntry* find(const std::string& id) const;
  std::unique_ptr<Brush> create(const std::string& id) const;
  size_t size() const { return entries_.size(); }
  // Writes a JSON catalogue (id, name, category, description, params...) for the host UI.
  std::string catalogJson() const;

 private:
  BrushLibrary() = default;
  std::vector<BrushEntry> entries_;
  std::map<std::string, size_t> index_;
};

// Each module registers its brushes here. (implemented in src/brushes/*.cpp)
void registerAllBrushes(BrushLibrary& lib);
void registerMediaBrushes(BrushLibrary& lib);     // pencil, charcoal, chalk, ink, pens, markers, airbrush, watercolor, oil...
void registerMediaOilZip(BrushLibrary& lib);      // oil texture tips from the zip
void registerNatureBrushes(BrushLibrary& lib);    // tree, grass, vines, fur, hair, clouds, water, rain, snow, terrain...
void registerLightBrushes(BrushLibrary& lib);     // light, lightning, fire, smoke, sparks, neon, stars, aurora...
void registerStampBrushes(BrushLibrary& lib);     // calls the two below
void registerStampNatureBrushes(BrushLibrary& lib); // zip stamps: flowers, leaves, grass, snow, insects, drops, footprints, clouds, fur...
void registerStampShapeBrushes(BrushLibrary& lib);  // zip stamps: geometric/symbolic + flat/net/circle/points/sputtering/light
void registerEraserBrushes(BrushLibrary& lib);    // eraser library
void registerDecorFxBrushes(BrushLibrary& lib);   // chain, rope, lace, manga, speed lines, screentone, animation FX...

}  // namespace ab
