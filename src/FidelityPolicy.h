#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "EntityLifecycle.h"

// Milestone 29: fidelity POLICY, separate from fidelity CAPABILITY.
//
// RuntimeWorld knows how to move an entity between Full, Coarse and
// Dormant (capability). A FidelityPolicy decides what each managed entity
// should be right now (policy). Nothing in the transition machinery
// assumes distance, or any other reason, is why a fidelity changes: a
// future game can hand the world a policy driven by visibility, ownership,
// gameplay relevance, simulation budget, region streaming or explicit
// rules. The world evaluates the policy once per fixed step for entities
// that are managed, not pinned, not forced and not required to stay Full.
struct FidelityPolicyContext {
    // Where attention is (the player, a camera, a region centre); what a
    // particular policy does with it is its own business.
    glm::vec3 focus{0.0f};
    double simulationTimeSeconds = 0.0;
};

// Per-entity view the policy is given; it never sees the live body.
struct FidelityPolicyEntity {
    EntityId id = kInvalidSceneObjectId;
    SimulationFidelity current = SimulationFidelity::Full;
    glm::vec3 position{0.0f};
    glm::vec3 linearVelocity{0.0f};
};

class FidelityPolicy {
public:
    virtual ~FidelityPolicy() = default;
    virtual SimulationFidelity Desired(const FidelityPolicyEntity& entity,
                                       const FidelityPolicyContext& context) const = 0;
};

// The M29 demonstration policy: distance from the focus. Full inside
// `fullRadius`, Coarse inside `coarseRadius`, Dormant beyond. A 10%
// hysteresis band keeps an entity sitting on a boundary from flapping.
class DistanceFidelityPolicy final : public FidelityPolicy {
public:
    DistanceFidelityPolicy(float fullRadius, float coarseRadius);
    SimulationFidelity Desired(const FidelityPolicyEntity& entity,
                               const FidelityPolicyContext& context) const override;
    float FullRadius() const { return m_fullRadius; }
    float CoarseRadius() const { return m_coarseRadius; }

private:
    float m_fullRadius;
    float m_coarseRadius;
};
