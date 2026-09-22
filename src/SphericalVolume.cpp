#include "SphericalVolume.h"

SphericalVolume::SphericalVolume(const glm::vec3& center, float radius)
    : m_center(center), m_radius(radius) {}

bool SphericalVolume::Contains(const glm::vec3& position) const {
    return glm::length(position - m_center) <= m_radius;
}
