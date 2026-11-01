#include <stdexcept>
#include "ProductionFluidCoupling.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <glm/gtc/matrix_inverse.hpp>
#include "FluidHydrostatics.h"
#include "FluidVolumeDrag.h"
#include "PlayerController.h"
#include "RuntimeWorld.h"
namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
bool SamePose(const BodyTransform& a, const BodyTransform& b) { return a.position==b.position && a.rotation==b.rotation; }
bool Inside(const glm::vec3& point, const BodyTransform& pose, const std::vector<SceneFluidCavity>& cavities) {
    const glm::vec3 local=glm::inverse(pose.rotation)*(point-pose.position);
    for(const auto& c:cavities) if(glm::all(glm::lessThanEqual(glm::abs(local-c.localCenter),c.halfExtents))) return true;
    return false;
}
BodyTransform EvaluateBodyPose(const std::vector<PhysicsWorld::BodyMotionSegment>& history,
                               double duration,float alpha) {
    const double time=static_cast<double>(alpha)*duration;
    for(auto it=history.rbegin();it!=history.rend();++it)
        if(time>=it->begin && time<=it->end)
            return time==it->end ? it->endPose : PhysicsWorld::EvaluateBodyMotionSegment(*it,time);
    return time<history.front().begin ? history.front().start : history.back().endPose;
}
}
struct ProductionFluidCoupling::Impl {
    struct Body {
        BodyHandle handle;
        FluidVolumeQuadrature volume;
        std::vector<SceneFluidCavity> cavities;
        FluidHydrostaticField exterior;
    };
    unsigned entityVersion=std::numeric_limits<unsigned>::max();
    double pending=0;
    std::size_t pendingFrames=0;
    bool first=true;
    std::uint64_t fieldRevision=0;
    FluidHydrostaticField field;
    FluidVolumeQuadrature playerVolume=MakeBoxFluidVolume(glm::vec3(PlayerController::CapsuleRadius(),
        PlayerController::CapsuleHalfHeight()+PlayerController::CapsuleRadius(),PlayerController::CapsuleRadius()));
    std::vector<Body> bodies;
    std::unordered_map<unsigned,BodyTransform> previousFluidPoses,stepStartPoses;
    std::unordered_map<unsigned,std::vector<PhysicsWorld::BodyMotionSegment>> pendingMotion;
    ProductionFluidMeasurements measured;
    // Rebuilt once per material revision. Far-away bodies must not rescan
    // every water particle merely to discover they own no cut cells.
    std::unordered_map<unsigned,std::vector<std::size_t>> boundaryIndices;
    void CollectBoundaries(const FluidWorld& fluid) {
        boundaryIndices.clear();
        const auto& markers=fluid.DisplacementMarkers();
        for(std::size_t i=0;i<markers.size();++i)
            if(markers[i].owner.IsValid())boundaryIndices[markers[i].owner.id].push_back(i);
    }
    void RetainBoundary(FluidHydrostaticField& target, const Body& body,
                        const BodyTransform& pose, const FluidWorld& fluid,
                        const std::vector<bool>* excluded=nullptr) {
        const auto found=boundaryIndices.find(body.handle.id);
        if(found==boundaryIndices.end())return;
        const auto& markers=fluid.DisplacementMarkers();
        std::vector<FluidDisplacementMarker> cells;
        cells.reserve(found->second.size());
        for(const auto i:found->second) {
            if(excluded && (*excluded)[i])continue;
            if(markers[i].owner.world==body.handle.world)cells.push_back(markers[i]);
        }
        if(!cells.empty())target.RetainDisplacementCells(body.handle,body.volume,pose,cells);
    }
    void Rebuild(RuntimeWorld& world) {
        if(entityVersion==world.EntityVersion()) return;
        bodies.clear();
        for(const EntityRecord& e:world.Entities()) {
            if(e.lifecycle!=EntityLifecycle::Active || e.fidelity!=SimulationFidelity::Full || !e.definition.body) continue;
            // Explicit conserved-liquid ownership excludes legacy PBF loading.
            if(e.definition.liquidInteraction||e.definition.liquidContainer)continue;
            const auto& definition=*e.definition.body;
            const auto handle=world.DynamicBodies()[e.slot].Handle();
            if(!world.Physics().IsDynamicBody(handle)) continue;
            FluidVolumeQuadrature volume;
            if(definition.shape==SceneShape::Box) volume=MakeBoxFluidVolume(definition.halfExtents);
            else if(definition.shape==SceneShape::Sphere) volume=MakeSphereFluidVolume(definition.radius);
            else if(definition.shape==SceneShape::Compound){bool unsupported=false;for(auto& c:definition.compoundBoxes)unsupported|=c.type!=ShapeType::Box;if(unsupported&&world.Fluid().Particles().empty())continue;volume=MakeCompoundFluidVolume(definition.compoundBoxes);}
            else continue;
            bodies.push_back({handle,std::move(volume),definition.fluidCavities,{}});
        }
        entityVersion=world.EntityVersion();
    }
};
ProductionFluidCoupling::ProductionFluidCoupling():m_impl(std::make_unique<Impl>()) {}
ProductionFluidCoupling::~ProductionFluidCoupling()=default;
void ProductionFluidCoupling::Reset() { m_impl=std::make_unique<Impl>(); }
const ProductionFluidMeasurements& ProductionFluidCoupling::Measurements() const { return m_impl->measured; }
void ProductionFluidCoupling::PrepareRigidStep(RuntimeWorld& world,float dt) {
    auto& s=*m_impl;const auto started=Clock::now();auto& p=world.Physics();auto& fluid=world.Fluid();
    s.Rebuild(world);
    // External Clear/emission invalidates the field as well as its owned
    // boundary snapshot. Keep the ordinary first-step publication ordering.
    if(!s.first && s.fieldRevision!=fluid.Revision()) {
        s.field.Build(fluid.Particles(),world.FluidSettingsUsed().restDensity);
        s.CollectBoundaries(fluid);
        for(const auto& body:s.bodies) {
            const auto pose=s.previousFluidPoses.find(body.handle.id);
            if(pose!=s.previousFluidPoses.end())s.RetainBoundary(s.field,body,pose->second,fluid);
        }
        s.fieldRevision=fluid.Revision();
    }
    s.measured.executed=false;s.measured.particleMilliseconds=0;s.measured.exteriorReactionNotApplied=glm::vec3(0);s.measured.containedStaticSupportImpulse=glm::vec3(0);
    s.measured.contactBatches=0;s.measured.contactIterations=0;s.measured.normalMatrixProducts=0;
    s.measured.normalAccelerations=0;s.measured.contactIterationCaps=0;s.measured.normalBreakdowns=0;
    s.measured.normalAccelerationRejections=0;s.measured.normalAccelerationBoundaryDeclines=0;
    s.measured.maximumContactRelativeResidual=0;
    s.measured.motionFrames=0;s.measured.motionSegments=0;s.measured.motionStorageBytes=0;s.measured.motionResets=0;
    s.measured.bodies.assign(s.bodies.size(),{});
    for(std::size_t i=0;i<s.bodies.size();++i)s.measured.bodies[i].body=s.bodies[i].handle;
    s.stepStartPoses.clear();
    for(const auto handle:p.AliveBodies()) s.stepStartPoses.emplace(handle.id,p.GetTransform(handle));
    // Keys include the complete slot generation. Never retain old geometry or
    // a path after destruction/reuse, or pointers into PhysicsWorld storage.
    for(auto it=s.pendingMotion.begin();it!=s.pendingMotion.end();) {
        if(s.stepStartPoses.find(it->first)==s.stepStartPoses.end())it=s.pendingMotion.erase(it);
        else ++it;
    }
    for(auto it=s.previousFluidPoses.begin();it!=s.previousFluidPoses.end();) {
        if(s.stepStartPoses.find(it->first)==s.stepStartPoses.end())it=s.previousFluidPoses.erase(it);
        else ++it;
    }
    for(std::size_t i=0;i<s.bodies.size();++i) {
        auto& body=s.bodies[i];if(p.IsBodySensor(body.handle)||!p.IsBodyEnabled(body.handle))continue;const auto pose=p.GetTransform(body.handle);const FluidHydrostaticField* field=&s.field;
        if(!body.cavities.empty() && !s.first) {
            std::vector<bool> excluded;excluded.reserve(fluid.Particles().size());
            for(const auto& particle:fluid.Particles()) {
                // The held particle field belongs to its latest resolved liquid
                // endpoint. Test cavity exclusion at that same historical pose,
                // not at a newer rigid pose, to avoid duplicate exterior load.
                const auto found=s.previousFluidPoses.find(body.handle.id);
                const BodyTransform fluidPose=found==s.previousFluidPoses.end()?pose:found->second;
                const bool contained=Inside(particle.position,fluidPose,body.cavities);excluded.push_back(contained);
                if(contained)s.measured.bodies[i].containedMass+=particle.mass;
            }
            body.exterior.Build(fluid.Particles(),world.FluidSettingsUsed().restDensity,&excluded);
            const auto found=s.previousFluidPoses.find(body.handle.id);
            s.RetainBoundary(body.exterior,body,found==s.previousFluidPoses.end()?pose:found->second,fluid,&excluded);
            field=&body.exterior;
        }
        const auto hydro=EvaluateFluidHydrostatics(*field,body.volume,pose,world.Gravity(),
            FluidVolumeGeometry::DisplacedSolid,body.handle);
        auto& diagnostic=s.measured.bodies[i];diagnostic.fraction=hydro.fraction;diagnostic.displacedVolume=hydro.submergedVolume;
        diagnostic.buoyancyForce=hydro.buoyancyForce;
        p.ApplyForce(body.handle,hydro.buoyancyForce);p.ApplyTorque(body.handle,hydro.buoyancyTorque);
        if(hydro.fraction>0) {
            // Resolve the existing authored drag over the wetted body volume.
            // A single centroid damper has an angular nullspace even for a
            // fully immersed spinning body. The volume resistance includes its
            // physical lever arms; it introduces no independent damping knob.
            const double mass=p.GetMass(body.handle);
            const glm::dmat3 inertia(p.GetInertiaWorld(body.handle));
            const auto velocity=glm::dvec3(p.GetLinearVelocity(body.handle))+glm::dvec3(hydro.buoyancyForce)*(dt/mass);
            const auto omega=glm::dvec3(p.GetAngularVelocity(body.handle))+glm::inverse(inertia)*glm::dvec3(hydro.buoyancyTorque)*double(dt);
            const auto drag=EvaluateFluidVolumeDrag(hydro,mass,inertia,velocity,omega,
                world.FluidSettingsUsed().hydrostaticDragRate,dt,s.pending+dt);
            diagnostic.dragForce=glm::vec3(drag.linear/double(dt));
            p.ApplyForce(body.handle,diagnostic.dragForce);
            p.ApplyTorque(body.handle,glm::vec3(drag.angular/double(dt)));
        }
    }
    s.measured.totalMilliseconds=Milliseconds(started);
    s.measured.hydrostaticMilliseconds=s.measured.totalMilliseconds;s.measured.playerMilliseconds=0;
}
void ProductionFluidCoupling::AdvanceResolvedLiquid(RuntimeWorld& world,float dt) {
    auto& s=*m_impl;const auto started=Clock::now();auto& p=world.Physics();auto& fluid=world.Fluid();
    const double offset=s.pending;
    const auto alive=p.AliveBodies();
    std::unordered_map<unsigned,bool> actualMotionAvailable;
    actualMotionAvailable.reserve(alive.size());
    for(const auto handle:alive) {
        const auto now=p.GetTransform(handle);
        const auto found=s.stepStartPoses.find(handle.id);
        const BodyTransform begin=found==s.stepStartPoses.end()?now:found->second;
        auto& history=s.pendingMotion[handle.id];
        if(!history.empty() && !SamePose(history.back().endPose,begin)) {
            // ResetBody/door writes are discontinuities, not continuous wall
            // motion. Discard this body's cached trajectory rather than sweep
            // a segment through the teleport; unrelated bodies retain theirs.
            history.clear();++s.measured.motionResets;
        }
        if(history.empty() && offset>0)history.push_back({0,offset,begin,begin,{}, {},false});
        auto resolved=p.GetBodyMotionSegments(handle);
        actualMotionAvailable.emplace(handle.id,!resolved.empty());
        if(resolved.empty())resolved.push_back({0,double(dt),begin,now,{}, {},false});
        for(auto segment:resolved) {
            segment.begin+=offset;segment.end+=offset;history.push_back(segment);
        }
    }
    s.pending+=dt;++s.pendingFrames;
    s.measured.motionFrames=s.pendingFrames;
    for(const auto& entry:s.pendingMotion) {
        s.measured.motionSegments+=entry.second.size();
        s.measured.motionStorageBytes+=entry.second.capacity()*sizeof(PhysicsWorld::BodyMotionSegment);
    }
    const double period=1.0/world.FluidSettingsUsed().updateRateHz;
    if(s.first || s.pending+std::numeric_limits<float>::epsilon()*period>=period) {
        const double motionDuration=s.pending;
        const float fluidDt=static_cast<float>(motionDuration);s.pending=0;s.first=false;
        std::vector<FluidBoxCollider> boxes;std::vector<FluidSphereCollider> spheres;std::vector<FluidTerrainCollider> terrains;
        std::unordered_map<unsigned,BodyTransform> nextPoses, startPoses;
        for(const auto handle:alive) {
            if(p.IsBodySensor(handle)||!p.IsBodyEnabled(handle))continue;
            Shape shape;BodyTransform now;if(!p.GetBodyShape(handle,shape,now))continue;
            const auto& history=s.pendingMotion.at(handle.id);
            // Only the engine's genuine continuous ledger selects anchored
            // path sampling. Static/ResetBody geometry has no such ledger:
            // retain its original constant-pose mix/slerp arithmetic and do
            // not turn endpoint bookkeeping into a new wall trajectory.
            const bool hasMotion=actualMotionAvailable.at(handle.id);
            const BodyTransform begin=hasMotion ? EvaluateBodyPose(history,motionDuration,0) : now;
            const BodyTransform end=hasMotion ? EvaluateBodyPose(history,motionDuration,1) : now;
            nextPoses.emplace(handle.id,end);startPoses.emplace(handle.id,begin);
            if(shape.type==ShapeType::Sphere) {
                FluidSphereCollider collider{handle,begin,end,shape.radius};
                if(hasMotion) {collider.resolvedMotion=history;collider.motionDuration=motionDuration;}
                spheres.push_back(std::move(collider));
            } else if(shape.type==ShapeType::Box) {
                FluidBoxCollider collider{handle,begin,end,shape.halfExtents};
                if(hasMotion) {collider.resolvedMotion=history;collider.motionDuration=motionDuration;}
                boxes.push_back(std::move(collider));
            } else if(shape.type==ShapeType::CompoundBoxes)for(const auto& b:shape.boxes) {
                if(b.type!=ShapeType::Box)throw std::invalid_argument("particle-fluid coupling supports primitive box children only");
                FluidBoxCollider collider{handle,{begin.position+begin.rotation*b.localCenter,begin.rotation*b.rotation},
                    {end.position+end.rotation*b.localCenter,end.rotation*b.rotation},b.halfExtents};
                if(hasMotion) {collider.resolvedMotion=history;collider.motionDuration=motionDuration;}
                collider.localCenter=b.localCenter;collider.localRotation=b.rotation;
                boxes.push_back(std::move(collider));
            } else if(shape.type==ShapeType::Terrain) {
                FluidTerrainCollider collider{handle,begin,end,shape.terrain.get()};
                if(hasMotion) {collider.resolvedMotion=history;collider.motionDuration=motionDuration;}
                terrains.push_back(std::move(collider));
            }
        }
        std::vector<std::vector<bool>> insideBefore(s.bodies.size());
        for(std::size_t i=0;i<s.bodies.size();++i)if(!s.bodies[i].cavities.empty()) {
            const auto pose=startPoses.at(s.bodies[i].handle.id);auto& mask=insideBefore[i];mask.reserve(fluid.Particles().size());
            for(const auto& particle:fluid.Particles())mask.push_back(Inside(particle.position,pose,s.bodies[i].cavities));
        }
        std::vector<FluidContactImpulse> reactions;
        const auto particleStart=Clock::now();
        const FluidVelocityBatchResponse response=[&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts,std::vector<glm::vec3>& impulses) {
            std::vector<PhysicsWorld::ContactParticle> points;points.reserve(particles.size());
            for(const auto& particle:particles)points.push_back({particle.position,particle.velocity,particle.mass});
            std::vector<PhysicsWorld::ParticleBoundaryContact> rows;rows.reserve(contacts.size());
            std::vector<std::size_t> owners;owners.reserve(contacts.size());
            for(const auto& c:contacts) {
                std::size_t owner=s.bodies.size();
                for(std::size_t i=0;i<s.bodies.size();++i) {
                    const auto& body=s.bodies[i];if(body.handle.id!=c.owner.id||body.cavities.empty())continue;
                    const BodyTransform pose=EvaluateBodyPose(s.pendingMotion.at(body.handle.id),
                        motionDuration,c.stepFraction);
                    if(insideBefore[i][c.particleIndex] || Inside(c.particlePosition,pose,body.cavities))owner=i;
                    break;
                }
                owners.push_back(owner);
                rows.push_back({c.particleIndex,c.owner,c.point,c.normal,c.wallVelocity,owner<s.bodies.size()});
            }
            glm::vec3 supportImpulse(0);
            // This post-rigid velocity exchange reuses already cached support
            // with the approved next-interval gap-closing duration. It does
            // not replay rigid gravity, impact timing, or pose integration.
            p.SolveParticleContacts(points,rows,impulses,&supportImpulse,dt);
            const auto contactStats=p.GetParticleContactStats();
            ++s.measured.contactBatches;s.measured.contactIterations+=contactStats.iterations;
            s.measured.normalMatrixProducts+=contactStats.normalMatrixProducts;
            s.measured.normalAccelerations+=contactStats.normalAccelerations;
            s.measured.contactIterationCaps+=contactStats.iterationCapReached?1:0;
            s.measured.normalBreakdowns+=contactStats.normalAccelerationBreakdowns;
            s.measured.normalAccelerationRejections+=contactStats.normalAccelerationRejections;
            s.measured.normalAccelerationBoundaryDeclines+=contactStats.normalAccelerationBoundaryDeclines;
            s.measured.maximumContactRelativeResidual=std::max(s.measured.maximumContactRelativeResidual,
                contactStats.maximumClosingSpeed/contactStats.speedScale);
            s.measured.containedStaticSupportImpulse+=supportImpulse;
            for(std::size_t i=0;i<particles.size();++i)particles[i].velocity=points[i].velocity;
            for(std::size_t row=0;row<rows.size();++row)if(owners[row]<s.bodies.size())
                s.measured.bodies[owners[row]].containedImpulse-=impulses[row];
        };
        fluid.Step(fluidDt,world.Gravity(),boxes,spheres,terrains,&reactions,response);

        s.measured.particleMilliseconds=Milliseconds(particleStart);
        s.measured.executed=true;++s.measured.executedSteps;s.measured.lastFluidDeltaTime=fluidDt;
        // Reactions were applied exactly once inside the velocity contact.
        // The remaining dynamic-boundary exchange is deliberately one-way.
        for(const auto& reaction:reactions)if(p.IsDynamicBody(reaction.owner))
            s.measured.exteriorReactionNotApplied+=reaction.impulse;
        for(const auto& body:s.measured.bodies)s.measured.exteriorReactionNotApplied-=body.containedImpulse;
        s.previousFluidPoses.swap(nextPoses);
        s.field.Build(fluid.Particles(),world.FluidSettingsUsed().restDensity);
        s.CollectBoundaries(fluid);
        for(const auto& body:s.bodies) {
            const auto pose=s.previousFluidPoses.find(body.handle.id);
            if(pose!=s.previousFluidPoses.end())s.RetainBoundary(s.field,body,pose->second,fluid);
        }
        s.fieldRevision=fluid.Revision();
        // History is bounded by the configured cadence, not accumulated over
        // gameplay time: default 30 Hz contains at most two 60 Hz rigid frames.
        // Slower authored rates retain their required finite number of frames.
        for(auto& entry:s.pendingMotion)entry.second.clear();
        s.pendingFrames=0;
    }
    // Report cavity mass at the particle field's actual timestamp, including
    // updates performed after the rigid solve. These are observations, not a
    // second contained-liquid force or impulse path.
    for(std::size_t i=0;i<s.bodies.size();++i) {
        const auto& body=s.bodies[i];if(body.cavities.empty())continue;
        s.measured.bodies[i].containedMass=0;
        const auto found=s.previousFluidPoses.find(body.handle.id);
        const BodyTransform pose=found==s.previousFluidPoses.end()?p.GetTransform(body.handle):found->second;
        for(const auto& particle:fluid.Particles())if(Inside(particle.position,pose,body.cavities))
            s.measured.bodies[i].containedMass+=particle.mass;
    }
    s.measured.totalMilliseconds+=Milliseconds(started);
}
PlayerFluidSample ProductionFluidCoupling::SamplePlayer(const RuntimeWorld& world,const PlayerController& player) const {
    auto& s=*m_impl;const auto started=Clock::now();
    const auto sample=EvaluateFluidHydrostatics(s.field,s.playerVolume,
        {player.GetPosition(),player.GetOrientation()},world.Gravity(),
        FluidVolumeGeometry::NonDisplacingVolume);
    const PlayerFluidSample result{sample.fraction,sample.density,
        sample.fluidVelocity+sample.fluidAcceleration*static_cast<float>(s.pending),sample.fluidAcceleration};
    // Passive accounting includes the ordinary controller's field query too.
    // It changes no particle, body, controller or force state.
    s.measured.playerMilliseconds=Milliseconds(started);s.measured.totalMilliseconds+=s.measured.playerMilliseconds;
    return result;
}

FluidFieldSample ProductionFluidCoupling::SampleField(glm::vec3 point,glm::vec3 up,float halfHeight,float radius,glm::vec3 tangent) const {
    return m_impl->field.Query(point,up,halfHeight,radius,tangent,nullptr,true);
}
