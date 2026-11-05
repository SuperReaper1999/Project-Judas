#pragma once

#include "../VisualParticles.h"
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace JudasRender::Shaders {
enum class Profile;
}

namespace JudasRender {
struct ParticleDepth {
    float depth = 0;
    std::size_t index = 0;
};
// CPU preparation shared by the real pass and its independent order controls.
// Each depth uses the legacy view*vec4 expression exactly once. Failure clears
// output rather than leaving a partial permutation available for submission.
bool PrepareParticleOrder(const glm::mat4 &view, const std::vector<ParticleBillboard> &,
                          std::vector<ParticleDepth> &output, std::string &error);

struct ParticlePassStats {
    std::uint64_t draws = 0, triangles = 0, particles = 0, uploadedBytes = 0, uploadCalls = 0;
    std::size_t arenaBytes = 0;
    std::uint64_t waitNs = 0;
};

// Optional, context/thread-owned billboard presentation. Simulation and input
// data stay CPU-owned. The borrowed native texture is resolved by Renderer.
// Healthy uploads are page-chunked without limiting or dropping particle count.
// Stats count actual submitted chunks if a later allocation/upload fails.
// Shutdown must occur in the owner context; destruction never calls GL.
class ParticlePass {
  public:
    ParticlePass();
    ~ParticlePass();
    ParticlePass(const ParticlePass &) = delete;
    ParticlePass &operator=(const ParticlePass &) = delete;
    bool Draw(const glm::mat4 &view, const glm::mat4 &projection,
              const std::vector<ParticleBillboard> &particles, unsigned nativeTexture, bool imageLinear,
              bool sceneLinear, bool persistent, Shaders::Profile, std::string &error);
    bool Shutdown(std::string *error = nullptr);
    ParticlePassStats Stats() const;
    static constexpr std::size_t InstanceBytes = 32;

  private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};
} // namespace JudasRender
