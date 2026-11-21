#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace JudasHair {
struct Collider {
    glm::vec3 start{0}, end{0};
    float radius = .1f;
};

struct Groom {
    bool enabled = true, simulated = false, castsShadow = true, worldCollision = false;
    // Roots lie on an elliptical dome or strip in the owner's local frame.
    glm::vec3 center{0}, extent{.2f, .16f, .2f}, flow{0, -1, 0};
    glm::vec3 color{.21f, .12f, .07f};
    float length = .055f, width = .004f, variation = .25f, clump = .35f, comb = .65f;
    float coverageDegrees = 125, undercoat = 1, wave = .12f;
    float stiffness = .45f, damping = .18f, wind = .1f;
    float lodStart = 8, lodEnd = 35;
    unsigned guides = 16, segments = 3, strands = 128, seed = 1;
    std::vector<Collider> colliders;
};

enum class Preset { Coat, Mane, Tail, Hair, Beard };
Groom MakePreset(Preset);
bool Validate(const Groom&, std::string& error);
std::string Encode(const Groom&);
bool Decode(const std::string&, Groom&, std::string& error);
}
