#pragma once

#include "BasinFixture.h"
#include "PhysicsConstraintBatch.h"
#include "PhysicsWorld.h"

#include <functional>

namespace judas::volume_grid::basin_probe {

struct Forecast {
    glm::vec3 center;
    glm::vec3 velocity;
    GridVec translation;
};

struct Trial {
    PhysicsConstraintBatch::Context context;
    PhysicsConstraintBatch::Body body;
    BoxMotion start;
    GridVec gravity;
    std::function<Forecast(glm::dvec3 worldImpulse)> predict;
};

struct Proposal {
    glm::dvec3 worldImpulse{0};
    Forecast forecast{};
    // Called after actual native pose/velocity/impulse match the prediction.
    // Precompute every material change before proposing the rigid impulse.
    std::function<void()> commit;
};

struct Warmup {
    BoxMotion body;
    double dt;
    unsigned step;
};

struct NativeResult {
    BodyTransform finalPose;
    glm::vec3 finalVelocity{0};
    glm::vec3 finalAngularVelocity{0};
    std::string error;
    unsigned warmupSteps = 0, proposals = 0, published = 0, accepted = 0;
    unsigned completedRigidSteps = 0;
    double maxNativeImpulseError = 0.;
    double chartInitialError = 0.;
    bool placeholderDestroyed = false;
};

using WarmupStep = std::function<bool(const Warmup&, std::string&)>;
using BuildProposal = std::function<bool(const Trial&, Proposal&, std::string&)>;
using Observe = std::function<void(unsigned, const BodyTransform&, glm::vec3)>;

// Advances the exact authored body and wall shapes through real MK2. A
// proposal may return false only as a terminal failure. The callback owns
// material state and must not mutate it until its commit function is called.
NativeResult RunMk2(const Fixture& fixture, unsigned warmupSteps, unsigned rigidSteps,
                    const WarmupStep& warmup, const BuildProposal& stage, const Observe& observe = {});

} // namespace judas::volume_grid::basin_probe
