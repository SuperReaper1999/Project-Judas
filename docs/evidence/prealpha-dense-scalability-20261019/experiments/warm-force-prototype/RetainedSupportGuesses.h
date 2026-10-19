#pragma once

// Isolated experiment. These are ordinary supporting-force guesses for the
// NEXT fixed step, never impulses to replay at this chronological impact.
#include "WorldInternal.h"

namespace JudasPhysicsDetail {
class RetainedSupportGuesses {
public:
    explicit RetainedSupportGuesses(const std::vector<CachedContact>& ordinarySupport)
        : m_rows(ordinarySupport), m_used(m_rows.size(),0), m_invalid(m_rows.size(),0) {}

    void BeginMatchPass() { std::fill(m_used.begin(),m_used.end(),0); }

    std::pair<float,glm::vec3> Match(const ContactKey& key, const glm::dvec3& anchor,
                                    const glm::vec3& normal,
                                    std::uint64_t revisionA, std::uint64_t revisionB) {
        const auto range = std::equal_range(m_rows.begin(),m_rows.end(),
            CachedContact{key,{}, {},0.0f,{}},
            [](const CachedContact& a, const CachedContact& b) { return a.key<b.key; });
        std::size_t best = m_rows.size();
        double bestDistance = kWarmStartAnchorDistance;
        for (auto it=range.first; it!=range.second; ++it) {
            const auto index = std::size_t(it-m_rows.begin());
            if (m_used[index] || m_invalid[index] || !it->key.SameBodies(key) ||
                it->observationRevisionA!=revisionA || it->observationRevisionB!=revisionB ||
                glm::dot(it->normal,normal)<kWarmStartNormalDot ||
                !(it->normalImpulse>=0) || !std::isfinite(it->normalImpulse) ||
                !FiniteVector(it->tangentImpulse))
                continue;
            const double distance = glm::distance(it->localAnchorA,anchor);
            if (distance<bestDistance) { bestDistance=distance; best=index; }
        }
        if (best==m_rows.size()) return {0.0f,glm::vec3(0)};
        m_used[best]=1;
        return {m_rows[best].normalImpulse,m_rows[best].tangentImpulse};
    }

    // Elastic departure, joints and external couplers preserve the original
    // conservative invalidation policy, including later events in this step.
    void InvalidateBodies(const std::vector<unsigned char>& included) {
        for (std::size_t i=0; i<m_rows.size(); ++i)
            if (included[m_rows[i].key.slotA] || included[m_rows[i].key.slotB])
                m_invalid[i]=1;
    }

private:
    std::vector<CachedContact> m_rows;
    std::vector<unsigned char> m_used, m_invalid;
};
} // namespace JudasPhysicsDetail
