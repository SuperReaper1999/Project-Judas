#include "PhysicsConstraintBatch.h"
#include "PhysicsWorld.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main() {
    PhysicsWorld world;
    if (!world.Init()) return 2;
    const auto floor = world.CreateStaticBox({0.f, -.5f, 0.f}, {3.f, .5f, 3.f}, 0.f, 0.f);
    const auto sphere = world.CreateDynamicSphere({0.f, .495f, 0.f}, .5f, 80.f, 0.f, 0.f);
    if (!floor.IsValid() || !sphere.IsValid()) return 2;
    PhysicsConstraintParticipant participant;
    participant.bodies = {sphere};
    unsigned rows = 0;
    float startY = -1.f, publishedY = -1.f, publishedVy = -1.f, observedY = -1.f,
          observedVy = -1.f, motionGeneratorVy = -1.f, normalTarget = -1.f;
    participant.propose = [&](const auto& batch, auto& proposal) {
        if (batch.context.index != 0)
            return false;
        rows = unsigned(batch.contacts.size());
        if (rows != 1 || !batch.joints.empty())
            return false;
        const auto owner = batch.bodies[0].handle.id == sphere.id ? 0u : 1u;
        const auto& row = batch.contacts.front();
        startY = batch.bodies[owner].center.y;
        normalTarget = row.minimumNormalSpeed;
        proposal.linearImpulses.assign(batch.bodies.size(), glm::dvec3(0));
        proposal.angularImpulses.assign(batch.bodies.size(), glm::dvec3(0));
        proposal.linearImpulses[owner] = -glm::dvec3(row.a == owner ? row.normal : -row.normal);
        proposal.contactImpulses = {1.f};
        return true;
    };
    participant.published = [&](const auto&) {
        publishedY = world.GetTransform(sphere).position.y;
        publishedVy = world.GetLinearVelocity(sphere).y;
    };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const auto& context) {
        if (context.index != 0)
            return;
        observedY = world.GetTransform(sphere).position.y;
        observedVy = world.GetLinearVelocity(sphere).y;
        const auto path = world.GetBodyMotionSegments(sphere);
        if (!path.empty()) motionGeneratorVy = path.front().linearVelocity.y;
    };
    world.Step(1.f / 60.f, &coupling);
    const bool projected = rows == 1 && normalTarget == 0.f && publishedY == startY &&
        publishedVy == 0.f && observedY > publishedY && std::abs(observedVy) < .00001f &&
        motionGeneratorVy > .001f;
    std::cout << std::setprecision(17) << "rows=" << rows << " physical-target=" << normalTarget
              << " start-y=" << startY << " published-y=" << publishedY << " published-vy="
              << publishedVy << " observed-y=" << observedY << " observed-vy=" << observedVy
              << " swept-geometry-vy=" << motionGeneratorVy << " drift=" << observedY - publishedY
              << " pass=" << projected << '\n';
    return projected ? EXIT_SUCCESS : EXIT_FAILURE;
}
