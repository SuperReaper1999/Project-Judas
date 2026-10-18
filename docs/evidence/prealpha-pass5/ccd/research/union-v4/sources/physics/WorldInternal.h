#pragma once

// Private, single-owner implementation state. Never an engine or SDK API.
// See WorldModules.md for module responsibilities and publication order.
#include "PerformanceProfiler.h"
#include "JobSystem.h"
#include <exception>
#include "PhysicsWorld.h"
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "SaveArchive.h"
#include "PhysicsCastGeometry.h"
#include "OriginLimits.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <tuple>
#include <utility>
#include <unordered_set>
#include <vector>

#include "Broadphase.h"
#include "CollisionShapes.h"
#include "ContactSolver.h"
#include "Contacts.h"
#include "Narrowphase.h"
#include "RadialTerrain.h"
#include "RigidBody.h"
#include "RigidMotion.h"
#include "RigidMotionBounds.h"
#include "ImpactSolver.h"
#include "ImpactPairVisitSet.h"
#include "JointSolver.h"
#include <atomic>
#include <cstdlib>

#include "WorldProfiling.h"
#include "MeshUnionContinuity.h"

namespace JudasPhysicsDetail {

inline bool FiniteVector(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline bool UsableVelocity(glm::vec3 v) {
    return FiniteVector(v) && std::isfinite(glm::length(v));
}
inline bool UsableKinematicPose(const BodyTransform &pose) {
    const double norm = glm::dot(glm::dquat(pose.rotation), glm::dquat(pose.rotation));
    return FiniteVector(pose.position) && std::isfinite(norm) && norm > 1e-12 && norm < 1e12;
}
inline bool KinematicShape(const Shape &shape) {
    if (shape.type == ShapeType::Box || shape.type == ShapeType::Sphere ||
        shape.type == ShapeType::ConvexHull)
        return true;
    if (shape.type != ShapeType::CompoundBoxes || shape.boxes.empty())
        return false;
    return std::all_of(shape.boxes.begin(), shape.boxes.end(), [](const auto &c) {
        return c.type == ShapeType::Box || c.type == ShapeType::Sphere ||
               c.type == ShapeType::ConvexHull;
    });
}

// Milestone 32: how far a proxy's fat bound extends past its tight bound.
// A body that moves less than this within its fat box costs the tree
// nothing; one that escapes is reinserted. Static bodies use the same
// margin (only kinematically-driven static bodies such as doors move).
constexpr float kBroadphaseMargin = 0.1f;
// Continuous-impact stall cutoff: this many consecutive conservative advances,
// each covering less than this fraction of the interval still to search. Chosen
// from a sweep of 2-12 advances and 0.5-5% (docs/evidence/ccd-mesh-contacts):
// fewer advances also cut off ordinary short stalls on flat meshes, slower there.
constexpr int kImpactStallAdvances = 8;
constexpr double kImpactStallFraction = .05;
// Tolerance added to the player sweep's query box.
constexpr float kSweepQueryEpsilon = 1.0e-3f;
// Warm starting: a new contact inherits last step's converged impulses from
// the same body pair/primitive pair when its anchor (in body A's frame) is
// within this distance of a cached one and the normals agree.
constexpr float kWarmStartAnchorDistance = 0.05f;
constexpr float kWarmStartNormalDot = 0.9f;

struct ContactKey {
    unsigned int slotA = 0, slotB = 0;
    unsigned int generationA = 0, generationB = 0;
    int partA = 0, partB = 0;
    bool operator<(const ContactKey &o) const {
        return std::tie(slotA, slotB, partA, partB) < std::tie(o.slotA, o.slotB, o.partA, o.partB);
    }
    bool SameBodies(const ContactKey &o) const {
        return slotA == o.slotA && slotB == o.slotB && partA == o.partA && partB == o.partB &&
               generationA == o.generationA && generationB == o.generationB;
    }
};
struct CachedContact {
    ContactKey key;
    glm::dvec3 localAnchorA{0.0};
    glm::vec3 normal{0.0f};
    float normalImpulse = 0.0f;
    glm::vec3 tangentImpulse{0.0f};
    float separation = 0;
    uint64_t observationRevisionA = 0, observationRevisionB = 0;
};

std::vector<BodyBox> BoxesAt(const Shape &shape, const glm::vec3 &position,
                             const glm::quat &orientation);

glm::mat3 CompoundInverseInertia(float mass, const std::vector<CompoundBox> &boxes);

} // namespace

using JudasPhysicsDetail::ContactKey;
using JudasPhysicsDetail::CachedContact;
using JudasPhysicsDetail::PhysicsWorkSample;
using JudasPhysicsDetail::PhysicsWorkTimer;

// Distance (and separating normal/owning body index) from the player's
// capsule, placed at a given segment, to the closest world body — used by
// SweepPlayerShape's substep march. `bodyIndex == -1` means no body was
// found closer than `distance`'s initial +infinity (never happens once any
// world geometry exists, but keeps the result total).
struct ClosestBodyResult {
    float distance = std::numeric_limits<float>::max();
    glm::vec3 normal{0.0f};
    int bodyIndex = -1;
    glm::vec3 point{0};
};

struct PhysicsWorld::Impl {
    const uint64_t worldToken = []() {
        static std::atomic<uint64_t> next{1};
        return next.fetch_add(1);
    }();
    struct PoseBound {
        glm::vec3 position{0};
        glm::quat orientation{1, 0, 0, 0};
        Aabb bound;
        bool valid = false;
    };
    struct Body {
        RigidBody rigidBody;
        // Previous fixed-step pose for player movement sweeps against
        // dynamic targets; initialized together with the current pose and
        // refreshed immediately before every physics integration.
        glm::vec3 previousPosition{0.0f};
        RigidMotion motion;
        glm::quat previousOrientation{1.0f, 0.0f, 0.0f, 0.0f};
        Shape shape;
        // AddBody replaces the entire cache on slot reuse. Explicit static
        // geometry replacement invalidates shape-derived bounds in place.
        float boundingRadius = 0;
        std::optional<ContactPreparedOrientation> orientationCache;
        std::optional<PreparedShapeBounds> boundPreparation;
        bool boundPreparationValid = false;
        PoseBound currentBound, previousBound;
        unsigned collisionLayer = 0;
        CategoryMask collisionMask = kAllCategories, tags = 0;
        std::string physicalMaterial;
        float friction = 0.5f;
        float restitution = 0.0f;
        bool sensor = false, enabled = true, queryOnly = false, queriesEnabled = true,
             simulationSuspended = false;
        bool isDynamic = false, isKinematic = false;
        KinematicMotionState kinematic;
        double kinematicActiveDuration = 0;
        bool HasMotion() const {
            return !simulationSuspended && (isDynamic || isKinematic);
        }
        bool sleeping = false, deferredAcceleration = false;
        float quietSeconds = 0;
        glm::vec3 lastAcceleration{0};
        bool alive = false;
        // Milestone 29: a slot is reused after DestroyBody; the generation
        // in the handle's upper bits makes a handle from a previous
        // occupant of the same slot invalid instead of aliasing the new one.
        unsigned int generation = 0;
        uint64_t contactObservationRevision =
            0; // invalidates only native observations, not solver warm starts
        // Milestone 32: this body's leaf in the broadphase tree.
        int proxy = DynamicAabbTree::kNull;
    };

    static constexpr unsigned int kSlotBits = 20;
    static constexpr unsigned int kSlotMask = (1u << kSlotBits) - 1u;

    std::vector<Body> bodies;
    // Slots of destroyed bodies awaiting reuse, and the sorted slots of
    // every live body — the loops below iterate this so a world whose
    // entities were unloaded pays for the bodies that exist, not for the
    // slots they once occupied.
    std::vector<unsigned int> freeSlots;
    std::vector<unsigned int> aliveSlots;
    std::vector<PhysicsWorld::DebugContact> lastStepContacts;
    using TouchKey = std::pair<unsigned, unsigned>;
    std::map<TouchKey, PhysicsWorld::TouchEvent> previousTouches, stepTouches, preStepQueryTouches;
    bool queryTouchesPending = false, collectingPreStepQueries = false, coupledStep = false,
         stepping = false;
    std::vector<PhysicsWorld::TouchEvent> touchEvents;
    void Observe(unsigned a, unsigned b, const Contact &c, float impulse = 0, bool preStep = false);
    void FinishTouches();

    // Milestone 32: broadphase, the per-step candidate list, the solver
    // (reused so its storage is not reallocated every step) and statistics.
    DynamicAabbTree tree;
    std::vector<std::pair<unsigned int, unsigned int>> candidatePairs;
    mutable std::vector<unsigned int> queryScratch;
    struct JointRecord {
        JointHandle handle;
        JointState state;
        std::array<float, 12> warm{};
        float previousCoordinate = 0, restoredContinuationDt = 0;
    };
    void InvalidateRestoredContinuation(unsigned slot);
    std::string ContinuationBodyContext(const Body &body) const;
    std::string ContinuationJointContext(const JointRecord &joint) const;
    std::vector<JointRecord> joints;
    std::set<std::pair<unsigned, unsigned>> suppressedPairs;
    JointSolver jointSolver, eventJointSolver;
    RigidBody worldAnchor;
    bool JointActive(const JointRecord &joint) const;
    std::vector<JointInput> JointInputs(const std::vector<unsigned char> *included = nullptr);
    bool sleepingEnabled = true;
    void Wake(unsigned slot);
    uint64_t sleepTraceStep = 0;
    static constexpr unsigned kNoSleepMember = std::numeric_limits<unsigned>::max();
    struct SleepIsland {
        unsigned first = kNoSleepMember, last = kNoSleepMember, count = 0;
        bool quiet = true, anchored = false;
        float seconds = 1000;
    };
    // Derived, step-local graph data. Capacities survive, but every logical
    // member/edge is rebuilt from current generation-validated physical state.
    std::vector<unsigned> sleepParents, sleepNext, sleepRoots;
    std::vector<SleepIsland> sleepIslands;
    std::size_t sleepGraphEdges = 0, sleepIslandCount = 0, sleepScratchBytes = 0;
    void Settle(float dt);
    ContactSolver solver;
    ContactSolver particleSolver; // independent storage; never replays the rigid warm cache
    PhysicsWorld::ParticleContactStats particleContactStats;
    ImpactSolver impactSolver;
    struct StartContact {
        unsigned a, b;
        std::size_t constraint;
        float friction, warmNormal;
        glm::vec3 warmTangent;
        bool newImpact;
    };
    std::vector<StartContact> startContacts;
    std::vector<std::array<unsigned, 4>> separatedPairs;
    std::vector<std::array<unsigned, 4>> supportPairs;
    std::vector<std::array<unsigned, 4>>
        spinningPairs; // sorted; continuous search with a penetration target
    struct ScheduledImpact {
        std::array<unsigned, 4> key;
        double time;
    };
    std::vector<ScheduledImpact> scheduledImpacts;
    std::vector<unsigned> impactCounts;
    std::vector<unsigned char> impactCapped;
    double stepDuration = 0;

    // A derived CCD sample, never an integration anchor. Many incident pairs
    // ask for the same body's pose at the same event time. Keep its exact
    // represented pose/orientation preparation once until that trajectory
    // changes. Time and generation are part of the key; every step and every
    // Begin/ChangeVelocity invalidates samples explicitly.
    struct ImpactPoseSample {
        RigidBody pose;
        std::optional<ContactPreparedOrientation> orientation;
        double time = std::numeric_limits<double>::quiet_NaN();
        unsigned generation = 0;
        glm::vec3 tightPosition{0};
        glm::quat tightOrientation{1, 0, 0, 0};
        Aabb tightBound;
        bool tightValid = false;
        Aabb remainingBound;
        double boundStart = std::numeric_limits<double>::quiet_NaN(), boundFinish = 0;
        unsigned boundGeneration = 0;
    };
    std::vector<ImpactPoseSample> impactPoseSamples, impactAnchorSamples;
    double impactQueryAnchorTime = 0;
    std::size_t impactPoseEvaluations = 0, impactPoseReuses = 0, impactOrientationBuilds = 0,
                impactTruthRequests = 0, impactTruthReuses = 0, impactBoundsRejected = 0;
    std::size_t impactActualBoundsRejected = 0;
    struct WorkDiagnostics {
        bool active = false;
        PhysicsWorkSample toi, geometry, impactIslands, impactSolve, jointPreparation,
            impactObservations, continuationProof, continuationCache;
        std::size_t continuationAttempts = 0, continuationAccepted = 0, continuationPartitions = 0,
                    continuationOverBudget = 0;
        std::size_t coupledFallbackEvents = 0, elasticFallbackEvents = 0,
                    geometryFallbackEvents = 0, peakImpactBodies = 0, peakImpactRows = 0;
        std::size_t candidatePasses = 0, candidateQueries = 0, candidateLeaves = 0;
    } work;
    // Indexed output only: workers never mutate physics bodies, geometry
    // caches, solver rows, touches, warm starts or event scheduling.
    struct InitialContactGeometry {
        unsigned a = 0, b = 0;
        float margin = 0;
        const ContactPreparedOrientation *orientationA = nullptr;
        const ContactPreparedOrientation *orientationB = nullptr;
        ContactManifold manifold;
        bool touching = false, ready = false;
    };
    struct InitialGeometryDiagnostics {
        ContactGeometryDiagnostics contacts;
        CollisionGeometryStats cooked;
        std::size_t manifolds = 0;
    };
    // Optional borrowed process service; no physical state or worker ownership.
    JobSystem *contactJobs = nullptr;
    unsigned contactJobLanes = 4;
    static constexpr std::size_t kMaxInitialContactPairs = 16384;
    std::vector<InitialContactGeometry> initialContactGeometry;
    ContactGeometryDiagnostics parallelContactDiagnostics;
    CollisionGeometryStats parallelCookedDiagnostics;
    static void AddContactDiagnostics(ContactGeometryDiagnostics &into,
                                      const ContactGeometryDiagnostics &after,
                                      const ContactGeometryDiagnostics &before = {});
    static void AddCookedDiagnostics(CollisionGeometryStats &into,
                                     const CollisionGeometryStats &after,
                                     const CollisionGeometryStats &before = {});
    void PrepareInitialContactGeometry(float dt);
    void PrepareJoints(JointSolver &target, const std::vector<JointInput> &inputs, float dt,
                       bool warm = true, bool impact = false, bool append = false);
    void InvalidateImpactPose(unsigned slot);

    RigidBody PoseAt(unsigned slot, double time) const;
    static double NormalSpeed(const Contact &c, const RigidBody &a, const RigidBody &b);
    // This RigidBody-shaped adapter serves the existing geometric primitive
    // APIs only. It deliberately does not transport angular velocity; physical
    // event/solver publication must continue to use PoseAt above.
    RigidBody GeometryPoseAt(unsigned slot, double time) const;
    const ImpactPoseSample &ImpactPoseAt(unsigned slot, double time);
    const Aabb &RemainingImpactBound(unsigned slot, double start, double finish);
    const Aabb &ActualImpactBound(unsigned slot, const ImpactPoseSample &pose);
    bool SimpleImpactBoundsSeparated(unsigned a, unsigned b, const ImpactPoseSample &aa,
                                     const ImpactPoseSample &bb);
    ContactManifold PairAt(unsigned a, unsigned b, int pa, int pb, double time, float margin);
    // Deepest actual (zero-margin) penetration of the pair at `time`, metres.
    double PairPenetration(unsigned a, unsigned b, int pa, int pb, double time);
    bool PairPenetrationExceeds(unsigned a, unsigned b, int pa, int pb, double time, double target);
    bool PairTouches(unsigned a, unsigned b, int pa, int pb, double time);
    // CCD bounds use the geometric drift generator, not the transported
    // physical endpoint angular velocity of an anisotropic body.
    glm::vec3 DriftGenerator(unsigned slot, double time) const;
    // Lower bound on a fixed separating-plane gap along the anchored drift.
    // For q(t)=normalize(q0+t*Omega*q0/2), instantaneous angular speed is
    // |omega|/(1+(|omega|t/2)^2). Every rotated vertex has |r''|<=|omega|^2|r_perp|.
    // Evaluate ALL support vertices, including those not currently extremal.
    double PlaneDrift(unsigned a, unsigned b, int pa, int pb, double time, const glm::dvec3 &n);

    // Conservative advancement along a separating plane. Translation projected
    // onto that plane plus |omega|*radius bounds its possible closing speed.
    // Anchored quaternion interpolation turns no faster than |omega|.
    // Half the thinnest dimension of a primitive (sphere radius); infinite for
    // terrain and meshes, which use the other primitive's size.
    double PrimitiveFeature(unsigned slot, int part) const;
    // A touching pair is normally left to the contact solver (support pair).
    // That holds while rotation cannot carry other features of a non-round
    // primitive through the support within the interval; a light body spun up
    // by an off-centre impact can turn several radians in one step. When the
    // rotation can sweep a point further than half the thinnest primitive's
    // thickness, the pair stays under continuous search with a penetration
    // target instead. (A lower bound also catches ragdoll limbs knocked about
    // on the ground, and changes how they come to rest.)
    bool SpinsThroughSupport(const std::array<unsigned, 4> &key, double remaining, double time = 0,
                             bool changedVelocity = false);
    static bool ContinuationPrimitive(const Body &body);
    bool SupportContinuationSafe(const std::array<unsigned, 4> &key, double start, double finish);
    // A quarter of the existing half-dimension feature (one eighth of the
    // full thickness for boxes), measured relative to the current overlap.
    double SpinPenetrationTarget(const std::array<unsigned, 4> &key) const;
    // target>0: find the first time the pair's deepest penetration exceeds
    // its depth at `start` by target (a pair already touching, e.g. pivoting
    // on one corner), rather than the first touch. Measuring from the start
    // depth lets the search continue after an event that ended at the target.
    double ImpactTime(unsigned a, unsigned b, int pa, int pb, double start, double finish,
                      double target = 0);

    void ResolveInitialImpacts(float dt);

    void AdvanceImpacts(float dt, bool coupledVelocityChanged = false);

    static void KinematicVelocity(const Body &body, float dt, glm::vec3 &linear, glm::vec3 &angular,
                                  double &duration);
    void PublishKinematics(float dt);

    // Milestone 32 warm-start state: last step's converged impulses, sorted
    // by key, and the keys/anchors of this step's constraints in solver order.
    std::vector<CachedContact> contactCache;
    // Completed solver observations are not warm-start storage.
    // Island velocity changes may invalidate warm starts without erasing
    // untouched load-bearing contacts from the completed physical step.
    std::vector<CachedContact> completedContactObservations;
    uint64_t completedObservationEpoch = 0;
    float completedObservationDt = 0, completedSolverDt = 0;
    bool completedObservationsReady = false, restoredCompletedObservations = false;

    std::vector<CachedContact> pendingCache;
    std::vector<char> cacheUsed;
    PhysicsWorld::StepStats stats;
    std::size_t reinsertionsSinceStep = 0;
    struct CacheCounters {
        std::size_t orientationHits = 0, orientationRebuilds = 0;
        std::size_t boundHits = 0, boundRebuilds = 0, shapeRebuilds = 0, allocations = 0,
                    allocatedBytes = 0;
    } cacheCounters;

    static bool SameFloat(float a, float b);
    static bool SamePosition(const glm::vec3 &a, const glm::vec3 &b);
    static bool SameRotation(const glm::quat &a, const glm::quat &b);
    static bool Matches(const PoseBound &bound, const glm::vec3 &p, const glm::quat &q);
    const ContactPreparedOrientation &Orientation(Body &body);
    const Aabb &CurrentBound(Body &body);
    const Aabb &PreviousBound(Body &body);
    std::size_t CacheBytes() const;
    void EnsureImpactPoseStorage();

    // The tight broadphase bound a body must stay inside. For a dynamic
    // body it covers both the previous and the current fixed-step pose —
    // player sweeps interpolate between the two — and, if the body turned
    // during the step, the orientation-independent bounding sphere at both
    // positions (an interpolated orientation can poke outside both
    // endpoint boxes; it can never leave the swept sphere).
    Aabb TightBound(Body &body);

    // Milestone 32: how far this body's surface can move within the current
    // step at its post-force velocities — the most any of its contacts can
    // close before the next detection.
    float StepReach(const Body &body, float fixedDeltaTime) const;

    // Before candidate generation: a dynamic proxy's fat bound covers the
    // body grown by its reach, so every pair that can come into contact
    // within this step is a candidate (speculative contacts).
    void CoverStepReach(Body &body, float fixedDeltaTime);

    void RefreshProxy(Body &body);

    // Live slots whose fat bounds overlap `box`, ascending.
    const std::vector<unsigned int> &QuerySlots(const Aabb &box) const;

    // Candidate pairs (lo, hi) for every pair with at least one movable
    // body, in ascending lexicographic slot order — the same order the
    // pre-M32 all-pairs loop visited them in, restricted to candidates.
    bool CanCollide(unsigned a, unsigned b) const;
    bool CanRespond(unsigned a, unsigned b) const;
    bool MatchesQuery(unsigned slot, const PhysicsQueryFilter &filter) const;
    void GenerateCandidatePairs();

    BodyHandle MakeHandle(unsigned int slot) const;

    unsigned playerCollisionLayer = 0;
    CategoryMask playerCollisionMask = kAllCategories;
    bool hasPlayerShape = false;
    Shape playerShape;

    // A member of Impl (rather than a free function taking a body list)
    // specifically so it can name `Body` without exposing this private
    // nested type outside PhysicsWorld.cpp.
    // Milestone 32: evaluates only `candidates` (the broadphase's answer for
    // the whole sweep). Any body not among them has a positive distance, so
    // it could never be the reported closest body of a hit.
    ClosestBodyResult ClosestBodyToCapsule(const std::vector<unsigned int> &candidates,
                                           const glm::vec3 &segA, const glm::vec3 &segB,
                                           float capsuleRadius, float bodyMotionAlpha,
                                           bool interpolateDynamicBodyMotion) const;

    Body *Get(BodyHandle handle);
    const Body *Get(BodyHandle handle) const;

    BodyHandle AddBody(const Shape &shape, const glm::vec3 &position, const glm::quat &rotation,
                       bool isDynamic, float mass, float friction, float restitution);

    void Remove(BodyHandle handle);
    struct EpisodeRecord { ContactKey key; uint64_t revisionA=0,revisionB=0; const CollisionAsset* asset=nullptr;MeshContactEpisode episode; double anchorTime=0; };
    std::map<std::array<unsigned,4>,EpisodeRecord> meshEpisodes;
    bool MeshPair(unsigned a,unsigned b,int pa,int pb) const {
        const auto type=[](const Shape& shape,int part){return shape.type==ShapeType::CompoundBoxes?shape.boxes[std::size_t(part)].type:shape.type;};
        const auto first=type(bodies[a].shape,pa),second=type(bodies[b].shape,pb);const auto poly=[](ShapeType t){return t==ShapeType::Box||t==ShapeType::ConvexHull;};
        return (first==ShapeType::TriangleMesh&&!bodies[a].HasMotion()&&poly(second))||(second==ShapeType::TriangleMesh&&!bodies[b].HasMotion()&&poly(first));
    }
    bool RawMeshFrontSafe(unsigned a,unsigned b,int pa,int pb,double start,double finish) {
        if(!MeshPair(a,b,pa,pb))return false;
        const auto& aa=ImpactPoseAt(a,start);const auto& bb=ImpactPoseAt(b,start);
        const auto first=PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),second=PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation);
        const bool firstMesh=first.shape.type==ShapeType::TriangleMesh;const auto& mesh=firstMesh?first:second;
        const auto moving=firstMesh?b:a,stationary=firstMesh?a:b;const auto& body=bodies[moving];
        if(body.shape.type!=ShapeType::Box||body.shape.pivotOffset!=glm::vec3(0)||mesh.shape.asset->twoSided)return false;
        const auto& from=ImpactPoseAt(moving,start).pose;const auto& fixed=ImpactPoseAt(stationary,start).pose;
        const RigidMotionSegment* segment=nullptr;
        for(const auto& entry:body.motion.Segments())if(entry.begin<=start&&entry.end>=finish)segment=&entry;
        if(body.HasMotion()&&!segment)return false;
        const auto& bound=RemainingImpactBound(moving,start,finish);const auto rotation=PrimitiveRotation(mesh),inverse=glm::transpose(rotation);const auto origin=PrimitiveCenter(mesh);
        glm::dvec3 lo(INFINITY),hi(-INFINITY);
        for(unsigned corner=0;corner<8;++corner){const glm::dvec3 world(corner&1?bound.max.x:bound.min.x,corner&2?bound.max.y:bound.min.y,corner&4?bound.max.z:bound.min.z);const auto local=inverse*(world-origin);lo=glm::min(lo,local);hi=glm::max(hi,local);}
        for(unsigned k=0;k<3;++k){lo[k]=std::nextafter(lo[k],-INFINITY);hi[k]=std::nextafter(hi[k],INFINITY);}
        std::vector<uint32_t> candidates;mesh.shape.asset->Candidates(lo,hi,candidates);
        const auto offset=origin-glm::dvec3(fixed.position);
        using contact_geometry::Iv;
        for(const auto feature:candidates){const auto& face=mesh.shape.asset->faces[feature];const auto normal=rotation*face.normal;
            const auto center=rigid_motion_bounds::TryRelativeCentreProjection(segment,nullptr,from,fixed,start,finish,normal);if(!center)return false;
            const auto vertex=offset+rotation*mesh.shape.asset->vertices[face.vertices[0]];Iv plane(0);for(unsigned k=0;k<3;++k)plane=plane+Iv(vertex[k])*Iv(normal[k]);
            const double guard=1e-10*(1+glm::length(vertex)+glm::length(glm::dvec3(from.position)-glm::dvec3(fixed.position)));
            if(center->lo-plane.hi<=guard)return false;
        }
        return true;
    }
    const MeshContactEpisode* Episode(unsigned a,unsigned b,int pa,int pb) const {
        if(std::getenv("JUDAS_RESEARCH_NO_EPISODE"))return nullptr;
        if(a>b){std::swap(a,b);std::swap(pa,pb);}
        const auto it=meshEpisodes.find({a,b,unsigned(pa),unsigned(pb)});if(it==meshEpisodes.end())return nullptr;
        const auto& e=it->second;const auto first=PrimitiveAt(bodies[a].shape,bodies[a].rigidBody,pa),second=PrimitiveAt(bodies[b].shape,bodies[b].rigidBody,pb);
        const auto asset=first.shape.type==ShapeType::TriangleMesh?first.shape.asset.get():second.shape.asset.get();
        return e.key.SameBodies(ContactKey{a,b,bodies[a].generation,bodies[b].generation,pa,pb})&&e.revisionA==bodies[a].contactObservationRevision&&e.revisionB==bodies[b].contactObservationRevision&&e.asset==asset?&e.episode:nullptr;
    }
    void PublishEpisode(unsigned a,unsigned b,int pa,int pb,double time) {
        if(a>b){std::swap(a,b);std::swap(pa,pb);}
        if(!MeshPair(a,b,pa,pb))return;
        const auto& aa=ImpactPoseAt(a,time);const auto& bb=ImpactPoseAt(b,time);
        const auto first=PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),second=PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation);
        const auto& mesh=first.shape.type==ShapeType::TriangleMesh?first:second;const auto& query=first.shape.type==ShapeType::TriangleMesh?second:first;
        const auto old=EpisodeForTime(a,b,pa,pb,time);auto features=MeshContactFeatures(query,mesh,old);
        const std::array<unsigned,4> key{a,b,unsigned(pa),unsigned(pb)};
        if(features.empty())meshEpisodes.erase(key);else meshEpisodes[key]={ContactKey{a,b,bodies[a].generation,bodies[b].generation,pa,pb},bodies[a].contactObservationRevision,bodies[b].contactObservationRevision,mesh.shape.asset.get(),MeshContactEpisode{std::move(features)},time};
        episodeDerived.clear();
    }
    ContactManifold EpisodeContacts(unsigned a,unsigned b,int pa,int pb,const PrimitivePose& first,const PrimitivePose& second,float margin,const ContactPreparedOrientation* aa,const ContactPreparedOrientation* bb,double time=-1) {
        return MeshPair(a,b,pa,pb)?EpisodeCookedContacts(first,second,margin,time<0?Episode(a,b,pa,pb):EpisodeForTime(a,b,pa,pb,time)):ComputeContacts(first,second,margin,aa,bb);
    }

    std::map<std::array<unsigned,4>,std::pair<std::pair<double,double>,bool>> meshFrontCertificates;
    std::map<std::tuple<unsigned,unsigned,int,int,double>,MeshContactEpisode> episodeDerived;
    unsigned long long unionQueries=0,unionIntervals=0,unionCertificates=0,unionUncertain=0;
    bool MeshFrontSafe(unsigned a,unsigned b,int pa,int pb,double start,double finish) {
        const std::array<unsigned,4> key{a,b,unsigned(pa),unsigned(pb)};
        const auto found=meshFrontCertificates.find(key);
        if(found!=meshFrontCertificates.end()&&found->second.first==std::make_pair(start,finish))return found->second.second;
        const bool safe=RawMeshFrontSafe(a,b,pa,pb,start,finish);
        meshFrontCertificates[key]={{start,finish},safe};return safe;
    }
    const MeshContactEpisode* EpisodeForTime(unsigned a,unsigned b,int pa,int pb,double time) {
        if(a>b){std::swap(a,b);std::swap(pa,pb);}
        const auto front=meshFrontCertificates.find({a,b,unsigned(pa),unsigned(pb)});
        if(front!=meshFrontCertificates.end()&&front->second.second&&front->second.first.first<=time&&front->second.first.second>=time)return nullptr;
        const auto* previous=Episode(a,b,pa,pb);
        if(!previous||previous->fronts.empty())return nullptr;
        const auto identity=std::make_tuple(a,b,pa,pb,time);
        const auto cached=episodeDerived.find(identity);
        if(cached!=episodeDerived.end())return cached->second.fronts.empty()?nullptr:&cached->second;
        const auto foundRecord=meshEpisodes.find({a,b,unsigned(pa),unsigned(pb)});
        if(foundRecord==meshEpisodes.end())throw std::runtime_error("prototype canonical episode record absent");
        const auto& record=foundRecord->second;
        MeshContactEpisode selected=*previous;
        const auto first=PrimitiveAt(bodies[a].shape,bodies[a].rigidBody,pa),second=PrimitiveAt(bodies[b].shape,bodies[b].rigidBody,pb);
        const bool fm=first.shape.type==ShapeType::TriangleMesh;
        const unsigned moving=fm?b:a;
        const auto& query=fm?second:first;const auto& mesh=fm?first:second;
        double covered=record.anchorTime;
        if(time<covered)throw std::runtime_error("finite-mesh episode query precedes causal anchor");
        for(const auto& segment:bodies[moving].motion.Segments()) {
            if(segment.end<=covered||segment.begin>time)continue;
            if(segment.begin>covered)throw std::runtime_error("finite-mesh episode trajectory gap");
            const double finish=std::min(time,segment.end);
            if(finish<=covered)continue;
            auto result=episode_probe::ConnectedUnion(query,mesh,selected,segment,covered,finish);
            ++unionQueries;unionIntervals+=result.measure.intervals;
            unionCertificates+=result.measure.pointCertificates+result.measure.constantCertificates;
            if(result.measure.result==episode_probe::Result::Uncertain){++unionUncertain;std::ostringstream message;message<<"finite-mesh episode continuity uncertified within bounded policy: pair="<<a<<","<<b<<" anchor="<<record.anchorTime<<" start="<<covered<<" finish="<<finish<<" intervals="<<result.measure.intervals<<" certificates="<<(result.measure.pointCertificates+result.measure.constantCertificates)<<" features="<<selected.fronts.size();throw std::runtime_error(message.str());}
            if(result.measure.result==episode_probe::Result::Separated){selected.fronts.clear();covered=time;break;}
            selected=std::move(result.selected);covered=finish;if(covered==time)break;
        }
        if(covered<time&&bodies[moving].HasMotion())throw std::runtime_error("finite-mesh episode trajectory incomplete");
        auto entry=episodeDerived.emplace(identity,std::move(selected)).first;
        return entry->second.fronts.empty()?nullptr:&entry->second;
    }
    void BeginMeshEpisodes() {
        episodeDerived.clear();meshFrontCertificates.clear();
        for(const auto& pair:candidatePairs)for(int pa=0;pa<PrimitiveCount(bodies[pair.first].shape);++pa)for(int pb=0;pb<PrimitiveCount(bodies[pair.second].shape);++pb)
            if(CanRespond(pair.first,pair.second)&&MeshPair(pair.first,pair.second,pa,pb)) {
                const auto& a=bodies[pair.first];const auto& b=bodies[pair.second];
                auto first=PrimitiveAt(a.shape,a.rigidBody,pa),second=PrimitiveAt(b.shape,b.rigidBody,pb);
                const bool fm=first.shape.type==ShapeType::TriangleMesh;
                auto features=MeshContactFeatures(fm?second:first,fm?first:second,Episode(pair.first,pair.second,pa,pb));
                const std::array<unsigned,4> key{pair.first,pair.second,unsigned(pa),unsigned(pb)};
                if(features.empty())meshEpisodes.erase(key);
                else meshEpisodes[key]={ContactKey{pair.first,pair.second,a.generation,b.generation,pa,pb},a.contactObservationRevision,b.contactObservationRevision,(fm?first:second).shape.asset.get(),MeshContactEpisode{std::move(features)},0};
            }
    }
    void FinishMeshEpisodes(double dt) {
        std::vector<std::array<unsigned,4>> keys;for(const auto& e:meshEpisodes)keys.push_back(e.first);
        for(const auto& key:keys){
            if(key[0]>=bodies.size()||key[1]>=bodies.size()||!bodies[key[0]].alive||!bodies[key[1]].alive||!CanRespond(key[0],key[1])||!MeshPair(key[0],key[1],key[2],key[3])){meshEpisodes.erase(key);continue;}
            auto episode=EpisodeForTime(key[0],key[1],key[2],key[3],dt);
            const auto first=PrimitiveAt(bodies[key[0]].shape,bodies[key[0]].rigidBody,key[2]),second=PrimitiveAt(bodies[key[1]].shape,bodies[key[1]].rigidBody,key[3]);const bool fm=first.shape.type==ShapeType::TriangleMesh;
            auto features=MeshContactFeatures(fm?second:first,fm?first:second,episode);
            if(features.empty())meshEpisodes.erase(key);else {auto& e=meshEpisodes.at(key);e.episode.fronts=std::move(features);e.anchorTime=0;}
        }
        episodeDerived.clear();meshFrontCertificates.clear();
    }

};

#include "WorldHotAccessors.h"
#include "WorldImpactQueries.h"
