#pragma once

#include "ContactSolver.h"
#include <cstddef>
#include <functional>
#include <glm/glm.hpp>
#include <vector>

// An event contains actual touching/overlapping geometry at one physical time.
// Solve takes a complete connected island. SolveGrowing asks its owner to append
// contacts reached by real impulses, at the same immutable physical instant.
struct ImpactContact {
    RigidBody* a = nullptr;
    RigidBody* b = nullptr;
    Contact geometry;
    float friction = 0.0f;
    float restitution = 0.0f;
    bool persistent = false;
    // Only an existing authoritative support may retain its accepted incoming
    // normal-velocity defect. Selected/new contacts remain strict. The allowance
    // is bounded by the existing slop and by the owner's physical feature size.
    bool continuing = false;
    float maxDefectDisplacement = ContactSolver::kPenetrationSlop;
    // Concave mesh/terrain row: solved as points, outside normal blocks.
    bool pointNormals = false;
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
    // Transaction kernel: no warm replay, fixed iteration budget, immutable
    // incoming body snapshots. expand may append actual contacts, never remove
    // rows/change existing poses. false rejects the trial; the caller must then
    // discard its private bodies. Energy/residual rejection is not a repair.
    bool SolveGrowing(std::vector<ImpactContact>& contacts, float dt,
                      const std::function<bool(const std::vector<RigidBody*>&)>& expand,
                      ImpactResult& result);
    // Discard prepared body pointers before their owner's backing storage moves.
    // Retains vector and frame-pool capacities; no numerical state is retained.
    void Clear() { m_saved.clear(); m_solver.Clear(); }
    void EnableBlockNormals(bool enabled) { m_solver.EnableBlockNormals(enabled); }
    const std::vector<ContactConstraint>& Constraints() const { return m_solver.Constraints(); }

private:
    struct SavedVelocity {
        RigidBody* body;
        glm::vec3 linear, angular;
    };
    ContactSolver m_solver;
    std::vector<SavedVelocity> m_saved;
};
