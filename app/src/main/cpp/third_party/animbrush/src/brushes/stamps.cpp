#include "../../include/animbrush/library.hpp"
namespace ab {
void registerStampBrushes(BrushLibrary& lib) {
  registerStampNatureBrushes(lib);
  registerStampShapeBrushes(lib);
}
}
