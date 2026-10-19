// Internal numerical regression: exercise the actual continuation certificate.
// Access is confined to this test; the engine has no test-only public API.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#include "physics/WorldInternal.h"
#undef private

namespace {
struct Observation {
    bool certified = false;
    double maximumDepth = 0;
    double endDepth = 0;
    std::size_t partitions = 0;
};

Observation Observe(bool commonTranslation, bool interiorViolation, bool closing) {
    PhysicsWorld physics;
    physics.Init();
    auto &world = *physics.m_impl;
    world.bodies.resize(2);
    constexpr double duration = 1.0 / 60;
    for (unsigned side = 0; side < 2; ++side) {
        auto &body = world.bodies[side];
        body.alive = body.enabled = body.isDynamic = true;
        body.generation = 0;
        body.shape = Shape::Box(side == 0 && interiorViolation ? glm::vec3(.5f, .1f, .5f)
                                                            : glm::vec3(.5f));
        body.boundingRadius = ShapeBoundingRadius(body.shape);
        body.rigidBody.position = {float(side), 0, 0};
        body.rigidBody.orientation = {1, 0, 0, 0};
        body.rigidBody.inverseMass = 1;
        body.rigidBody.inverseInertiaLocal = glm::mat3(1);
        body.rigidBody.linearVelocity = commonTranslation
            ? glm::vec3(13.173f, -7.19f, 3.27f) : glm::vec3(0);
        if (side == 1 && closing)
            body.rigidBody.linearVelocity.x -= 1;
        body.rigidBody.angularVelocity = interiorViolation
            ? (side == 0 ? glm::vec3(0, 0, 120) : glm::vec3(0))
            : glm::vec3(side == 0 ? 10.f : -10.f, 0, 0);

        RigidMotionSegment segment;
        segment.owner = side;
        segment.begin = 0;
        segment.end = duration;
        segment.position = body.rigidBody.position;
        segment.orientation = body.rigidBody.orientation;
        segment.inverseMass = 1;
        segment.inverseInertiaLocal = glm::mat3(1);
        segment.linearVelocity = body.rigidBody.linearVelocity;
        segment.angularVelocity = segment.driftAngularVelocity = body.rigidBody.angularVelocity;
        const auto end = segment.Evaluate(duration);
        segment.endPosition = end.position;
        segment.endOrientation = end.orientation;
        body.motion.SetRecorded({segment});
    }
    world.aliveSlots = {0, 1};
    world.stepDuration = duration;
    world.impactQueryAnchorTime = 0;
    world.work.active = true;
    Observation result;
    result.certified = world.SupportContinuationSafe({0, 1, 0, 0}, 0, duration);
    result.partitions = world.work.continuationPartitions;

    // Actual native manifold samples independently confirm that the negative
    // fixtures cross the unchanged allowance. These samples are not used to
    // certify the positive path or replace its continuous interval proof.
    for (unsigned sample = 0; sample <= 512; ++sample) {
        const double time = duration * double(sample) / 512;
        const auto manifold = world.PairAt(0, 1, 0, 0, time, 0);
        for (int row = 0; row < manifold.count; ++row) {
            const double depth = -manifold.points[row].signedSeparation;
            result.maximumDepth = std::max(result.maximumDepth, depth);
            if (sample == 512)
                result.endDepth = std::max(result.endDepth, depth);
        }
    }
    return result;
}
} // namespace

int main() {
    unsigned checks = 0, failures = 0;
    auto check = [&](bool passed, const char *name) {
        ++checks;
        if (!passed) {
            ++failures;
            std::cout << "FAIL " << name << '\n';
        }
    };
    for (bool commonTranslation : {false, true}) {
        // Rotation about the separating X normal leaves each exact X extent
        // unchanged. Whole-interval quaternion bounds lose that correlation;
        // the existing bounded subdivision must be allowed to tighten them.
        const auto result = Observe(commonTranslation, false, false);
        std::cout << std::setprecision(17) << "rotation_about_normal translation="
                  << commonTranslation << " certified=" << result.certified
                  << " partitions=" << result.partitions
                  << " maximum_depth_m=" << result.maximumDepth << '\n';
        check(result.certified, "co-moving rotation has a valid bounded continuation certificate");
        check(result.maximumDepth < ContactSolver::kPenetrationSlop,
              "positive fixture stays within the original penetration allowance");
        check(result.partitions > 1 && result.partitions <= 15,
              "certificate genuinely subdivides within the original depth-three budget");
    }
    for (bool interiorViolation : {false, true}) {
        const auto result = Observe(false, interiorViolation, !interiorViolation);
        std::cout << "over_allowance interior=" << interiorViolation
                  << " certified=" << result.certified << " partitions=" << result.partitions
                  << " maximum_depth_m=" << result.maximumDepth
                  << " endpoint_depth_m=" << result.endDepth << '\n';
        check(!result.certified, "over-allowance penetration remains rejected");
        check(result.maximumDepth > ContactSolver::kPenetrationSlop,
              "negative fixture genuinely crosses the unchanged allowance");
        if (interiorViolation)
            check(result.endDepth == 0,
                  "clear endpoint cannot hide an over-allowance interior collision");
    }
    std::cout << "Support continuation: checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
