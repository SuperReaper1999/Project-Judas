#pragma once
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>

#include "CollisionShapes.h"
#include "Classification.h"
#include "JointTypes.h"

// Opaque handle to a body inside PhysicsWorld. Deliberately not tied to
// any concrete physics-engine body-ID representation — no file outside
// PhysicsWorld.cpp needs to know what implements this class.
// Query policy is independent of bilateral physical collision masks.
struct PhysicsQueryFilter {
    CategoryMask includeLayers=kAllCategories, excludeLayers=0;
    CategoryMask requiredTags=0, excludedTags=0;
    bool includeSensors=false;
    std::vector<BodyHandle> ignoredBodies;
};

// A snapshot query result. No hit leaves hit=false; point/normal then have no
// meaning. BodyHandle includes slot generation. primitiveIndex identifies a
// compound child (0 for a single collider); terrain has one sampled surface.
struct PhysicsCastHit {
    bool hit=false, initialOverlap=false;
    BodyHandle body;
    glm::vec3 point{0}, normal{0};
    float distance=0, fraction=0;
    int primitiveIndex=0;
    ShapeType shape=ShapeType::Sphere;
    uint32_t childKey=0,feature=UINT32_MAX;
};
struct PhysicsClosestPoint {
    bool hit=false,contains=false,containmentKnown=false,normalUnique=true;
    BodyHandle body;glm::vec3 point{0},normal{0};float distance=0;
    uint32_t childKey=0,feature=UINT32_MAX;int primitiveIndex=0;ShapeType shape=ShapeType::Sphere;
};
struct PhysicsCastStats { unsigned broadphaseCandidates=0, filteredCandidates=0, primitivesTested=0; };

struct BodyTransform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // identity
};

// Motion authority is separate from inverse mass: a prescribed body supplies
// external work but receives no force/contact impulse integration of its own.
enum class BodyMotionType { Static, Dynamic, Kinematic };
enum class KinematicControl { Stopped, Target, Velocity };
struct KinematicMotionState {
    KinematicControl control=KinematicControl::Stopped;
    BodyTransform target;
    glm::vec3 linearVelocity{0}, angularVelocity{0}; // world COM m/s; world rad/s
    float remainingSeconds=0;
    bool targetNextStep=false;
};

// One box of a rigid body's collision geometry after transforming it into
// simulation/world space. Ordinary boxes return one entry; compound bodies
// return their children in stable order; spheres return none. Consumers such
// as the fluid solver can collide with the same geometry as rigid bodies.
struct BodyBox {
    glm::vec3 center{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 halfExtents{0.0f};
};

// The result of sweeping a shape through the world: the only question
// Judas's player controller ever asks the physics engine ("how far can
// this shape move, and what does it touch?"). What the answer MEANS —
// sliding, support, grounded state — is entirely PlayerController's
// decision; this struct carries no interpretation of its own.
struct ShapeSweepHit {
    float penetration = 0.0f; // signed geometry overlap depth at sweep origin
    bool hit = false;
    float distance = 0.0f;      // world-space distance traveled before the hit
    glm::vec3 normal{0.0f};     // meaningful only when `hit` is true; contact normal,
                                 // pointing back toward the caster — no default direction
                                 // is implied when there was no hit
    glm::vec3 point{0}; // known point on the collided surface
    BodyHandle hitBody;         // meaningful only when `hit` is true; identifies what was
                                 // hit (added in Milestone 7-A so a caller can distinguish
                                 // static world geometry from a pushable dynamic body —
                                 // still says nothing about WHAT that means, same as
                                 // `normal`; interpretation stays the caller's job)
};

// Wraps Judas's own physics engine (see docs/ARCHITECTURE.md, "Physics
// ownership" — Jolt Physics served this role through the first Milestone
// 7-Final attempt and is no longer used at all). Owns collision detection,
// contact resolution, and rigid-body integration.
//
// Ownership boundary (see docs/ARCHITECTURE.md for the full rationale):
// Judas owns gravity, reference frames, and world coordinates. This class
// and the engine behind it own collision/contact/rigid-body solving ONLY.
// Nothing in this class ever computes or applies gravity of its own —
// gravity always arrives from the outside via ApplyLinearAcceleration,
// sourced from Judas's own GravityField. Nothing in this header or its
// implementation assumes gravity points in any particular direction.
//
// No concrete physics-engine type appears in this header, so no other
// engine file needs to include one just to hold a body, ask for its
// transform, or sweep the player's collision shape.
class PhysicsWorld {
public:
    PhysicsWorld() = default;
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    bool SetPhysicalMaterial(BodyHandle,const std::string& identity,float friction,float restitution);
    bool GetPhysicalMaterial(BodyHandle,std::string& identity,float& friction,float& restitution)const;
    JointHandle CreateJoint(const JointSettings& settings);
    bool DestroyJoint(JointHandle handle);
    bool GetJoint(JointHandle handle,JointState& state) const;
    bool SetJoint(JointHandle handle,const JointSettings& settings);

    bool Init();
    void Shutdown();

    PhysicsClosestPoint ClosestPoint(const glm::vec3& point,float maximum,const PhysicsQueryFilter& filter={}) const;

    // Generic authored-pivot factory. Concave mesh is static-only.
    BodyHandle CreateShape(const Shape&,const BodyTransform&,bool dynamic,float mass,float friction,float restitution);

    BodyHandle CreateStaticBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                float friction, float restitution);

    // Same as above, with an explicit orientation. Added in Milestone 10
    // so demo geometry (steps, ramps) can be authored with its own "up"
    // aligned to wherever local gravity actually points at that position
    // — e.g. a step on a curved planet's surface, whose own local up is
    // radial, not world +Y — rather than being restricted to axis-aligned
    // boxes. The original 4-argument overload is unchanged and still
    // exactly equivalent to passing an identity rotation here.
    BodyHandle CreateStaticBox(const glm::vec3& position, const glm::quat& rotation,
                                const glm::vec3& halfExtents, float friction, float restitution);
    BodyHandle CreateStaticSphere(const glm::vec3& position, float radius, float friction,
                                   float restitution);
    // One static surface defined in its own local frame. Neither the
    // planet's centre nor orientation is baked into RadialTerrain itself;
    // ordinary body transforms place it like any other collision shape.
    BodyHandle CreateStaticTerrain(const glm::vec3& position, const glm::quat& rotation,
                                   std::shared_ptr<const RadialTerrain> terrain,
                                   float friction, float restitution);
    BodyHandle CreateDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                 float mass, float friction, float restitution);
    BodyHandle CreateDynamicSphere(const glm::vec3& position, float radius, float mass,
                                    float friction, float restitution);
    // A set of local boxes sharing one dynamic body's centre of mass, pose,
    // momentum, and inertia. `position` is the centre of mass; child centres
    // are relative to it. No semantic knowledge of the assembled object is
    // needed by physics or by callers querying its box geometry.
    BodyHandle CreateDynamicCompoundBoxes(const glm::vec3& position,
                                           const std::vector<CompoundBox>& boxes,
                                           float mass, float friction, float restitution);
    void DestroyBody(BodyHandle handle);
    // Milestone 29: how many bodies currently exist (and how many of those
    // are dynamic). A destroyed body's slot is reused by a later Create*;
    // its old handle stays invalid (generation-checked), so no consumer can
    // reach the new occupant through a stale handle.
    std::size_t AliveBodyCount() const;
    std::size_t DynamicBodyCount() const;

    // Contacts submitted to initial support and later impact solves in the
    // most recent Step (one entry per manifold point, so a resting box
    // on a floor reports 4). Read-only diagnostics for the profiler and the
    // debug view; nothing in simulation reads this back.
    struct DebugContact {
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        float penetration = 0.0f;
    };
    const std::vector<DebugContact>& LastStepContacts() const;
    std::size_t LastStepContactCount() const { return LastStepContacts().size(); }

    // Pair events are snapshots from the authoritative Step, ordered by
    // generation-checked body handles. One event per pair, not manifold point.
    enum class TouchPhase { Enter, Stay, Exit };
    struct TouchEvent {
        BodyHandle a,b;
        TouchPhase phase=TouchPhase::Enter;
        bool sensor=false;
        glm::vec3 point{0},normal{0},relativeVelocity{0}; // normal toward A; vB-vA at point
        bool impulseAvailable=false;
        float normalImpulse=0; // available accumulated support impulse
    };
    const std::vector<TouchEvent>& LastStepTouchEvents() const;
    void ClearTouchHistory();
    // Massless character geometry participates in queries/events, never rigid response.
    BodyHandle CreateQueryCapsule(float radius,float halfHeight,const BodyTransform&);
    // Owner-thread fixed-step prephase: retain motor observations until the
    // normal post-physics contact publication. No query geometry is rerun.
    void BeginPreStepQueryContacts();
    void ObserveQueryContact(BodyHandle,const ShapeSweepHit&,const glm::vec3& velocity);
    void FinishQueryTouches();
    void PersistTouches(class SaveArchive&,const std::function<uint64_t(BodyHandle)>&,const std::function<BodyHandle(uint64_t)>&,const std::function<bool(BodyHandle)>& include = {});
    void PersistBodyForces(class SaveArchive&,BodyHandle);
    bool SetBodySensor(BodyHandle handle,bool sensor);
    bool IsBodySensor(BodyHandle handle) const;
    // Internal articulation boundaries drive joints without appearing in casts.
    bool SetBodyQueriesEnabled(BodyHandle,bool);
    bool SetBodyEnabled(BodyHandle handle,bool enabled);
    bool IsBodyEnabled(BodyHandle handle) const;

    // Milestone 32: what the most recent Step's broadphase, narrowphase and
    // solver did. `possiblePairs` is the number of pairs an exhaustive
    // all-pairs pass would have had to test (every pair with at least one
    // movable body); `candidatePairs` is what the broadphase actually handed
    // the initial narrowphase; `collidingPairs` and `contactPoints` count
    // that initial pass. Later event work has separate impact counters;
    // LastStepContacts also includes its actual contact rows.
    struct StepStats {
        std::size_t bodies = 0;
        std::size_t dynamicBodies = 0;
        std::size_t sleepingBodies=0,sleepingIslands=0,awakeBodies=0;
        std::size_t possiblePairs = 0;
        std::size_t candidatePairs = 0;
        std::size_t layerRejectedPairs = 0;
        std::size_t collidingPairs = 0;
        std::size_t contactPoints = 0;
        std::size_t proxyReinsertions = 0;
        // FTFT4A geometry diagnostics; no influence on contact decisions.
        std::size_t geometryPredicates = 0;
        std::size_t geometryExactFallbacks = 0;
        std::size_t geometryUnresolved = 0;
        std::size_t geometryNumericGapFallbacks = 0;
        // Derived geometry caches; count actual work within this Step only.
        std::size_t orientationCacheHits = 0, orientationCacheRebuilds = 0;
        std::size_t boundCacheHits = 0, boundCacheRebuilds = 0;
        std::size_t shapeCacheRebuilds = 0, solverFrameBuilds = 0;
        std::size_t geometryCacheBytes = 0, geometryCacheAllocations = 0;
        std::size_t geometryCacheAllocatedBytes = 0, solverFrameNodeRequests = 0;
        std::size_t impactEvents=0, impactSafetyFallback=0, impactEventCapFallback=0;
        std::size_t impactQueries=0, impactSearchIterations=0, impactPeakIterations=0, impactSearchLimit=0;
        std::size_t impactUncertifiedAdvances=0; // proximity geometry cannot certify an advance
        // Bounded pair-local fallback after conservative-advancement exhaustion.
        // A contact interval shorter than max interval may be missed.
        std::size_t impactSamplingFallbacks=0, impactSamplingTests=0, impactSamplingResolutionCaps=0;
        double impactSamplingMaxInterval=0.0;
        std::size_t motionSegments=0, motionStorageBytes=0;
        int treeHeight = 0;
        double broadphaseMilliseconds = 0.0;
        double narrowphaseMilliseconds = 0.0;
        double solverMilliseconds = 0.0;
        double totalMilliseconds = 0.0;
    };
    const StepStats& LastStepStats() const;

    // Milestone 32: the one spatial query the broadphase exposes — every
    // body whose broadphase bound overlaps the box. Conservative (bounds are
    // grown by a margin); callers still test real geometry. Results are in
    // ascending handle-slot order, so iteration order is deterministic.
    // Read-only casts against current resolved geometry, independent of physical
    // masks. Direction is normalized internally; zero direction/invalid dimensions
    // throw invalid_argument. Zero distance asks for initial overlap. Orientation
    // stays fixed (no angular trajectory). Optional stats belong to the caller.
    PhysicsCastHit Raycast(const glm::vec3& origin,const glm::vec3& direction,float maximum,
        const PhysicsQueryFilter& filter={},PhysicsCastStats* stats=nullptr) const;
    PhysicsCastHit SphereCast(const glm::vec3& origin,float radius,const glm::vec3& direction,float maximum,
        const PhysicsQueryFilter& filter={},PhysicsCastStats* stats=nullptr) const;
    PhysicsCastHit CapsuleCast(const BodyTransform& pose,float radius,float halfHeight,const glm::vec3& direction,float maximum,
        const PhysicsQueryFilter& filter={},PhysicsCastStats* stats=nullptr) const;
    PhysicsCastHit BoxCast(const BodyTransform& pose,const glm::vec3& halfExtents,const glm::vec3& direction,float maximum,
        const PhysicsQueryFilter& filter={},PhysicsCastStats* stats=nullptr) const;

    void QueryBodiesInAabbInto(const glm::vec3& min,const glm::vec3& max,std::vector<BodyHandle>& output,const PhysicsQueryFilter& filter={})const;
    std::vector<BodyHandle> QueryBodiesInAabb(const glm::vec3& min, const glm::vec3& max, const PhysicsQueryFilter& filter = {}) const;
    bool SetCollisionFilter(BodyHandle,unsigned layer,CategoryMask mask);
    // Articulations may suppress selected generation-aware pairs without changing query policy.
    bool SetPairCollisionEnabled(BodyHandle a,BodyHandle b,bool enabled);
    bool GetCollisionFilter(BodyHandle,unsigned& layer,CategoryMask& mask) const;
    bool SetBodyTags(BodyHandle,CategoryMask tags);
    void SetPlayerCollisionFilter(unsigned layer,CategoryMask mask);
    // The (fat) broadphase bound of a body, for the debug view.
    bool GetBodyBroadphaseBounds(BodyHandle handle, glm::vec3& outMin, glm::vec3& outMax) const;

    // Read-only cache inspection: never recomputes or updates a cached result.
    // Useful for checking stored geometry against an independent fresh query.
    struct GeometryCacheState {
        BodyTransform currentPose, previousPose;
        glm::vec3 currentMin{0}, currentMax{0}, previousMin{0}, previousMax{0};
        float boundingRadius = 0;
        bool currentValid = false, previousValid = false;
    };
    bool GetBodyGeometryCacheState(BodyHandle handle, GeometryCacheState& state) const;

    // Milestone 32 oracle support (tests and the benchmark): every pair of
    // bodies whose shapes currently touch, found through the broadphase and
    // confirmed by the narrowphase, without stepping anything. A brute-force
    // reference lives in the tests, not here — see tests/BroadphaseTests.cpp.
    struct CollidingPair {
        BodyHandle a;
        BodyHandle b;
        int contactPoints = 0;
    };
    std::vector<CollidingPair> FindCollidingPairs() const;
    std::vector<BodyHandle> AliveBodies() const;
    bool GetBodyShape(BodyHandle handle, Shape& outShape, BodyTransform& outPose) const;

    // True only for a body created via CreateDynamic*. Added in Milestone
    // 7-A so a caller holding a ShapeSweepHit::hitBody can tell "pushable
    // object" apart from "static world geometry" without needing to
    // remember which handles it created dynamic vs. static itself.
    bool IsDynamicBody(BodyHandle handle) const;
    bool IsKinematicBody(BodyHandle handle) const;
    BodyMotionType GetMotionType(BodyHandle handle) const;
    bool SetMotionType(BodyHandle,BodyMotionType,float dynamicMass=1,bool preserveVelocity=false);
    // Target is the authored pivot pose; zero seconds means the next fixed
    // interval. Positive durations are bounded to 60 s. Last command wins.
    bool MoveKinematic(BodyHandle,const BodyTransform&,float seconds=0);
    bool SetKinematicVelocity(BodyHandle,const glm::vec3& worldCOMLinear,const glm::vec3& worldAngular);
    bool StopKinematic(BodyHandle);
    bool GetKinematicMotion(BodyHandle,KinematicMotionState&) const;
    void PersistKinematic(class SaveArchive&,BodyHandle);
    glm::vec3 GetPointVelocity(BodyHandle,const glm::vec3& worldPoint) const;
    glm::vec3 GetPreviousPointVelocity(BodyHandle,const glm::vec3& worldPoint) const;
    float GetMass(BodyHandle handle) const;
    bool SetMassDistribution(BodyHandle,float,const glm::mat3&);
    void PersistJointSolverState(SaveArchive&,JointHandle);
    void ApplyLinearImpulse(BodyHandle handle, const glm::vec3& impulse);    void ApplyImpulseAtPoint(BodyHandle handle, const glm::vec3& impulse,
                             const glm::vec3& worldPoint);

    // FTFT9 finite-mass normal coupling. One batch shares the ordinary
    // inelastic contact solver with the touching rigid island and, when
    // remainingRigidDt>0, only its already-cached signed-gap support.
    // It changes velocities only: no forces, pose integration, warm cache or
    // motion-ledger mutation. Normals point from the boundary into the particle.
    struct ContactParticle {
        glm::vec3 position{0}, velocity{0};
        float mass = 0;
    };
    struct ParticleBoundaryContact {
        std::size_t particleIndex = 0;
        BodyHandle body;
        glm::vec3 point{0}, normal{0}, wallVelocity{0};
        // false: prescribed boundary, with no reaction on the actual owner.
        // true: owner and its touching rigid island receive the same-solve reaction.
        bool twoWay = false;
    };
    // particleImpulses is per input contact, applied TO the particle. Optional
    // staticSupportImpulse is the external impulse applied BY physical static
    // support to the finite rigid island (excludes prescribed particle walls).
    struct ParticleContactStats {
        int iterations = 0;
        std::size_t rigidContactRows = 0;
        std::size_t persistentGapRows = 0;
        int normalAccelerations = 0;
        std::size_t normalMatrixProducts = 0;
        int normalAccelerationBreakdowns = 0;
        int normalAccelerationRejections = 0; // optional correction failed actual prepared-row admissibility
        int normalAccelerationBoundaryDeclines = 0; // moving prescribed normal endpoint: bounded PGS only
        float maximumClosingSpeed = 0; // excess closing beyond the existing row target
        float speedScale = 1;
        bool iterationCapReached = false;
    };
    ParticleContactStats GetParticleContactStats() const;
    void SolveParticleContacts(std::vector<ContactParticle>& particles,
                               const std::vector<ParticleBoundaryContact>& contacts,
                               std::vector<glm::vec3>& particleImpulses,
                               glm::vec3* staticSupportImpulse = nullptr,
                               float remainingRigidDt = 0);

    // Generic rigid-body velocity access — the same category as
    // GetTransform/ResetBody below, just for velocity. Added in Milestone
    // 7-A for the player's minimal push-on-contact behavior (see
    // PlayerController::FixedUpdate); nothing about gravity or gameplay
    // lives here, only the query/command itself.
    glm::vec3 GetLinearVelocity(BodyHandle handle) const;
    void SetLinearVelocity(BodyHandle handle, const glm::vec3& velocity);

    // Same shape, for angular velocity. Added in Milestone 8 for the flying
    // primitive's rotation control (see src/FlyingPrimitiveControl.h) and
    // for PlayerController's moving-support velocity carry (a rotating
    // support's own angular velocity contributes to the point-velocity a
    // standing player must inherit) — not vehicle-specific, ordinary
    // rigid-body angular velocity access of the same kind
    // GetLinearVelocity/SetLinearVelocity already provide.
    glm::vec3 GetAngularVelocity(BodyHandle handle) const;
    void SetAngularVelocity(BodyHandle handle, const glm::vec3& angularVelocity);
    // World-space inertia tensor of a dynamic body. Consumers that implement
    // torque controllers can convert a desired angular acceleration into a
    // physical torque without duplicating the body's shape/orientation math.
    glm::mat3 GetInertiaWorld(BodyHandle handle) const;

    // Shape support distances used for separation along an arbitrary world
    // direction. Directions are normalized internally. Finite primitives
    // use their current orientation; a terrain shape returns its conservative
    // radial bound. The player result is the capsule's maximum
    // support distance so it remains safe if the player's frame is
    // reoriented immediately after release.
    float GetBodySupportDistance(BodyHandle handle, const glm::vec3& worldDirection) const;
    float GetPlayerShapeMaxSupportDistance() const;

    // Integrates `acceleration` into the body's linear velocity over
    // `fixedDeltaTime` (velocity += acceleration * dt). This is how Judas
    // hands a sampled GravityField value to a physics body — the
    // middleware never computes gravity itself.
    void ApplyLinearAcceleration(BodyHandle handle, const glm::vec3& acceleration,
                                  float fixedDeltaTime);

    // Milestone 12: adds `force`/`torque` (world-space) to the body's
    // RigidBody force/torque accumulator (RigidBody::ApplyForce/
    // ApplyTorque) — genuine F=ma physics, turned into an actual velocity/
    // angular-velocity change by the NEXT Step() call (via
    // IntegrateRigidBody, which also clears both accumulators
    // afterward — see Step()'s own comment). Unlike
    // ApplyLinearAcceleration (which mutates velocity immediately and
    // unconditionally, mass-independent, exactly right for gravity) or
    // SetLinearVelocity/SetAngularVelocity (which overwrite velocity
    // outright), these two respect the body's actual mass/inertia and
    // compose additively with whatever else affected the accumulator or
    // velocity that same step — nothing here overwrites anything. Must be
    // called fresh every fixed step a force/torque should act; nothing
    // persists it across steps (a no-op call this step means no
    // contribution this step — see law #22/#32 in Project_Persistent_Memory.md).
    // A no-op on a static or unknown handle.
    void ApplyForce(BodyHandle handle, const glm::vec3& force);
    void ApplyTorque(BodyHandle handle, const glm::vec3& torque,bool wake=true);

    // Advances the simulation by exactly one fixed step. The caller owns
    // the accumulator that decides how many times to call this per frame.
    //
    // Milestone 32 order: (1) velocity from forces/torques; (2) broadphase
    // candidate pairs from the dynamic AABB tree (src/Broadphase.h) — no
    // all-pairs pass exists any more; (3) narrowphase at the current poses;
    // (4) accumulated-impulse contact solve (src/ContactSolver.h), position
    // integration from the solved velocities, direct penetration removal;
    // (5) proxy refresh. Before M32 positions were integrated before contacts
    // were detected; the reordering is what lets a resting body's gravity
    // increment be cancelled before it becomes a penetration.
    // Milestone 12: each dynamic body's own accumulated force/torque
    // (ApplyForce/ApplyTorque above) is integrated into its velocity/
    // angular velocity here (via RigidBody.h's IntegrateRigidBody — the
    // same free function the standalone physics/collision test suites
    // already exercise directly against a bare RigidBody, now genuinely
    // used by the live simulation for the first time), then cleared,
    // before position/orientation integrate from the resulting velocity —
    // so gravity (already folded into velocity directly via
    // ApplyLinearAcceleration, called before Step()) and any force/torque
    // applied this step compose into the same integration pass, in the
    // order they were applied, with no double-counting.
    void Step(float fixedDeltaTime);
    bool IsSleeping(BodyHandle) const;
    void Wake(BodyHandle);
    void SetSleepingEnabled(bool enabled); // diagnostic reference; default enabled
    void PersistSleep(class SaveArchive&,BodyHandle);

    BodyTransform GetTransform(BodyHandle handle) const;
    // Pose at the start of the most recent fixed step for dynamic bodies;
    // static bodies return their current pose.
    BodyTransform GetPreviousTransform(BodyHandle handle) const;
    // Passive, value-owned view of the latest resolved fixed-step path. Full
    // handle-generation validation happens before copying; no body storage or
    // solver state is exposed. Endpoint position repairs are retained in the
    // last endPose, rather than mistaken for a velocity-driven trajectory.
    struct BodyMotionSegment {
        double begin = 0, end = 0;
        BodyTransform start, endPose;
        glm::vec3 linearVelocity{0}, angularVelocity{0};
        bool movable = false;
        glm::vec3 pivotOffset{0}; // COM-relative authored pivot, value owned
        bool prescribed = false; // exponential rotation; dynamic Euler unchanged
    };
    std::vector<BodyMotionSegment> GetBodyMotionSegments(BodyHandle handle) const;
    // Uses the authoritative anchored position integrator, never restarts
    // quaternion integration from an intermediate observation.
    static BodyTransform EvaluateBodyMotionSegment(const BodyMotionSegment& segment, double time);
    std::vector<BodyBox> GetBodyBoxes(BodyHandle handle) const;
    std::vector<BodyBox> GetPreviousBodyBoxes(BodyHandle handle) const;

    // Restores a body to a pose with zero linear and angular velocity.
    void ResetBody(BodyHandle handle, const glm::vec3& position, const glm::quat& rotation);

    // --- Player collision shape & queries ---
    //
    // As of Milestone 5, the player is NOT a physics-engine body or
    // character controller of any kind — see docs/ARCHITECTURE.md,
    // "Player/controller ownership." Judas (PlayerController) owns the
    // player's position, velocity, orientation, and support interpretation
    // entirely as plain data. The only thing this class provides is a
    // capsule shape used purely for on-demand geometry queries; it is
    // never added to the world as a body, so it never appears in the
    // broadphase/contact-resolution pass and never needs a layer,
    // activation state, or mass of its own.
    bool CreatePlayerShape(float radius, float halfHeight);
    void DestroyPlayerShape();

    // Sweeps the player's capsule shape (at `fromCenter`/`rotation`) along
    // `displacement` (direction and length together) and reports the
    // closest thing it would hit, if any. This is Judas's ONLY question to
    // the physics engine about player movement or support — everything the answer is
    // used for (sliding along a surface, deciding "grounded," permitting a
    // jump) is PlayerController's decision, not this class's.
    // Queries can interpolate dynamic bodies between the previous and
    // current PhysicsWorld::Step transforms. Airborne movement uses this to
    // compare both trajectories over one fixed-step interval; support probes
    // pin both endpoints to the previous pose so they query the player's
    // start-of-step state. `bodyMotionStart`/`bodyMotionEnd` preserve timing
    // when a move-and-slide sweep continues after contact.
    // Same proven capsule/motion-ledger query, with per-consumer geometry.
    ShapeSweepHit SweepCapsuleMotion(float radius, float halfHeight,
        const glm::vec3& center, const glm::quat& rotation, const glm::vec3& displacement,
        bool interpolateMotion=false, float motionStart=0, float motionEnd=1,
        const PhysicsQueryFilter* filter=nullptr, unsigned collisionLayer=0,
        CategoryMask collisionMask=kAllCategories, bool bilateralFilter=true) const;
    ShapeSweepHit SweepPlayerShape(const glm::vec3& fromCenter, const glm::quat& rotation,
                                    const glm::vec3& displacement,
                                    bool interpolateDynamicBodyMotion = false,
                                    float bodyMotionStart = 0.0f,
                                    float bodyMotionEnd = 1.0f,
                                    const PhysicsQueryFilter* filter = nullptr) const;

private:
    PhysicsCastHit Cast(const Shape& shape,const BodyTransform& pose,const glm::vec3& direction,float maximum,
        const PhysicsQueryFilter& filter,PhysicsCastStats* stats) const;
    struct Impl;
    Impl* m_impl = nullptr;
};
