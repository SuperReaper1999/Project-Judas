#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Milestone 30: engine-side debug geometry — plain world-space line lists
// that Renderer draws unlit through its own debug-line path. Nothing here
// touches GL, and nothing here is gameplay state: a debug view is rebuilt
// from engine truth every frame it is enabled and forgotten otherwise.
struct DebugLine {
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    glm::vec3 color{1.0f};
};

class DebugLineList {
public:
    void Clear() { m_lines.clear(); }
    void Line(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
    void Box(const glm::vec3& center, const glm::quat& rotation, const glm::vec3& halfExtents, const glm::vec3& color);
    void Sphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 24);
    void Circle(const glm::vec3& center, const glm::vec3& axis, float radius, const glm::vec3& color, int segments = 32);
    // A capsule along local +Y (the player's convention).
    void Capsule(const glm::vec3& center, const glm::quat& rotation, float radius, float halfHeight, const glm::vec3& color);
    void Arrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color, float headSize = 0.15f);
    void Axes(const glm::vec3& origin, const glm::quat& rotation, float length);
    void Cross(const glm::vec3& center, float size, const glm::vec3& color);
    void Append(const DebugLineList& other);

    const std::vector<DebugLine>& Lines() const { return m_lines; }
    bool Empty() const { return m_lines.empty(); }

private:
    std::vector<DebugLine> m_lines;
};
