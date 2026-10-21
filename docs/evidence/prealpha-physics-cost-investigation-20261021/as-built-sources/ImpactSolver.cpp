#include "ImpactSolver.h"
#include "ResponseAudit.h"
#include "RigidBody.h"
#include "JointSolver.h"

#include <algorithm>
#include <array>
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
    response_audit::Count(response_audit::stats.solver_budget_body_measurements);
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
    response_audit::ContextScope auditContext(response_audit::Context::LegacyImpact);
    response_audit::Timer auditTimer(response_audit::stats.ns_solver_legacy);
    response_audit::Count(response_audit::stats.solver_calls_legacy);
    response_audit::Count(response_audit::stats.solver_legacy_input_rows, contacts.size());
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
            response_audit::Count(response_audit::stats.solver_budget_rows);
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
        response_audit::Count(response_audit::stats.solver_legacy_safety_fallbacks);
        result.safetyFallback = true;
        result.effectiveRestitution = 0;
        run(true);
    }
    return result;
}

// All contacts and bodies belong to one trial physical instant. Appending rows
// never reapplies earlier impulses or changes poses. The caller publishes only
// after successful closure and budget verification.
bool ImpactSolver::SolveGrowing(std::vector<ImpactContact>& contacts, float dt,
                                const std::function<bool(const std::vector<RigidBody*>&)>& expand,
                                ImpactResult& result) {
    response_audit::ContextScope auditContext(response_audit::Context::GrowingImpact);
    response_audit::Timer auditTimer(response_audit::stats.ns_solver_growing);
    response_audit::Count(response_audit::stats.solver_calls_growing);
    if (!(dt > 0) || !std::isfinite(dt))
        throw std::invalid_argument("invalid growing impact interval");
    m_saved.clear();
    m_solver.Clear();
    result = {};
    result.ambiguous = true;
    const auto append = [&](std::size_t first) {
        response_audit::Timer auditAppendTimer(response_audit::stats.ns_solver_append);
        response_audit::Count(response_audit::stats.solver_append_calls);
        response_audit::Count(response_audit::stats.solver_appended_rows, contacts.size() - first);
        const auto firstFrame = m_solver.FrameNodeRequests();
        for (std::size_t i = first; i < contacts.size(); ++i) {
            const auto& c = contacts[i];
            if (!c.a || !c.b || c.a == c.b || !c.geometry.hit ||
                (c.a->IsStatic() && c.b->IsStatic()))
                throw std::invalid_argument("invalid growing impact contact");
            const double gap = c.geometry.hasLocalAnchors ? c.geometry.signedSeparation
                                                          : -double(c.geometry.penetration);
            if (!std::isfinite(gap) || gap > 0 || !std::isfinite(c.friction) || c.friction < 0 ||
                !std::isfinite(c.maxDefectDisplacement) || c.maxDefectDisplacement < 0 ||
                c.maxDefectDisplacement > ContactSolver::kPenetrationSlop)
                throw std::invalid_argument("invalid growing impact constraint");
            for (auto* body : {c.a, c.b})
                if (!Finite(body->linearVelocity) || !Finite(body->angularVelocity))
                    throw std::invalid_argument("nonfinite growing impact state");
            m_solver.AddContact(*c.a, *c.b, c.geometry, c.friction, 0);
            const auto& frame = m_solver.Constraints().back();
            const std::array<std::pair<RigidBody*, std::size_t>, 2> participants{
                {{c.a, frame.frameA}, {c.b, frame.frameB}}};
            for (const auto& participant : participants) {
                auto* body = participant.first;
                const auto index = participant.second;
                if (index < m_saved.size()) {
                    if (m_saved[index].body != body)
                        throw std::logic_error("growing body index mismatch");
                    continue;
                }
                if (index != m_saved.size())
                    throw std::logic_error("growing body index gap");
                response_audit::CapacityProbe auditSaved(m_saved, response_audit::stats.solver_saved_growth_count, response_audit::stats.solver_saved_growth_bytes);
                m_saved.push_back({body, body->linearVelocity, body->angularVelocity});
                const auto budget = Measure(*body);
                result.kineticBefore += budget.kinetic;
                result.momentumBefore += budget.momentum;
                result.angularMomentumBefore += budget.angular;
            }
        }
        response_audit::Count(response_audit::stats.solver_append_frame_requests, m_solver.FrameNodeRequests() - firstFrame);
        m_solver.PrepareAppended(0, first, firstFrame);
        for (std::size_t i = first; i < contacts.size(); ++i) {
            const auto& c = m_solver.Constraints()[i];
            if (m_saved[c.frameA].body != c.bodyA || m_saved[c.frameB].body != c.bodyB)
                throw std::logic_error("growing impact frame mapping changed");
            const auto& a = m_saved[c.frameA];
            const auto& b = m_saved[c.frameB];
            const auto va = a.linear + glm::cross(a.angular, c.velocityOffsetA);
            const auto vb = b.linear + glm::cross(b.angular, c.velocityOffsetB);
            const float incoming = std::min(glm::dot(va - vb, c.normal), 0.f);
            // Ordinary support has already accepted this incoming state.
            // Solve only the NEW impact increment, without another projection
            // of an unrelated resting residual. The per-event baseline never
            // changes during iterations or domain growth. Large defects are
            // not waived: they retain a strict zero-velocity target.
            const float target = contacts[i].continuing && std::isfinite(incoming) &&
                                         -double(incoming) * dt <= contacts[i].maxDefectDisplacement
                                     ? incoming
                                     : 0.f;
            m_solver.SetInelasticVelocityTarget(i, target);
        }
    };
    append(0);
    bool auditPreviousFixedPoint = false;
    for (int iteration = 0; iteration < ContactSolver::kVelocityIterations; ++iteration) {
        response_audit::Count(response_audit::stats.solver_sweeps);
        const auto auditChangesBefore = response_audit::stats.growing_accumulator_changes;
        const auto auditRowsBefore = response_audit::stats.growing_rows;
        const auto auditContactsBefore = contacts.size();
        std::size_t first = 0;
        for (;;) {
            const auto count = contacts.size();
            std::vector<RigidBody*> activated;
            response_audit::Count(response_audit::stats.solver_ranges);
            m_solver.SolveTrackedVelocityRange(first, 1, activated);
            if (!expand(activated))
                return false;
            if (contacts.size() == count)
                break;
            append(count);
            first = count;
        }
        if (response_audit::enabled) {
            const auto rowVisits = response_audit::stats.growing_rows - auditRowsBefore;
            response_audit::stats.solver_rows_per_sweep_sum += rowVisits;
            const bool fixed = response_audit::stats.growing_accumulator_changes == auditChangesBefore && contacts.size() == auditContactsBefore;
            response_audit::stats.solver_sweep_fixed_points += fixed;
            if (fixed && auditPreviousFixedPoint) {
                ++response_audit::stats.solver_sweep_after_fixed_point;
                response_audit::stats.solver_fixed_point_row_visits += rowVisits;
            }
            auditPreviousFixedPoint = fixed;
        }
    }
    response_audit::Timer auditBudgetTimer(response_audit::stats.ns_solver_budget);
    result.contacts = contacts.size();
    for (std::size_t i = 0; i < m_solver.Constraints().size(); ++i) {
        response_audit::Count(response_audit::stats.solver_contact_closure_checks);
        const auto& c = m_solver.Constraints()[i];
        const auto va =
            c.bodyA->linearVelocity + glm::cross(c.bodyA->angularVelocity, c.velocityOffsetA);
        const auto vb =
            c.bodyB->linearVelocity + glm::cross(c.bodyB->angularVelocity, c.velocityOffsetB);
        if (-double(glm::dot(va - vb, c.normal)) * dt > contacts[i].maxDefectDisplacement) {
            response_audit::Count(response_audit::stats.solver_contact_closure_failures);
            return false;
        }
    }
    for (const auto& saved : m_saved) {
        const auto budget = Measure(*saved.body);
        result.kineticAfter += budget.kinetic;
        result.momentumAfter += budget.momentum;
        result.angularMomentumAfter += budget.angular;
    }
    // Reaction at prescribed infinite-mass boundaries is external work,
    // with exactly the established legacy impact-budget convention.
    for (const auto& c : m_solver.Constraints()) {
        response_audit::Count(response_audit::stats.solver_budget_rows);
        const glm::dvec3 impulse =
            glm::dvec3(c.normal) * double(c.normalImpulse) + glm::dvec3(c.tangentImpulse);
        if (c.bodyA->IsStatic())
            result.prescribedBoundaryWork -=
                glm::dot(impulse, PointVelocity(*c.bodyA, glm::dvec3(c.velocityOffsetA)));
        if (c.bodyB->IsStatic())
            result.prescribedBoundaryWork +=
                glm::dot(impulse, PointVelocity(*c.bodyB, glm::dvec3(c.velocityOffsetB)));
    }
    result.energyTolerance =
        kEnergyRelativeTolerance *
        std::max({1.0, result.kineticBefore, std::abs(result.prescribedBoundaryWork)});
    result.energyBudgetSatisfied = std::isfinite(result.kineticAfter) &&
                                   result.kineticAfter <= result.kineticBefore +
                                                              result.prescribedBoundaryWork +
                                                              result.energyTolerance;
    for (const auto& saved : m_saved)
        result.energyBudgetSatisfied &=
            Finite(saved.body->linearVelocity) && Finite(saved.body->angularVelocity);
    return result.energyBudgetSatisfied;
}
