#include "RadicalGravity.h"

RadicalGravity::RadicalGravity(const glm::vec3& center, float magnitude)
    : m_center(center), m_magnitude(magnitude) {}

glm::vec3 RadicalGravity::Sample(const glm::vec3& worldPosition) const {
    const glm::vec3 towardCenter = m_center - worldPosition;
    const float distance = glm::length(towardCenter);
    if (distance < 1.0e-6f) {
        // Degenerate case: exactly at the center, no defined direction.
        return glm::vec3(0.0f);
    }
    return (towardCenter / distance) * m_magnitude;
}
