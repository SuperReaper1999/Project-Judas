#pragma once

#include <glm/glm.hpp>

// An absolute world location is a double-precision origin plus a small
// simulation-space displacement. Physics, gameplay, cameras, and rendering
// use the same local float coordinates; only this boundary handles large
// absolute positions. Translating an entire scene changes origin, not any
// local displacement, velocity, force, or physical relationship.
//
// This is one active local scene, not streaming or automatic rebasing. Local
// coordinates must remain within the precision range documented for M23.
class WorldCoordinates {
public:
    explicit WorldCoordinates(const glm::dvec3& origin = glm::dvec3(0.0))
        : m_origin(origin) {}

    const glm::dvec3& Origin() const { return m_origin; }

    glm::dvec3 ToGlobal(const glm::vec3& localPosition) const {
        return m_origin + glm::dvec3(localPosition);
    }

    glm::vec3 ToLocal(const glm::dvec3& globalPosition) const {
        // Subtract in double precision before narrowing. Casting the global
        // position first would erase ordinary metre/centimetre movement at
        // the large offsets M23 is designed to support.
        return glm::vec3(globalPosition - m_origin);
    }

private:
    glm::dvec3 m_origin;
};
