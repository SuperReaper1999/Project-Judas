#include "BoxVolume.h"

#include <cmath>

BoxVolume::BoxVolume(const glm::vec3& center, const glm::vec3& halfExtents)
    : m_center(center), m_halfExtents(halfExtents) {}

bool BoxVolume::Contains(const glm::vec3& position) const {
    const glm::vec3 local = position - m_center;
    return std::abs(local.x) <= m_halfExtents.x && std::abs(local.y) <= m_halfExtents.y &&
           std::abs(local.z) <= m_halfExtents.z;
}
