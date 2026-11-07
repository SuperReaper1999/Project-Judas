#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>
class SaveArchive;
struct CollisionAsset;

// Optional game-resolution water. The grid owns quantity; no render particle
// contributes mass. This backend is not selected by the Alpha fluid policy.
namespace JudasWater {
struct Settings {
    glm::ivec3 cells{32, 12, 24};
    float cellSize = .4f;
    glm::vec3 origin{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 gravity{0, -9.81f, 0}; // reference world acceleration, chart local -Y
    float density = 1000.f;
    unsigned pressureIterations = 240;
    float pressureTolerance = 1e-4f;
    // Air features smaller than this fraction of a voxel remain unresolved
    // material storage, not separate pressure reservoirs.
    float minimumAirFraction = .1f;
    float minimumMobileFraction = .001f;
    unsigned minimumReservoirCells = 4;
};
struct SolidBox {
    enum class Kind { Box, Sphere, Capsule, Convex };
    std::uint64_t id = 0;
    glm::vec3 center{0}, half{.5f}, velocity{0}, angularVelocity{0};
    glm::quat rotation{1, 0, 0, 0};
    Kind kind = Kind::Box;
    float radius = 0, halfHeight = 0;
    std::shared_ptr<const CollisionAsset> asset{};
};
float SolidVolume(const SolidBox &);
struct Cell {
    double water = 0, displaced = 0, airMass = 0;
    float capacity = 0, staticCapacity = 0, pressure = 0;
    int air = -1;
};
struct Face {
    int a = -1, b = -1, axis = 0;
    float velocity = 0, open = 1;
};
struct AirPocket {
    double volume = 0, mass = 0;
    float pressure = 0; // gauge Pa, atmosphere is zero
    bool atmosphere = false;
    bool unresolved = false; // retained air content; no independent pressure solve
};
struct Measurements {
    double milliseconds = 0, geometryMs = 0, pressureMs = 0, transportMs = 0;
    double water = 0, storedDisplacement = 0, exteriorAirMass = 0;
    double maximumSpeed = 0, pressureResidual = 0;
    double kineticEnergy = 0, rmsSpeed = 0, unresolvedWater = 0;
    double totalAirMass = 0, sealedAirMass = 0;
    unsigned iterations = 0, substeps = 0;
    std::size_t airPockets = 0, bytes = 0;
    std::size_t sealedAmbiguousCells = 0;
    std::size_t resolvedAirPockets = 0;
};
class Grid {
  public:
    bool Initialize(const Settings &, std::string &error);
    // Closed sides/bottom, atmospheric gas at the top. World callbacks can
    // sample existing terrain/collision geometry during authoring/cooking.
    void BakeStatic(const std::function<bool(glm::vec3)> &occupied);
    void SetSolids(const std::vector<SolidBox> &);
    // Derived scene data, rebuilt after loading. Empty uses constant gravity.
    // A local chart may follow a planetary field; it cannot span an antipode.
    bool SetGravityField(const std::vector<glm::vec3> &worldAcceleration, std::string &error);
    glm::vec3 Gravity(glm::vec3 world) const;
    double FillBox(glm::vec3 worldLo, glm::vec3 worldHi);
    double Fill(const std::function<bool(glm::vec3)> &worldVolume);
    bool Step(float seconds, std::string &error);
    glm::vec3 ToWorld(glm::vec3 local) const;
    glm::vec3 ToLocal(glm::vec3 world) const;
    glm::vec3 Center(int index) const;
    int Index(int x, int y, int z) const;
    glm::ivec3 Coordinates(int index) const;
    const Settings &Configuration() const { return settings_; }
    const std::vector<Cell> &Cells() const { return cells_; }
    const std::vector<Face> &Faces() const { return faces_; }
    // Read-only static connectivity, independent of a moving body's exclusion.
    float StaticOpening(int a,int b) const;
    const std::vector<AirPocket> &Air() const { return air_; }
    const Measurements &Stats() const { return stats_; }
    // Visual/query height only. Returns false for a column containing no water.
    bool Surface(int x, int z, float &localHeight) const;
    bool CellSurface(int index, float &localHeight) const;
    bool WetPoint(glm::vec3 world) const;
    // Rebase owned coordinates without changing cell quantities or motion.
    void ShiftOrigin(glm::dvec3 delta);
    glm::vec3 Velocity(glm::vec3 world) const;
    // Coarse immersed-volume quadrature. Samples only local wet witnesses near
    // the body's excluded volume, not a global bounding water plane.
    float ImmersedBox(const SolidBox &, glm::vec3 &worldCenter) const;
    float NearbyMass(glm::vec3 world, float radius) const;
    void AddImpulse(glm::vec3 world, glm::vec3 impulse, float radius = 0);
    // Returns accepted signed m3. Sources admit only open/atmospheric capacity;
    // a sealed or full receiver applies backpressure instead of losing volume.
    double ChangeWater(glm::vec3 world, float radius, double cubicMetres);
    double Quantity() const;
    double InitialQuantity() const { return initialQuantity_; }
    double AddedQuantity() const { return addedQuantity_; }
    double RemovedQuantity() const { return removedQuantity_; }
    double ExteriorAirMass() const { return exteriorAirMass_; }
    bool Save(const std::string &file, std::string &error) const;
    bool Load(const std::string &file, std::string &error);
    void Persist(SaveArchive &);

  private:
    struct Edge {
        int a, b;
        double weight;
    };
    Settings settings_;
    std::vector<Cell> cells_;
    std::vector<Face> faces_;
    std::vector<float> staticFaces_;
    std::vector<std::uint64_t> staticSamples_;
    std::vector<std::uint64_t> samples_;
    std::vector<std::uint16_t> staticFaceSamples_, faceSamples_;
    std::vector<float> waterHeight_;
    std::vector<int> solidOwner_;
    std::vector<std::vector<int>> incident_;
    std::vector<glm::vec3> velocity_, gravity_;
    std::vector<SolidBox> solids_;
    std::vector<AirPocket> air_;
    std::vector<double> gasReserve_;
    Measurements stats_;
    double exteriorAirMass_ = 0;
    double initialQuantity_ = 0, addedQuantity_ = 0, removedQuantity_ = 0;
    bool airInitialized_ = false;
    std::size_t sealedAmbiguousCells_ = 0;
    bool Geometry(std::string &);
    void ClassifyAir();
    void AdvectVelocity(float dt);
    bool Project(float dt, std::string &);
    void Transport(float dt);
    void UpdateVelocity();
    void ReleaseDisplacement();
    bool Inside(glm::vec3 world, const SolidBox &) const;
    void UpdateHeights();
    float DryArea(std::size_t face) const;
};
} // namespace JudasWater
