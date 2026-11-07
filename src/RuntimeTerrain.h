#pragma once
#include "VolumeTerrainGeometry.h"
#include "NavigationAsset.h"
#include "JobSystem.h"
#include "Renderer.h"
#include <map>
class RuntimeWorld;
struct CollisionAsset;
#include "terrain/TerrainStatus.h"
namespace TerrainV2 { struct Source; struct SurfaceBrush; struct SolidSnapshot; struct PreparedAsset; class Runtime; }
struct PlanetView;
// Optional world-owned mutable instances. Workers own immutable copies only.
// Pending work never replaces the last committed collision/navigation/mesh.
class RuntimeTerrain {
public:
 explicit RuntimeTerrain(RuntimeWorld&);~RuntimeTerrain();
 bool Create(uint64_t,VolumeTerrainSource,bool navigation,unsigned profile,std::string&);
 bool CreateLandscape(uint64_t,TerrainV2::Source,std::string&);
 bool CreateAsset(uint64_t,const std::string&,std::string&);
 bool InstallPrepared(uint64_t,TerrainV2::PreparedAsset&,std::string&);
 uint64_t OwnerOf(BodyHandle)const;
 bool OwnsGenerated(BodyHandle)const;
 bool IsLandscape(uint64_t id)const;
 void Synchronize();
 bool HasLandscapes()const;
 void PrepareViews(const std::vector<PlanetView>&,glm::dvec3 origin);
 std::vector<BodyHandle> Bodies(uint64_t id)const;
 std::vector<TerrainV2::SolidSnapshot> Solids(glm::vec3 minimum,glm::vec3 maximum)const;
 bool Brush(uint64_t,glm::vec3 worldPoint,const VolumeBrush&,std::string&);
 bool Surface(uint64_t,glm::vec3 worldPoint,const TerrainV2::SurfaceBrush&,std::string&);
 const RuntimeTerrainStatus* Status(uint64_t)const;
 // Source, LOD, sample overrides, mesh and collision/BVH payload; not allocator
 // overhead, physics-world storage or GPU residency. Cached at publication.
 size_t LandscapePayloadBytes(uint64_t)const;
 bool Has(uint64_t)const;bool ReadyToSave()const;
 void Advance();void Remove(uint64_t);void Draw(Renderer&)const;
 bool Wait(uint64_t,std::string&);
 bool Capture(uint64_t,std::string&,std::string&)const;
 bool Restore(uint64_t,const std::string&,std::string&,bool asynchronous=false);
 std::vector<uint64_t> Owners()const;
private:
 struct Product {VolumeTerrainSource source;MeshData mesh;std::shared_ptr<CollisionAsset> collision;std::shared_ptr<NavigationData> navigation;bool navigated=false;unsigned profile=0;double ms=0;std::string error;};
 struct Record {std::unique_ptr<TerrainV2::Runtime> landscape;bool privateOwner=false;RuntimeTerrainStatus status;std::shared_ptr<Product> committed,pending;JobHandle job;mutable MeshHandle mesh;mutable uint64_t drawnRevision=0;};
 RuntimeWorld& world;std::map<uint64_t,Record> records;
 bool Submit(uint64_t,VolumeTerrainSource,bool,unsigned,std::string&,bool privateOwner=false);
 bool Publish(uint64_t,Record&,std::shared_ptr<Product>,std::string&,bool privateOwner=false);
 static bool Prepare(Product&,const ProjectNavigation&,const JobContext* =nullptr);
};
