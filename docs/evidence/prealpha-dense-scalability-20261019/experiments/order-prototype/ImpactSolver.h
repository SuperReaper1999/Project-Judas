#pragma once

#include <cstddef>
#include <vector>
#include <glm/glm.hpp>
#include "ContactSolver.h"

// An event contains actual touching/overlapping geometry at one physical time.
// The caller supplies the complete connected island, including contacts which
// were separating before the event but can be induced by another impulse.
struct ImpactContact {
    RigidBody* a = nullptr;
    RigidBody* b = nullptr;
    Contact geometry;
    float friction = 0.0f;
    float restitution = 0.0f;
    bool persistent = false;
};

struct ImpactResult {
    std::size_t contacts = 0;
    std::size_t materialClosingContacts = 0;
    bool ambiguous = false;
    bool safetyFallback = false;
    bool energyBudgetSatisfied = true;
    float effectiveRestitution = 0.0f;
    double kineticBefore = 0.0;
    double kineticAfter = 0.0;
    double prescribedBoundaryWork = 0.0;
    double energyTolerance = 0.0;
    glm::dvec3 momentumBefore{0.0}, momentumAfter{0.0};
    glm::dvec3 angularMomentumBefore{0.0}, angularMomentumAfter{0.0};
};

// Game-engine impact policy, separate from persistent warm-start support:
// single isolated closing point: material restitution; connected multi-point
// or persistent island: inelastic normal response. No warm impulse is replayed.
// This owns reusable solver storage; no pointers survive the next Solve call.
class JointSolver;
class ImpactSolver {
public:
    ImpactResult Solve(const std::vector<ImpactContact>& contacts, float dt,
                       bool forceInelastic = false, JointSolver* joints = nullptr);
    const std::vector<ContactConstraint>& Constraints() const { return m_solver.Constraints(); }

private:
    struct SavedVelocity {
        RigidBody* body;
        glm::vec3 linear, angular;
    };
    ContactSolver m_solver;
    std::vector<SavedVelocity> m_saved;
};
