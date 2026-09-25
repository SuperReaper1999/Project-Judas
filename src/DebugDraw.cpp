#include "DebugDraw.h"

#include <cmath>

#include <glm/gtc/constants.hpp>

void DebugLineList::Line(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color) {
    m_lines.push_back({a, b, color});
}

void DebugLineList::Box(const glm::vec3& center, const glm::quat& rotation, const glm::vec3& h, const glm::vec3& color) {
    glm::vec3 corners[8];
    int index = 0;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                corners[index++] = center + rotation * glm::vec3(x * h.x, y * h.y, z * h.z);
            }
        }
    }
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& e : edges) Line(corners[e[0]], corners[e[1]], color);
}

void DebugLineList::Circle(const glm::vec3& center, const glm::vec3& axisIn, float radius, const glm::vec3& color, int segments) {
    const glm::vec3 axis = glm::normalize(axisIn);
    const glm::vec3 helper = std::abs(axis.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 u = glm::normalize(glm::cross(helper, axis));
    const glm::vec3 v = glm::cross(axis, u);
    glm::vec3 previous = center + u * radius;
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments) * glm::two_pi<float>();
        const glm::vec3 point = center + (u * std::cos(t) + v * std::sin(t)) * radius;
        Line(previous, point, color);
        previous = point;
    }
}

void DebugLineList::Sphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments) {
    Circle(center, glm::vec3(1.0f, 0.0f, 0.0f), radius, color, segments);
    Circle(center, glm::vec3(0.0f, 1.0f, 0.0f), radius, color, segments);
    Circle(center, glm::vec3(0.0f, 0.0f, 1.0f), radius, color, segments);
}

void DebugLineList::Capsule(const glm::vec3& center, const glm::quat& rotation, float radius, float halfHeight,
                            const glm::vec3& color) {
    const glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 top = center + up * halfHeight;
    const glm::vec3 bottom = center - up * halfHeight;
    Circle(top, up, radius, color, 20);
    Circle(bottom, up, radius, color, 20);
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    for (const glm::vec3& side : {right, -right, forward, -forward}) {
        Line(top + side * radius, bottom + side * radius, color);
    }
    // Hemisphere arcs.
    for (const glm::vec3& side : {right, forward}) {
        glm::vec3 previousTop = top + side * radius, previousBottom = bottom + side * radius;
        for (int i = 1; i <= 8; ++i) {
            const float t = static_cast<float>(i) / 8.0f * glm::half_pi<float>();
            const glm::vec3 offset = side * std::cos(t) * radius;
            const glm::vec3 rise = up * std::sin(t) * radius;
            Line(previousTop, top + offset + rise, color);
            Line(previousBottom, bottom + offset - rise, color);
            Line(top - offset + rise, top - side * radius * std::cos(static_cast<float>(i - 1) / 8.0f * glm::half_pi<float>()) +
                                          up * std::sin(static_cast<float>(i - 1) / 8.0f * glm::half_pi<float>()) * radius, color);
            Line(bottom - offset - rise, bottom - side * radius * std::cos(static_cast<float>(i - 1) / 8.0f * glm::half_pi<float>()) -
                                             up * std::sin(static_cast<float>(i - 1) / 8.0f * glm::half_pi<float>()) * radius, color);
            previousTop = top + offset + rise;
            previousBottom = bottom + offset - rise;
        }
    }
}

void DebugLineList::Arrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color, float headSize) {
    Line(from, to, color);
    const glm::vec3 direction = to - from;
    const float length = glm::length(direction);
    if (length < 1.0e-5f) return;
    const glm::vec3 d = direction / length;
    const glm::vec3 helper = std::abs(d.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 u = glm::normalize(glm::cross(helper, d));
    const glm::vec3 v = glm::cross(d, u);
    const float size = std::min(headSize, length * 0.4f);
    for (const glm::vec3& side : {u, -u, v, -v}) Line(to, to - d * size + side * size * 0.5f, color);
}

void DebugLineList::Axes(const glm::vec3& origin, const glm::quat& rotation, float length) {
    Line(origin, origin + rotation * glm::vec3(length, 0.0f, 0.0f), glm::vec3(0.95f, 0.25f, 0.25f));
    Line(origin, origin + rotation * glm::vec3(0.0f, length, 0.0f), glm::vec3(0.3f, 0.95f, 0.3f));
    Line(origin, origin + rotation * glm::vec3(0.0f, 0.0f, length), glm::vec3(0.3f, 0.5f, 1.0f));
}

void DebugLineList::Cross(const glm::vec3& c, float s, const glm::vec3& color) {
    Line(c - glm::vec3(s, 0, 0), c + glm::vec3(s, 0, 0), color);
    Line(c - glm::vec3(0, s, 0), c + glm::vec3(0, s, 0), color);
    Line(c - glm::vec3(0, 0, s), c + glm::vec3(0, 0, s), color);
}

void DebugLineList::Append(const DebugLineList& other) {
    m_lines.insert(m_lines.end(), other.m_lines.begin(), other.m_lines.end());
}
