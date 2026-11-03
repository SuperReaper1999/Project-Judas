#pragma once

#include "ContactSolver.h"
#include "RigidBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

// A frictionless particle against prescribed boundaries is a 3D minimum-change
// projection. Attempt it only if the ordinary bounded point solve has not
// satisfied the normal rows. There is no finite rigid response to solve.
struct PrescribedParticleProjection {
    static constexpr std::size_t kMaxRows = 8;
    glm::vec3 velocity{0};
    std::array<float, kMaxRows> normalImpulses{};
};

inline bool ProjectPrescribedParticleVelocity(const ContactConstraint *rows, std::size_t count,
                                              glm::vec3 incoming, float tolerance,
                                              PrescribedParticleProjection &projection) {
    if (count < 2 || count > PrescribedParticleProjection::kMaxRows)
        return false;
    const RigidBody *particle = rows[0].bodyA;
    const glm::dvec3 v0(incoming);
    std::array<glm::dvec3, PrescribedParticleProjection::kMaxRows> normal{};
    std::array<double, PrescribedParticleProjection::kMaxRows> target{};
    double maximumSourceSpeedSquared = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto &c = rows[i];
        if (c.bodyA != particle || !(particle->inverseMass > 0) || !c.bodyB->IsStatic() ||
            c.friction != 0 || !(c.restitutionBias <= 0) ||
            c.bodyB->angularVelocity != glm::vec3(0) ||
            c.inverseInertiaA != glm::mat3(0))
            return false;
        normal[i] = glm::dvec3(c.normal);
        const glm::dvec3 wall(c.bodyB->linearVelocity);
        target[i] = double(c.restitutionBias) - glm::dot(normal[i], v0 - wall);
        const auto source = wall - v0;
        maximumSourceSpeedSquared = std::max(maximumSourceSpeedSquared, glm::dot(source, source));
    }

    double bestDistance = std::numeric_limits<double>::infinity();
    PrescribedParticleProjection best;
    const auto consider = [&](const std::array<std::size_t, 3> &indices,
                              const std::array<double, 3> &rates, std::size_t active) {
        glm::dvec3 delta(0);
        PrescribedParticleProjection trial;
        for (std::size_t k = 0; k < active; ++k) {
            if (!(rates[k] >= 0) || !std::isfinite(rates[k]))
                return;
            const auto i = indices[k];
            trial.normalImpulses[i] = float(rates[k] / particle->inverseMass);
            if (!std::isfinite(trial.normalImpulses[i]))
                return;
            delta += normal[i] * (double(trial.normalImpulses[i]) * particle->inverseMass);
        }
        const double distance = glm::dot(delta, delta);
        // A very narrow moving wedge can supply unbounded prescribed-wall
        // work. Certify only corrections bounded by an actual incoming-to-wall
        // relative speed. Excessive solutions remain with the bounded solver.
        constexpr double rounding = 8 * std::numeric_limits<float>::epsilon();
        if (!(distance < bestDistance) || distance > maximumSourceSpeedSquared * (1 + rounding))
            return;
        trial.velocity = glm::vec3(v0 + delta);
        // Recheck feasibility and complementarity with the actual represented
        // velocity and impulses, including every row outside the active set.
        for (std::size_t i = 0; i < count; ++i) {
            const float slack = glm::dot(trial.velocity - rows[i].bodyB->linearVelocity, rows[i].normal)
                                - rows[i].restitutionBias;
            if (!std::isfinite(slack) || slack < -tolerance ||
                (trial.normalImpulses[i] > 0 && std::abs(slack) > tolerance))
                return;
        }
        bestDistance = distance;
        best = trial;
    };

    // In 3D the minimum-norm correction has a conic certificate using at
    // most three independent normals. Rank rejection is relative double
    // roundoff, with no artificial compliance or change to a physical row.
    constexpr double rankTolerance = 64 * std::numeric_limits<double>::epsilon();
    consider({}, {}, 0);
    for (std::size_t a = 0; a < count; ++a) {
        const double aa = glm::dot(normal[a], normal[a]);
        if (!(aa > 0))
            continue;
        consider({a, 0, 0}, {target[a] / aa, 0, 0}, 1);
        for (std::size_t b = a + 1; b < count; ++b) {
            const double ab = glm::dot(normal[a], normal[b]);
            const double bb = glm::dot(normal[b], normal[b]);
            const double determinant = aa * bb - ab * ab;
            if (!(determinant > rankTolerance * aa * bb))
                continue;
            consider({a, b, 0}, {(bb * target[a] - ab * target[b]) / determinant,
                                  (aa * target[b] - ab * target[a]) / determinant, 0}, 2);
            for (std::size_t c = b + 1; c < count; ++c) {
                const double ac = glm::dot(normal[a], normal[c]);
                const double bc = glm::dot(normal[b], normal[c]);
                const double cc = glm::dot(normal[c], normal[c]);
                const glm::dmat3 gram({aa, ab, ac}, {ab, bb, bc}, {ac, bc, cc});
                if (!(glm::determinant(gram) > rankTolerance * aa * bb * cc))
                    continue;
                const auto rates = glm::inverse(gram) * glm::dvec3(target[a], target[b], target[c]);
                consider({a, b, c}, {rates.x, rates.y, rates.z}, 3);
            }
        }
    }
    if (!std::isfinite(bestDistance))
        return false;
    projection = best;
    return true;
}
