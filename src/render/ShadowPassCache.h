#pragma once

#include "../Light.h"
#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace JudasRender {
struct ShadowPassKey {
    int slot = -1;
    std::uint64_t resourceRevision = 0;
    std::string content;
    std::uint64_t source = 0;
    glm::mat4 lightViewProjection{1};
    bool viewDependentLod = false;
    bool compactContent = false;
    glm::mat4 cameraView{1}, cameraProjection{1};
    glm::ivec4 viewport{0};
};

// Context-owned exact observations of bounded physical slots. Callers supply
// complete relevant caster content and a resource revision, then still validate
// the owning depth allocation. A dirty chart is never reused by age alone.
class ShadowPassCache {
  public:
    static constexpr std::size_t MaxContentBytes=256*1024;
    enum class Match { Hit, Empty, Invalid, TooLarge, Source, Chart, View, Resources, Casters };
    Match Inspect(const ShadowPassKey &) const;
    bool Matches(const ShadowPassKey &key) const { return Inspect(key)==Match::Hit; }
    // Publish only after a real completed shadow pass, never after a cache hit.
    bool Remember(const ShadowPassKey &);
    // Exact interning for large caster sets. Tokens never alias while a chart
    // can refer to them. Oversized snapshots fall back to redraw.
    // Reports when at least one requested directional chart can recur.
    bool ObserveDirectionalCharts(const std::array<glm::mat4, kDirectionalShadowCascades>&, unsigned count);
    bool ObserveMovingCasters(unsigned slot, const std::string& content);
    void BeginCasterSnapshot();
    std::uint64_t CasterToken(const std::string &);
    void Clear();
    void Forget(unsigned slot);
    void AssignLocalSlots(std::vector<DynamicLight>& selected);

  private:
    std::array<std::optional<ShadowPassKey>, kShadowMapCount> slots;
    std::array<std::uint64_t,8> sourceIds{},lastUse{};
    std::uint64_t useClock=0;
    struct CasterEntry { std::uint64_t token, lastSeen; };
    std::unordered_map<std::string, CasterEntry> casterTokens;
    std::uint64_t casterObservation = 0;
    std::array<glm::mat4, kDirectionalShadowCascades> lastDirectionalCharts{};
    unsigned lastDirectionalCount=0;
    std::array<std::string, kDirectionalShadowCascades> lastMovingCasters;
    std::uint64_t nextCasterToken = 1;
    std::size_t casterTokenBytes = 0;
    bool reclaimedCasters = false;
};
} // namespace JudasRender
