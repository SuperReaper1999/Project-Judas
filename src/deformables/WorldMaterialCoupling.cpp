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
        if(!SampleDeformableTargets(id))continue;
        UpdatePanelCollisionExclusions(*material);
        active|=material->BeginPhysicalStep();
    }
    return active;
}
void RuntimeWorld::AdvanceDeformablesSubstep(const PhysicsWorld::VelocityCoupling::Substep& interval){
    if(interval.iteration!=0)return;
    JUDAS_PROFILE_SCOPE("Coupled material interval");
    for(auto& [id,record]:m_deformables){auto& material=record.simulation;
        if(!material.PhysicalStepActive()||!IsPublished(id))continue;
        if(!SampleDeformableTargets(id,true)){material.error="required material target unavailable during physical interval";continue;}
        const unsigned subdivisions=std::max(1u,(material.settings.substeps+interval.count-1)/interval.count);
        material.AdvancePhysicalInterval(interval.deltaTime,subdivisions,m_physics,GravityForEntity(id),record.targets,true);
    }
    for(auto a=m_deformables.begin();a!=m_deformables.end();++a)for(auto b=std::next(a);b!=m_deformables.end();++b){auto& x=a->second.simulation;auto& y=b->second.simulation;
        if(!x.PhysicalStepActive()||!y.PhysicalStepActive()||!x.error.empty()||!y.error.empty()||(x.sleeping&&y.sleeping))continue;
        double thickness=x.settings.material.thickness+y.settings.material.thickness;
        if(glm::any(glm::lessThan(x.Maximum()+glm::dvec3(thickness),y.Minimum()))||glm::any(glm::lessThan(y.Maximum()+glm::dvec3(thickness),x.Minimum())))continue;
        DeformableInstance::SurfaceContacts(x,y,interval.deltaTime);
        DeformableInstance::SurfaceContacts(y,x,interval.deltaTime);
        ++a->second.revision;++b->second.revision;
    }
}
