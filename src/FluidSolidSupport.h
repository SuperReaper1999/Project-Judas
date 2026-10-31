#pragma once

#include "FluidHydrostatics.h"
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace FluidHydrostaticDetail {

// Query-local geometry; no field or pose pointers survive the query. Strict
// occlusion excludes the whole solid from bulk-motion donors. Support contact
// is separate: only the actual solid component containing the sample may bridge
// missing water columns. Nearby, disconnected compound children cannot help it.
class PreparedSolidQuery {
    struct BoxIntervals {
        glm::dvec3 center;
        glm::dvec3 half;
        std::array<glm::dvec3, 3> bodyAxes;
        std::array<glm::dvec3, 15> axes;
        std::array<double, 15> reach;
    };
    glm::dvec3 m_axes[3], m_half, m_center;
    double m_radius;
    std::vector<BoxIntervals> m_boxes;
    std::vector<std::size_t> m_supportBoxes;

    // Poses and dimensions arrive in binary32. Admit touching geometry within
    // input roundoff, not an additional spacing- or force-dependent wet radius.
    static double ContactTolerance(glm::dvec3 a, glm::dvec3 b) {
        return 8 * std::numeric_limits<float>::epsilon() *
               (1 + glm::length(a) + glm::length(b));
    }
    static bool Connected(const BoxIntervals& a, const BoxIntervals& b) {
        const auto delta = b.center - a.center;
        const double tolerance = ContactTolerance(a.center, b.center);
        const auto separates = [&](glm::dvec3 axis) {
            const double length2 = glm::dot(axis, axis);
            if (length2 == 0) return false;
            double reach = 0;
            for (int k = 0; k < 3; ++k) {
                reach += a.half[k] * std::abs(glm::dot(a.bodyAxes[k], axis)) +
                         b.half[k] * std::abs(glm::dot(b.bodyAxes[k], axis));
            }
            return std::abs(glm::dot(delta, axis)) > reach + tolerance * std::sqrt(length2);
        };
        for (int i = 0; i < 3; ++i) {
            if (separates(a.bodyAxes[i]) || separates(b.bodyAxes[i])) return false;
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (separates(glm::cross(a.bodyAxes[i], b.bodyAxes[j]))) return false;
            }
        }
        return true;
    }

public:
    PreparedSolidQuery(const glm::dvec3& tangent, const glm::dvec3& up,
                       const glm::dvec3& bitangent, double width, double halfHeight,
                       const FluidSolidQuery& solid)
        : m_axes{tangent, up, bitangent}, m_half(width * .5, halfHeight, width * .5),
          m_center(solid.pose.position), m_radius(solid.volume.solidSphereRadius) {
        if (m_radius > 0) return;
        const glm::dquat rotation(solid.pose.rotation);
        m_boxes.reserve(solid.volume.solidBoxes.size());
        for (const auto& box : solid.volume.solidBoxes) {
            const auto childRotation = rotation * glm::dquat(box.rotation);
            const glm::dvec3 bodyAxes[3] = {childRotation * glm::dvec3(1, 0, 0),
                                          childRotation * glm::dvec3(0, 1, 0),
                                          childRotation * glm::dvec3(0, 0, 1)};
            BoxIntervals prepared;
            prepared.half = box.halfExtents;
            prepared.center = glm::dvec3(solid.pose.position) + rotation * glm::dvec3(box.localCenter);
            int axis = 0;
            for (int i = 0; i < 3; ++i) {
                prepared.bodyAxes[i] = bodyAxes[i];
                prepared.axes[axis++] = m_axes[i];
                prepared.axes[axis++] = bodyAxes[i];
            }
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) prepared.axes[axis++] = glm::cross(m_axes[i], bodyAxes[j]);
            }
            for (int k = 0; k < 15; ++k) {
                double reach = 0;
                const glm::dvec3 bodyHalf(box.halfExtents);
                for (int i = 0; i < 3; ++i) {
                    reach += m_half[i] * std::abs(glm::dot(m_axes[i], prepared.axes[k])) +
                             bodyHalf[i] * std::abs(glm::dot(bodyAxes[i], prepared.axes[k]));
                }
                prepared.reach[k] = reach;
            }
            m_boxes.push_back(prepared);
        }
    }

    bool Occluded(const glm::dvec3& center) const {
        if (m_radius > 0) {
            const glm::dvec3 delta = m_center - center;
            glm::dvec3 coordinates;
            for (int i = 0; i < 3; ++i) coordinates[i] = glm::dot(delta, m_axes[i]);
            const glm::dvec3 nearest = glm::clamp(coordinates, -m_half, m_half);
            const glm::dvec3 difference = coordinates - nearest;
            return glm::dot(difference, difference) < m_radius * m_radius;
        }
        for (const auto& box : m_boxes) {
            const glm::dvec3 delta = box.center - center;
            bool apart = false;
            for (int k = 0; k < 15 && !apart; ++k) {
                apart = glm::dot(box.axes[k], box.axes[k]) != 0 &&
                        std::abs(glm::dot(delta, box.axes[k])) >= box.reach[k];
            }
            if (!apart) return true;
        }
        return false;
    }

    void SelectSupport(const glm::dvec3& point) {
        m_supportBoxes.clear();
        std::vector<bool> selected(m_boxes.size(), false);
        for (std::size_t i = 0; i < m_boxes.size(); ++i) {
            const auto& box = m_boxes[i];
            const auto delta = point - box.center;
            const double tolerance = ContactTolerance(point, box.center);
            bool inside = true;
            for (int axis = 0; axis < 3; ++axis) {
                inside &= std::abs(glm::dot(delta, box.bodyAxes[axis])) <= box.half[axis] + tolerance;
            }
            if (inside) {
                selected[i] = true;
                m_supportBoxes.push_back(i);
            }
        }
        for (std::size_t cursor = 0; cursor < m_supportBoxes.size(); ++cursor) {
            for (std::size_t i = 0; i < m_boxes.size(); ++i) {
                if (!selected[i] && Connected(m_boxes[m_supportBoxes[cursor]], m_boxes[i])) {
                    selected[i] = true;
                    m_supportBoxes.push_back(i);
                }
            }
        }
    }

    bool Touches(const glm::dvec3& center, double waterHalfWidth = -1) const {
        const double halfWidth = waterHalfWidth < 0 ? m_half.x : waterHalfWidth;
        const glm::dvec3 half(halfWidth, m_half.y, halfWidth);
        const double tolerance = ContactTolerance(center, m_center);
        if (m_radius > 0) {
            const glm::dvec3 delta = m_center - center;
            glm::dvec3 coordinates;
            for (int i = 0; i < 3; ++i) coordinates[i] = glm::dot(delta, m_axes[i]);
            const auto difference = coordinates - glm::clamp(coordinates, -half, half);
            return glm::dot(difference, difference) <= (m_radius + tolerance) * (m_radius + tolerance);
        }
        for (const auto index : m_supportBoxes) {
            const auto& box = m_boxes[index];
            const auto delta = box.center - center;
            bool apart = false;
            for (int k = 0; k < 15 && !apart; ++k) {
                const double length2 = glm::dot(box.axes[k], box.axes[k]);
                apart = length2 > 0 && std::abs(glm::dot(delta, box.axes[k])) >
                        box.reach[k] + (halfWidth - m_half.x) *
                            (std::abs(glm::dot(m_axes[0], box.axes[k])) + std::abs(glm::dot(m_axes[2], box.axes[k]))) +
                            tolerance * std::sqrt(length2);
            }
            if (!apart) return true;
        }
        return false;
    }
};
} // namespace FluidHydrostaticDetail
