#include "../RuntimeWorld.h"
#include "../ResourceManager.h"
#include "../PlanetTiles.h"
#include "../UniformGravity.h"
#include "../RadicalGravity.h"
#include "../FaithfulGravity.h"
#include "../PerformanceProfiler.h"
#include "CollisionSnapshot.h"
#include "GpuParticles.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
using Clock=std::chrono::steady_clock;
bool Finite(glm::dvec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool TextureValid(ResourceManager* resources,const std::string& id){
    if(id.empty())return true;
    const auto* asset=resources&&resources->Assets()?resources->Assets()->Find(id):nullptr;
    return asset&&!asset->missing&&asset->type==AssetType::Texture;
}
bool CopyGpuToCpu(RuntimeWorld::ParticleEmitter& e,std::string& error){
    if(!e.gpu)return true;
    std::vector<JudasParticles::GpuParticleState> state;
    // Exceptional backend transition only. Ordinary GPU ticks and drawing do
    // not download particle arrays. Preserve every live state before retiring
    // an unsupported GPU path; report this synchronization/transfer explicitly.
    if(!e.gpu->ReadStateForDiagnostics(state,error))return false;
    std::vector<VisualParticle> live;live.reserve(state.size());
    std::uint64_t next=1;
    for(const auto& p:state){
        next=std::max(next,std::uint64_t(p.identity[1])+1);
        if(p.identity[0])live.push_back({{p.positionAge[0],p.positionAge[1],p.positionAge[2]},
            {p.velocityLife[0],p.velocityLife[1],p.velocityLife[2]},p.positionAge[3]});
    }
    const auto diagnostics=e.gpu->Diagnostics();next=std::max(next,diagnostics.emitted+1);
    e.pool.ImportTransient(std::move(live),e.gpuCredit,e.gpuStarted,next);
    for(const auto& impact:e.gpu->TakeImpacts())e.pool.ImpactState().Add(impact,e.pool.settings);
    e.retiredGpuDrops+=e.gpu->Stats().cosmeticDropped;
    if(!e.gpu->Shutdown(error))return false;
    e.gpu.reset();e.cpuPinned=true;e.runtime.stateTransferBytes+=state.size()*sizeof(state[0]);
    return true;
}
}

void RuntimeWorld::UpdateVisualParticles(float dt){
    if(m_particleEmitters.empty())return;
    JUDAS_PROFILE_SCOPE("Visual particle simulation");
    const auto tick=std::uint64_t(std::max(0.,std::round(SimulationTimeSeconds()/std::max(double(dt),1e-9))));
    // Only this completed fixed-boundary call is shared. The predecessor owner
    // is part of the key: stick anchors and suspended reliable work must not
    // adopt another owner's geometry-revision history. Keep at most 64 groups;
    // overflow captures independently rather than reducing collision coverage.
    struct Batch {
        std::shared_ptr<JudasParticles::CollisionSnapshot> previous, snapshot;
        CategoryMask layers;
        BodyHandle ignored;
        Aabb domain;
        std::shared_ptr<JudasParticles::GpuParticles> gpu;
    };
    std::vector<Batch> batches;
    for(auto& e:m_particleEmitters){
        if(!IsPublished(e.id))continue;
        if(const auto* entity=FindEntity(e.id))if(entity->lifecycle==EntityLifecycle::Destroyed)continue;
        const auto start=Clock::now();const auto& settings=e.pool.settings;
        const auto pose=PresentedTransform(e.id,e.transform,1);
        e.runtime.requested=settings.backend;
        if(e.pool.Paused()){e.runtime.paused=true;continue;}
        e.runtime.paused=false;
        e.runtime.snapshotMs=0;
        // CPU births from rate/initial burst occur after UpdateWorld integrates
        // the existing population. Empty CPU steps still advance that emission
        // clock, but have no collision queries to prepare. Queued explicit
        // bursts are different: fallback consumes them before integration.
        // Never infer GPU emptiness from its asynchronous live-count readback.
        const bool emptyCpu=!e.gpu&&(settings.backend==ParticleBackend::Cpu||e.cpuPinned)&&
                            e.pool.Particles().empty()&&e.gpuBurst==0&&!e.pool.PendingStep();
        if(!settings.collision||emptyCpu){
            e.runtime.snapshotBodies=0;e.runtime.snapshotColliders=0;e.runtime.snapshotTriangles=0;
        }
        std::size_t batch=batches.size();
        if(settings.collision&&!emptyCpu&&!e.pool.PendingStep()){
            Aabb domain{{-8192, -8192, -8192}, {8192, 8192, 8192}};
            if (settings.collisionRegion) {
                const auto center = pose.position +
                    pose.rotation * (pose.scale * settings.collisionRegionOffset);
                const auto rotation = glm::mat3_cast(pose.rotation);
                const auto half = glm::abs(pose.scale) * settings.collisionRegionHalfExtents;
                const auto extent = glm::abs(rotation[0]) * half.x +
                    glm::abs(rotation[1]) * half.y + glm::abs(rotation[2]) * half.z;
                domain = {center - extent, center + extent};
            }
            const auto own=RuntimeBody(e.id);
            for(std::size_t i=0;i<batches.size();++i)
                if(batches[i].previous==e.collision&&batches[i].layers==settings.collisionLayers&&
                   batches[i].domain.min==domain.min&&batches[i].domain.max==domain.max&&
                   batches[i].ignored.id==own.id&&batches[i].ignored.world==own.world){batch=i;break;}
            if(batch<batches.size()){
                e.collision=batches[batch].snapshot;
                JUDAS_PROFILE_COUNTER("Particle snapshot reuses",1,ProfileCounterMode::Sum);
            }else{
                const auto prepare=Clock::now();
                auto next=e.collision?std::make_shared<JudasParticles::CollisionSnapshot>(*e.collision):
                                      std::make_shared<JudasParticles::CollisionSnapshot>();
                PhysicsQueryFilter filter;filter.includeLayers=settings.collisionLayers;
                if(own.IsValid())filter.ignoredBodies.push_back(own);
                auto identity=[&](BodyHandle body){
                    for(const auto& [id,planet]:m_planets)if(planet->Owns(body))return id;
                    return EntityIdOfBody(body);
                };
                std::string error;
                // Coverage is the whole chart unless the author explicitly
                // declares a local region. Capture still includes swept movers;
                // no current-pose culling or guessed GPU particle bounds.
                if(!next->Capture(m_physics,domain,dt,error,filter,identity)){
                    e.runtime.diagnostic="particle collision snapshot unavailable: "+error;
                    ++e.runtime.queryFailures;continue;
                }
                if(batches.size()<64){
                    if(batches.empty())batches.reserve(std::min<std::size_t>(64,m_particleEmitters.size()));
                    batch=batches.size();
                    batches.push_back({e.collision,next,settings.collisionLayers,own,domain,{}});
                }
                e.collision=std::move(next);
                JUDAS_PROFILE_COUNTER("Particle snapshot captures",1,ProfileCounterMode::Sum);
                e.runtime.snapshotMs=std::chrono::duration<double,std::milli>(Clock::now()-prepare).count();
            }
            e.runtime.snapshotBodies=e.collision->Stats().retainedBodies;
            e.runtime.snapshotColliders=e.collision->Stats().colliders;
            e.runtime.snapshotTriangles=e.collision->Stats().uniqueTriangles;
        }
        const auto wind=settings.useWind?glm::vec3(SampleWeather(m_settings.worldOrigin+glm::dvec3(pose.position)).wind):glm::vec3(0);
        bool gpuRequested=settings.backend!=ParticleBackend::Cpu&&!e.cpuPinned;
        JudasParticles::GpuParticleStepInputs inputs;inputs.dt=dt;inputs.tick=tick;
        inputs.position=pose.position;inputs.rotation=pose.rotation;inputs.scale=pose.scale;
        inputs.worldOrigin=m_settings.worldOrigin;inputs.wind=wind;
        std::string reason;
        auto* renderer=m_assets?m_assets->GetRenderer():nullptr;
        if(gpuRequested&&(!renderer||!renderer->Capabilities().modernTier))reason="modern desktop compute context unavailable";
        if(gpuRequested&&settings.impacts==ParticleImpactDelivery::Reliable)reason="reliable gameplay impacts use the CPU backend";
        if(gpuRequested&&settings.localSpace)reason="local-space collision/transform semantics use the CPU backend";
        if(gpuRequested&&settings.useGravity&&reason.empty()){
            const auto* d=RuntimeDefinition(e.id);
            const GravityField* selected=SelectedGravitySource(e.id);
            if(d&&d->gravitySelection&&d->gravitySelection->mode==GravitySelection::Mode::Uniform){
                inputs.gravityKind=JudasParticles::GpuGravityKind::Uniform;inputs.gravity=d->gravitySelection->acceleration;
            }else if(selected&&(dynamic_cast<const UniformGravity*>(selected)||dynamic_cast<const FaithfulGravity*>(selected))){
                inputs.gravityKind=JudasParticles::GpuGravityKind::Uniform;inputs.gravity=selected->Sample(pose.position);
            }else if(selected&&dynamic_cast<const RadicalGravity*>(selected)){
                GravityEquilibrium equilibrium;
                if(selected->Equilibrium(pose.position,equilibrium)){
                    inputs.gravityKind=JudasParticles::GpuGravityKind::Radial;
                    inputs.gravityCenter=glm::vec3(equilibrium.center);inputs.gravityMagnitude=float(equilibrium.magnitude);
                }else inputs.gravityKind=JudasParticles::GpuGravityKind::None;
            }else if(m_gravityRegions.empty())inputs.gravityKind=JudasParticles::GpuGravityKind::None;
            else reason="GPU gravity requires an explicit uniform/radial source; spatial/custom gravity remains CPU";
        }
        if(gpuRequested&&!e.gpu&&!e.pool.Particles().empty())reason="live CPU particles retain their backend until cleared";
        if(gpuRequested&&reason.empty()){
            if(!e.gpu){
                auto gpu=std::make_shared<JudasParticles::GpuParticles>();
                if(gpu->Initialize(renderer->Capabilities(),settings,{},reason))e.gpu=std::move(gpu);
            }
            if(e.gpu){
                double nextCredit=e.gpuCredit;bool nextStarted=e.gpuStarted;unsigned spawn=e.gpuBurst;
                if(settings.enabled&&e.pool.Emitting()){
                    if(!nextStarted){nextStarted=true;spawn+=unsigned(settings.burst);}
                    if(settings.loop){nextCredit+=double(dt)*settings.rate;const auto count=unsigned(nextCredit);nextCredit-=count;spawn+=count;}
                }
                inputs.spawnCount=spawn;
                auto* peer=batch<batches.size()?batches[batch].gpu.get():nullptr;
                if(e.gpu->Step(settings,inputs,settings.collision?e.collision.get():nullptr,reason,peer)){
                    if(batch<batches.size()&&!batches[batch].gpu)batches[batch].gpu=e.gpu;
                    e.gpuCredit=nextCredit;e.gpuStarted=nextStarted;e.gpuBurst=0;
                    std::string polling;e.gpu->PollEvents(polling);
                    for(const auto& impact:e.gpu->TakeImpacts())e.pool.ImpactState().Add(impact,settings);
                    const auto capture=e.runtime; e.runtime=e.gpu->Stats();
                    e.runtime.active=ParticleBackend::Gpu;e.runtime.requested=settings.backend;
                    e.runtime.snapshotMs=capture.snapshotMs;e.runtime.snapshotBodies=capture.snapshotBodies;
                    e.runtime.snapshotColliders=capture.snapshotColliders;e.runtime.snapshotTriangles=capture.snapshotTriangles;
                    if(!polling.empty())e.runtime.diagnostic=polling;
                    e.runtime.simulationMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
                    continue;
                }
            }
        }
        if(settings.backend==ParticleBackend::Gpu&&!reason.empty()){
            e.runtime.diagnostic="required GPU particle step not advanced: "+reason;++e.runtime.queryFailures;continue;
        }
        if(e.gpu){
            std::string error;
            if(!CopyGpuToCpu(e,error)){e.runtime.diagnostic="particle backend transition pending: "+error;++e.runtime.queryFailures;continue;}
        }
        if(gpuRequested&&!reason.empty())e.cpuPinned=true;
        if(e.gpuBurst){
            e.pool.Burst(e.gpuBurst,pose.position,pose.rotation,pose.scale);
            e.gpuBurst=0;
        }
        e.pool.UpdateWorld(dt,pose.position,pose.rotation,pose.scale,GravityForEntity(e.id),
                          settings.collision?e.collision:nullptr,m_settings.worldOrigin,tick,wind);
        const auto snapshot=e.runtime;e.runtime=e.pool.RuntimeStats();e.runtime.active=ParticleBackend::Cpu;
        e.runtime.snapshotMs=snapshot.snapshotMs;e.runtime.snapshotBodies=snapshot.snapshotBodies;
        e.runtime.snapshotColliders=snapshot.snapshotColliders;e.runtime.snapshotTriangles=snapshot.snapshotTriangles;
        e.runtime.stateTransferBytes+=snapshot.stateTransferBytes;
        e.runtime.cosmeticDropped+=e.retiredGpuDrops;
        if(!reason.empty())e.runtime.diagnostic=reason+(e.runtime.diagnostic.empty()?"":"; "+e.runtime.diagnostic);
        else if(e.cpuPinned&&snapshot.active==ParticleBackend::Cpu&&!snapshot.diagnostic.empty()&&e.runtime.diagnostic.empty())
            e.runtime.diagnostic=snapshot.diagnostic;
        e.runtime.simulationMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        JUDAS_PROFILE_COUNTER("Particle CPU live",double(e.runtime.live),ProfileCounterMode::Sum);
        JUDAS_PROFILE_COUNTER("Particle contacts",double(e.runtime.contacts),ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("Particle query failures",double(e.runtime.queryFailures),ProfileCounterMode::Latest);
    }
}

bool RuntimeWorld::EmitParticleBurst(SceneObjectId id,unsigned count){
    for(auto& e:m_particleEmitters)if(e.id==id){
        if(!e.pool.settings.enabled||e.pool.PendingStep()||e.pool.Paused()||!e.pool.Emitting())return false;
        // start() can request a burst before the first fixed tick initializes
        // the selected backend. Keep that request out of the CPU particle array.
        if(e.gpu||(!e.cpuPinned&&e.pool.settings.backend!=ParticleBackend::Cpu)){
            if(count>100000-e.gpuBurst)return false;
            e.gpuBurst+=count;
            return true;
        }
        const auto t=PresentedTransform(e.id,e.transform,1);e.pool.Burst(count,t.position,t.rotation,t.scale);return true;
    }
    return false;
}
JudasParticles::ParticleRuntimeStats RuntimeWorld::GetParticleRuntimeStats(EntityId id)const{
    for(const auto& e:m_particleEmitters)if(e.id==id){auto stats=e.runtime;
        stats.requested=e.pool.settings.backend;
        const auto cpu=e.pool.RuntimeStats();stats.paused=e.pool.Paused();stats.pendingStep=e.pool.PendingStep();
        if(e.gpu){stats.active=ParticleBackend::Gpu;stats.gpuBytes=e.gpu->Stats().gpuBytes;}
        else {stats.active=ParticleBackend::Cpu;stats.gpuBytes=0;}
        stats.queuedImpacts=cpu.queuedImpacts;stats.deliveredImpacts=cpu.deliveredImpacts;
        stats.cosmeticDropped=e.gpu?stats.cosmeticDropped+cpu.cosmeticDropped:
                                   e.retiredGpuDrops+cpu.cosmeticDropped;
        if(!e.gpu)stats.live=cpu.live;
        return stats;
    }
    return {};
}
const std::vector<JudasParticles::ParticleImpact>* RuntimeWorld::PeekParticleImpacts(EntityId id)const{
    for(const auto& e:m_particleEmitters)if(e.id==id){const auto* state=e.pool.ImpactState();return state?&state->Pending():nullptr;}
    return nullptr;
}
std::vector<JudasParticles::ParticleImpact> RuntimeWorld::TakeParticleImpacts(EntityId id){
    for(auto& e:m_particleEmitters)
        if(e.id==id)return e.pool.ImpactState().Take();
    return {};
}
bool RuntimeWorld::AcknowledgeParticleImpacts(EntityId id,std::uint64_t last){
    for(auto& e:m_particleEmitters)if(e.id==id){auto& state=e.pool.ImpactState();
        if(state.Pending().empty())return last==0;
        if(state.Pending().back().sequence!=last)return false;
        state.Take();return true;
    }return false;
}
bool RuntimeWorld::ControlParticles(EntityId id,const std::string& command){
    for(auto& e:m_particleEmitters)if(e.id==id){
        if(e.gpu&&(command=="clear"||command=="restart")){
            std::string error;
            if(!e.gpu->CanMutate(error)){e.runtime.diagnostic=error;return false;}
        }
        if(!e.pool.Control(command))return false;
        if(command=="clear"||command=="restart"){
            if(e.gpu){std::string error;if(!e.gpu->Clear(error)){e.runtime.diagnostic=error;return false;}}
            e.gpuCredit=0;e.gpuStarted=false;e.gpuBurst=0;e.cpuPinned=false;
            e.retiredGpuDrops=0;
        }
        return true;
    }return false;
}
bool RuntimeWorld::SetParticleSettings(EntityId id,const ParticleEmitterSettings& settings){
    const auto* definition=RuntimeDefinition(id);
    if(!definition||!definition->particleEmitter||!ValidParticleSettings(settings)||!TextureValid(m_assets,settings.textureAsset))return false;
    if(settings.impacts==ParticleImpactDelivery::Reliable&&IsTransientEntity(id))return false;
    for(auto& e:m_particleEmitters)if(e.id==id){
        if(ParticleSettingsEqual(e.pool.settings,settings))return true;
        if(e.pool.PendingStep())return false;
        const auto* pending=static_cast<const VisualParticlePool&>(e.pool).ImpactState();
        if(pending&&e.pool.settings.impacts==ParticleImpactDelivery::Reliable&&!pending->Pending().empty())return false;
        const auto old=e.pool.settings;
        if(e.gpu&&(settings.backend==ParticleBackend::Cpu||settings.localSpace!=old.localSpace||settings.maxParticles!=old.maxParticles||settings.impacts==ParticleImpactDelivery::Reliable)){
            std::string error;if(!CopyGpuToCpu(e,error)){e.runtime.diagnostic=error;return false;}
        }
        const bool textureChanged=old.textureAsset!=settings.textureAsset;
        e.pool.SetSettings(settings);e.cpuPinned=false;
        m_scriptDefinitions.at(id).particleEmitter=settings;
        if(auto* record=FindEntity(id))record->definition.particleEmitter=settings;
        if(textureChanged)ReconcileRenderResources(id);
        return true;
    }return false;
}
bool RuntimeWorld::HasPendingParticleImpacts(EntityId id)const{
    for(const auto& e:m_particleEmitters){
        if(e.pool.settings.impacts!=ParticleImpactDelivery::Reliable)continue;
        const auto* queue=e.pool.ImpactState();
        if(e.id==id&&(e.pool.PendingStep()||(queue&&!queue->Pending().empty())))return true;
        if(queue)for(const auto& impact:queue->Pending())if(impact.receiver==id)return true;
        if(e.pool.PendingStep()&&e.collision)for(const auto& body:e.collision->Bodies())if(body.entity==id)return true;
    }return false;
}
void RuntimeWorld::ShutdownParticleEmitter(SceneObjectId id){
    for(auto& e:m_particleEmitters)if(!id||e.id==id){
        if(e.gpu){std::string error;if(!e.gpu->Shutdown(error))std::fprintf(stderr,"Particle GPU shutdown: %s\n",error.c_str());e.gpu.reset();}
        e.collision.reset();
    }
}
bool RuntimeWorld::CanShiftParticleOrigin(glm::dvec3 delta,std::string& error)const{
    if(!Finite(delta)){error="nonfinite particle origin translation";return false;}
    for(const auto& e:m_particleEmitters)
        if(e.gpu&&!e.gpu->CanShiftOrigin(delta,error))return false;
    return true;
}
void RuntimeWorld::ShiftParticleOrigin(glm::dvec3 delta){
    for(auto& e:m_particleEmitters){
        e.transform.position=glm::vec3(glm::dvec3(e.transform.position)-delta);e.pool.ShiftOrigin(delta);
        if(e.gpu){std::string error;if(!e.gpu->ShiftOrigin(delta,error))throw std::runtime_error("particle origin shift: "+error);}
        // Keep geometry revision ownership; next capture rebinds chart poses.
    }
}
