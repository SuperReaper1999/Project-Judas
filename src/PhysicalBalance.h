#pragma once
#include "PhysicsWorld.h"
#include <array>
#include <string>

// Explicit, bounded two-chain articulation control. These mappings are native
// runtime handles, rebuilt from authored skeleton keys by the world owner.
// Body zero is the dynamic root; all remaining bodies are parent-first.
struct BalanceBody {
    BodyHandle body;
    JointHandle joint;
    int parent=-1;
    glm::quat restRotation{1,0,0,0}; // body orientation in authored root frame
};
struct BalanceContactPatch {
    glm::vec3 offset{0},normal{0,1,0};
    float radius=.2f;
};
struct BalanceEffector {
    unsigned upper=0,lower=0,foot=0;
    glm::vec3 contactOffset{0}; // physical foot frame, metres
    glm::vec3 halfExtents{.2f}; // conservative swing clearance envelope
    std::vector<BalanceContactPatch> patches;
    float normalAcceptance=.65f;
};
// One get-up pose: joint angles (degrees) about the rig's lateral axis
// (up x forward), applied to both chains, held for `seconds`.
struct BalanceGetUpKey {float seconds=1,hip=0,knee=0,ankle=0;};
struct BalanceSettings {
    glm::vec3 up{0,1,0},forward{0,0,1}; // authored root-local axes, not world axes
    std::array<BalanceEffector,2> effectors;
    float stiffness=800,damping=80,maxTorque=150;
    float positionResponse=.8f,velocityResponse=.3f,maximumLean=.25f;
    float legLeanFraction=1,trunkLeanFraction=1;
    float plantedOrientationWeight=0; // 0 preserves contacting face, 1 uses authored upright foot reference
    float standingHeight=1.2f,standingTolerance=.18f,standingSpeed=.16f;
    float settleSeconds=1,attemptSeconds=15,referenceSeconds=3;
    float gravityCoherence=.96f;
    float stepTrigger=.18f,stepReach=.4f,stepHeight=.18f,stepSeconds=.7f;
    float touchdownTolerance=.12f;
    float transferLoadFraction=.6f,transferRadiusFraction=.75f,transferSeconds=2;
    unsigned maximumSteps=3;
    // Capture-point stepping controller (the default) for balance and
    // corrective steps after measured standing (world-frame trunk, tracked
    // capture-point placement, ZMP-bounded stance, trailing-foot follow-up
    // steps). Get-up and the initial stand use the established controller
    // either way. false selects the established stepping controller.
    // JUDAS_BALANCE_CAPTURE_STEPPING=0/1 forces it off/on for validation.
    bool captureStepping=true;
    // Get-up from lying: when the ordinary stand has not lifted the root past
    // getUpUpright within getUpPatience seconds of the task, a staged
    // joint-space motion chosen by which way the rig faces, then the ordinary
    // stand takes over once the root is upright on both feet. A failed rise
    // is retried from a fresh assessment, up to getUpAttempts. Defaults were
    // searched under MK2 for the balance lab's three rigs (both facings).
    std::array<BalanceGetUpKey,3> getUpFaceUp{{{.71f,161.35f,-78.00f,-70.21f},{.18f,151.17f,-99.20f,45.30f},{.81f,.60f,-2.96f,5.61f}}};
    std::array<BalanceGetUpKey,3> getUpFaceDown{{{.73f,143.05f,-1.02f,82.19f},{.20f,154.62f,-87.71f,94.34f},{.47f,150.48f,-28.05f,-17.34f}}};
    float getUpUpright=.6f,getUpPatience=2.5f;
    unsigned getUpAttempts=4;
    PhysicsQueryFilter query;
};
// Stable authored skeleton keys; physical body/constraint handles are resolved
// by RuntimeWorld only after the immutable asset and articulation are ready.
struct BalanceChainKeys {std::string upper,lower,foot;};
struct BalanceDefinition {
    bool enabled=true,visualize=false;
    BalanceSettings settings;
    std::array<BalanceChainKeys,2> chains;
};
bool ValidBalanceDefinition(const BalanceDefinition&,std::string&);
std::string SerializeBalanceDefinition(const BalanceDefinition&);
bool ParseBalanceDefinition(const std::string&,BalanceDefinition&,std::string&);
enum class BalanceTask { None, Stand, Recover };
enum class BalanceProgress { Idle, Executing, Succeeded, Failed, Cancelled };
struct BalanceSupport {
    BodyHandle body;
    glm::vec3 point{0},velocity{0},normal{0}; // point: load-weighted centre of pressure
    glm::vec3 centre{0}; // unweighted centroid of the accepted contacts
    float load=0;
    unsigned contacts=0;
};
struct BalanceObservation {
    bool available=false;
    glm::vec3 com{0},velocity{0},gravity{0},up{0};
    std::array<BalanceSupport,2> supports;
    float upright=0,height=0,speed=0;
    float mass=0;
    unsigned saturated=0;
};
// Semantic history is separate from observations and handle mapping. Save
// participants can retain clocks/references/plans and reacquire contacts.
struct BalanceHistory {
    BalanceTask task=BalanceTask::None;
    BalanceProgress progress=BalanceProgress::Idle;
    bool stepping=false,effort=true,hasStood=false;
    float elapsed=0,settled=0,referenceTime=0;
    bool frameReady=false;
    glm::quat rootReference{1,0,0,0},supportReference{1,0,0,0};
    BodyHandle frameSupport;
    std::vector<glm::quat> initialTargets;
    std::vector<glm::quat> postStepTargets;
    float postStepTime=0;
    int swing=-1;
    int lastSwing=-1;
    BodyHandle plannedSupport;
    glm::vec3 startLocal{0},landingLocal{0};
    glm::vec3 landedLocal{0};
    BodyHandle stanceSupport;
    glm::vec3 stanceLocal{0},rootStartLocal{0},comStartLocal{0};
    float swingTime=0,transferTime=0;
    bool lifted=false;
    unsigned landings=0;
    std::string reason;
};
class SaveArchive;
class PhysicalBalanceController {
public:
    bool Configure(PhysicsWorld&,const std::vector<BalanceBody>&,
                   const BalanceSettings&,std::string&);
    bool Request(PhysicsWorld&,BalanceTask,bool allowSteps,bool effort,std::string&);
    void Cancel(PhysicsWorld&);
    void ShiftOrigin(glm::dvec3 delta);
    // Called before one ordinary PhysicsWorld step. Acceleration is sampled
    // for EVERY mapped physical body through its ordinary world gravity path.
    void Prepare(PhysicsWorld&,const std::vector<glm::vec3>& accelerations,float dt);
    void PersistHistory(SaveArchive&,const std::function<uint64_t(BodyHandle)>&,const std::function<BodyHandle(uint64_t)>&);
    const BalanceObservation& Observation()const{return m_observation;}
    const BalanceHistory& History()const{return m_history;}
    const BalanceSettings& Settings()const{return m_settings;}
    const std::vector<BalanceBody>& Bodies()const{return m_bodies;}
private:
    void Release(PhysicsWorld&);
    void Fail(PhysicsWorld&,const char*);
    bool Observe(PhysicsWorld&,const std::vector<glm::vec3>&);
    bool PlanStep(PhysicsWorld&,const glm::vec3&,const glm::quat&);
    bool SwingReferences(PhysicsWorld&,const glm::quat&,std::vector<glm::quat>&);
    // Capture stepping controller (PhysicalBalanceCapture.cpp).
    bool CaptureStep(PhysicsWorld&,const glm::quat& upright,float dt);
    bool CapturePlan(PhysicsWorld&,glm::vec3 capture,const glm::quat& upright);
    bool CaptureFoothold(PhysicsWorld&,int foot,const BodyTransform& pose,glm::vec3 base,glm::vec3 displacement,const glm::quat& upright,BodyHandle& support,glm::vec3& destination) const;
    void CaptureRestSoles(PhysicsWorld&,const glm::quat& upright,std::array<glm::vec3,2>& soles) const;
    bool CaptureLeg(PhysicsWorld&,unsigned chain,glm::vec3 hip,glm::vec3 ankle,const glm::quat& upright,std::vector<glm::quat>& attitudes) const;
    std::vector<BalanceBody> m_bodies;
    BalanceSettings m_settings;
    BalanceHistory m_history;
    BalanceObservation m_observation;
    std::vector<PhysicsWorld::BodyContactSample> m_contacts;
    std::vector<glm::quat> m_desired;
    std::vector<glm::quat> m_captureAhead; // swing reference one control step ahead
    // Root offset from the soles' midpoint in the upright frame, measured at
    // the task's first confirmed standing. Derived from measurement and not
    // persisted: a restored task measures it again when it next stands.
    glm::vec3 m_captureStance{0};bool m_captureStanceReady=false;
    bool m_waitingForSample=false;
    // Get-up progress (PhysicalBalanceGetUp.cpp). Derived and not persisted:
    // a restored task assesses the lying rig again and restarts the motion.
    bool GetUp(PhysicsWorld&,const glm::vec3& up,float dt);
    bool m_getUp=false,m_getUpFaceUp=false;float m_getUpTime=0;unsigned m_getUpAttempts=0;bool m_getUpReached=false;
};
