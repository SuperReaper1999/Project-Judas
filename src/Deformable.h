#pragma once
#include "Classification.h"
#include "FractureTypes.h"
#include "MeshData.h"
#include "PhysicsWorld.h"
#include "GravityField.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

class SaveArchive;
struct SceneObject;
enum class DeformableKind { Cloth, Solid };
struct DeformableMaterial {
    double density=0.4; // kg/m² for cloth, kg/m³ for solids
    double stretchCompliance=1e-6,shearCompliance=1e-5,bendCompliance=1e-3,volumeCompliance=1e-7;
    double damping=0.4,thickness=0.025,friction=0.4,airDrag=0;
    double yieldStrain=0,plasticRate=0,maximumPlasticStrain=0.4;
    glm::dvec3 airVelocity{0};
    void Save(SaveArchive&);
};
struct DeformableAttachment {
    enum class Kind { World, Body, Bone };
    std::string group,joint;
    Kind kind=Kind::World;
    uint64_t target=0;
    glm::dvec3 offset{0};
    bool enabled=true;
    void Save(SaveArchive&);
};
struct DeformableSettings {
    std::string asset;
    bool enabled=true,selfContact=true;
    unsigned substeps=4,iterations=4,collisionLayer=0;
    CategoryMask collisionMask=kAllCategories;
    DeformableMaterial material;
    std::vector<DeformableAttachment> attachments;
};
bool ValidDeformableSettings(const DeformableSettings&,std::string&);
std::map<std::string,std::string> DeformableProperties(const SceneObject&);
bool ApplyDeformableProperties(const std::map<std::string,std::string>&,SceneObject&,std::string&);

// Immutable decoded/baked topology. Render seams are separate vertices whose
// stored weights address deliberate simulation indices, never position welding.
struct DeformableBinding {
    glm::uvec4 nodes{0}; glm::dvec4 weights{1,0,0,0};
    void Save(SaveArchive&);
};
struct DeformableTriangle {
    glm::uvec3 nodes{0}; glm::dmat2 inverseRest{1};double area=0;
};
struct DeformableTet {
    glm::uvec4 nodes{0};glm::dmat3 rest{1},inverseRest{1};double volume=0;
};
struct DeformableBend {unsigned a=0,b=0;double rest=0;};
struct DeformableAsset {
    DeformableKind kind=DeformableKind::Cloth;
    std::shared_ptr<FractureCook> fracture;
    std::vector<glm::dvec3> nodes;
    std::vector<double> massWeights; // positive nodal mass multiplier
    std::vector<glm::uvec3> triangles;
    std::vector<glm::uvec4> tetrahedra;
    std::map<std::string,std::vector<unsigned>> groups;
    MeshData render;
    std::vector<DeformableBinding> binding;
    std::string sourceAsset,sourceFingerprint;
    // Derived, validated once during preparation. Not mutable instance state.
    std::vector<double> measures;
    std::vector<DeformableTriangle> cloth;
    std::vector<DeformableTet> solids;
    std::vector<DeformableBend> bends;
    std::vector<glm::uvec2> edges;
    std::vector<std::vector<unsigned>> neighbors,excludedFaces,excludedEdges;
    double cellSize=0;
};
bool PrepareDeformableAsset(DeformableAsset&,std::string&);
bool DecodeDeformableAsset(const std::vector<unsigned char>&,DeformableAsset&,std::string&);
std::string EncodeDeformableAsset(const DeformableAsset&);
DeformableAsset MakeDeformableSheet(unsigned columns,unsigned rows,double width,double height,unsigned renderSubdivision=1);
DeformableAsset MakeDeformableBlock(glm::uvec3 cells,glm::dvec3 size);
bool ImportDeformableCloth(const MeshData&,DeformableAsset&,std::string&);

struct DeformableLocation {uint64_t generation=0;unsigned triangle=0;glm::dvec3 weights{1,0,0};uint64_t topologyRevision=0;};
struct DeformableHit {bool hit=false;double distance=0;glm::dvec3 point{0},normal{0};DeformableLocation location;};
struct DeformableStats {size_t nodes=0,triangles=0,tets=0,contacts=0,candidates=0;unsigned substeps=0,iterations=0;double minimumJacobian=1,maximumStrain=0;bool capacityExceeded=false;};

// Targets are sampled by RuntimeWorld AFTER ordinary pose resolution, once per
// fixed step. Prescribed targets never receive reaction; Body targets do.
struct DeformableTarget {bool valid=false;glm::dmat4 previous{1},current{1};BodyHandle body;};
class DeformableInstance {
public:
    std::shared_ptr<const DeformableAsset> asset;
    DeformableSettings settings;
    uint64_t generation=0;
    std::vector<glm::dvec3> positions,previous,velocities;
    std::vector<glm::dmat3> plasticRest;
    std::vector<uint8_t> released;
    bool sleeping=false;double quietSeconds=0;
    std::string error;
    DeformableStats stats;
    FractureState fracture;
    double NodeMass(unsigned n)const{return mass.at(n);}
    bool PartLoad(unsigned part,glm::dvec3 value,bool impulse);
    bool FaceVisible(unsigned face)const{return !asset->fracture||fracture.FaceVisible(*asset,face);}
    void Cohesive(double h);
    void Initialize(std::shared_ptr<const DeformableAsset>,const DeformableSettings&,const glm::dmat4&,uint64_t);
    void Reset(const glm::dmat4&);
    bool SetMaterial(const DeformableMaterial&,std::string&);
    bool Impulse(const std::string& group,glm::dvec3 impulse);
    bool Impulse(const DeformableLocation&,glm::dvec3 impulse);
    bool Force(const std::string& group,glm::dvec3 force);
    bool Release(const std::string& group);
    void Wake(){sleeping=false;quietSeconds=0;}
    void Step(double dt,PhysicsWorld&,const GravityField&,const std::vector<DeformableTarget>&);
    void MapRender(double alpha,MeshData&)const;
    DeformableHit Raycast(glm::dvec3 origin,glm::dvec3 direction,double maximum)const;
    void Persist(SaveArchive&,bool materialFrame=true);
    glm::dvec3 Minimum()const;
    glm::dvec3 Maximum()const;
    double Mass()const;
    // Shared accelerated surface solve, including self contact with topology
    // exclusions. Both participants receive the same mass-weighted projection.
    static void SurfaceContacts(DeformableInstance&,DeformableInstance&,double);
private:
    std::vector<double> inverseMass,mass,lambda,volumeLambda;
    std::vector<glm::dvec3> start,forces,lastGravity;
    glm::dmat4 initial{1};
    struct SurfaceNode {glm::dvec3 low{0},high{0};unsigned left=~0u,right=~0u,primitive=~0u;};
    std::vector<SurfaceNode> faceTree,edgeTree;
    std::vector<unsigned> candidates,faceExcluded,edgeExcluded;
    unsigned queryEpoch=0;
    std::vector<glm::uvec2> selfFacePairs,selfEdgePairs;
    std::vector<glm::dvec3> contactReference,contactStartReference;
    bool selfCandidatesValid=false;
    void PrepareSelfCandidates();
    void InitializeSurfaceTree(std::vector<SurfaceNode>&,bool);
    std::vector<BodyHandle> rigidCandidates;
    void Internal(double);
    void Plastic(double);
    void Attach(double,double,PhysicsWorld&,const std::vector<DeformableTarget>&);
    void RigidContacts(double,double,PhysicsWorld&);
    void BuildSurfaceTree();
    void SurfaceCandidates(glm::dvec3,glm::dvec3,bool,const std::vector<unsigned>* exclusions=nullptr,unsigned minimum=0);
};
