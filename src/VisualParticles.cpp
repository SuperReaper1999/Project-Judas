#include "VisualParticles.h"
#include "GravityField.h"
#include "particles/CpuParticles.h"
#include "particles/ParticleRandom.h"
#include <algorithm>
#include <cmath>
#include <tuple>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
namespace {
bool Extended(const ParticleEmitterSettings& s){return s.backend!=ParticleBackend::Cpu||s.collision||s.useWind||s.impacts!=ParticleImpactDelivery::Off;}
}
VisualParticlePool::VisualParticlePool(const ParticleEmitterSettings& s):settings(s),random(s.seed){
    if(!ValidParticleSettings(s))throw std::invalid_argument("invalid particle emitter settings");
    particles.reserve(s.maxParticles);billboards.reserve(s.maxParticles);
}
VisualParticlePool::~VisualParticlePool()=default;
VisualParticlePool::VisualParticlePool(VisualParticlePool&&) noexcept=default;
VisualParticlePool& VisualParticlePool::operator=(VisualParticlePool&&) noexcept=default;
VisualParticlePool::VisualParticlePool(const VisualParticlePool& b):settings(b.settings),random(b.random),credit(b.credit),started(b.started),nextBirth(b.nextBirth),
    runtime(b.runtime?std::make_unique<JudasParticles::CpuParticles>(*b.runtime):nullptr),particles(b.particles),billboards(b.billboards),bounds(b.bounds){}
VisualParticlePool& VisualParticlePool::operator=(const VisualParticlePool& b){if(this!=&b){auto copy=b;*this=std::move(copy);}return *this;}
void VisualParticlePool::SetSettings(const ParticleEmitterSettings& next){
    if(!ValidParticleSettings(next))throw std::invalid_argument("invalid particle emitter settings");
    if(PendingStep()||(runtime&&settings.impacts==ParticleImpactDelivery::Reliable&&!runtime->Impacts().Pending().empty()))
        throw std::logic_error("consume reliable particle impacts before changing settings");
    const bool spaceChanged=settings.localSpace!=next.localSpace;
    std::vector<std::size_t> retained;
    retained.reserve(std::min(particles.size(),std::size_t(next.maxParticles)));
    if(!spaceChanged)for(std::size_t i=0;i<particles.size()&&retained.size()<std::size_t(next.maxParticles);++i)
        if(particles[i].age<next.lifetime)retained.push_back(i);
    if(runtime)runtime->RemapParticles(retained,spaceChanged);
    for(std::size_t i=0;i<retained.size();++i)particles[i]=particles[retained[i]];
    particles.resize(retained.size());
    if(settings.seed!=next.seed)random=next.seed;
    if(settings.loop!=next.loop)credit=0;
    settings=next;
    billboards.clear();
    bounds={};
}
void VisualParticlePool::ShiftOrigin(glm::dvec3 delta){
    if(!settings.localSpace)for(auto& p:particles)p.position=glm::vec3(glm::dvec3(p.position)-delta);
    // These are derived presentation data; no age, emission credit or RNG reset.
    billboards.clear();bounds={};
    if(runtime)runtime->ShiftOrigin(delta);
}
float VisualParticlePool::Random(){random=random*1664525u+1013904223u;return static_cast<float>(random>>8)*(1.0f/16777216.0f);}
glm::vec3 VisualParticlePool::Variation(glm::vec3 e){return glm::vec3(2*Random()-1,2*Random()-1,2*Random()-1)*e;}
void VisualParticlePool::Spawn(glm::vec3 p,glm::quat q,glm::vec3 scale){if(particles.size()>=static_cast<size_t>(settings.maxParticles))return;
    VisualParticle v;
    if(Extended(settings)){
        if(nextBirth>UINT32_MAX)throw std::overflow_error("particle birth identity exhausted; restart emitter");
        const auto sample=JudasParticles::SampleParticleBirth(settings,std::uint32_t(nextBirth++));v.position=sample.position;v.velocity=sample.velocity;
    }else{v.position=Variation(settings.spread);v.velocity=settings.velocity+Variation(settings.velocityVariation);}
    if(!settings.localSpace){v.position=p+q*(v.position*scale);v.velocity=q*v.velocity;}
    if(settings.localSpace&&(Extended(settings)||runtime)){
        if(!runtime)runtime=std::make_unique<JudasParticles::CpuParticles>();
        runtime->RecordBirth(particles.size(),{p,scale,q});
    }
    particles.push_back(v);}
void VisualParticlePool::Burst(unsigned count,glm::vec3 p,glm::quat q,glm::vec3 scale){if(!settings.enabled||PendingStep()||Paused()||!Emitting())return;for(unsigned i=0;i<count&&particles.size()<static_cast<size_t>(settings.maxParticles);++i)Spawn(p,q,scale);}
void VisualParticlePool::Update(float dt,glm::vec3 p,glm::quat q,glm::vec3 scale,const GravityField& gravity){
    if(Extended(settings)||runtime){UpdateWorld(dt,p,q,scale,gravity,nullptr,{0,0,0},0);return;}
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
void VisualParticlePool::UpdateWorld(float dt,glm::vec3 p,glm::quat q,glm::vec3 scale,
        const GravityField& gravity,std::shared_ptr<const JudasParticles::CollisionSnapshot> collision,
        glm::dvec3 origin,std::uint64_t tick,glm::vec3 wind){
    if(!Extended(settings)&&!runtime){Update(dt,p,q,scale,gravity);return;}
    if(!runtime)runtime=std::make_unique<JudasParticles::CpuParticles>();
    if(!runtime->Update(particles,settings,dt,{p,scale,q},gravity,std::move(collision),origin,tick,wind))return;
    if(!settings.enabled||!Emitting())return;
    if(!started){started=true;Burst(settings.burst,p,q,scale);}
    if(settings.loop){credit+=runtime->CompletedSeconds()*settings.rate;auto count=unsigned(credit);credit-=count;Burst(count,p,q,scale);}
    billboards.clear();bounds={};
}
bool VisualParticlePool::Control(const std::string& command){
    if(!runtime)runtime=std::make_unique<JudasParticles::CpuParticles>();
    if(!runtime->Control(command,particles))return false;
    if(command=="restart"){started=false;credit=0;random=settings.seed;nextBirth=1;}
    billboards.clear();bounds={};return true;
}
bool VisualParticlePool::PendingStep()const{return runtime&&runtime->PendingStep();}
bool VisualParticlePool::Paused()const{return runtime&&runtime->Paused();}
bool VisualParticlePool::Emitting()const{return !runtime||runtime->Emitting();}
JudasParticles::ParticleRuntimeStats VisualParticlePool::RuntimeStats()const{
    auto stats=runtime?runtime->Stats():JudasParticles::ParticleRuntimeStats{};
    stats.live=particles.size();stats.requested=settings.backend;return stats;
}
const JudasParticles::ParticleImpacts* VisualParticlePool::ImpactState()const{return runtime?&runtime->Impacts():nullptr;}
JudasParticles::ParticleImpacts& VisualParticlePool::ImpactState(){if(!runtime)runtime=std::make_unique<JudasParticles::CpuParticles>();return runtime->Impacts();}
void VisualParticlePool::ImportTransient(std::vector<VisualParticle> next,double emissionCredit,bool emissionStarted,std::uint64_t birth){
    if(PendingStep())throw std::logic_error("cannot replace a pending reliable particle step");
    if(runtime)runtime->RemapParticles({},true);
    particles=std::move(next);credit=emissionCredit;started=emissionStarted;nextBirth=birth;billboards.clear();bounds={};
}
const std::vector<ParticleBillboard>& VisualParticlePool::Presentation(glm::vec3 p,glm::quat q,glm::vec3 scale){billboards.clear();bounds={};
    for(const auto& v:particles){float t=v.age/settings.lifetime;float size=glm::mix(settings.size,settings.endSize,t);
        if(settings.localSpace)size*=glm::max(glm::max(std::abs(scale.x),std::abs(scale.y)),std::abs(scale.z));
        auto pos=settings.localSpace?p+q*(v.position*scale):v.position;
        billboards.push_back({pos,size,glm::mix(settings.color,settings.endColor,t)});
        // Camera-facing square corner radius, independent of camera orientation.
        glm::vec3 extent(size*0.7071068f);bounds.Include(pos-extent);bounds.Include(pos+extent);}
    return billboards;}
