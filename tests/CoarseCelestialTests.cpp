// FTFT7: actual StepPlayedWorld reciprocal gravity across Full/Coarse states.
// The Newton force oracle is scalar double arithmetic, independent of the
// production force helper and participant construction.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <glm/gtc/quaternion.hpp>
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "Window.h"
namespace {
int checks = 0, failures = 0, steps = 0;
void Check(bool ok, const std::string& name) {
    ++checks; failures += !ok;
    std::printf("FTFT7 COARSE CHECK %s %s\n", name.c_str(), ok ? "PASS" : "FAIL");
}
constexpr double g = 6.67430e-11;
constexpr float dt = 1.0f / 60.0f;
glm::dvec3 Force(glm::dvec3 a, double ma, glm::dvec3 b, double mb) {
    const glm::dvec3 r = b - a;
    const double distance = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
    return r * (g * ma * mb / (distance*distance*distance));
}
void Case(const char* name, bool localVehicle, bool bothCoarse, bool initiallyResting,
          bool reversedOrder, const glm::quat& rotation, const glm::vec3& translation,
          const glm::vec3& boost) {
    Scene scene; scene.Settings().name = name;
    EntityId ids[2]{};
    const float authoredMass[2] = {2.0e12f, 1.0e12f};
    for (int index = 0; index < 2; ++index) {
        const int i = reversedOrder ? 1-index : index;
        auto& body = scene.CreateObject(i == 0 ? "A" : "B"); ids[i] = body.id;
        body.transform.position = translation + rotation * glm::vec3(i == 0 ? -5.0f : 5.0f, 0, 0);
        body.body = SceneBodyComponent{}; body.body->motion = SceneBodyMotion::Dynamic;
        body.body->shape = localVehicle && i == 1 ? SceneShape::Box : SceneShape::Sphere;
        body.body->radius = 0.1f; body.body->halfExtents = glm::vec3(0.1f);
        body.body->mass = authoredMass[i]; body.body->friction = 0; body.body->restitution = 0;
        body.body->initialLinearVelocity = boost + rotation * (initiallyResting ? glm::vec3(0) :
            (i == 0 ? glm::vec3(.3f,-.2f,.1f) : glm::vec3(-8.0f,.4f,-.2f)));
        if (localVehicle && i == 1) body.celestial = SceneCelestialComponent{};
        else body.celestial = SceneCelestialComponent{};
    }
    std::string text, error; SaveSceneToString(scene, text); Scene loaded;
    Check(LoadSceneFromString(text, loaded, error), std::string(name)+" serialized_scene");
    RuntimeWorld world;  Window window; window.SetTestInputMode(true);
    if (!world.Build(loaded, nullptr, error) || !world.IsBuilt()) { Check(false,std::string(name)+" construct "+error); return; }
    // Effective represented mass is the physical mass used by each fidelity:
    // live rigid inverse mass; authored mass retained by Coarse records.
    const BodyHandle fullHandle = world.DynamicBodies()[world.FindEntity(ids[1])->slot].Handle();
    const double masses[2] = {authoredMass[0], bothCoarse ? authoredMass[1] : world.Physics().GetMass(fullHandle)};
    Check(world.SetEntityFidelity(ids[0], SimulationFidelity::Coarse, &error), std::string(name)+" demote_A");
    if (bothCoarse) Check(world.SetEntityFidelity(ids[1], SimulationFidelity::Coarse, &error), std::string(name)+" demote_B");
    EntityPhysicalState before[2]; world.GetEntityState(ids[0],before[0]); world.GetEntityState(ids[1],before[1]);
    const glm::dvec3 initialP = masses[0]*glm::dvec3(before[0].linearVelocity)+masses[1]*glm::dvec3(before[1].linearVelocity);
    const double scale = std::max(masses[0]+masses[1], masses[0]*glm::length(glm::dvec3(before[0].linearVelocity))+
                                 masses[1]*glm::length(glm::dvec3(before[1].linearVelocity)));
    const glm::dvec3 firstForce = Force(before[0].position,masses[0],before[1].position,masses[1]);
    double worst = 0;
    for (int step=0; step<12; ++step) {
        StepPlayedWorld(world,window,dt); ++steps;
        EntityPhysicalState state[2]; world.GetEntityState(ids[0],state[0]); world.GetEntityState(ids[1],state[1]);
        const glm::dvec3 p = masses[0]*glm::dvec3(state[0].linearVelocity)+masses[1]*glm::dvec3(state[1].linearVelocity);
        const double residual = glm::length(p-initialP); worst=std::max(worst,residual/scale);
        std::printf("FTFT7 COARSE BUDGET %s step=%d px=%.17g py=%.17g pz=%.17g residual=%.17g normalized=%.17g\n",
                    name,step+1,p.x,p.y,p.z,residual,residual/scale);
        if (step==0) {
            const glm::dvec3 expectedA = glm::dvec3(before[0].linearVelocity)+firstForce*(double(dt)/masses[0]);
            const glm::dvec3 expectedB = glm::dvec3(before[1].linearVelocity)-firstForce*(double(dt)/masses[1]);
            Check(glm::length(glm::dvec3(state[0].linearVelocity)-expectedA)<5e-6,std::string(name)+" coarse_initial_Newton_dv");
            Check(glm::length(glm::dvec3(state[1].linearVelocity)-expectedB)<5e-6,std::string(name)+" other_initial_Newton_dv");
        }
        Check(world.Physics().LastStepContactCount()==0,std::string(name)+" collision_free_step_"+std::to_string(step));
    }
    Check(worst<2e-6,std::string(name)+" closed_system_momentum_budget");
}
void AuthoredGravityAcrossFidelity() {
    Scene scene; scene.Settings().name = "FTFT7 authored gravity fidelity";
    auto& source = scene.CreateObject("Static source"); source.body = SceneBodyComponent{};
    source.body->shape = SceneShape::Sphere; source.body->radius = .1f;
    source.celestial = SceneCelestialComponent{}; source.celestial->gravitationalParameter = 1000.0f;
    source.gravity = SceneGravityComponent{}; source.gravity->kind = SceneGravityKind::Uniform;
    source.gravity->magnitude = 3.0f; source.gravity->regionShape = SceneRegionShape::Box;
    source.gravity->regionHalfExtents = glm::vec3(100);
    auto& body = scene.CreateObject("Ordinary local body"); const EntityId id = body.id;
    body.transform.position = glm::vec3(5,8,0); body.body = SceneBodyComponent{};
    body.body->motion = SceneBodyMotion::Dynamic; body.body->shape = SceneShape::Sphere;
    body.body->radius = .1f; body.body->initialLinearVelocity = glm::vec3(1,2,3);
    RuntimeWorld full, coarse;  Window window;
    window.SetTestInputMode(true); std::string error;
    if (!full.Build(scene,nullptr,error) || !coarse.Build(scene,nullptr,error) ||
        !full.IsBuilt() || !coarse.IsBuilt()) {
        Check(false,"gravity_fidelity construct "+error); return;
    }
    Check(coarse.SetEntityFidelity(id,SimulationFidelity::Coarse,&error),"gravity_fidelity demote");
    StepPlayedWorld(full,window,dt); StepPlayedWorld(coarse,window,dt); steps+=2;
    EntityPhysicalState a,b; full.GetEntityState(id,a); coarse.GetEntityState(id,b);
    const glm::dvec3 expected = glm::dvec3(1,2,3)+glm::dvec3(0,-3,0)*double(dt);
    std::printf("FTFT7 GRAVITY FIDELITY full=(%.9g,%.9g,%.9g) coarse=(%.9g,%.9g,%.9g) expected=(%.17g,%.17g,%.17g)\n",
        a.linearVelocity.x,a.linearVelocity.y,a.linearVelocity.z,b.linearVelocity.x,b.linearVelocity.y,b.linearVelocity.z,
        expected.x,expected.y,expected.z);
    Check(glm::length(glm::dvec3(a.linearVelocity)-expected)<5e-7,"full_ordinary_uses_authored_local_gravity");
    Check(glm::length(glm::dvec3(b.linearVelocity)-expected)<5e-7,"coarse_ordinary_uses_same_authored_local_gravity");
    Check(glm::length(a.position-b.position)<5e-7,"gravity_fidelity_preserves_trajectory");
}
}
int main() {
    const glm::quat identity(1,0,0,0), rotated=glm::angleAxis(.73f,glm::normalize(glm::vec3(2,-1,3)));
    Case("full_coarse",false,false,false,false,identity,glm::vec3(0),glm::vec3(0));
    Case("coarse_local_vehicle",true,false,false,false,identity,glm::vec3(0),glm::vec3(0));
    Case("coarse_coarse",false,true,false,false,identity,glm::vec3(0),glm::vec3(0));
    Case("coarse_at_rest",false,false,true,false,identity,glm::vec3(0),glm::vec3(0));
    Case("both_coarse_at_rest",false,true,true,false,identity,glm::vec3(0),glm::vec3(0));
    Case("rotated_translated",true,false,false,false,rotated,glm::vec3(32,-16,24),glm::vec3(0));
    Case("reversed_creation_boost",true,false,false,true,rotated,glm::vec3(32,-16,24),glm::vec3(3,-2,1));
    AuthoredGravityAcrossFidelity();
    std::printf("FTFT7 coarse celestial: %d checks, %d failures, %d steps\n",checks,failures,steps);
    return failures?1:0;
}
