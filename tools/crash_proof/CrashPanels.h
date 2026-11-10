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
    void Initialize(RuntimeWorld&, const CrashRig&, const MeshData& hood);
    void Step(RuntimeWorld&, const CrashRig&, double);
    void Persist(SaveArchive&);
    double PlasticExtent() const;
    bool AttachedTo(const DeformableInstance&,unsigned referencePart) const;
private:
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
