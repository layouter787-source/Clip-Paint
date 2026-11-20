#include "nature_util.hpp"

namespace ab {
void registerNatureBrushes(BrushLibrary& lib) {
  nat::registerTrees(lib);
  nat::registerPlants(lib);
  nat::registerTerrainWater(lib);
  nat::registerClimateFur(lib);
}
}  // namespace ab
