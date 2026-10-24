#pragma once
#include "Scene.h"
#include <string_view>

// Authored fluid components survive a disabled project policy. Admission and
// dependency checks use this copy; it must never replace the saved definition.
inline void RemoveDormantFluidComponents(SceneObject& object, bool advanced) {
    if (advanced) return;
    object.fluidVolume.reset();
    object.liquidBasin.reset();
    object.liquidContainer.reset();
    object.liquidConnection.reset();
}
inline bool IsDormantFluidProperty(std::string_view key, bool advanced) {
    return !advanced && (key.substr(0, 7) == "liquid." ||
                         key.substr(0, 6) == "fluid." || key == "fluid-volume");
}
