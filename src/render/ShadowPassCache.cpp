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
void ShadowPassCache::AssignLocalSlots(std::vector<DynamicLight>& selected){
    std::array<bool,8> used{};
    for(auto& light:selected)if(light.shadowMapIndex>=0){
        int found=-1;
        for(unsigned n=0;n<sourceIds.size();++n)if(light.identity&&sourceIds[n]==light.identity&&!used[n]){found=int(n);break;}
        if(found>=0){used[unsigned(found)]=true;lastUse[unsigned(found)]=++useClock;light.shadowMapIndex=kLocalShadowBase+found*6;}
        else light.shadowMapIndex=-2;
    }
    for(auto& light:selected)if(light.shadowMapIndex==-2){
        unsigned best=unsigned(sourceIds.size());
        for(unsigned n=0;n<sourceIds.size();++n)if(!used[n]&&(!sourceIds[n]||best==sourceIds.size()||lastUse[n]<lastUse[best])){best=n;if(!sourceIds[n])break;}
        if(best==sourceIds.size()){light.shadowMapIndex=-1;continue;}
        used[best]=true;sourceIds[best]=light.identity;lastUse[best]=++useClock;light.shadowMapIndex=kLocalShadowBase+int(best)*6;
    }
}
void ShadowPassCache::Forget(unsigned slot){if(slot<slots.size())slots[slot].reset();}
void ShadowPassCache::Clear() {
    sourceIds.fill(0);lastUse.fill(0);useClock=0;
    for (auto &slot : slots)
        slot.reset();
}
} // namespace JudasRender
