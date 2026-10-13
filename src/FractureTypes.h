#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>
class SaveArchive;
struct DeformableAsset;
// A physical partition is independent of render seams and display names.
struct FracturePart {
    std::string key;
    std::vector<unsigned> nodes,tets;
    glm::dvec3 proxyCenter{0},proxyHalf{.5};
    void Save(SaveArchive&);
};
struct FractureBond {
    std::string key;
    unsigned a=0,b=0;
    std::vector<glm::uvec2> pairs;
    glm::dvec3 normal{1,0,0},centroid{0};
    double area=1;
    // M79 optional authored strengths; zero inherits the family material.
    double tension=0,shear=0,compression=0;
    // Derived area moments, never mutable solver state or serialized caches.
    glm::dmat3 sectionInverse{0};
    double polarMoment=0;
    std::vector<glm::dvec3> sectionPoints;
    std::vector<double> sectionWeights; // Reference-area centroid weights for soft interfaces.
    void Save(SaveArchive&);
};
struct FractureCook {
    bool rigid=false;
    bool structural=false; // Opt-in section stress model; legacy cooks stay unchanged.
    double tension=10000,shear=10000,compression=0,compliance=1e-8;
    unsigned pieceBudget=256;
    std::vector<FracturePart> parts;
    std::vector<FractureBond> bonds;
    // Derived per simulation boundary face; -1 is exterior.
    std::vector<int> faceBond;
    std::vector<unsigned> nodePart;
    void Save(SaveArchive&);
    void SaveStructural(SaveArchive&);
};
bool PrepareFractureCook(DeformableAsset&,std::string&);
bool ImportFracturePartition(const std::string&,DeformableAsset&,std::string&);
DeformableAsset MakeFractureBlock(glm::uvec3 cells,glm::dvec3 size,bool rigid);
// Immutable topology, independent irreversible instance state. Pending failures
// are published only after the complete authoritative step.
struct FractureState {
    uint64_t revision=1;
    std::vector<uint8_t> broken,removed,pending,pendingRemoved;
    std::vector<glm::dvec3> multipliers;
    std::vector<double> tensionDemand,shearDemand;
    std::vector<unsigned> component;
    std::vector<unsigned> committed;
    std::string error;
    void Initialize(const FractureCook&);
    void BeginSubstep();
    void Observe(const FractureCook&,unsigned bond,glm::dvec3 force,glm::dvec3 normal);
    void ObserveSection(const FractureCook&,unsigned bond,glm::dvec3 localForce,glm::dvec3 localMoment);
    bool Request(const FractureCook&,unsigned bond,uint64_t expectedRevision);
    bool RequestRemoval(const FractureCook&,unsigned part,uint64_t expectedRevision);
    bool Commit(const FractureCook&);
    void Connectivity(const FractureCook&);
    bool FaceVisible(const DeformableAsset&,unsigned face)const;
    void Persist(SaveArchive&,const FractureCook&);
};
