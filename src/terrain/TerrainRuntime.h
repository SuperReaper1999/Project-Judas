#pragma once
#include "JobSystem.h"
#include "PhysicsWorld.h"
#include "Renderer.h"
#include "TerrainBrush.h"
#include "TerrainLod.h"
#include "TerrainPersistence.h"
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
    bool Brush(glm::vec3 worldPoint, const Edit &, std::string &);
    bool Surface(glm::vec3 worldPoint, const SurfaceBrush &, std::string &);
    void Advance();
    void Wait();
    void Draw(Renderer &) const;
    bool Capture(std::string &, std::string &) const;
    bool Restore(const std::string &, std::string &, bool asynchronous);
    const RuntimeTerrainStatus &State() const { return status; }
    bool Owns(BodyHandle) const;
    std::vector<BodyHandle> Bodies() const;
    SolidSnapshot Solid() const;
    void Synchronize();
    void PrepareViews(const std::vector<PlanetView> &, glm::dvec3 origin);
    void BindAsset(std::string id, std::string fingerprint);

  private:
    struct Prepared {
        Chunk chunk;
        std::shared_ptr<CollisionAsset> collision;
    };
    struct Work {
        std::shared_ptr<const Source> source;
        std::vector<Prepared> chunks;
        double milliseconds = 0;
        std::string error;
        std::shared_ptr<const HeightLod> lod;
        bool replaceAll = false;
        std::string baseAsset, baseFingerprint;
        RasterChanges raster;
    };
    struct Resident {
        Prepared geometry;
        BodyHandle body;
        mutable MeshHandle mesh;
    };
    RuntimeWorld &world;
    uint64_t owner;
    std::shared_ptr<const Source> source;
    std::shared_ptr<Work> pending;
    JobHandle job;
    std::map<ChunkKey, Resident> chunks;
    std::unordered_set<unsigned> bodyIds;
    uint64_t geometryRevision = 0;
    std::shared_ptr<const void> geometryEpoch;
    RuntimeTerrainStatus status;
    std::string baseAsset, baseFingerprint;
    RasterChanges raster;
    mutable TextureHandle weightTexture;
    mutable TextureHandle heightTexture;
    std::shared_ptr<const HeightLod> lod;
    std::vector<HeightPatch> drawPatches;
    mutable uint64_t appearanceRevision = 0;
    const SceneObject *Definition() const;
    BodyHandle Seed() const;
    bool Submit(std::shared_ptr<const Source>, std::vector<ChunkKey>, std::string &,
                bool replaceAll = false);
    bool Publish(std::string &);
    void ReleaseMesh(Resident &) const;
    void ReleaseGraphics();
    void CommitPresentation();
};
} // namespace TerrainV2
