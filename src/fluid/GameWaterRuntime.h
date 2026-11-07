#pragma once
#include "BasicWater.h"
#include "GameWater.h"
#include "MeshData.h"
#include "PhysicsWorld.h"
#include "Scene.h"
#include <map>
class RuntimeWorld;
class Renderer;
class SaveArchive;

namespace JudasWater {
// One owner per authored water region. RuntimeWorld coordinates this optional
// service; state, geometry publication, queries and failures live here.
class WorldSystem {
  public:
    explicit WorldSystem(RuntimeWorld &);
    ~WorldSystem();
    bool Build(const Scene &, std::string &error);
    bool Step(float seconds, std::string &error);
    void ShiftOrigin(glm::dvec3);
    BasicWaterSample Sample(glm::vec3, glm::vec3, float halfHeight, float radius) const;
    std::vector<glm::vec2> OpticalPaths(const glm::mat4 &, const glm::mat4 &, unsigned,
                                        unsigned) const;
    void Draw(Renderer &) const;
    void Persist(SaveArchive &);
    const Grid *Find(std::uint64_t) const;
    std::vector<std::uint64_t> Owners() const;
    bool Paused(std::uint64_t) const;
    bool SetPaused(std::uint64_t, bool);
    bool AddImpulse(std::uint64_t, glm::vec3, glm::vec3, float radius);
    double ChangeWater(std::uint64_t, glm::vec3, float radius, double cubicMetres);
    struct RuntimeStats {
        double totalMs = 0, preparationMs = 0, couplingMs = 0;
        std::size_t bytes = 0;
        unsigned geometryBakes = 0;
        std::string error;
    };
    RuntimeStats Stats(std::uint64_t) const;
    const std::string &Error() const { return error_; }

  private:
    struct Region;
    RuntimeWorld &world_;
    std::map<std::uint64_t, std::unique_ptr<Region>> regions_;
    std::string error_;
};
} // namespace JudasWater
