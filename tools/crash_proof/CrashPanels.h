#pragma once
#include "CrashRig.h"
#include "Deformable.h"
class RuntimeWorld;
class ResourceManager;
class AssetDatabase;
class SaveArchive;
// Private acceptance-fixture authoring/telemetry. Physics is an ordinary scene
// deformable, integrated and persisted by RuntimeWorld, never a second owner.
class CrashPanel {
public:
    SceneObjectId owner=0;
    MeshData mapped;
    std::vector<double> peakForces,peakMoments;
    void Author(Scene&,const DeformableAsset&,const CrashRig&,AssetDatabase&,ResourceManager&,const MeshData&);
    void Bind(RuntimeWorld&,SceneObjectId existing=0);
    DeformableInstance& Material()const;
    void Observe();
    void Persist(SaveArchive&); // diagnostic/hash capture only; restoration is WorldPersistence
    double PlasticExtent()const;
    double MountWork(unsigned)const;
    bool CommitMountFailures();
    static constexpr double MountTearWork=20;
    glm::dvec3 Center()const;
    glm::dvec3 Momentum()const;
    double maximumSpeed=0,maximumKineticEnergy=0;
    bool AttachedTo(const DeformableInstance&,unsigned referencePart)const;
    void Map();
private:
    RuntimeWorld* world=nullptr;
    std::vector<uint8_t> observedRelease;
};
MeshData ExtractCrashHood(MeshData& body,const std::vector<std::string>& additionalParts={});
