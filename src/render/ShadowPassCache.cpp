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
    return a.resourceRevision == b.resourceRevision && a.content == b.content &&
           a.slot == b.slot && a.source == b.source &&
           a.viewDependentLod == b.viewDependentLod &&
           Equal(a.lightViewProjection, b.lightViewProjection) &&
           (!a.viewDependentLod ||
            (Equal(a.cameraView, b.cameraView) && Equal(a.cameraProjection, b.cameraProjection) &&
             a.viewport == b.viewport));
}
} // namespace
bool ShadowPassCache::Matches(const ShadowPassKey &key) const {
    return key.content.size()<=256*1024 && Valid(key) && slots[unsigned(key.slot)] && Equal(*slots[unsigned(key.slot)], key);
}
bool ShadowPassCache::Remember(const ShadowPassKey &key) {
    if (!Valid(key))
        return false;
    if(key.content.size()>256*1024){Forget(unsigned(key.slot));return false;}
    slots[unsigned(key.slot)] = key;
    return true;
}
int ShadowPassCache::LocalSlotForSource(std::uint64_t source)const{
    if(!source)return -1;
    for(unsigned slot=kLocalShadowBase;slot<slots.size();slot+=6)
        if(slots[slot]&&slots[slot]->source==source)return int(slot);
    return -1;
}
void ShadowPassCache::Forget(unsigned slot){if(slot<slots.size())slots[slot].reset();}
void ShadowPassCache::Clear() {
    for (auto &slot : slots)
        slot.reset();
}
} // namespace JudasRender
