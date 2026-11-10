#include "Deformable.h"
#include "SaveArchive.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace {
using V=glm::dvec3;using M=glm::dmat3;
bool finite(V p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
M columns(const std::vector<V>& p,glm::uvec4 n){return M(p[n.y]-p[n.x],p[n.z]-p[n.x],p[n.w]-p[n.x]);}
}
void DeformableInstance::Step(double dt,PhysicsWorld& physics,const GravityField& gravity,const std::vector<DeformableTarget>& targets){
    if(!std::isfinite(dt)||dt<=0)return;
    if(!BeginPhysicalStep())return;
    AdvancePhysicalInterval(dt,settings.substeps,physics,gravity,targets,false);
    EndPhysicalStep(dt,physics);
}
bool DeformableInstance::BeginPhysicalStep(){
    if(integrating)throw std::logic_error("material step already in progress");
    if(!asset||!settings.enabled||!error.empty()){std::fill(forces.begin(),forces.end(),V(0));return false;}
    integrating=true;integrationTargetMoving=false;previous=positions;
    attachmentState.assign(settings.attachments.size(),{});
    attachmentImpulse.resize(settings.attachments.size());attachmentMoment.resize(settings.attachments.size());attachmentPending.assign(settings.attachments.size(),0);
    stats={};
    stats.nodes=positions.size();
    stats.triangles=asset->triangles.size();
    stats.tets=asset->solids.size();
    stats.substeps=0;
    stats.iterations=settings.iterations;
    return true;
}
void DeformableInstance::AdvancePhysicalInterval(double dt,unsigned subdivisions,PhysicsWorld& physics,const GravityField& gravity,const std::vector<DeformableTarget>& targets,bool rigidPoseIsCurrent){
    if(!integrating)throw std::logic_error("material interval without BeginPhysicalStep");
    if(!std::isfinite(dt)||dt<=0||subdivisions<1||subdivisions>16)throw std::invalid_argument("invalid material interval budget");
    if(!error.empty())return;
    // A prescribed world/bone/static frame has no finite dynamic reaction in
    // this solver. Refuse a finite-strength claim rather than silently treating
    // such a target as an infinite-strength connection. Unlimited legacy roots
    // continue through exactly the original prescribed-target path.
    for(size_t i=0;i<settings.attachments.size();++i){const auto& a=settings.attachments[i];
        if(!released[i]&&a.enabled&&(a.forceLimit>0||a.torqueLimit>0)&&i<targets.size()&&targets[i].valid&&!physics.IsDynamicBody(targets[i].body)){
            error="finite attachment strength requires a dynamic body target";return;
        }
    }
    for(size_t n=0;n<inverseMass.size();++n)inverseMass[n]=1/mass[n];
    for(size_t i=0;i<settings.attachments.size();++i)if(!released[i]&&settings.attachments[i].enabled&&i<targets.size()&&targets[i].valid&&!targets[i].body.IsValid()){auto group=asset->groups.find(settings.attachments[i].group);
        if(group!=asset->groups.end())for(auto n:group->second)inverseMass[n]=0;
        }
    bool targetMoving=false;
    for(size_t targetIndex=0;targetIndex<targets.size();++targetIndex){
        if(targetIndex>=settings.attachments.size()||released[targetIndex]||!settings.attachments[targetIndex].enabled)continue;
        const auto& t=targets[targetIndex];if(!t.valid)continue;double d=0;
        for(int k=0;k<4;++k)d+=glm::length(t.current[k]-t.previous[k]);
        targetMoving|=d>1e-8||(t.body.IsValid()&&glm::length(physics.GetLinearVelocity(t.body))+glm::length(physics.GetAngularVelocity(t.body))>1e-5f);
        }
    bool gravityChanged=false;
    for(size_t i=0;i<positions.size();++i){V g=gravity.Sample(glm::vec3(positions[i]));
        gravityChanged|=glm::length(g-lastGravity[i])>1e-5;
        lastGravity[i]=g;
        }
    integrationTargetMoving|=targetMoving;
    if(targetMoving||gravityChanged||settings.material.airDrag>0)Wake();
    if(sleeping){
        bool moving=false;
        if(sleepingContacts.Refresh(physics,Minimum()-V(settings.material.thickness+asset->cellSize),Maximum()+V(settings.material.thickness+asset->cellSize),settings.collisionLayer,settings.collisionMask,moving)||moving)Wake();
        if(sleeping)return;
    }
    integrationSubsteps=subdivisions;integrationDeltaTime=dt;intervalReference=positions;
    stats.substeps+=subdivisions;
    currentRigidContactPose=rigidPoseIsCurrent;
    double h=dt/subdivisions;
    std::array<BodyHandle,64> awakened{};unsigned awakenedCount=0;
    for(unsigned sub=0;sub<subdivisions;++sub){
        std::fill(attachmentImpulse.begin(),attachmentImpulse.end(),V(0));
        std::fill(attachmentMoment.begin(),attachmentMoment.end(),V(0));
        {JUDAS_PROFILE_SCOPE("Deformable force prediction");
            start=positions;
            for(size_t n=0;n<positions.size();++n){if(asset->fracture&&fracture.removed[asset->fracture->nodePart[n]]){velocities[n]=V(0);continue;}V g=gravity.Sample(glm::vec3(positions[n]));
                V acceleration=g+forces[n]/mass[n];
                velocities[n]+=acceleration*h;
                double air=1-std::exp(-settings.material.airDrag*h);
                velocities[n]+=air*(settings.material.airVelocity-velocities[n]);
                velocities[n]*=std::exp(-settings.material.damping*h);
                positions[n]+=velocities[n]*h;
                }}
        if(asset->fracture)fracture.BeginSubstep();
        if(panelBending)panelBending->BeginSubstep();
        std::fill(lambda.begin(),lambda.end(),0);
        std::fill(volumeLambda.begin(),volumeLambda.end(),0);
        double fraction=double(sub+1)/subdivisions;
        {PhysicsQueryFilter filter;
            filter.includeLayers=settings.collisionMask;
            physics.QueryBodiesInAabbInto(glm::vec3(Minimum()-V(settings.material.thickness+asset->cellSize)),glm::vec3(Maximum()+V(settings.material.thickness+asset->cellSize)),rigidCandidates,filter);
            }
        for(unsigned iteration=0;iteration<settings.iterations;++iteration){Internal(h);Cohesive(h);
            RigidContacts(h,fraction,physics);
            SurfaceContacts(*this,*this,h);
            Attach(h,fraction,physics,targets,awakened,awakenedCount);
            }
        if(panelBending)panelBending->Plastic(positions,h);
        for(size_t a=0;a<settings.attachments.size();++a){auto& state=attachmentState[a];
            state.force=attachmentImpulse[a]/h;state.moment=attachmentMoment[a]/h;
            const double force=glm::length(state.force),torque=glm::length(state.moment);
            if(!std::isfinite(force)||!std::isfinite(torque)){error="nonfinite material attachment reaction; instance stopped";return;}
            state.peakForce=std::max(state.peakForce,force);state.peakTorque=std::max(state.peakTorque,torque);
            const auto& setting=settings.attachments[a];
            if((setting.forceLimit>0&&force>setting.forceLimit)||(setting.torqueLimit>0&&torque>setting.torqueLimit))attachmentPending[a]=1;
        }
        for(size_t i=0;i<asset->solids.size();++i){if(asset->fracture&&fracture.removed[asset->fracture->nodePart[asset->solids[i].nodes.x]])continue;double J=glm::determinant(columns(positions,asset->solids[i].nodes)*glm::inverse(plasticRest[i]));if(!std::isfinite(J)||J<=0){error="deformable element inversion; outside supported deformation envelope; instance stopped";return;}}
        if(asset->fracture){const auto& c=*asset->fracture;
            for(unsigned b=0;b<c.bonds.size();++b){const auto& bond=c.bonds[b];
                if(fracture.broken[b]||fracture.removed[bond.a]||fracture.removed[bond.b])continue;
                const auto& part=c.parts[bond.a];M rotation(0);double volume=0;
                // A volume-weighted material frame avoids attributing an
                // entire bent section to one arbitrarily chosen tetrahedron.
                if(c.structural){for(auto index:part.tets){const auto& tet=asset->solids[index];rotation+=columns(positions,tet.nodes)*tet.inverseRest*tet.volume;volume+=tet.volume;}rotation/=volume;}
                else{const auto& tet=asset->solids[part.tets.front()];rotation=columns(positions,tet.nodes)*tet.inverseRest;}
                for(int k=0;k<(c.structural?8:5);++k)rotation=.5*(rotation+glm::transpose(glm::inverse(rotation)));
                bool valid=true;for(int k=0;k<3;++k)valid&=finite(rotation[k]);
                if(!valid||glm::determinant(rotation)<=0){error="invalid cohesive material frame; instance stopped";return;}
                V normal=glm::normalize(rotation*bond.normal),force(0),moment(0),centroid(0);
                if(c.structural){auto first=bond.pairs.front();V origin=(positions[first.x]+positions[first.y])*.5;centroid=origin;
                    for(unsigned pair=0;pair<bond.pairs.size();++pair){auto nodes=bond.pairs[pair];centroid+=((positions[nodes.x]+positions[nodes.y])*.5-origin)*bond.sectionWeights[pair];}}
                for(unsigned pair=0;pair<bond.pairs.size();++pair){V demand=-fracture.multipliers[b*16+pair]/(h*h);force+=demand;
                    if(c.structural){auto nodes=bond.pairs[pair];moment+=glm::cross((positions[nodes.x]+positions[nodes.y])*.5-centroid,demand);}
                    else fracture.Observe(c,b,demand*double(bond.pairs.size()),normal);
                }
                if(c.structural){M inverse=glm::transpose(rotation);fracture.ObserveSection(c,b,inverse*force,inverse*moment);}
                else fracture.Observe(c,b,force,normal);
            }
        }
        Plastic(h);
        for(size_t n=0;n<positions.size();++n){if(!finite(positions[n])){error="nonfinite deformable solve; instance stopped";
                return;
                }velocities[n]=(positions[n]-start[n])/h;
            }
        if(stats.capacityExceeded){error="deformable surface candidate capacity exceeded; instance stopped";
            return;
            }
    }
}
void DeformableInstance::EndPhysicalStep(double dt,PhysicsWorld& physics){
    if(!integrating)throw std::logic_error("material publication without BeginPhysicalStep");
    integrating=false;
    if(!error.empty()){std::fill(forces.begin(),forces.end(),V(0));return;}
    if(sleeping){std::fill(forces.begin(),forces.end(),V(0));return;}
    // Failure is an irreversible change of connection ownership at the fixed
    // step boundary. Both material and body retain their already solved real
    // velocities; release adds no impulse, transform correction or reset.
    for(size_t a=0;a<released.size();++a)if(attachmentPending[a]&&!released[a]){
        released[a]=1;const auto& mount=settings.attachments[a];
        attachmentEvents[mount.group]=mount.forceLimit>0&&attachmentState[a].peakForce>mount.forceLimit?"force":"torque";
    }
    CommitPanelFailures();
    std::fill(forces.begin(),forces.end(),V(0));
    double speed=0;
    for(auto v:velocities)speed=std::max(speed,glm::length(v));
    if(speed<.015&&!integrationTargetMoving&&settings.material.airDrag==0)quietSeconds+=dt;
    else quietSeconds=0;
    if(quietSeconds>1){
        sleeping=true;bool moving=false;
        sleepingContacts.Refresh(physics,Minimum()-V(settings.material.thickness+asset->cellSize),Maximum()+V(settings.material.thickness+asset->cellSize),settings.collisionLayer,settings.collisionMask,moving);
    }
    JUDAS_PROFILE_COUNTER("Deformable nodes",double(stats.nodes),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Deformable contacts",double(stats.contacts),ProfileCounterMode::Sum);
}
