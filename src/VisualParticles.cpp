#include "VisualParticles.h"
#include "GravityField.h"
#include <algorithm>
#include <cmath>
#include <tuple>
#include <glm/gtc/matrix_transform.hpp>
bool ValidParticleSettings(const ParticleEmitterSettings& s){
    auto finite=[](auto v){for(int i=0;i<v.length();++i)if(!std::isfinite(v[i]))return false;return true;};
    return std::isfinite(s.rate)&&s.rate>=0&&s.rate<=100000 &&std::isfinite(s.lifetime)&&s.lifetime>0&&s.lifetime<=3600 &&
        std::isfinite(s.size)&&s.size>=0&&std::isfinite(s.endSize)&&s.endSize>=0&&s.maxParticles>0&&s.maxParticles<=100000 &&s.burst>=0&&s.burst<=s.maxParticles&&
        finite(s.spread)&&finite(s.velocity)&&finite(s.velocityVariation)&&finite(s.acceleration)&&finite(s.color)&&finite(s.endColor)&&
        glm::all(glm::greaterThanEqual(s.spread,glm::vec3(0)))&&glm::all(glm::greaterThanEqual(s.velocityVariation,glm::vec3(0)))&&s.color.a>=0&&s.color.a<=1&&s.endColor.a>=0&&s.endColor.a<=1;
}
bool ParticleSettingsEqual(const ParticleEmitterSettings& a,const ParticleEmitterSettings& b){return
    std::tie(a.enabled,a.loop,a.localSpace,a.useGravity,a.rate,a.lifetime,a.size,a.endSize,a.burst,a.maxParticles,a.spread,a.velocity,a.velocityVariation,a.acceleration,a.color,a.endColor,a.textureAsset,a.seed)==
    std::tie(b.enabled,b.loop,b.localSpace,b.useGravity,b.rate,b.lifetime,b.size,b.endSize,b.burst,b.maxParticles,b.spread,b.velocity,b.velocityVariation,b.acceleration,b.color,b.endColor,b.textureAsset,b.seed);}
void VisualParticlePool::ShiftOrigin(glm::dvec3 delta){
    if(!settings.localSpace)for(auto& p:particles)p.position=glm::vec3(glm::dvec3(p.position)-delta);
    // These are derived presentation data; no age, emission credit or RNG reset.
    billboards.clear();bounds={};
}
float VisualParticlePool::Random(){random=random*1664525u+1013904223u;return static_cast<float>(random>>8)*(1.0f/16777216.0f);}
glm::vec3 VisualParticlePool::Variation(glm::vec3 e){return glm::vec3(2*Random()-1,2*Random()-1,2*Random()-1)*e;}
void VisualParticlePool::Spawn(glm::vec3 p,glm::quat q,glm::vec3 scale){if(particles.size()>=static_cast<size_t>(settings.maxParticles))return;
    VisualParticle v;v.position=Variation(settings.spread);v.velocity=settings.velocity+Variation(settings.velocityVariation);
    if(!settings.localSpace){v.position=p+q*(v.position*scale);v.velocity=q*v.velocity;}particles.push_back(v);}
void VisualParticlePool::Burst(unsigned count,glm::vec3 p,glm::quat q,glm::vec3 scale){if(!settings.enabled)return;for(unsigned i=0;i<count&&particles.size()<static_cast<size_t>(settings.maxParticles);++i)Spawn(p,q,scale);}
void VisualParticlePool::Update(float dt,glm::vec3 p,glm::quat q,glm::vec3 scale,const GravityField& gravity){
    // Age even while disabled/offscreen; disabling stops only new emission and drawing.
    for(auto& v:particles){
        v.age+=dt;
        glm::vec3 a=settings.localSpace?settings.acceleration:q*settings.acceleration;
        if(settings.useGravity){
            auto g=gravity.Sample(settings.localSpace?p+q*(v.position*scale):v.position);
            if(settings.localSpace){
                g=glm::inverse(q)*g;
                // A collapsed transform axis has no world-space motion along it.
                for(int axis=0;axis<3;++axis)g[axis]=scale[axis]!=0?g[axis]/scale[axis]:0;
            }
            a+=g;
        }
        v.velocity+=a*dt;v.position+=v.velocity*dt;
    }
    particles.erase(std::remove_if(particles.begin(),particles.end(),[&](const auto& v){return v.age>=settings.lifetime;}),particles.end());
    if(!settings.enabled)return;
    if(!started){started=true;Burst(settings.burst,p,q,scale);}
    if(settings.loop){credit+=dt*settings.rate;unsigned count=static_cast<unsigned>(credit);credit-=count;Burst(count,p,q,scale);}
}
const std::vector<ParticleBillboard>& VisualParticlePool::Presentation(glm::vec3 p,glm::quat q,glm::vec3 scale){billboards.clear();bounds={};
    for(const auto& v:particles){float t=v.age/settings.lifetime;float size=glm::mix(settings.size,settings.endSize,t);
        if(settings.localSpace)size*=glm::max(glm::max(std::abs(scale.x),std::abs(scale.y)),std::abs(scale.z));
        auto pos=settings.localSpace?p+q*(v.position*scale):v.position;
        billboards.push_back({pos,size,glm::mix(settings.color,settings.endColor,t)});
        // Camera-facing square corner radius, independent of camera orientation.
        glm::vec3 extent(size*0.7071068f);bounds.Include(pos-extent);bounds.Include(pos+extent);}
    return billboards;}
