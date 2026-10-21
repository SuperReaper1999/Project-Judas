#include "nlohmann/json.hpp"
#include "RowDependency.h"
#include "ContactSolver.h"
#include "RigidBody.h"
#include <cstring>
#include <iostream>
#include <random>

bool bits(glm::vec3 a, glm::vec3 b) { return !std::memcmp(&a, &b, sizeof(a)); }
int main() {
    std::mt19937 random(0x79aa13u);
    std::uniform_real_distribution<float> value(-1.f, 1.f), positive(0.f, 1.f);
    unsigned checks = 0, failures = 0;
    row_dependency::SetStatsEnabled(true);
    row_dependency::ResetStats();
    std::size_t fullVisits = 0, trackedVisits = 0;
    for (unsigned trial = 0; trial < 300; ++trial) {
        const unsigned count = 96 + (trial % 4) * 32;
        std::vector<RigidBody> a(count), b;
        for (unsigned i = 0; i < count; ++i) {
            auto& body = a[i];
            body.position = {float(i), 0, 0};
            body.inverseMass = i % 19 == 0 ? 0 : .5f + positive(random);
            body.inverseInertiaLocal = glm::mat3(body.inverseMass);
            const float angle = value(random);
            body.orientation = glm::angleAxis(angle, glm::normalize(glm::vec3(.2f, .4f, .7f)));
            if ((trial % 3 == 1 && i < 5) || (trial % 3 == 2 && i % 31 == 0)) {
                body.linearVelocity = {value(random), value(random), value(random)};
                body.angularVelocity = {value(random), value(random), value(random)};
            }
        }
        b = a;
        ContactSolver full, tracked;
        row_dependency::Graph dependencies(&tracked);
        row_dependency::Scope dependencyScope(dependencies);
        for (unsigned row = 0; row < count * 3; ++row) {
            unsigned first = random() % count, second = random() % count;
            if (second == first || (a[first].IsStatic() && a[second].IsStatic()))
                second = (first + 1) % count;
            Contact c;
            c.hit = true;
            c.hasLocalAnchors = true;
            c.signedSeparation = 0;
            c.localAnchorA = {value(random) * .3, value(random) * .3, value(random) * .3};
            c.localAnchorB = {value(random) * .3, value(random) * .3, value(random) * .3};
            c.normal = glm::normalize(glm::vec3(value(random), value(random), value(random)));
            c.preciseNormal = c.normal;
            const auto warm = (trial % 7 == 0 && row % 73 == 0) ? positive(random) * .1f : 0.f;
            const float friction = positive(random);
            full.AddContact(a[first], a[second], c, friction, 0, warm);
            tracked.AddContact(b[first], b[second], c, friction, 0, warm);
        }
        full.Prepare(0);
        tracked.Prepare(0);
        const auto& rows = full.Constraints();
        for (unsigned i = 0; i < rows.size(); ++i) {
            const auto& c = rows[i];
            const auto va =
                c.bodyA->linearVelocity + glm::cross(c.bodyA->angularVelocity, c.velocityOffsetA);
            const auto vb =
                c.bodyB->linearVelocity + glm::cross(c.bodyB->angularVelocity, c.velocityOffsetB);
            const float bias = trial % 5 == 0 ? std::min(glm::dot(va - vb, c.normal), 0.f) : 0;
            full.SetInelasticVelocityTarget(i, bias);
            tracked.SetInelasticVelocityTarget(i, bias);
        }
        const int iterations = 1 + trial % 12;
        full.SolveVelocities(iterations);
        std::vector<RigidBody*> activated;
        tracked.SolveTrackedVelocities(iterations, activated);
        trackedVisits += rows.size() * iterations;
        fullVisits += rows.size() * iterations;
        bool equal = true;
        for (unsigned i = 0; i < count; ++i)
            equal &= bits(a[i].linearVelocity, b[i].linearVelocity) &&
                     bits(a[i].angularVelocity, b[i].angularVelocity);
        for (unsigned i = 0; i < rows.size(); ++i) {
            const auto& x = full.Constraints()[i];
            const auto& y = tracked.Constraints()[i];
            equal &= x.normalImpulse == y.normalImpulse && x.tangentImpulse == y.tangentImpulse;
        }
        ++checks;
        if (!equal) {
            ++failures;
            std::cerr << "mismatch trial=" << trial << '\n';
        }
    }
    std::cout << nlohmann::json{{"checks", checks},
                                {"failures", failures},
                                {"full_visits", fullVisits},
                                {"tracked_visits", trackedVisits},
                                {"actually_evaluated", row_dependency::stats.evaluated},
                                {"exact_skips", row_dependency::stats.skipped},
                                {"accumulator_only_changes", row_dependency::stats.accumulatorOnlyChanges}}
                     .dump(2)
              << '\n';
    return failures ? 1 : 0;
}
