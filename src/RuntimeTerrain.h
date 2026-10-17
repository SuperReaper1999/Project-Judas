#pragma once
#include "VolumeTerrainGeometry.h"
#include "NavigationAsset.h"
#include "JobSystem.h"
#include "Renderer.h"
#include <map>
class RuntimeWorld;
struct CollisionAsset;
struct RuntimeTerrainStatus {
 bool ready=false,busy=false,navigation=false;uint64_t revision=0;
 size_t samples=0,triangles=0,bytes=0;double prepareMs=0,publishMs=0;std::string error;
};
// Optional world-owned mutable instances. Workers own immutable copies only.
// Pending work never replaces the last committed collision/navigation/mesh.
class RuntimeTerrain {
public:
 explicit RuntimeTerrain(RuntimeWorld&);~RuntimeTerrain();
 bool Create(uint64_t,VolumeTerrainSource,bool navigation,unsigned profile,std::string&);
 bool Brush(uint64_t,glm::vec3 worldPoint,const VolumeBrush&,std::string&);
 const RuntimeTerrainStatus* Status(uint64_t)const;
 bool Has(uint64_t)const;bool ReadyToSave()const;
 void Advance();void Remove(uint64_t);void Draw(Renderer&)const;
 bool Capture(uint64_t,std::string&,std::string&)const;
 bool Restore(uint64_t,const std::string&,std::string&,bool asynchronous=false);
 std::vector<uint64_t> Owners()const;
private:
 struct Product {VolumeTerrainSource source;MeshData mesh;std::shared_ptr<CollisionAsset> collision;std::shared_ptr<NavigationData> navigation;bool navigated=false;unsigned profile=0;double ms=0;std::string error;};
 struct Record {bool privateOwner=false;RuntimeTerrainStatus status;std::shared_ptr<Product> committed,pending;JobHandle job;mutable MeshHandle mesh;mutable uint64_t drawnRevision=0;};
 RuntimeWorld& world;std::map<uint64_t,Record> records;
 bool Submit(uint64_t,VolumeTerrainSource,bool,unsigned,std::string&,bool privateOwner=false);
 bool Publish(uint64_t,Record&,std::shared_ptr<Product>,std::string&,bool privateOwner=false);
 static bool Prepare(Product&,const ProjectNavigation&,const JobContext* =nullptr);
};
