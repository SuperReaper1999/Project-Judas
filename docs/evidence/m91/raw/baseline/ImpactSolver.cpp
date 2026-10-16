#include "ImpactSolver.h"
#include "RigidBody.h"
#include "JointSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
// Matches the existing persistent solver's low-speed capture threshold.
constexpr double kRestitutionVelocityThreshold = 0.5;
// Declared engineering tolerance for a short binary32 impulse solve. Budgets
// themselves accumulate in binary64; this is not a rigorous rounding bound.
constexpr double kEnergyRelativeTolerance = 64.0 * std::numeric_limits<float>::epsilon();

struct Budget {
    double kinetic = 0;
    glm::dvec3 momentum{0}, angular{0};
};
Budget Measure(const RigidBody& b) {
    Budget result;
    if (b.IsStatic()) return result;
    const double mass = 1.0 / double(b.inverseMass);
    const glm::dvec3 v(b.linearVelocity), omega(b.angularVelocity);
    const glm::dmat3 inverseInertia(b.InverseInertiaWorld());
    glm::dvec3 spin(0);
    if (glm::determinant(inverseInertia) != 0.0) {
        spin = glm::inverse(inverseInertia) * omega;
    } else if (glm::dot(omega, omega) != 0.0) {
        throw std::invalid_argument("impact energy requires finite rotational inertia for rotating dynamic bodies");
    }
    result.momentum = mass * v;
    result.angular = glm::cross(glm::dvec3(b.position), result.momentum) + spin;
    result.kinetic = 0.5 * (mass * glm::dot(v, v) + glm::dot(omega, spin));
    return result;
}
glm::dvec3 Offset(const RigidBody& b, const Contact& c, bool a) {
    if (!c.hasLocalAnchors) return glm::dvec3(c.point) - glm::dvec3(b.position);
    const glm::dvec3 local = a ? c.localAnchorA : c.localAnchorB;
    if (a ? c.anchorAInWorldFrame : c.anchorBInWorldFrame) return local;
    return ContactRotation(b.orientation) * local;
}
glm::dvec3 PointVelocity(const RigidBody& b, const glm::dvec3& offset) {
    return glm::dvec3(b.linearVelocity) + glm::cross(glm::dvec3(b.angularVelocity), offset);
}
bool Finite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace

ImpactResult ImpactSolver::Solve(const std::vector<ImpactContact>& contacts, float dt,
                                 bool forceInelastic,JointSolver* joints) {
    if (!(dt >= 0) || !std::isfinite(dt)) throw std::invalid_argument("invalid impact interval");
    m_saved.clear();
    m_solver.Clear();
    ImpactResult result;
    bool persistent = false;
    auto remember = [&](RigidBody* b) {
        if (std::find_if(m_saved.begin(), m_saved.end(),
                        [b](const SavedVelocity& s) { return s.body == b; }) != m_saved.end()) return;
        if (!Finite(b->linearVelocity) || !Finite(b->angularVelocity))
            throw std::invalid_argument("nonfinite impact velocity");
        m_saved.push_back({b, b->linearVelocity, b->angularVelocity});
        const Budget budget = Measure(*b);
        result.kineticBefore += budget.kinetic;
        result.momentumBefore += budget.momentum;
        result.angularMomentumBefore += budget.angular;
    };
    for (const auto& c : contacts) {
        if (!c.a || !c.b || c.a == c.b || !c.geometry.hit)
            throw std::invalid_argument("invalid impact contact");
        const double separation = c.geometry.hasLocalAnchors ? c.geometry.signedSeparation
                                                            : -double(c.geometry.penetration);
        if (!std::isfinite(separation) || separation > 0)
            throw std::invalid_argument("impact requires actual contact, not positive separation");
        if (!(c.friction >= 0) || !std::isfinite(c.friction) ||
            !(c.restitution >= 0 && c.restitution <= 1))
            throw std::invalid_argument("invalid impact material");
        if (c.a->IsStatic() && c.b->IsStatic()) continue;
        remember(c.a); remember(c.b);
        ++result.contacts;
        persistent = persistent || c.persistent;
        const glm::dvec3 normal = c.geometry.hasLocalAnchors ? c.geometry.preciseNormal
                                                           : glm::dvec3(c.geometry.normal);
        const double vn = glm::dot(PointVelocity(*c.a, Offset(*c.a, c.geometry, true)) -
                                   PointVelocity(*c.b, Offset(*c.b, c.geometry, false)), normal);
        if (vn < -kRestitutionVelocityThreshold) ++result.materialClosingContacts;
    }
    if(joints)for(auto* body:joints->Bodies())remember(body);
    // Including every connected contact is deliberately conservative: a second
    // initially separating row can become closing after the first impulse.
    result.ambiguous = forceInelastic || persistent || result.contacts > 1 || (joints&&!joints->Empty());
    if (!result.ambiguous && result.materialClosingContacts == 1)
        for (const auto& c : contacts)
            if (!c.a->IsStatic() || !c.b->IsStatic()) result.effectiveRestitution = c.restitution;

    auto run = [&](bool safe) {
        m_solver.Clear();
        for (const auto& c : contacts) {
            if (c.a->IsStatic() && c.b->IsStatic()) continue;
            m_solver.AddContact(*c.a, *c.b, c.geometry, safe ? 0.0f : c.friction,
                                safe ? 0.0f : result.effectiveRestitution);
        }
        // Geometry is already at the event, so no speculative gap/time target.
        // The persistent solver and its numerical expressions are unchanged.
        m_solver.Prepare(0.0f);
        if(!joints||joints->Empty())m_solver.SolveVelocities();
        else {joints->ResetImpulses();for(int i=0;i<ContactSolver::kVelocityIterations;++i){m_solver.SolveVelocities(1);joints->SolveIteration();}}
        result.kineticAfter = 0;
        result.momentumAfter = glm::dvec3(0);
        result.angularMomentumAfter = glm::dvec3(0);
        for (const auto& s : m_saved) {
            const Budget budget = Measure(*s.body);
            result.kineticAfter += budget.kinetic;
            result.momentumAfter += budget.momentum;
            result.angularMomentumAfter += budget.angular;
        }
        result.prescribedBoundaryWork = 0;
        for (const auto& c : m_solver.Constraints()) {
            const glm::dvec3 impulse = glm::dvec3(c.normal) * double(c.normalImpulse) +
                                      glm::dvec3(c.tangentImpulse);
            // Work by prescribed infinite-mass boundaries is external. A fixed
            // wall contributes zero; moving walls may legitimately add energy.
            if (c.bodyA->IsStatic())
                result.prescribedBoundaryWork -= glm::dot(impulse, PointVelocity(*c.bodyA, glm::dvec3(c.velocityOffsetA)));
            if (c.bodyB->IsStatic())
                result.prescribedBoundaryWork += glm::dot(impulse, PointVelocity(*c.bodyB, glm::dvec3(c.velocityOffsetB)));
        }
        result.energyTolerance = kEnergyRelativeTolerance *
            std::max({1.0, result.kineticBefore, std::abs(result.prescribedBoundaryWork)});
        result.energyBudgetSatisfied = std::isfinite(result.kineticAfter) &&
            result.kineticAfter <= result.kineticBefore + result.prescribedBoundaryWork + result.energyTolerance;
        for (const auto& s : m_saved)
            result.energyBudgetSatisfied = result.energyBudgetSatisfied &&
                Finite(s.body->linearVelocity) && Finite(s.body->angularVelocity);
    };
    run(false);
    if (!result.energyBudgetSatisfied) {
        for (const auto& s : m_saved) {
            s.body->linearVelocity = s.linear;
            s.body->angularVelocity = s.angular;
        }
        result.safetyFallback = true;
        result.effectiveRestitution = 0;
        run(true);
    }
    return result;
}
