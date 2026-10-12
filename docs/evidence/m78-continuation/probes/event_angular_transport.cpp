#include "PhysicsWorld.h"
#include "RigidMotion.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

// Observer only. This is an isolated two-body impact, independent of the M77
// controller and its authored gains, input, geometry, or success thresholds.
namespace {
glm::dmat3 BoxInertia(double mass, const glm::dvec3& half) {
    glm::dmat3 result(0);
    result[0][0] = mass * (half.y * half.y + half.z * half.z) / 3;
    result[1][1] = mass * (half.x * half.x + half.z * half.z) / 3;
    result[2][2] = mass * (half.x * half.x + half.y * half.y) / 3;
    return result;
}
glm::dvec3 Momentum(const PhysicsWorld& world, BodyHandle body, double mass,
                     const glm::dmat3& localInertia) {
    const auto transform = world.GetTransform(body);
    const auto rotation = glm::mat3_cast(glm::normalize(glm::dquat(transform.rotation)));
    const auto velocity = glm::dvec3(world.GetLinearVelocity(body));
    const auto omega = glm::dvec3(world.GetAngularVelocity(body));
    return glm::cross(glm::dvec3(transform.position), mass * velocity)
        + rotation * localInertia * glm::transpose(rotation) * omega;
}
void Case(const char* name, bool collision, bool sphere, glm::vec3 omega) {
    PhysicsWorld world;
    world.Init();
    const glm::vec3 half(.2f, .5f, .9f);
    const double massA = 3, massB = 5;
    const auto a = sphere
        ? world.CreateDynamicSphere({-.5f,.1f,0}, .2f, float(massA), 0, 0)
        : world.CreateDynamicBox({-.5f,.1f,0}, half, float(massA), 0, 0);
    const auto b = world.CreateDynamicSphere({collision ? .2f : 10.f,0,0},
        .2f, float(massB), 0, 0);
    world.SetLinearVelocity(a, {30,0,0});
    world.SetAngularVelocity(a, omega);
    const glm::dmat3 inertiaA = sphere
        ? glm::dmat3((2.0/5) * massA * .2 * .2)
        : BoxInertia(massA, glm::dvec3(half));
    const glm::dmat3 inertiaB((2.0/5) * massB * .2 * .2);
    const auto totalL = [&] { return Momentum(world,a,massA,inertiaA)
        + Momentum(world,b,massB,inertiaB); };
    const auto totalP = [&] { return massA*glm::dvec3(world.GetLinearVelocity(a))
        + massB*glm::dvec3(world.GetLinearVelocity(b)); };
    const auto initialL = totalL(), initialP = totalP();
    world.Step(1.f/60);
    const auto finalL = totalL(), finalP = totalP();
    const auto& stats = world.LastStepStats();
    double first = 1;
    const auto segments = world.GetBodyMotionSegments(a);
    for (const auto& segment : segments) if (segment.begin > 0)
        first = std::min(first, segment.begin);
    std::printf("%s impacts=%zu segments=%zu first_event=%.17g dL=(%.9g,%.9g,%.9g) abs_L=%.9g rel_L=%.9g abs_P=%.9g\n",
        name, stats.impactEvents, segments.size(), first,
        (finalL-initialL).x,(finalL-initialL).y,(finalL-initialL).z,
        glm::length(finalL-initialL),glm::length(finalL-initialL)/glm::length(initialL),
        glm::length(finalP-initialP));
}
}
int main() {
    Case("free_anisotropic",false,false,{1,2,3});
    Case("impact_no_spin",true,false,{0,0,0});
    Case("impact_anisotropic",true,false,{1,2,3});
    Case("impact_sphere",true,true,{1,2,3});

    // A geometry/history observation must keep the original left-limit rule,
    // while impulse processing needs the already-applied right-limit state.
    // This distinguishes that separate latent boundary hazard from the current
    // PoseAt dynamic path, which does not copy sampled angular velocity at all.
    RigidBody body;
    body.inverseMass = 1;
    body.inverseInertiaLocal = glm::inverse(glm::mat3(BoxInertia(1,{.2,.5,.9})));
    body.angularVelocity = {1,2,3};
    RigidMotion motion;
    motion.Begin(1,body,1.f/60);
    const double event = 1.0/120;
    body = motion.Evaluate(event);
    body.angularVelocity += glm::vec3(.7f,-.2f,.3f);
    motion.ChangeVelocity(body,event);
    const auto left = motion.Evaluate(event);
    const auto right = motion.Segments().back().Evaluate(event);
    std::printf("same_time_geometry_left_vs_physical_right omega_difference=%.9g\n",
        glm::length(left.angularVelocity-right.angularVelocity));
    const double finish=1.f/60;
    body=motion.Segments().back().Evaluate(finish);
    body.angularVelocity+=glm::vec3(.1f,.2f,.3f);
    motion.ChangeVelocity(body,finish);
    std::printf("end_time_geometry_left_vs_physical_right omega_difference=%.9g\n",
        glm::length(motion.Evaluate(finish).angularVelocity-
            motion.Segments().back().Evaluate(finish).angularVelocity));
}
