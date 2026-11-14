#pragma once
#include "Classification.h"
#include "FractureTypes.h"
#include "MeshData.h"
#include "Narrowphase.h"
#include "PhysicsWorld.h"
#include "GravityField.h"
#include "deformables/PanelBending.h"
#include "deformables/SleepContacts.h"
#include "terrain/TerrainSolid.h"
#include <optional>
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
    // Stable authored structural section key; empty addresses an ordinary body.
    std::string part;
    Kind kind=Kind::World;
    uint64_t target=0;
    glm::dvec3 offset{0};
    bool enabled=true;
    // Optional finite strength of a physical body/material patch. Zero keeps
    // an unbreakable authored connection. Limits are resultant N and N m, not
    // per-node thresholds or a gameplay damage counter.
    double forceLimit=0,torqueLimit=0;
    double tearWork=0; // J of irreversible local mounting-sheet work; zero disables this failure mode.
    void Save(SaveArchive&);
};
struct DeformablePanelMaterial {
    bool enabled=false;
    double stiffness=2000,yieldMoment=80,hardening=80;
    void Save(SaveArchive&);
};
struct DeformableSettings {
    std::string asset;
    bool enabled=true,selfContact=true;
    unsigned substeps=4,iterations=4,collisionLayer=0;
    CategoryMask collisionMask=kAllCategories;
    DeformableMaterial material;
    DeformablePanelMaterial panel;
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
class StructuralSkin;
struct DeformableAsset {
    DeformableKind kind=DeformableKind::Cloth;
    std::shared_ptr<FractureCook> fracture;
    std::vector<glm::dvec3> nodes;
    std::vector<double> massWeights; // positive nodal mass multiplier
    std::vector<glm::uvec3> triangles;
    std::vector<glm::uvec4> tetrahedra;
    std::map<std::string,std::vector<unsigned>> groups;
    bool panelBinding=false; // Detailed rest offsets and smooth source shading follow local sheet frames.
    MeshData render;
    std::vector<DeformableBinding> binding;
    // Immutable derived presentation data, rebuilt during asset preparation.
    // Many detailed vertices share one physical triangle. Keep their original
    // vertex indices; neither render topology nor physical state is changed.
    struct PanelRenderGroup {
        glm::uvec3 nodes{0};
        glm::dvec3 restX{0},restY{0},restNormal{0};
        std::vector<unsigned> vertices;
    };
    std::vector<PanelRenderGroup> panelRenderGroups;
    std::string sourceAsset,sourceFingerprint;
    // Shared immutable detailed exterior, prepared by the resource worker.
    // Physical topology and per-instance damage remain separate.
    std::shared_ptr<const StructuralSkin> structuralSkin;
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

// RuntimeWorld samples physical Body targets at native mechanical boundaries.
// Prescribed skeletal targets currently use the final animation phase.
// Prescribed targets never receive reaction; Body targets do.
struct DeformableTarget {bool valid=false;glm::dmat4 previous{1},current{1};BodyHandle body;};
struct DeformableAttachmentState {
    glm::dvec3 force{0},moment{0}; // Last substep's actual body reaction, world-space.
    double peakForce=0,peakTorque=0; // Maximum resultant demand in the completed fixed step.
};
class DeformableInstance {
public:
    std::shared_ptr<const DeformableAsset> asset;
    DeformableSettings settings;
    uint64_t generation=0;
    std::vector<glm::dvec3> positions,previous,velocities;
    std::vector<glm::dmat3> plasticRest;
    std::vector<uint8_t> released;
    std::vector<DeformableAttachmentState> attachmentState;
    // Bounded transient completed releases. Restoring state never replays events.
    std::map<std::string,std::string> attachmentEvents;
    // Instance-owned signed-dihedral material; persisted with the physical state.
    std::optional<PanelBending> panelBending;
    bool sleeping=false;double quietSeconds=0;
    struct TerrainRegion {TerrainV2::SolidSnapshot solid;std::vector<BodyHandle> bodies;};
    // Derived current-world collision publication; never persisted as ownership.
    std::vector<TerrainRegion> terrainRegions;
    void RecoverTerrainContacts(double,PhysicsWorld&);
    std::string error;
    DeformableStats stats;
    // Derived exclusions for attached structural carriers; never saved handles.
    std::vector<BodyHandle> carrierCollisionExclusions;
    FractureState fracture;
    double NodeMass(unsigned n)const{return mass.at(n);}
    bool PartLoad(unsigned part,glm::dvec3 value,bool impulse);
    bool FaceVisible(unsigned face)const{return !asset->fracture||fracture.FaceVisible(*asset,face);}
    void Cohesive(double h);
    void Initialize(std::shared_ptr<const DeformableAsset>,const DeformableSettings&,const glm::dmat4&,uint64_t);
    void Reset(const glm::dmat4&);
    bool SetMaterial(const DeformableMaterial&,std::string&);
    bool SetPanelMaterial(const DeformablePanelMaterial&,std::string&);
    bool Impulse(const std::string& group,glm::dvec3 impulse);
    bool Impulse(const DeformableLocation&,glm::dvec3 impulse);
    bool Force(const std::string& group,glm::dvec3 force);
    bool Release(const std::string& group);
    void Wake(){sleeping=false;quietSeconds=0;}
    void ShiftOrigin(glm::dvec3 delta);
    void Step(double dt,PhysicsWorld&,const GravityField&,const std::vector<DeformableTarget>&);
    // Owner-thread chronological coupling. Forces and irreversible publication
    // belong to the whole fixed step, not each rigid interval.
    bool BeginPhysicalStep();
    void AdvancePhysicalInterval(double dt,unsigned subdivisions,PhysicsWorld&,const GravityField&,const std::vector<DeformableTarget>&,bool rigidPoseIsCurrent);
    void EndPhysicalStep(double dt,PhysicsWorld&);
    bool PhysicalStepActive()const{return integrating;}
    void MapRender(double alpha,MeshData&)const;
    DeformableHit Raycast(glm::dvec3 origin,glm::dvec3 direction,double maximum)const;
    void Persist(SaveArchive&);
    double AttachmentWork(unsigned)const;
    bool CommitPanelFailures();
    glm::dvec3 Minimum()const;
    glm::dvec3 Maximum()const;
    double Mass()const;
    // Shared accelerated surface solve, including self contact with topology
    // exclusions. Both participants receive the same mass-weighted projection.
    static void SurfaceContacts(DeformableInstance&,DeformableInstance&,double);
private:
    bool integrating=false,integrationTargetMoving=false,currentRigidContactPose=false;
    unsigned integrationSubsteps=1;
    double integrationDeltaTime=0;
    std::vector<glm::dvec3> intervalReference;
    JudasDeformables::SleepContacts sleepingContacts;
    std::vector<double> inverseMass,mass,lambda,volumeLambda;
    std::vector<glm::dvec3> start,forces,lastGravity;
    std::vector<glm::dvec3> attachmentImpulse,attachmentMoment;
    std::vector<uint8_t> attachmentPending;
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
    struct PreparedRigidPrimitive {PrimitivePose geometry;glm::dvec3 center;glm::dquat rotation,inverse;};
    struct PreparedRigidContact {
        bool ready=false,valid=false,expanded=false,responseReady=false;
        double inverseMass=0;glm::dvec3 center{0};glm::dmat3 inverseInertia{0};
        Shape shape;BodyTransform pose;Aabb bounds;
        glm::dquat rotation,inverse;
        std::vector<PreparedRigidPrimitive> primitives;
    };
    // Candidate-indexed scratch, valid only inside one frozen material substep.
    std::vector<PreparedRigidContact> rigidPreparation;

    void Internal(double);
    void Plastic(double);
    void Attach(double,double,PhysicsWorld&,const std::vector<DeformableTarget>&,std::array<BodyHandle,64>&,unsigned&);
    void RigidContacts(double,double,PhysicsWorld&,std::array<BodyHandle,64>&,unsigned&);
    void BuildSurfaceTree();
    void SurfaceCandidates(glm::dvec3,glm::dvec3,bool,const std::vector<unsigned>* exclusions=nullptr,unsigned minimum=0);
};
