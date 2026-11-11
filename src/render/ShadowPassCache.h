#pragma once

#include "../Light.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace JudasRender {
struct ShadowPassKey {
    int slot = -1;
    std::uint64_t resourceRevision = 0;
    std::string content;
    std::uint64_t source = 0;
    glm::mat4 lightViewProjection{1};
    bool viewDependentLod = false;
    glm::mat4 cameraView{1}, cameraProjection{1};
    glm::ivec4 viewport{0};
};

// Context-owned exact observations of bounded physical slots. Callers supply
// complete relevant caster content and a resource revision, then still validate
// the owning depth allocation. A dirty chart is never reused by age alone.
class ShadowPassCache {
  public:
    bool Matches(const ShadowPassKey &) const;
    // Publish only after a real completed shadow pass, never after a cache hit.
    bool Remember(const ShadowPassKey &);
    void Clear();
    void Forget(unsigned slot);
    void AssignLocalSlots(std::vector<DynamicLight>& selected);

  private:
    std::array<std::optional<ShadowPassKey>, kShadowMapCount> slots;
    std::array<std::uint64_t,8> sourceIds{},lastUse{};
    std::uint64_t useClock=0;
};
} // namespace JudasRender
