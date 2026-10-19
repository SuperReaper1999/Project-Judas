// Isolated detecting control. The same source compiles against the untouched
// production headers/archive and the coherent prototype; no oracle changes.
#include <bits/stdc++.h>
#define private public
#include "physics/WorldInternal.h"
#undef private

namespace {
struct Results {
    unsigned samples=0, exact=0, positionMismatch=0, rotationMismatch=0;
    unsigned splitBodies=0, segments=0, events=0;
    double maximumPositionError=0,maximumRotationPointError=0,maximumStoredEndpointRepair=0;
};
void Inspect(PhysicsWorld& world,BodyHandle handle,Results& result){
    const auto exported=world.GetBodyMotionSegments(handle);
    const auto* body=world.m_impl->Get(handle);
    if(!body||exported.empty())throw std::runtime_error("fixture did not produce motion history");
    const auto slot=unsigned(body-world.m_impl->bodies.data());
    result.segments+=unsigned(exported.size());result.splitBodies+=exported.size()>1;
    result.events+=world.LastStepStats().impactEvents;
    for(const auto& segment:exported){
        // Compare to actual sampled publication, not last endPose repairs: the
        // public header explicitly retains repairs separately from drift.
        for(double fraction:{0.,.125,.3333333333333333,.5,.875,1.}){
            const double time=segment.begin+(segment.end-segment.begin)*fraction;
            const auto actual=world.m_impl->PoseAt(slot,time);
            const auto reconstructed=PhysicsWorld::EvaluateBodyMotionSegment(segment,time);
            ++result.samples;
            const bool samePosition=std::memcmp(&actual.position,&reconstructed.position,sizeof(glm::vec3))==0;
            const bool sameRotation=std::memcmp(&actual.orientation,&reconstructed.rotation,sizeof(glm::quat))==0;
            result.exact+=samePosition&&sameRotation;
            result.positionMismatch+=!samePosition;result.rotationMismatch+=!sameRotation;
            result.maximumPositionError=std::max(result.maximumPositionError,
                glm::length(glm::dvec3(actual.position)-glm::dvec3(reconstructed.position)));
            const auto first=ContactRotation(actual.orientation),second=ContactRotation(reconstructed.rotation);
            for(unsigned axis=0;axis<3;++axis)result.maximumRotationPointError=std::max(result.maximumRotationPointError,
                glm::length(first[axis]-second[axis]));
        }
    }
    const auto& last=exported.back();
    const auto actualDrift=world.m_impl->PoseAt(slot,last.end);
    result.maximumStoredEndpointRepair=std::max(result.maximumStoredEndpointRepair,
        glm::length(glm::dvec3(last.endPose.position)-glm::dvec3(actualDrift.position)));
}
}
int main(int argc,char**argv){
    const bool requireExact=argc>1&&std::string(argv[1])=="--require-exact";
    Results free,kinematic,impact;
    for(unsigned trial=0;trial<16;++trial){
        const float shift=float(trial)*.37123f;
        const auto orientation=glm::normalize(glm::quat(.71f,.23f+float(trial)*.001f,-.17f,.31f));
        {
            PhysicsWorld world;world.Init();
            const auto h=world.CreateShape(Shape::Box({.12f,.16f,.09f}),{{.12345f+shift,.23456f,.34567f},orientation},true,3,0,0);
            world.SetLinearVelocity(h,{.173f,-.319f,.237f});world.SetAngularVelocity(h,{.97f,2.13f,-.61f});
            world.Step(1.f/60);Inspect(world,h,free);
        }
        {
            PhysicsWorld world;world.Init();
            const auto h=world.CreateShape(Shape::Box({.12f,.16f,.09f}),{{.12345f+shift,.23456f,.34567f},orientation},false,0,0,0);
            world.SetMotionType(h,BodyMotionType::Kinematic);
            world.SetKinematicVelocity(h,{.173f,-.319f,.237f},{.97f,2.13f,-.61f});
            world.Step(1.f/60);Inspect(world,h,kinematic);
        }
        {
            PhysicsWorld world;world.Init();
            world.CreateStaticBox({1.137f+shift,0,0},{.051f,2,2},.27f,.13f);
            const auto h=world.CreateShape(Shape::Box({.12f,.16f,.09f}),{{.12345f+shift,.23456f,.34567f},orientation},true,3,.27f,.13f);
            world.SetLinearVelocity(h,{97.13f,-.319f,.237f});world.SetAngularVelocity(h,{.97f,2.13f,-.61f});
            world.Step(1.f/60);Inspect(world,h,impact);
        }
    }
    auto print=[](const char* name,const Results&r){
        std::cout<<std::setprecision(17)<<name<<" samples="<<r.samples<<" exact="<<r.exact
                 <<" position_mismatches="<<r.positionMismatch<<" rotation_mismatches="<<r.rotationMismatch
                 <<" split_bodies="<<r.splitBodies<<" segments="<<r.segments<<" events="<<r.events
                 <<" max_position_error_m="<<r.maximumPositionError
                 <<" max_unit_radius_rotation_error_m="<<r.maximumRotationPointError
                 <<" stored_endpoint_repair_m="<<r.maximumStoredEndpointRepair<<'\n';};
    print("free",free);print("kinematic",kinematic);print("impact",impact);
    if(free.samples==0||kinematic.samples==0||impact.splitBodies==0)return 2;
    return requireExact&&(free.exact!=free.samples||kinematic.exact!=kinematic.samples||impact.exact!=impact.samples)?1:0;
}
