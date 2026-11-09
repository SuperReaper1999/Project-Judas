#pragma once
#include "MeshData.h"
#include "PhysicsWorld.h"
#include "fluid/GameWater.h"
#include <vector>
namespace TerrainV2 {struct Source;}

namespace WaterLab {
struct Body {
    BodyHandle handle;
    glm::vec3 half, color;
    float density = 500;
};
struct Wall {
    glm::vec3 center, half;
    glm::quat rotation{1, 0, 0, 0};
};
class Scene {
  public:
    Scene(unsigned mode = 0, bool rotated = false, bool noBodies = false);
    void Advance();
    void Spawn(glm::vec3 chartPosition, glm::vec3 half, float density, glm::vec3 color);
    void SetGate(bool);
    bool Save(const std::string &directory, std::string &error);
    static std::unique_ptr<Scene> Load(const std::string &directory, std::string &error);
    JudasWater::Grid water;
    PhysicsWorld physics;
    std::vector<Body> bodies;
    std::vector<Wall> walls;
    std::shared_ptr<TerrainV2::Source> terrain;
    MeshData terrainMesh;
    JudasWater::Settings settings;
    unsigned step = 0, mode = 0;
    bool gate = false, rotated = false, noBodies = false;
    double initial = 0;
    double lastStepMs = 0;
    Wall gateShape{};

  private:
    BodyHandle gateHandle_{};
};
} // namespace WaterLab
