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
} // namespace
ShadowPassCache::Match ShadowPassCache::Inspect(const ShadowPassKey& key) const {
    if(key.content.size()>MaxContentBytes)return Match::TooLarge;
    if(!Valid(key))return Match::Invalid;
    const auto& existing=slots[unsigned(key.slot)];
    if(!existing)return Match::Empty;
    const auto& before=*existing;
    if(before.source!=key.source)return Match::Source;
    if(!Equal(before.lightViewProjection,key.lightViewProjection))return Match::Chart;
    if(before.viewDependentLod!=key.viewDependentLod||
       (key.viewDependentLod&&(!Equal(before.cameraView,key.cameraView)||
                              !Equal(before.cameraProjection,key.cameraProjection)||before.viewport!=key.viewport)))return Match::View;
    if(before.resourceRevision!=key.resourceRevision)return Match::Resources;
    if(before.compactContent!=key.compactContent||before.content!=key.content)return Match::Casters;
    return Match::Hit;
}
bool ShadowPassCache::Remember(const ShadowPassKey &key) {
    if (!Valid(key))
        return false;
    if(key.content.size()>MaxContentBytes){Forget(unsigned(key.slot));return false;}
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
bool ShadowPassCache::ObserveDirectionalCharts(
    const std::array<glm::mat4, kDirectionalShadowCascades>& charts, unsigned count) {
    bool possible=lastDirectionalCount!=count;
    for(unsigned i=0;i<count&&!possible;++i)possible=Equal(charts[i],lastDirectionalCharts[i]);
    lastDirectionalCharts=charts;
    lastDirectionalCount=count;
    return possible;
}
bool ShadowPassCache::ObserveMovingCasters(unsigned slot, const std::string& content) {
    auto& last=lastMovingCasters[slot];
    if(last==content)return true;
    const bool first=last.empty();
    last=content;
    return first;
}
void ShadowPassCache::BeginCasterSnapshot() {
    ++casterObservation;
    reclaimedCasters = false;
}
std::uint64_t ShadowPassCache::CasterToken(const std::string &content) {
    if (const auto found = casterTokens.find(content); found != casterTokens.end()) {
        found->second.lastSeen = casterObservation;
        return found->second.token;
    }
    constexpr std::size_t maxBytes = 8 * 1024 * 1024, maxEntries = 32 * 1024;
    const auto full = [&] {
        return content.size() > maxBytes - casterTokenBytes || casterTokens.size() >= maxEntries;
    };
    if (full() && !reclaimedCasters) {
        // Old projections can still own depth and reference retired tokens.
        // Never recycle token numbers or remove either live observation.
        for (auto item = casterTokens.begin(); item != casterTokens.end();) {
            if (casterObservation - item->second.lastSeen > 1) {
                casterTokenBytes -= item->first.size();
                item = casterTokens.erase(item);
            } else ++item;
        }
        reclaimedCasters = true;
    }
    if (full())
        return 0;
    const auto inserted = casterTokens.emplace(content, CasterEntry{nextCasterToken++, casterObservation});
    casterTokenBytes += inserted.first->first.size();
    return inserted.first->second.token;
}
void ShadowPassCache::Clear() {
    casterTokens.clear();
    nextCasterToken = 1;
    casterTokenBytes = 0;
    casterObservation = 0;
    lastDirectionalCount = 0;
    for(auto& moving:lastMovingCasters)moving.clear();
    reclaimedCasters = false;
    sourceIds.fill(0);lastUse.fill(0);useClock=0;
    for (auto &slot : slots)
        slot.reset();
}
} // namespace JudasRender
