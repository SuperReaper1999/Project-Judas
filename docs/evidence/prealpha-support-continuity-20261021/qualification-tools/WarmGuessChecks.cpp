// Private adversarial initial-guess checks. No new engine API or altered budgets.
// Build/run only after the root agent grants the serialized workload slot.
#include "PhysicsWorld.h"
#include "RigidBody.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

namespace {
constexpr float dt = 1.f / 60;
constexpr float tolerance = 1e-5f; // absolute m/s, single-contact float cancellation check
unsigned checks = 0, failures = 0;
void Check(bool value, const std::string& label) {
    ++checks;
    if (!value) { ++failures; std::cout << "FAIL " << label << '\n'; }
}
struct SphereFloor {
    PhysicsWorld world;
    BodyHandle floor{}, body{};
    explicit SphereFloor(float mass = 16, float restitution = 0) {
        world.Init();
        world.SetSleepingEnabled(false);
        floor = world.CreateStaticBox({0, -.5f, 0}, {20, .5f, 20}, 0, restitution);
        body = world.CreateDynamicSphere({0, .5f, 0}, .5f, mass, 0, restitution);
    }
    void GravityStep() {
        world.ApplyLinearAcceleration(body, {0, -9.81f, 0}, dt);
        world.Step(dt);
    }
    void Warm() { for (unsigned i = 0; i < 120; ++i) GravityStep(); }
};
bool Finite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
void CompareCold(float mass, float upwardSpeed) {
    SphereFloor warm;
    warm.Warm();
    const glm::mat3 inertia = glm::inverse(SolidSphereInverseInertia(mass, .5f));
    Check(warm.world.SetMassDistribution(warm.body, mass, inertia), "mass/inertia mutation accepted");
    const auto pose = warm.world.GetTransform(warm.body);
    SphereFloor cold(mass);
    cold.world.ResetBody(cold.body, pose.position, pose.rotation);
    warm.world.SetLinearVelocity(warm.body, {0, upwardSpeed, 0});
    cold.world.SetLinearVelocity(cold.body, {0, upwardSpeed, 0});
    warm.world.SetAngularVelocity(warm.body, {0, 0, 0});
    for (unsigned step = 0; step < 8; ++step) {
        warm.GravityStep(); cold.GravityStep();
        const auto a = warm.world.GetLinearVelocity(warm.body);
        const auto b = cold.world.GetLinearVelocity(cold.body);
        Check(Finite(a) && Finite(b), "warm/cold velocities finite");
        Check(glm::length(a - b) <= tolerance, "single-contact warm estimate agrees with independently cold reference");
        if (step == 0) std::cout << "cold_reference mass=" << mass << " upward=" << upwardSpeed
                                << " warm_v=" << a.y << " cold_v=" << b.y << '\n';
    }
}
double Kinetic(PhysicsWorld& world, BodyHandle body) {
    const auto v = glm::dvec3(world.GetLinearVelocity(body));
    const auto omega = glm::dvec3(world.GetAngularVelocity(body));
    return .5 * world.GetMass(body) * glm::dot(v, v) +
           .5 * glm::dot(omega, glm::dmat3(world.GetInertiaWorld(body)) * omega);
}
void NewInelasticContact(float incoming) {
    SphereFloor fixture(2);
    fixture.Warm();
    // New pair starts outside contact. Keep gravity applied equally to both;
    // actual TOI and physical response must occur, not a fabricated cache row.
    const auto shot = fixture.world.CreateDynamicSphere({.7501f, .5f, 0}, .25f, 1, 0, 0);
    fixture.world.SetLinearVelocity(shot, {-incoming, 0, 0});
    unsigned events = 0;
    bool struckTarget = false;
    for (unsigned i = 0; i < 60; ++i) {
        fixture.world.ApplyLinearAcceleration(fixture.body, {0, -9.81f, 0}, dt);
        fixture.world.ApplyLinearAcceleration(shot, {0, -9.81f, 0}, dt);
        fixture.world.Step(dt);
        events += unsigned(fixture.world.LastStepStats().impactEvents);
        for (const auto& touch : fixture.world.LastStepTouchEvents())
            struckTarget = struckTarget ||
                ((touch.a.id == fixture.body.id && touch.b.id == shot.id) ||
                 (touch.b.id == fixture.body.id && touch.a.id == shot.id));
        Check(Finite(fixture.world.GetLinearVelocity(fixture.body)) &&
              Finite(fixture.world.GetLinearVelocity(shot)), "new-contact physical velocities finite");
    }
    const double energy = Kinetic(fixture.world, fixture.body) + Kinetic(fixture.world, shot);
    std::cout << "new_inelastic incoming=" << incoming << " events=" << events << " final_kinetic=" << energy << '\n';
    Check(events > 0, "new inelastic fixture exercises real impact events");
    Check(struckTarget, "projectile touches the intended physical target, not merely the floor");
    Check(std::isfinite(energy), "post-impact kinetic energy finite");
    // Energy is measured here, not certified: gravity did external work and
    // this check intentionally does not invent a false zero-work energy gate.
}
void ElasticReturn() {
    PhysicsWorld world;
    world.Init();
    world.SetSleepingEnabled(false);
    world.CreateStaticBox({-2.25f, 0, 0}, {.25f, 5, 5}, 0, 1);
    world.CreateStaticBox({2.25f, 0, 0}, {.25f, 5, 5}, 0, 1);
    const auto ball = world.CreateDynamicSphere({0, 0, 0}, .25f, 1, 0, 1);
    world.SetLinearVelocity(ball, {4, 0, 0});
    unsigned events = 0, reversals = 0;
    float previous = 4;
    const double initial = Kinetic(world, ball);
    const double allowed = 64 * std::numeric_limits<float>::epsilon() * std::max(1., initial);
    for (unsigned i = 0; i < 180; ++i) {
        world.Step(dt);
        events += unsigned(world.LastStepStats().impactEvents);
        const auto velocity = world.GetLinearVelocity(ball);
        reversals += velocity.x * previous < 0;
        previous = velocity.x;
        Check(std::abs(std::abs(velocity.x) - 4) <= tolerance, "isolated elastic return retains restitution");
        Check(Kinetic(world, ball) <= initial + allowed, "isolated static-wall elastic energy budget");
    }
    std::cout << "elastic_return events=" << events << " reversals=" << reversals << '\n';
    Check(events >= 2 && reversals >= 2, "fixture includes departure and later genuine return");
}
} // namespace
int main() {
    std::cout << std::setprecision(17);
    for (float mass : {.125f, 1.f, 16.f, 128.f})
        for (float separatingSpeed : {0.f, 2.f, 50.f}) CompareCold(mass, separatingSpeed);
    NewInelasticContact(.25f);
    NewInelasticContact(50.f);
    ElasticReturn();
    std::cout << "Warm guess checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
