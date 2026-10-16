#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Visibility.h"
class GravityField;
struct ParticleEmitterSettings {
    bool enabled=true,loop=true,localSpace=false,useGravity=false;
    float rate=20,lifetime=2,size=0.2f,endSize=0.6f;
    int burst=0,maxParticles=256;
    glm::vec3 spread{0.1f},velocity{0,1,0},velocityVariation{0.2f},acceleration{0};
    glm::vec4 color{0.7f,0.7f,0.7f,0.6f},endColor{0.4f,0.4f,0.4f,0};
    std::string textureAsset;
    std::uint32_t seed=1;
};
bool ValidParticleSettings(const ParticleEmitterSettings& s);
bool ParticleSettingsEqual(const ParticleEmitterSettings& a,const ParticleEmitterSettings& b);
struct VisualParticle {glm::vec3 position{0},velocity{0};float age=0;};
struct ParticleBillboard {glm::vec3 position;float size;glm::vec4 color;};
class VisualParticlePool {
public:
    explicit VisualParticlePool(const ParticleEmitterSettings& s={}):settings(s),random(s.seed){particles.reserve(s.maxParticles);billboards.reserve(s.maxParticles);}
    void Update(float dt,glm::vec3 position,glm::quat rotation,glm::vec3 scale,const GravityField& gravity);
    void Burst(unsigned count,glm::vec3 position,glm::quat rotation,glm::vec3 scale);
    void ShiftOrigin(glm::dvec3 delta);
    const std::vector<ParticleBillboard>& Presentation(glm::vec3 position,glm::quat rotation,glm::vec3 scale);
    const std::vector<VisualParticle>& Particles()const{return particles;}
    const VisualBounds& Bounds()const{return bounds;}
    ParticleEmitterSettings settings;
private:
    float Random();
    glm::vec3 Variation(glm::vec3 extent);
    void Spawn(glm::vec3 position,glm::quat rotation,glm::vec3 scale);
    std::uint32_t random;double credit=0;bool started=false;
    std::vector<VisualParticle> particles;
    std::vector<ParticleBillboard> billboards;
    VisualBounds bounds;
};
