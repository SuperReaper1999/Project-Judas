#pragma once

#include "../Light.h"
#include <array>
#include <cstdint>
#include <optional>

namespace JudasRender {
struct ShadowPassKey {
    int slot = -1;
    std::uint64_t source = 0;
    glm::mat4 lightViewProjection{1};
    bool viewDependentLod = false;
    glm::mat4 cameraView{1}, cameraProjection{1};
    glm::ivec4 viewport{0};
};

// A RenderWorldFrame-local observation of the latest content in each physical
// shadow slot. It owns no depth textures and never spans simulation/render
// samples. Callers must keep caster geometry/material inputs immutable while
// consulting it, and must still ask Renderer to validate the actual depth slot.
class ShadowPassCache {
  public:
    bool Matches(const ShadowPassKey &) const;
    // Publish only after a real completed shadow pass, never after a cache hit.
    bool Remember(const ShadowPassKey &);
    void Clear();

  private:
    std::array<std::optional<ShadowPassKey>, kShadowMapCount> slots;
};
} // namespace JudasRender
