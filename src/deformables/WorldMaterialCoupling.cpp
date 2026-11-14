#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include <algorithm>
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
    JUDAS_PROFILE_SCOPE("Coupled material interval");
    const auto measuredStart=PerformanceProfiler::Get().Active()?PerformanceProfiler::Now():0;
    for(auto& [id,record]:m_deformables){auto& material=record.simulation;
        if(!material.PhysicalStepActive()||!IsPublished(id))continue;
        if(!SampleDeformableTargets(id,true)){material.error="required material target unavailable during physical interval";continue;}
        const unsigned subdivisions=std::max(1u,(material.settings.substeps+interval.count-1)/interval.count);
        material.AdvancePhysicalInterval(interval.deltaTime,subdivisions,m_physics,GravityForEntity(id),record.targets,true);
    }
    const auto crossStart=measuredStart?PerformanceProfiler::Now():0;
    struct Region {
        DeformableRecord* record;
        glm::dvec3 minimum{0},maximum{0};
        bool prepared=false;
    };
    std::vector<Region> regions;
    regions.reserve(m_deformables.size());
    for(auto& [id,record]:m_deformables){auto& material=record.simulation;
        if(material.PhysicalStepActive()&&material.error.empty())
            regions.push_back({&record});
    }
    const auto prepareBounds=[](Region& region){
        if(region.prepared)return;
        region.minimum=region.record->simulation.Minimum();
        region.maximum=region.record->simulation.Maximum();
        region.prepared=true;
    };
    // Keep the established map/pair order. Sleeping pairs need no bounds at
    // all; unchanged active patches need no repeated node scan. Invalidate
    // both enclosures after projection so an earlier pair's correction cannot
    // hide a later contact.
    for(std::size_t a=0;a<regions.size();++a)for(std::size_t b=a+1;b<regions.size();++b){
        auto& first=regions[a];auto& second=regions[b];
        auto& x=first.record->simulation;auto& y=second.record->simulation;
        if(x.sleeping&&y.sleeping)continue;
        prepareBounds(first);prepareBounds(second);
        double thickness=x.settings.material.thickness+y.settings.material.thickness;
        if(glm::any(glm::lessThan(first.maximum+glm::dvec3(thickness),second.minimum))||glm::any(glm::lessThan(second.maximum+glm::dvec3(thickness),first.minimum)))continue;
        DeformableInstance::SurfaceContacts(x,y,interval.deltaTime);
        DeformableInstance::SurfaceContacts(y,x,interval.deltaTime);
        first.prepared=second.prepared=false;
        ++first.record->revision;++second.record->revision;
    }
    if(measuredStart){
        JUDAS_PROFILE_COUNTER("Deformable cross-material contacts CPU ms",double(PerformanceProfiler::Now()-crossStart)*1e-6,ProfileCounterMode::Sum);
        JUDAS_PROFILE_COUNTER("Deformable interval CPU ms",double(PerformanceProfiler::Now()-measuredStart)*1e-6,ProfileCounterMode::Sum);
    }
}
