#include "ImpactSolver.h"
#include "RigidBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void Check(bool okay, const char* message) {
    ++checks;
    if (!okay) throw std::runtime_error(message);
}
RigidBody Sphere(glm::vec3 position, glm::vec3 velocity) {
    RigidBody body;
    body.position = position;
    body.linearVelocity = velocity;
    body.inverseMass = 1.0f;
    body.inverseInertiaLocal = SolidSphereInverseInertia(1.0f, 0.5f);
    return body;
}
ImpactContact Touch(RigidBody& a, RigidBody& b, float restitution = 0.0f) {
    return {&a, &b, SphereVsSphere(a.position, .5f, b.position, .5f), .6f, restitution, false};
}
void Verify(const ImpactSolver& solver, const std::vector<ImpactContact>& rows,
            const ImpactResult& result) {
    Check(result.energyBudgetSatisfied, "physical energy guard remains satisfied");
    Check(solver.Constraints().size() == rows.size(), "all rows preserved");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = solver.Constraints()[i];
        Check(row.bodyA == rows[i].a && row.bodyB == rows[i].b, "published original row identity");
        Check(row.normalImpulse >= 0, "unilateral normal impulse");
        Check(glm::length(row.tangentImpulse) <= row.friction * row.normalImpulse + 1e-5f,
              "unchanged Coulomb friction bound");
        Check(std::isfinite(glm::length(row.bodyA->linearVelocity)), "finite physical state");
    }
}
}

int main() {
    std::cout << std::setprecision(17);
    for (int axis = 0; axis < 3; ++axis) for (float sign : {-1.f, 1.f})
    for (bool reverse : {false, true}) for (float boundarySpeed : {0.f, .25f}) {
        glm::vec3 normal(0); normal[axis] = sign;
        RigidBody boundary;
        boundary.linearVelocity = normal * boundarySpeed;
        std::array<RigidBody, 6> bodies;
        for (std::size_t i = 0; i < bodies.size(); ++i)
            bodies[i] = Sphere(normal * float(i + 1), -normal);
        std::vector<ImpactContact> rows;
        rows.push_back(Touch(bodies[0], boundary));
        for (std::size_t i = 1; i < bodies.size(); ++i) rows.push_back(Touch(bodies[i], bodies[i - 1]));
        if (reverse) std::reverse(rows.begin(), rows.end());
        ImpactSolver solver;
        const auto result = solver.Solve(rows, 1.f / 60.f, true);
        Verify(solver, rows, result);
        Check(boundary.linearVelocity == normal * boundarySpeed, "prescribed boundary unchanged");
        std::cout << "anchored " << axis << ' ' << sign << ' ' << reverse << ' ' << boundarySpeed;
        for (const auto& body : bodies) std::cout << ' ' << glm::dot(normal, body.linearVelocity);
        std::cout << ' ' << result.kineticAfter << ' ' << result.prescribedBoundaryWork << '\n';
    }
    for (float restitution : {0.f, .5f, 1.f}) {
        auto a = Sphere({0,0,0}, {2, .25f, 0});
        auto b = Sphere({1,0,0}, {-1, -.5f, 0});
        std::vector<ImpactContact> rows{Touch(a,b,restitution)};
        ImpactSolver solver;
        const auto result = solver.Solve(rows, 1.f / 60.f);
        Verify(solver, rows, result);
        std::cout << "free " << restitution;
        for (auto* body : {&a, &b})
            std::cout << ' ' << body->linearVelocity.x << ' ' << body->linearVelocity.y
                      << ' ' << body->angularVelocity.z;
        std::cout << '\n';
    }
    std::cout << "order controls " << checks << " checks PASS\n";
}
