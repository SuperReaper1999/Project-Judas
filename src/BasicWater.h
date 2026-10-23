#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <cstdint>
#include <vector>

// Prescribed water: no mass ledger, particles, pressure solve or rigid coupling.
// Local +Y is the surface normal. Occupancy is [-x,x] x [-depth,0] x [-z,z].
// Visual waves deliberately do not change the swimming volume.
struct BasicWaterSettings {
    bool enabled = true;
    glm::vec3 halfExtents{12, 2, 12};
    glm::vec3 color{.035f, .30f, .38f};
    glm::vec3 flow{0}; // local prescribed current, used only by swimming queries
    float waveAmplitude = .10f, wavelength = 5, waveSpeed = 1;
    float opacity = .82f, density = 1000;
};
struct BasicWaterInstance {
    BasicWaterSettings settings;
    glm::vec3 position{0};
    glm::quat rotation{1,0,0,0};
    std::uint64_t entity=0;
};
struct BasicWaterSample {
    float immersion = 0, density = 0;
    glm::vec3 velocity{0};
};
bool ValidateBasicWater(const BasicWaterSettings&, std::string& error);
std::string EncodeBasicWater(const BasicWaterSettings&);
bool DecodeBasicWater(const std::string&, BasicWaterSettings&, std::string& error);
BasicWaterSample SampleBasicWater(const std::vector<BasicWaterInstance>&,
                                 glm::vec3 center, glm::vec3 up,
                                 float halfHeight, float radius);
// Near-plane distance intervals consumed by the existing underwater optical pass.
std::vector<glm::vec2> BasicWaterOpticalPaths(const std::vector<BasicWaterInstance>&,
    const glm::mat4& view, const glm::mat4& projection, unsigned columns, unsigned rows);
