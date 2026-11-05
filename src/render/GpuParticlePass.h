#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
namespace JudasParticles {class GpuParticles;}
namespace JudasRender {
struct GpuParticlePassStats {std::uint64_t draws=0;};
// Presentation only. Simulation remains once per fixed tick in the world-owned
// emitter; each camera prepares independent GPU sort/cull/indirect records.
class GpuParticlePass {
public:
    GpuParticlePass();
    ~GpuParticlePass();
    GpuParticlePass(const GpuParticlePass&)=delete;
    GpuParticlePass& operator=(const GpuParticlePass&)=delete;
    bool Draw(JudasParticles::GpuParticles&,const glm::mat4& view,const glm::mat4& projection,
              unsigned nativeTexture,bool imageLinear,bool sceneLinear,std::string& error);
    bool Shutdown(std::string* error=nullptr);
    GpuParticlePassStats Stats()const;
private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};
}
