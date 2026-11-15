#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "MaterialBatch.h"
#include <cstdlib>
#include "PerformanceProfiler.h"
#include <algorithm>
#include "Broadphase.h"
#include <cmath>
#include <limits>
bool RuntimeWorld::BeginDeformableStep(){
    bool active=false;
    for(auto id:m_deformableOwners){if(!IsPublished(id))continue;
        std::string error;auto* material=RuntimeDeformable(id,error);if(!material)continue;
        if(material->asset->fracture&&material->asset->fracture->rigid)continue;
        // Prescribed skeletal targets still require their final animation phase.
        // This first coupling qualification moves physical body/world panels;
        // skeletal phase unification is explicitly not claimed by this slice.
        if(std::any_of(material->settings.attachments.begin(),material->settings.attachments.end(),[](const auto& a){return a.enabled&&a.kind==DeformableAttachment::Kind::Bone;}))continue;
        PrepareDeformableTerrain(id);
        if(!SampleDeformableTargets(id))continue;
        UpdatePanelCollisionExclusions(*material);
        active|=material->BeginPhysicalStep();
    }
    return active;
}
void RuntimeWorld::PrepareDeformableTerrain(EntityId id){
    auto found=m_deformables.find(id);if(found==m_deformables.end())return;
    auto& material=found->second.simulation;
    std::vector<DeformableInstance::TerrainRegion> regions;
    if(m_runtimeTerrain)for(auto& solid:m_runtimeTerrain->Solids(glm::vec3(material.Minimum()),glm::vec3(material.Maximum()))){
        DeformableInstance::TerrainRegion region;region.solid=std::move(solid);region.bodies=OwnedCollisionBodies(region.solid.owner);regions.push_back(std::move(region));
    }
    bool changed=regions.size()!=material.terrainRegions.size();
    for(unsigned n=0;!changed&&n<regions.size();++n)changed=!regions[n].solid.SameGeometry(material.terrainRegions[n].solid);
    if(changed)material.Wake();
    material.terrainRegions=std::move(regions);
}
void RuntimeWorld::AdvanceDeformablesSubstep(const PhysicsWorld::VelocityCoupling::Substep& interval){
    if(interval.iteration!=0)return;
    m_physics.DebugPoint(PhysicsWorld::DebugPhase::MaterialBegin);
    JUDAS_PROFILE_SCOPE("Coupled material interval");
    const auto measuredStart=PerformanceProfiler::Get().Active()?PerformanceProfiler::Now():0;
    {
        unsigned lanes=4;
        if(const char* value=std::getenv("JUDAS_MATERIAL_LANES")){char* end=nullptr;const auto parsed=std::strtol(value,&end,10);if(end==value||*end||parsed<1||parsed>4)throw std::invalid_argument("JUDAS_MATERIAL_LANES must be 1..4");lanes=unsigned(parsed);}
        auto* jobs=m_assets?m_assets->Jobs():nullptr;
        // GravityForEntity returns a value. Keep its owner-thread adapters alive
        // through every queued phase, rather than retaining a temporary reference.
        std::map<EntityId,EntityGravityField> gravityFields;
        std::map<unsigned,std::vector<std::unique_ptr<MaterialInterval>>> cohorts;
        std::vector<EntityId> sleeping;
        for(auto& [id,record]:m_deformables){auto& material=record.simulation;
            if(!material.PhysicalStepActive()||!IsPublished(id))continue;
            if(material.sleeping){sleeping.push_back(id);continue;}
            if(!SampleDeformableTargets(id,true)){material.error="required material target unavailable during physical interval";continue;}
            const unsigned subdivisions=std::max(1u,(material.settings.substeps+interval.count-1)/interval.count);
            auto prepared=std::make_unique<MaterialInterval>(material,interval.deltaTime,subdivisions,m_physics,gravityFields.emplace(id,GravityForEntity(id)).first->second,record.targets,true);
            if(prepared->Ready())cohorts[subdivisions].push_back(std::move(prepared));
        }
        for(auto& [subdivisions,prepared]:cohorts){
            unsigned iterations=0;for(const auto& item:prepared)iterations=std::max(iterations,item->State().settings.iterations);
            std::vector<MaterialInterval*> active;active.reserve(prepared.size());
            for(unsigned sub=0;sub<subdivisions;++sub){
                for(auto& item:prepared)if(item->Ready())item->Predict(sub);
                for(unsigned iteration=0;iteration<iterations;++iteration){
                    active.clear();for(auto& item:prepared)if(item->Ready()&&iteration<item->State().settings.iterations)active.push_back(item.get());
                    JudasDeformables::IndependentMaterialPhase(active,MaterialInterval::Independent::Internal,jobs,lanes);
                    for(auto* item:active)item->RigidContacts();
                    JudasDeformables::IndependentMaterialPhase(active,MaterialInterval::Independent::SelfContact,jobs,lanes);
                    for(auto* item:active)item->Attach();
                }
                active.clear();for(auto& item:prepared)if(item->Ready())active.push_back(item.get());
                JudasDeformables::IndependentMaterialPhase(active,MaterialInterval::Independent::Finish,jobs,lanes);
            }
        }
        // Shared reactions may awaken a support after the first preparation.
        // Deferred sleepers receive their COMPLETE original interval once;
        // repeat only unresolved sleepers when a previous one has just woken.
        // No time, local subdivisions or requested iterations are discarded.
        bool progress=true;
        while(progress&&!sleeping.empty()){
            progress=false;
            for(auto it=sleeping.begin();it!=sleeping.end();){const auto id=*it;auto& record=m_deformables.at(id);auto& material=record.simulation;
                if(!SampleDeformableTargets(id,true)){material.error="required sleeping material target unavailable";it=sleeping.erase(it);continue;}
                const unsigned subdivisions=std::max(1u,(material.settings.substeps+interval.count-1)/interval.count);
                material.AdvancePhysicalInterval(interval.deltaTime,subdivisions,m_physics,GravityForEntity(id),record.targets,true);
                if(!material.sleeping||!material.error.empty()){progress=true;it=sleeping.erase(it);}else ++it;
            }
        }
        JUDAS_PROFILE_COUNTER("Material configured lanes",double(lanes),ProfileCounterMode::Maximum);
    }
    const auto crossStart=measuredStart?PerformanceProfiler::Now():0;
    struct Region {
        DeformableRecord* record;
        glm::dvec3 minimum{0},maximum{0};
        int proxy=DynamicAabbTree::kNull;
    };
    std::vector<Region> regions;
    regions.reserve(m_deformables.size());
    for(auto& [id,record]:m_deformables){auto& material=record.simulation;
        if(material.PhysicalStepActive()&&material.error.empty())
            regions.push_back({&record});
    }
    // Reuse the engine's spatial candidate structure. Publication remains in
    // the original entity/pair order, independent of tree traversal order.
    // Refit after each projection and query later pairs again: an earlier
    // correction may create a later contact in this SAME physical interval.
    DynamicAabbTree index;
    const auto refresh=[](Region& region){
        region.minimum=region.record->simulation.Minimum();region.maximum=region.record->simulation.Maximum();
        const double skin=region.record->simulation.settings.material.thickness;
        Aabb bounds;
        for(unsigned k=0;k<3;++k){
            bounds.min[k]=std::nextafter(float(region.minimum[k]-skin),-std::numeric_limits<float>::infinity());
            bounds.max[k]=std::nextafter(float(region.maximum[k]+skin),std::numeric_limits<float>::infinity());
        }
        return bounds;
    };
    size_t visits=0,refits=0;
    const bool active=std::any_of(regions.begin(),regions.end(),[](const auto& r){return !r.record->simulation.sleeping;});
    if(active){
        for(unsigned n=0;n<regions.size();++n)regions[n].proxy=index.CreateProxy(refresh(regions[n]),n);
        for(size_t a=0;a<regions.size();++a){size_t cursor=a;
            for(;;){size_t b=regions.size();
                index.Query(index.FatAabb(regions[a].proxy),[&](int proxy){
                    const auto candidate=index.UserData(proxy);
                    if(candidate>cursor&&candidate<b&&!(regions[a].record->simulation.sleeping&&regions[candidate].record->simulation.sleeping))b=candidate;
                });
                if(b==regions.size())break;
                cursor=b;++visits;
                auto& first=regions[a];auto& second=regions[b];auto& x=first.record->simulation;auto& y=second.record->simulation;
                const double thickness=x.settings.material.thickness+y.settings.material.thickness;
                // Outward-rounded float tree bounds only select candidates.
                // Retain the authoritative double enclosure test unchanged.
                if(glm::any(glm::lessThan(first.maximum+glm::dvec3(thickness),second.minimum))||glm::any(glm::lessThan(second.maximum+glm::dvec3(thickness),first.minimum)))continue;
                const auto before=x.stats.contacts+y.stats.contacts;
                DeformableInstance::SurfaceContacts(x,y,interval.deltaTime);
                DeformableInstance::SurfaceContacts(y,x,interval.deltaTime);
                if(x.stats.contacts+y.stats.contacts!=before){
                    index.MoveProxy(first.proxy,refresh(first));index.MoveProxy(second.proxy,refresh(second));refits+=2;
                }
                ++first.record->revision;++second.record->revision;
            }
        }
    }
    JUDAS_PROFILE_COUNTER("Deformable region candidate visits",double(visits),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Deformable region projection refits",double(refits),ProfileCounterMode::Sum);
    if(measuredStart){
        JUDAS_PROFILE_COUNTER("Deformable cross-material contacts CPU ms",double(PerformanceProfiler::Now()-crossStart)*1e-6,ProfileCounterMode::Sum);
        JUDAS_PROFILE_COUNTER("Deformable interval CPU ms",double(PerformanceProfiler::Now()-measuredStart)*1e-6,ProfileCounterMode::Sum);
    }
    m_physics.DebugPoint(PhysicsWorld::DebugPhase::MaterialEnd);
}

std::vector<PhysicsEventDebugger::Material> RuntimeWorld::DebugDeformables()const{
    std::vector<PhysicsEventDebugger::Material> result;std::size_t remaining=8192;
    for(const auto& [id,record]:m_deformables){const auto& d=record.simulation;if(!d.asset)continue;
        PhysicsEventDebugger::Material m;m.owner=id;m.nodes=d.positions.size();m.triangles=d.asset->triangles.size();m.tets=d.asset->solids.size();m.contacts=d.stats.contacts;m.candidates=d.stats.candidates;m.iterations=d.settings.iterations;m.substeps=d.stats.substeps;m.sleeping=d.sleeping;m.strain=d.stats.maximumStrain;
        for(auto v:d.released)m.released+=bool(v);
        for(auto v:d.fracture.broken)m.broken+=bool(v);
        for(auto v:d.fracture.plasticWork)m.plasticWork+=v;
        if(d.panelBending){m.hinges=d.panelBending->hinges.size();for(const auto& h:d.panelBending->hinges)m.plasticWork+=h.work;}
        const auto count=std::min(remaining,d.positions.size());m.positions.assign(d.positions.begin(),d.positions.begin()+count);remaining-=count;result.push_back(std::move(m));
    }return result;
}
