#pragma once
#include "JobSystem.h"
#include "PhysicsWorld.h"
#include "Renderer.h"
#include "TerrainBrush.h"
#include "TerrainLod.h"
#include "TerrainLodTransition.h"
#include "TerrainPersistence.h"
#include "TerrainPreparation.h"
#include "TerrainRenderPages.h"
#include "TerrainSolid.h"
#include "TerrainSource.h"
#include "TerrainStatus.h"
#include <map>
#include <memory>
#include <unordered_set>

class RuntimeWorld;
struct CollisionAsset;
struct SceneObject;
struct PlanetView;
namespace TerrainV2 {
// World-thread owner. Jobs capture immutable source/results, never this object
// or a live world. The existing RuntimeTerrain participant owns its lifetime,
// regional suspension, native-before-publication restore and save boundary.
class Runtime {
  public:
    Runtime(RuntimeWorld &, uint64_t owner);
    ~Runtime();
    bool Create(Source, std::string &);
    bool Install(PreparedAsset &, std::string &);
    bool Brush(glm::vec3 worldPoint, const Edit &, std::string &);
    bool Surface(glm::vec3 worldPoint, const SurfaceBrush &, std::string &);
    void Advance();
    void Wait();
    void Draw(Renderer &) const;
    bool Capture(std::string &, std::string &) const;
    bool Restore(const std::string &, std::string &, bool asynchronous);
    const RuntimeTerrainStatus &State() const;
    size_t PayloadBytes() const;
    bool Owns(BodyHandle) const;
    std::vector<BodyHandle> Bodies() const;
    SolidSnapshot Solid() const;
    void Synchronize();
    void PrepareViews(const std::vector<PlanetView> &, glm::dvec3 origin, float seconds = 0);
    void BindAsset(std::string id, std::string fingerprint);

  private:
    struct Work : Preparation {
        bool replaceAll = false;
        std::string baseAsset, baseFingerprint;
        RasterChanges raster;
    };
    struct Resident {
        PreparedChunk geometry;
        size_t triangles = 0;
        BodyHandle body;
    };
    RuntimeWorld &world;
    uint64_t owner;
    std::shared_ptr<const Source> source;
    std::shared_ptr<GeometryCache> geometryCache;
    std::shared_ptr<Work> pending;
    JobHandle job;
    std::map<ChunkKey, Resident> chunks;
    std::unordered_set<unsigned> bodyIds;
    uint64_t geometryRevision = 0;
    uint64_t boundPlanet = 0;
    bool bindingPublished = false;
    std::shared_ptr<const void> geometryEpoch;
    mutable RuntimeTerrainStatus status;
    size_t residentPayloadBytes = 0;
    std::string baseAsset, baseFingerprint;
    RasterChanges raster;
    mutable RenderPages renderPages;
    std::shared_ptr<const HeightLod> lod;
    std::shared_ptr<const NavigationData> navigation;
    bool navigationInstalled = false;
    LodTransition drawPatches;
    const SceneObject *Definition() const;
    BodyHandle Seed() const;
    bool Submit(std::shared_ptr<const Source>, std::vector<ChunkKey>, std::string &,
                bool replaceAll = false);
    bool ValidateSubmission(const Source &, std::string &) const;
    bool Publish(std::string &);
    void ReleaseGraphics();
    void CommitPresentation();
};
} // namespace TerrainV2
