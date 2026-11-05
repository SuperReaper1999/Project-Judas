#include "ShadowPassCache.h"
#include <cmath>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

namespace JudasRender {
namespace {
bool Finite(const glm::mat4 &matrix) {
    for (unsigned column = 0; column < 4; ++column)
        for (unsigned row = 0; row < 4; ++row)
            if (!std::isfinite(matrix[column][row]))
                return false;
    return true;
}
bool Equal(const glm::mat4 &a, const glm::mat4 &b) {
    // Conservative exact-bit matching: even signed-zero differences miss.
    // There is no epsilon that could merge distinct light/camera charts.
    return std::memcmp(glm::value_ptr(a), glm::value_ptr(b), 16 * sizeof(float)) == 0;
}
bool Valid(const ShadowPassKey &key) {
    return key.slot >= 0 && key.slot < kShadowMapCount && Finite(key.lightViewProjection) &&
           (!key.viewDependentLod || (Finite(key.cameraView) && Finite(key.cameraProjection) &&
                                      key.viewport.z > 0 && key.viewport.w > 0));
}
bool Equal(const ShadowPassKey &a, const ShadowPassKey &b) {
    return a.slot == b.slot && a.source == b.source && a.includePlayerModel == b.includePlayerModel &&
           a.viewDependentLod == b.viewDependentLod &&
           Equal(a.lightViewProjection, b.lightViewProjection) &&
           (!a.viewDependentLod ||
            (Equal(a.cameraView, b.cameraView) && Equal(a.cameraProjection, b.cameraProjection) &&
             a.viewport == b.viewport));
}
} // namespace
bool ShadowPassCache::Matches(const ShadowPassKey &key) const {
    return Valid(key) && slots[unsigned(key.slot)] && Equal(*slots[unsigned(key.slot)], key);
}
bool ShadowPassCache::Remember(const ShadowPassKey &key) {
    if (!Valid(key))
        return false;
    slots[unsigned(key.slot)] = key;
    return true;
}
void ShadowPassCache::Clear() {
    for (auto &slot : slots)
        slot.reset();
}
} // namespace JudasRender
