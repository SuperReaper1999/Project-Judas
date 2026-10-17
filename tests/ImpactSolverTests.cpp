#include "ImpactSolver.h"
#include "RigidBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
RigidBody Sphere(float mass, const glm::vec3& position, const glm::vec3& velocity) {
    RigidBody b;
    b.position = position;
    b.linearVelocity = velocity;
    b.inverseMass = 1.0f / mass;
    b.inverseInertiaLocal = SolidSphereInverseInertia(mass, 0.5f);
    return b;
}
ImpactContact Spheres(RigidBody& a, RigidBody& b, float friction = 0, float restitution = 1) {
    return {&a, &b, SphereVsSphere(a.position, 0.5f, b.position, 0.5f), friction, restitution, false};
}
void Budget(const ImpactResult& result, double momentumTolerance = 1e-4) {
    Check(result.energyBudgetSatisfied, "impact energy budget");
    Check(result.kineticAfter <= result.kineticBefore + result.prescribedBoundaryWork + result.energyTolerance,
          "unexplained energy gain");
    Check(glm::length(result.momentumAfter - result.momentumBefore) <= momentumTolerance,
          "linear impulse budget");
    Check(glm::length(result.angularMomentumAfter - result.angularMomentumBefore) <= momentumTolerance,
          "angular impulse budget");
}
} // namespace

int main() {
    ImpactSolver solver;
    int cases = 0, fallbacks = 0;
    for (int axis = 0; axis < 3; ++axis) for (float sign : {-1.0f, 1.0f})
    for (float boost : {0.0f, 2.0f, -7.0f}) for (bool reverseRows : {false, true}) {
        glm::vec3 direction(0); direction[axis] = sign;
        std::array<RigidBody, 3> bodies{
            Sphere(10, glm::vec3(0), direction * (1 + boost)),
            Sphere(0.125f, direction, direction * (-3 + boost)),
            Sphere(10, 2.0f * direction, direction * (4 + boost))};
        std::vector<ImpactContact> contacts{Spheres(bodies[0], bodies[1]), Spheres(bodies[1], bodies[2])};
        if (reverseRows) std::reverse(contacts.begin(), contacts.end());
        const auto result = solver.Solve(contacts, 1.0f / 60.0f);
        Budget(result);
        Check(result.ambiguous && result.effectiveRestitution == 0, "coupled response must be inelastic");
        Check(result.materialClosingContacts == 1, "second separating row remains part of connected island");
        if (axis == 0 && sign == 1 && boost == 0 && !reverseRows) {
            Check(std::abs(result.kineticBefore - 85.5625) < 2e-6, "independent initial energy");
            std::cout << std::setprecision(17) << "three_spheres energy_before=" << result.kineticBefore
                      << " energy_after=" << result.kineticAfter
                      << " momentum_before=" << result.momentumBefore.x
                      << " momentum_after=" << result.momentumAfter.x << '\n';
        }
        fallbacks += result.safetyFallback;
        ++cases;
    }
    {
        auto a = Sphere(2, {0,0,0}, {3,0,0});
        auto b = Sphere(1, {1,0,0}, {0,0,0});
        auto c = Sphere(3, {2,0,0}, {-2,0,0});
        const auto result = solver.Solve({Spheres(a,b,0,1), Spheres(b,c,0,0.25f)}, 1.0f/60);
        Budget(result);
        Check(result.materialClosingContacts == 2 && result.ambiguous && result.effectiveRestitution == 0,
              "mixed coupled restitution is intentionally inelastic");
        fallbacks += result.safetyFallback;
        ++cases;
    }
    for (float restitution : {0.0f, 0.5f, 1.0f}) {
        auto a = Sphere(2, {0,0,0}, {1,0,0});
        auto b = Sphere(3, {1,0,0}, {0,0,0});
        const auto result = solver.Solve({Spheres(a,b,0,restitution)}, 1.0f/60);
        Budget(result);
        Check(!result.ambiguous && result.effectiveRestitution == restitution, "isolated material restitution");
        // One-dimensional two-body mechanics, independent of solver implementation.
        const double impulse = (1.0 + restitution) / (1.0/2.0 + 1.0/3.0);
        Check(std::abs(a.linearVelocity.x - (1 - impulse/2)) < 1e-6, "isolated A velocity");
        Check(std::abs(b.linearVelocity.x - impulse/3) < 1e-6, "isolated B velocity");
        fallbacks += result.safetyFallback;
        ++cases;
    }
    {
        auto a = Sphere(1, {0,0,0}, {0.25f,0,0});
        auto b = Sphere(1, {1,0,0}, {0,0,0});
        const auto result = solver.Solve({Spheres(a,b)}, 1.0f/60);
        Budget(result);
        Check(result.effectiveRestitution == 0, "low speed capture");
        Check(a.linearVelocity.x == b.linearVelocity.x, "inelastic low speed velocity");
        ++cases;
    }
    {
        auto a = Sphere(1, {0,0,0}, {2,1,0});
        auto b = Sphere(2, {1,0,0}, {0,-1,0});
        auto contact = Spheres(a,b,0.6f,0.9f);
        contact.persistent = true;
        const auto result = solver.Solve({contact}, 1.0f/60);
        Budget(result);
        Check(result.ambiguous && result.effectiveRestitution == 0, "impact into support is inelastic");
        Check(glm::length(a.angularVelocity) + glm::length(b.angularVelocity) > 0, "Coulomb friction creates physical spin");
        fallbacks += result.safetyFallback;
        ++cases;
    }
    for (float boundarySpeed : {0.0f, 2.0f}) {
        auto a = Sphere(1, {0,0,0}, {-1,0,0});
        RigidBody wall;
        wall.position = {-1,0,0};
        wall.linearVelocity = {boundarySpeed,0,0};
        const auto result = solver.Solve({Spheres(a,wall,0,1)}, 1.0f/60);
        Check(result.energyBudgetSatisfied && !result.safetyFallback, "moving boundary work accounted");
        Check(std::abs(a.linearVelocity.x - (1 + 2*boundarySpeed)) < 1e-6, "moving wall isolated elastic oracle");
        Check(std::abs((result.kineticAfter-result.kineticBefore)-result.prescribedBoundaryWork) < 1e-6,
              "moving wall energy equals prescribed impulse work");
        ++cases;
    }
    {
        auto a = Sphere(1, {0,0,0}, {1,0,0});
        auto b = Sphere(1, {1,0,0}, {0,0,0});
        const auto result = solver.Solve({Spheres(a,b)}, 1.0f/60, true);
        Budget(result);
        Check(result.ambiguous && result.effectiveRestitution == 0, "event cap can request inelastic response");
        ++cases;
    }
    {
        auto a = Sphere(1, {0,0,0}, {1,0,0});
        auto b = Sphere(1, {1.001f,0,0}, {0,0,0});
        auto contact = Spheres(a,b);
        contact.geometry = SphereVsSphere(a.position,.5f,b.position,.5f,.01f);
        bool rejected = false;
        try { (void)solver.Solve({contact},1.0f/60); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected && a.linearVelocity.x == 1 && b.linearVelocity.x == 0,
              "positive gap must not receive premature impact");
        ++cases;
    }
    std::cout << "Impact response " << cases << " cases " << checks << " checks PASS; safety_fallbacks="
              << fallbacks << " (actual engine timing is validated separately)\n";
    return 0; // Preserve main's implicit success when the provenance wrapper calls this entrypoint.
}
