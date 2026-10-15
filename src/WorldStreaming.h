#pragma once
#include "Scene.h"
#include "Project.h"
#include "JobSystem.h"
#include <functional>
#include <memory>
#include <set>
class RuntimeWorld;
class ResourceManager;
struct WorldRegion {
    std::string id,scene,policy="snapshot";
    glm::dvec3 origin{0}; // absolute translation of source scene local origin
    glm::quat rotation{1,0,0,0};
    glm::vec3 halfExtents{16};
    int priority=0;
    size_t estimatedBytes=65536;
    std::vector<std::string> dependencies;
};
struct WorldReference {
    std::string region,field,target;SceneObjectId local=0,targetLocal=0;bool hard=true;
};
struct WorldManifest {
    std::map<std::string,WorldRegion> regions;
    std::vector<WorldReference> references;
    double installMilliseconds=2;
    unsigned unitsPerFrame=16,maxPreparing=2;
    size_t pendingBytes=32*1024*1024,liveBytes=64*1024*1024,retainedBytes=4*1024*1024,resourceCacheBytes=64*1024*1024;
};
bool ParseWorldManifest(const std::string&,WorldManifest&,std::string&);
bool LoadWorldManifest(const std::string&,WorldManifest&,std::string&);
bool ValidateWorldManifest(const WorldManifest&,const Project&,std::string&);
// Immutable worker product: source bake certificates are checked before ID/pose relocation.
struct PreparedWorldRegion {
    Scene source;
    std::map<SceneObjectId,std::shared_ptr<const struct NavigationData>> navigation;
    std::vector<std::string> requiredGeometry;
    std::string fingerprint;size_t bytes=0;
};
bool PrepareWorldRegion(const WorldRegion&,const Project&,const class AssetDatabase&,PreparedWorldRegion&,std::string&,const JobContext* =nullptr);
bool ValidateWorldQualifiedReferences(const WorldManifest&,const std::map<std::string,PreparedWorldRegion>&,std::string&);
struct RegionStatus {
    std::string id,state="unloaded",error;
    std::vector<std::string> pins;
    size_t entities=0,installed=0,bytes=0,retained=0,demands=0;
    bool visualReady=false;
    double preparationMs=0,integrationMs=0,largestUnitMs=0,loadMs=0;
};
struct StreamingStats {
    size_t pendingBytes=0,liveBytes=0,retainedBytes=0,active=0,pending=0,resourceResidentBytes=0,resourceCacheBudget=0;
    double integrationMs=0,largestUnitMs=0;
};
// One residency coordinator per played world. All mutation occurs in Advance,
// called at the outer frame boundary. Workers capture immutable data, never this.
class WorldStreaming {
public:
    WorldStreaming(RuntimeWorld&,ResourceManager&,Project,WorldManifest);
    ~WorldStreaming();
    uint64_t Request(const std::string&,bool preload,std::string& error,SceneObjectId requester=0,uint64_t slot=0);
    bool Activate(uint64_t);bool Release(uint64_t);
    void ReleaseRequester(SceneObjectId,uint64_t);
    bool Unload(const std::string&); // only succeeds after all demand has been released
    std::optional<RegionStatus> Status(uint64_t) const;
    std::vector<RegionStatus> Regions() const;
    const StreamingStats& Stats() const;
    std::string Owner(SceneObjectId) const; // "root" for persistent content
    SceneObjectId Resolve(const std::string&,SceneObjectId) const;
    bool Pin(const std::string&,const std::string&,bool);
    bool Adopt(SceneObjectId,const std::string&,std::string&);
    bool Interest(const std::string&,glm::vec3,float load,float retain,int priority,std::string&);
    void RemoveInterest(const std::string&);
    void Advance(bool paused);
    // Only committed residency is archived; partial installation/unload defers capture.
    bool SaveReady() const;
    bool CanChangeOrigin(std::string&) const;
    void ShiftOrigin(glm::dvec3);
    void ResumeOwnership();
    unsigned ArchiveVersion()const;
    void Persist(class SaveArchive&,unsigned version=1);
    std::string PersistentKey(SceneObjectId) const;
    SceneObjectId ResolvePersistentKey(const std::string&) const;
    // Tests hold a real read job before parsing. Production never sets this gate.
    void SetPreparationGate(std::function<bool(const JobContext&)>);
private:
    struct Impl;std::unique_ptr<Impl> m;
};
