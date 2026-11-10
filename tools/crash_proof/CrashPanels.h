#pragma once
#include "CrashRig.h"
#include "Deformable.h"
class RuntimeWorld;
class SaveArchive;
// Bounded Stage-1 physical hood experiment. Uses the existing finite-mass
// deformable material/contact kernel; no independent rigid chassis is created.
class CrashPanel {
public:
    DeformableInstance material;
    MeshData mapped;
    std::vector<double> peakForces,peakMoments;
    void Initialize(RuntimeWorld&, const CrashRig&, const MeshData& hood);
    void Step(RuntimeWorld&, const CrashRig&, double);
    void Persist(SaveArchive&);
    double PlasticExtent() const;
    double MountWork(unsigned) const;
    bool CommitMountFailures();
    // Effective mounting-sheet tear capacity, J. Physical work, not crash count.
    static constexpr double MountTearWork=20;
    glm::dvec3 Center() const;
    glm::dvec3 Momentum() const;
    double maximumSpeed=0,maximumKineticEnergy=0;
    bool AttachedTo(const DeformableInstance&,unsigned referencePart) const;
private:
    CategoryMask attachedCollisionMask=kAllCategories;
    void RefreshCollisionMask();
    std::vector<DeformableTarget> targets;
    std::vector<unsigned> carrierParts;
    std::shared_ptr<DeformableAsset> shell;
    MeshData exterior;
    std::vector<DeformableBinding> exteriorBinding;
    std::vector<double> normalOffset;
public:
    void Map();
};
// Explicit authored panel region, not automatic arbitrary tetrahedron tearing.
MeshData ExtractCrashHood(MeshData& body,const std::vector<std::string>& additionalParts={});
