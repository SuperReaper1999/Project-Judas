#include "CpuParticles.h"
#include "CollisionSnapshot.h"
#include "../VisualParticles.h"
#include "../GravityField.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace JudasParticles {
namespace {
constexpr double Skin=1e-4;
glm::vec3 Divide(glm::vec3 value,glm::vec3 scale){
    for(int i=0;i<3;++i)value[i]=scale[i]!=0?value[i]/scale[i]:0;
    return value;
}
bool Finite(glm::dvec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct Prepared {
    glm::dvec3 position{0},velocity{0},carrier{0};
    double fraction=0,endFraction=1;
    float age=0;
    unsigned contacts=0;
    bool removed=false,finished=false;
    std::optional<ParticleCollisionHit> waiting;
};
struct Work {
    ParticleEmitterSettings settings;
    ParticlePose pose;
    std::shared_ptr<const CollisionSnapshot> collision;
    glm::dvec3 origin{0},chartShift{0};
    std::uint64_t tick=0;
    float dt=0;
    std::size_t cursor=0;
    std::vector<Prepared> prepared;
};
}
struct CpuParticles::State {
    ParticleImpacts impacts;
    std::vector<std::optional<CollisionAnchor>> anchors;
    std::vector<std::optional<ParticlePose>> births;
    std::optional<Work> work;
    ParticlePose lastPose;
    bool hasPose=false,paused=false,emitting=true;
    float completed=0;
    ParticleRuntimeStats stats;
};
CpuParticles::CpuParticles():state(std::make_unique<State>()){}
CpuParticles::~CpuParticles()=default;
CpuParticles::CpuParticles(const CpuParticles& b):state(std::make_unique<State>(*b.state)){}
CpuParticles& CpuParticles::operator=(const CpuParticles& b){if(this!=&b)state=std::make_unique<State>(*b.state);return *this;}

bool CpuParticles::Update(std::vector<VisualParticle>& particles,const ParticleEmitterSettings& settings,
                         float dt,const ParticlePose& pose,const GravityField& gravity,
                         std::shared_ptr<const CollisionSnapshot> collision,glm::dvec3 origin,
                         std::uint64_t tick,glm::vec3 wind){
    auto& s=*state;s.completed=0;
    if(!std::isfinite(dt)||dt<=0){s.stats.diagnostic="particle step requires finite positive duration";return false;}
    if(s.paused){s.stats.paused=true;return false;}
    const auto start=std::chrono::steady_clock::now();
    if(!s.work){
        s.stats.diagnostic.clear();
        if(settings.collision&&!collision){s.stats.diagnostic="particle collision snapshot is not ready";return false;}
        Work w;w.settings=settings;w.pose=pose;w.collision=std::move(collision);w.origin=origin;w.tick=tick;w.dt=dt;
        w.prepared.reserve(particles.size());s.anchors.resize(particles.size());s.births.resize(particles.size());
        const auto previous=s.hasPose?s.lastPose:pose;
        for(std::size_t index=0;index<particles.size();++index){
            const auto& p=particles[index];
            Prepared next;next.age=p.age+dt;
            const float seconds=std::min(dt,std::max(0.f,settings.lifetime-p.age));
            next.endFraction=seconds/dt;
            if(seconds<=0){next.removed=true;next.finished=true;w.prepared.push_back(next);continue;}
            const auto& startPose=s.births[index]?*s.births[index]:previous;
            const auto oldWorld=settings.localSpace?startPose.position+startPose.rotation*(p.position*startPose.scale):p.position;
            const auto nowWorld=settings.localSpace?pose.position+pose.rotation*(p.position*pose.scale):p.position;
            auto acceleration=settings.localSpace?settings.acceleration:pose.rotation*settings.acceleration;
            const auto carrier=(glm::dvec3(nowWorld)-glm::dvec3(oldWorld))/double(dt);
            const auto oldVelocity=settings.localSpace?glm::dvec3(pose.rotation*(p.velocity*pose.scale))+carrier:glm::dvec3(p.velocity);
            auto external=(settings.useGravity?gravity.Sample(nowWorld):glm::vec3(0))+
                          (settings.useWind?wind-glm::vec3(oldVelocity):glm::vec3(0));
            if(settings.localSpace)external=Divide(glm::inverse(pose.rotation)*external,pose.scale);
            acceleration+=external;
            auto velocity=p.velocity+acceleration*seconds;
            next.position=glm::dvec3(oldWorld);
            next.carrier=settings.localSpace?carrier:glm::dvec3(0);
            next.velocity=settings.localSpace?glm::dvec3(pose.rotation*(velocity*pose.scale))+next.carrier:glm::dvec3(velocity);
            if(!Finite(next.position)||!Finite(next.velocity)){++s.stats.queryFailures;s.stats.diagnostic="nonfinite particle motion rejected";return false;}
            w.prepared.push_back(next);
        }
        s.work=std::move(w);s.stats.backpressure=false;
    }
    auto& w=*s.work;const auto& config=w.settings;
    const auto write=[&](std::size_t index){
        auto& p=particles[index];auto& prepared=w.prepared[index];p.age=prepared.removed?config.lifetime:prepared.age;
        if(config.localSpace){
            p.position=Divide(glm::inverse(w.pose.rotation)*(glm::vec3(prepared.position)-w.pose.position),w.pose.scale);
            p.velocity=Divide(glm::inverse(w.pose.rotation)*glm::vec3(prepared.velocity-prepared.carrier),w.pose.scale);
        }else{
            p.position=glm::vec3(prepared.position-w.chartShift);p.velocity=glm::vec3(prepared.velocity);
        }
    };
    for(;w.cursor<w.prepared.size();++w.cursor){
        auto& p=w.prepared[w.cursor];
        if(s.anchors[w.cursor]&&w.collision){
            glm::dvec3 point,normal,velocity;
            if(w.collision->ResolveAnchor(*s.anchors[w.cursor],1,point,normal,&velocity)){
                p.position=point+normal*(config.collisionRadius+Skin);p.velocity=velocity;p.finished=true;
            }else s.anchors[w.cursor].reset();
        }
        while(!p.finished&&p.fraction<p.endFraction){
            const double remaining=w.dt*(p.endFraction-p.fraction);
            if(!config.collision){p.position+=p.velocity*remaining;p.finished=true;break;}
            const auto hit=p.waiting?*p.waiting:w.collision->SweepSphere(p.position,p.position+p.velocity*remaining,
                config.collisionRadius,p.fraction,p.endFraction);
            if(hit.status==CollisionQueryStatus::Miss){p.position+=p.velocity*remaining;p.finished=true;break;}
            if(hit.status!=CollisionQueryStatus::Hit){
                ++s.stats.queryFailures;
                s.stats.diagnostic=hit.status==CollisionQueryStatus::Unsupported?"unsupported particle collision geometry; motion retained at last safe point":
                    hit.status==CollisionQueryStatus::OutsideDomain?"particle sweep left its collision snapshot domain; motion retained":
                    "particle collision search exhausted; motion retained at last safe point";
                p.finished=true;break;
            }
            const bool incoming=glm::dot(p.velocity-hit.surfaceVelocity,hit.normal)<-1e-7||hit.penetration>Skin;
            if(incoming&&config.impacts!=ParticleImpactDelivery::Off){
                ParticleImpact impact;impact.tick=w.tick;impact.receiver=hit.entity;impact.geometryRevision=hit.geometryRevision;
                impact.childKey=hit.childKey;impact.feature=hit.feature;impact.count=1;impact.weight=config.impactWeight;
                impact.position=w.origin+hit.point;impact.localPosition=hit.localPoint;
                impact.normal=glm::vec3(hit.normal);impact.localNormal=glm::vec3(hit.localNormal);
                impact.velocity=glm::vec3(p.velocity);impact.surfaceVelocity=glm::vec3(hit.surfaceVelocity);
                impact.age=std::min(config.lifetime,particles[w.cursor].age+float(hit.fraction*w.dt));
                const float age=impact.age/config.lifetime;
                impact.color=glm::mix(config.color,config.endColor,age);impact.size=glm::mix(config.size,config.endSize,age);
                if(!s.impacts.Add(impact,config)){
                    p.waiting=hit;s.stats.backpressure=true;s.stats.pendingStep=true;
                    s.stats.diagnostic="reliable particle impacts await consumption; captured contact and remaining step retained";
                    s.stats.live=particles.size();return false;
                }
            }
            p.waiting.reset();++s.stats.contacts;++p.contacts;
            p.position=hit.center+hit.normal*(hit.penetration+Skin);
            p.fraction=std::max(p.fraction,hit.fraction);
            if(config.response==ParticleResponse::Remove){p.removed=true;p.finished=true;break;}
            if(config.response==ParticleResponse::Stick){
                s.anchors[w.cursor]=w.collision->MakeAnchor(hit);
                glm::dvec3 point,normal,velocity;
                if(w.collision->ResolveAnchor(*s.anchors[w.cursor],1,point,normal,&velocity)){
                    p.position=point+normal*(config.collisionRadius+Skin);p.velocity=velocity;
                }
                p.finished=true;break;
            }
            p.velocity=CollisionResponse(p.velocity,hit,config.restitution,config.friction,config.response==ParticleResponse::Slide);
            if(p.contacts>=config.maxContacts){
                ++s.stats.queryFailures;s.stats.diagnostic="particle response contact budget exhausted; remaining movement stopped at last verified contact";
                p.finished=true;break;
            }
        }
        write(w.cursor);
    }
    std::size_t out=0;
    for(std::size_t i=0;i<particles.size();++i)if(particles[i].age<config.lifetime){
        particles[out]=particles[i];s.anchors[out]=s.anchors[i];++out;
    }
    particles.resize(out);s.anchors.resize(out);s.births.assign(out,std::nullopt);
    s.completed=w.dt;s.lastPose=w.pose;s.lastPose.position-=glm::vec3(w.chartShift);s.hasPose=true;
    s.stats.simulated+=w.prepared.size();s.work.reset();s.stats.pendingStep=false;s.stats.backpressure=false;
    s.stats.live=particles.size();s.stats.paused=false;
    s.stats.simulationMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return true;
}
bool CpuParticles::Control(const std::string& command,std::vector<VisualParticle>& particles){
    auto& s=*state;
    if(command=="play"){s.paused=false;s.emitting=true;return true;}
    if(command=="pause"){s.paused=true;return true;}
    if(command=="stop"){s.emitting=false;return true;}
    if(command!="clear"&&command!="restart")return false;
    if(s.work||!s.impacts.Pending().empty())return false;
    particles.clear();s.anchors.clear();s.births.clear();s.hasPose=false;s.stats.live=0;
    if(command=="restart"){s.emitting=true;s.paused=false;}
    return true;
}
void CpuParticles::RemapParticles(const std::vector<std::size_t>& retained,bool resetPose){
    auto& s=*state;
    if(s.work)throw std::logic_error("cannot reorder a pending reliable particle step");
    std::vector<std::optional<CollisionAnchor>> anchors;
    std::vector<std::optional<ParticlePose>> births;
    anchors.reserve(retained.size());births.reserve(retained.size());
    for(auto index:retained){
        anchors.push_back(index<s.anchors.size()?s.anchors[index]:std::nullopt);
        births.push_back(index<s.births.size()?s.births[index]:std::nullopt);
    }
    s.anchors.swap(anchors);s.births.swap(births);
    if(resetPose)s.hasPose=false;
}
void CpuParticles::RecordBirth(std::size_t index,const ParticlePose& pose){
    auto& s=*state;
    if(s.work)throw std::logic_error("cannot add a birth to a pending reliable particle step");
    if(s.births.size()<=index)s.births.resize(index+1);
    s.births[index]=pose;
}
void CpuParticles::ShiftOrigin(glm::dvec3 delta){
    if(state->hasPose)state->lastPose.position=glm::vec3(glm::dvec3(state->lastPose.position)-delta);
    for(auto& birth:state->births)if(birth)birth->position=glm::vec3(glm::dvec3(birth->position)-delta);
    if(state->work)state->work->chartShift+=delta;
}
bool CpuParticles::PendingStep()const{return state->work.has_value();}
bool CpuParticles::Emitting()const{return state->emitting;}
bool CpuParticles::Paused()const{return state->paused;}
float CpuParticles::CompletedSeconds()const{return state->completed;}
ParticleRuntimeStats CpuParticles::Stats()const{
    auto out=state->stats;out.paused=state->paused;out.pendingStep=PendingStep();
    for(const auto& p:state->impacts.Pending())out.queuedImpacts+=p.count;
    out.deliveredImpacts=state->impacts.Delivered();out.cosmeticDropped=state->impacts.Dropped();return out;
}
ParticleImpacts& CpuParticles::Impacts(){return state->impacts;}
const ParticleImpacts& CpuParticles::Impacts()const{return state->impacts;}
}
