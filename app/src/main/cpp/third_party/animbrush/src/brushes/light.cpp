#include "light_util.hpp"

namespace ab {
void registerLightBrushes(BrushLibrary& lib) {
  lu::registerLuz(lib);
  lu::registerRaios(lib);
  lu::registerFogo(lib);
  lu::registerFumaca(lib);
  lu::registerCeu(lib);
}
}  // namespace ab
