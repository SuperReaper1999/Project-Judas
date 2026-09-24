#pragma once

#include <memory>

#include <glm/glm.hpp>

class RadialTerrain;

// Authored M25 content. These coordinates live in the terrain body's own
// frame; rotating or translating that body changes no elevation rule.
namespace TerrainDemo {
constexpr float kBaseRadius = 80.0f;
constexpr float kWaterSpacing = 0.5f;
constexpr float kBasinAX = -4.0f;
constexpr float kBasinBX = 5.0f;
constexpr float kBasinZ = 3.0f;

float Elevation(const glm::vec3& localUnitDirection);
std::shared_ptr<const RadialTerrain> CreateSurface();
glm::vec3 LocalPointAbove(const RadialTerrain& terrain, float tangentX,
                          float tangentZ, float clearance);
}  // namespace TerrainDemo
