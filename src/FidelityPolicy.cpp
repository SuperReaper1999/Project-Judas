#include "FidelityPolicy.h"

#include <algorithm>

const char* FidelityName(SimulationFidelity fidelity) {
    switch (fidelity) {
        case SimulationFidelity::Full: return "full";
        case SimulationFidelity::Coarse: return "coarse";
        case SimulationFidelity::Dormant: return "dormant";
    }
    return "full";
}

const char* LifecycleName(EntityLifecycle lifecycle) {
    switch (lifecycle) {
        case EntityLifecycle::Active: return "active";
        case EntityLifecycle::Unloaded: return "unloaded";
        case EntityLifecycle::Destroyed: return "destroyed";
    }
    return "active";
}

DistanceFidelityPolicy::DistanceFidelityPolicy(float fullRadius, float coarseRadius)
    : m_fullRadius(std::max(fullRadius, 0.0f)), m_coarseRadius(std::max(coarseRadius, fullRadius)) {}

SimulationFidelity DistanceFidelityPolicy::Desired(const FidelityPolicyEntity& entity,
                                                   const FidelityPolicyContext& context) const {
    const float distance = glm::length(entity.position - context.focus);
    constexpr float kHysteresis = 1.1f;
    // Leaving a fidelity band takes 10% more distance than entering it.
    const float fullOut = m_fullRadius * kHysteresis;
    const float coarseOut = m_coarseRadius * kHysteresis;
    switch (entity.current) {
        case SimulationFidelity::Full:
            if (distance <= fullOut) return SimulationFidelity::Full;
            return distance <= m_coarseRadius ? SimulationFidelity::Coarse : SimulationFidelity::Dormant;
        case SimulationFidelity::Coarse:
            if (distance <= m_fullRadius) return SimulationFidelity::Full;
            if (distance <= coarseOut) return SimulationFidelity::Coarse;
            return SimulationFidelity::Dormant;
        case SimulationFidelity::Dormant:
            if (distance <= m_fullRadius) return SimulationFidelity::Full;
            if (distance <= m_coarseRadius) return SimulationFidelity::Coarse;
            return SimulationFidelity::Dormant;
    }
    return entity.current;
}
