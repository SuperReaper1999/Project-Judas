#pragma once
#include "WorldInternal.h"

// Private CCD observations. No body, motion ledger, contact history, event
// frontier or solver state is owned or published by a query. A lane borrows a
// frozen world and owns its derived samples and diagnostics until joined.
struct PhysicsWorld::Impl::CCDQueryStorage {
    std::vector<ImpactPoseSample> impactPoseSamples{};
    std::vector<ImpactPoseSample> impactAnchorSamples{};
    std::size_t impactPoseEvaluations{};
    std::size_t impactPoseReuses{};
    std::size_t impactOrientationBuilds{};
    std::size_t impactTruthRequests{};
    std::size_t impactTruthReuses{};
    std::size_t impactBoundsRejected{};
    std::size_t impactActualBoundsRejected{};
    PhysicsWorld::StepStats stats{};
    WorkDiagnostics work{};
    CacheCounters cacheCounters{};
    void Reset() {
        stats = {};
        work = {};
        cacheCounters = {};
        impactPoseEvaluations = impactPoseReuses = impactOrientationBuilds = 0;
        impactTruthRequests = impactTruthReuses = impactBoundsRejected = impactActualBoundsRejected = 0;
        for (auto* samples : {&impactPoseSamples, &impactAnchorSamples})
            for (auto& sample : *samples) {
                sample.time = sample.boundStart = std::numeric_limits<double>::quiet_NaN();
                sample.tightValid = false;
            }
    }
};

struct PhysicsWorld::Impl::CCDQuery {
    const Impl& world;
    Impl* serialWorld;
    const std::vector<Body>& bodies;
    double impactQueryAnchorTime;
    std::vector<ImpactPoseSample>& impactPoseSamples;
    std::vector<ImpactPoseSample>& impactAnchorSamples;
    std::size_t& impactPoseEvaluations;
    std::size_t& impactPoseReuses;
    std::size_t& impactOrientationBuilds;
    std::size_t& impactTruthRequests;
    std::size_t& impactTruthReuses;
    std::size_t& impactBoundsRejected;
    std::size_t& impactActualBoundsRejected;
    PhysicsWorld::StepStats& stats;
    WorkDiagnostics& work;
    CacheCounters& cacheCounters;
    explicit CCDQuery(Impl& owner)
        : world(owner), serialWorld(&owner), bodies(owner.bodies),
          impactQueryAnchorTime(owner.impactQueryAnchorTime),
          impactPoseSamples(owner.impactPoseSamples),
          impactAnchorSamples(owner.impactAnchorSamples),
          impactPoseEvaluations(owner.impactPoseEvaluations),
          impactPoseReuses(owner.impactPoseReuses),
          impactOrientationBuilds(owner.impactOrientationBuilds),
          impactTruthRequests(owner.impactTruthRequests),
          impactTruthReuses(owner.impactTruthReuses),
          impactBoundsRejected(owner.impactBoundsRejected),
          impactActualBoundsRejected(owner.impactActualBoundsRejected),
          stats(owner.stats),
          work(owner.work),
          cacheCounters(owner.cacheCounters) {}
    CCDQuery(const Impl& owner, CCDQueryStorage& scratch)
        : world(owner), serialWorld(nullptr), bodies(owner.bodies),
          impactQueryAnchorTime(owner.impactQueryAnchorTime),
          impactPoseSamples(scratch.impactPoseSamples),
          impactAnchorSamples(scratch.impactAnchorSamples),
          impactPoseEvaluations(scratch.impactPoseEvaluations),
          impactPoseReuses(scratch.impactPoseReuses),
          impactOrientationBuilds(scratch.impactOrientationBuilds),
          impactTruthRequests(scratch.impactTruthRequests),
          impactTruthReuses(scratch.impactTruthReuses),
          impactBoundsRejected(scratch.impactBoundsRejected),
          impactActualBoundsRejected(scratch.impactActualBoundsRejected),
          stats(scratch.stats),
          work(scratch.work),
          cacheCounters(scratch.cacheCounters) {}
    RigidBody GeometryPoseAt(unsigned slot, double time) const {
        return world.GeometryPoseAt(slot, time);
    }
    Aabb CurrentBound(const Body& body) {
        if (serialWorld)
            return serialWorld->CurrentBound(serialWorld->bodies[&body - bodies.data()]);
        // A query may reuse already-published derived bounds, but never writes
        // the body's shared cache. The fallback uses the identical geometry.
        if (Impl::Matches(body.currentBound, body.rigidBody.position, body.rigidBody.orientation)) {
            ++cacheCounters.boundHits;
            return body.currentBound.bound;
        }
        ++cacheCounters.boundRebuilds;
        return ShapeAabb(body.shape, body.rigidBody.position, body.rigidBody.orientation);
    }
    const ImpactPoseSample& ImpactPoseAt(unsigned slot, double time);
    const Aabb& RemainingImpactBound(unsigned slot, double start, double finish);
    const Aabb& ActualImpactBound(unsigned slot, const ImpactPoseSample &pose);
    bool SimpleImpactBoundsSeparated(unsigned a, unsigned b, const ImpactPoseSample& aa,
                                     const ImpactPoseSample& bb);
    ContactManifold PairAt(unsigned a, unsigned b, int pa, int pb, double time, float margin);
    double PairPenetration(unsigned a, unsigned b, int pa, int pb, double time);
    bool PairPenetrationExceeds(unsigned a, unsigned b, int pa, int pb, double time, double target);
    bool PairTouches(unsigned a, unsigned b, int pa, int pb, double time);
    glm::vec3 DriftGenerator(unsigned slot, double time) const {
        return world.DriftGenerator(slot, time);
    }
    double PlaneDrift(unsigned a, unsigned b, int pa, int pb, double time, const glm::dvec3 &n);
    bool StaticMeshPolyPair(unsigned a, unsigned b, int pa, int pb) const {
        return world.StaticMeshPolyPair(a, b, pa, pb);
    }
    bool MeshDepthGrowthSafe(unsigned moving, unsigned meshSlot, int movingPart, int meshPart,
                             double start, double finish, const MeshFeatureBudget& budget);
    double PrimitiveFeature(unsigned slot, int part) const {
        return world.PrimitiveFeature(slot, part);
    }
    bool SpinsThroughSupport(const std::array<unsigned, 4>& key, double remaining,
                             double time = 0, bool changedVelocity = false);
    bool SupportContinuationSafe(const std::array<unsigned, 4> &key, double start, double finish);
    double SpinPenetrationTarget(const std::array<unsigned, 4>& key) const {
        return world.SpinPenetrationTarget(key);
    }
    double ImpactTime(unsigned a, unsigned b, int pa, int pb, double start, double finish, double target = 0);
    void EnsureImpactPoseStorage();
};
