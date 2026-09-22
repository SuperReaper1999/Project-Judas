#pragma once

#include "GravityVolume.h"

// An axis-aligned box GravityVolume — used as a GravityTransition's own
// bounded extent (see GravityContextMap.h). Not oriented (no rotation):
// nothing in this milestone's demo needs a rotated box, and adding one
// would be speculative ahead of a future need.
class BoxVolume : public GravityVolume {
public:
    BoxVolume(const glm::vec3& center, const glm::vec3& halfExtents);

    bool Contains(const glm::vec3& position) const override;

private:
    glm::vec3 m_center;
    glm::vec3 m_halfExtents;
};
