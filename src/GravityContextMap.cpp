#include "GravityContextMap.h"

#include "GravityVolume.h"

void GravityContextMap::AddRegion(GravityField& field, const GravityVolume& volume) {
    m_regions.push_back(Region{&field, &volume});
}

glm::vec3 GravityContextMap::Sample(const glm::vec3& worldPosition) const {
    for (const Region& region : m_regions) {
        if (region.volume->Contains(worldPosition)) {
            return region.field->Sample(worldPosition);
        }
    }
    // No region claims this position: genuinely unclaimed space. Same
    // degenerate-safe "no defined gravity" signal a lone GravityField
    // already returns at its own singular point (e.g. RadicalGravity
    // sampled exactly at its center).
    return glm::vec3(0.0f);
}
