#pragma once
#include "FloorOracle.h"
#include "RuntimeWorld.h"
#include "Narrowphase.h"
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>

namespace temporal_quality {
struct GeometryObservation {
    std::size_t population=0,pairs=0,points=0,uncertain=0,unsupported=0;
    double deepest=0;
};
inline GeometryObservation ObserveGeometry(RuntimeWorld& world,const std::vector<EntityId>& entities) {
    GeometryObservation result;
    auto& physics=world.Physics();std::set<std::pair<unsigned,unsigned>> visited;
    for(auto entity:entities) {
        const auto body=world.RuntimeBody(entity);Shape a;BodyTransform pa;
        if(!physics.GetBodyShape(body,a,pa))continue;
        ++result.population;
        if((a.type!=ShapeType::Box&&a.type!=ShapeType::Sphere)||a.pivotOffset!=glm::vec3(0)||a.asset){++result.unsupported;continue;}
        const auto bound=ShapeAabb(a,pa.position,pa.rotation);PhysicsQueryFilter filter;filter.includeSensors=false;
        const auto neighbours=physics.QueryBodiesInAabb(bound.min,bound.max,filter);
        for(auto other:neighbours) {
            if(other.id==body.id || !visited.insert(std::minmax(body.id,other.id)).second)continue;
            unsigned la=0,lb=0;CategoryMask ma=0,mb=0;
            if(!physics.GetCollisionFilter(body,la,ma)||!physics.GetCollisionFilter(other,lb,mb)||!CollisionPermitted(la,ma,lb,mb))continue;
            Shape b;BodyTransform pb;if(!physics.GetBodyShape(other,b,pb))continue;
            if((b.type!=ShapeType::Box&&b.type!=ShapeType::Sphere)||b.pivotOffset!=glm::vec3(0)||b.asset){++result.unsupported;continue;}
            RigidBody ra,rb;ra.position=pa.position;ra.orientation=pa.rotation;rb.position=pb.position;rb.orientation=pb.rotation;
            const auto manifold=ComputeContacts(a,ra,b,rb,0);result.uncertain+=manifold.uncertain;result.points+=manifold.count;
            for(int i=0;i<manifold.count;++i)result.deepest=std::max(result.deepest,
                manifold.points[i].hasLocalAnchors?-manifold.points[i].signedSeparation:double(manifold.points[i].penetration));
        }
    }
    result.pairs=visited.size();return result;
}

class TemporalObserver {
    std::ofstream csv;
public:
    explicit TemporalObserver(const std::string& output):csv(output+"/temporal-quality.csv") {
        csv<<std::setprecision(17);
        csv<<"step,selected_entities,live_native_bodies,mass_kg,kinetic_linear_J,kinetic_rotational_J,potential_valid,potential_J,mechanical_energy_J,gravity_contract_mismatches,finite,floor_available,floor_supported,fully_inside_floor,centre_inside_floor,min_floor_clearance_m,geometry_sampled,geometry_population,geometry_candidates,geometry_points,geometry_uncertain,geometry_unsupported,deepest_actual_penetration_m\n";
    }
    void Observe(RuntimeWorld& world,unsigned step) {
        auto& physics=world.Physics();
        const int tag=world.Categories().tags.Find("loose_piece");
        const auto entities=tag<0?std::vector<EntityId>{}:world.QueryEntities(CategoryBit(unsigned(tag)));
        // Fixture ID3 identifies the real authored arcade floor. This is a
        // benchmark oracle, never an engine or gameplay special case.
        const auto floor=ReadFloor(physics,world.RuntimeBody(3));
        std::size_t live=0,supported=0,fullyInside=0,centreInside=0,gravityMismatches=0;
        double massTotal=0,linearEnergy=0,rotationalEnergy=0,potential=0;
        double minimumClearance=std::numeric_limits<double>::infinity();
        bool finite=true,potentialValid=!entities.empty();
        GravityEquilibrium reference;bool referenceReady=false;
        for(auto entity:entities) {
            const auto body=world.RuntimeBody(entity);Shape shape;BodyTransform pose;
            if(!physics.GetBodyShape(body,shape,pose)){finite=false;potentialValid=false;continue;}
            ++live;
            const double mass=physics.GetMass(body);
            const glm::dvec3 velocity(physics.GetLinearVelocity(body)),omega(physics.GetAngularVelocity(body));
            const glm::dmat3 inertia(physics.GetInertiaWorld(body));
            massTotal+=mass;linearEnergy+=.5*mass*glm::dot(velocity,velocity);
            rotationalEnergy+=.5*glm::dot(omega,inertia*omega);
            finite &= mass>0 && std::isfinite(mass);
            for(unsigned i=0;i<3;++i)finite &= std::isfinite(pose.position[i]) &&
                std::isfinite(velocity[i]) && std::isfinite(omega[i]);
            for(float q:{pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z})finite &= std::isfinite(q);
            const auto observation=ObserveFloor(floor,shape,pose);
            supported+=observation.valid;centreInside+=observation.valid&&observation.centreInside;
            fullyInside+=observation.valid&&observation.fullyInside;
            if(observation.valid&&observation.fullyInside)minimumClearance=std::min(minimumClearance,observation.clearance);
            GravityEquilibrium equilibrium;
            bool uniform=world.Gravity().Equilibrium(pose.position,equilibrium) && equilibrium.kind==GravityEquilibrium::Kind::Plane;
            const auto sampled=world.SampleEntityGravity(entity,pose.position);
            uniform &= std::isfinite(equilibrium.magnitude) && equilibrium.magnitude>=0 &&
                glm::length(glm::dvec3(sampled)+equilibrium.up*equilibrium.magnitude)<1e-5;
            if(!referenceReady&&uniform){reference=equilibrium;referenceReady=true;}
            uniform &= referenceReady && equilibrium.Equivalent(reference);
            gravityMismatches+=!uniform;potentialValid &= uniform;
            if(uniform) {
                // Reference height only changes an additive constant. Use the
                // actual floor centre to avoid arbitrary world-origin offsets.
                const auto origin=floor.valid?floor.centre:glm::dvec3(0);
                potential+=mass*(equilibrium.Potential(glm::dvec3(pose.position))-equilibrium.Potential(origin));
            }
        }
        finite &= std::isfinite(massTotal)&&std::isfinite(linearEnergy)&&std::isfinite(rotationalEnergy);
        potentialValid &= std::isfinite(potential);
        const bool sampled=step%10==0;
        GeometryObservation geometry;if(sampled)geometry=ObserveGeometry(world,entities);
        csv<<step<<','<<entities.size()<<','<<live<<','<<massTotal<<','<<linearEnergy<<','<<rotationalEnergy<<','<<potentialValid<<',';
        if(potentialValid)csv<<potential;
        csv<<',';if(potentialValid)csv<<linearEnergy+rotationalEnergy+potential;
        csv<<','<<gravityMismatches<<','<<finite<<','<<floor.valid<<','<<supported<<','<<fullyInside<<','<<centreInside<<',';
        if(std::isfinite(minimumClearance))csv<<minimumClearance;
        csv<<','<<sampled<<',';
        if(sampled)csv<<geometry.population<<','<<geometry.pairs<<','<<geometry.points<<','<<geometry.uncertain<<','<<geometry.unsupported<<','<<geometry.deepest;
        else csv<<",,,,,";
        csv<<'\n';csv.flush();
    }
};
} // namespace temporal_quality
