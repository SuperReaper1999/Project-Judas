#pragma once

#include "GravityVolume.h"

// The simplest possible GravityVolume: everything within `radius` of
// `center`. Used to give a planet's own gravity a bounded domain of
// ownership instead of an unbounded falloff — see GravityContextMap.h,
// "Gravity context ownership."
class SphericalVolume : public GravityVolume {
public:
    SphericalVolume(const glm::vec3& center, float radius);

    bool Contains(const glm::vec3& position) const override;

private:
    glm::vec3 m_center;
    float m_radius;
};
