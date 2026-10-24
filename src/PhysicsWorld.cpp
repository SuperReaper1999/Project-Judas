#include "physics/WorldInternal.h"
#include <cstdio>
#include <cstdlib>

using namespace JudasPhysicsDetail;

namespace {
// Latest means the completed public step, including all StepCoupled intervals.
void PublishImpactCounters(const PhysicsWorld::StepStats& stats) {
    JUDAS_PROFILE_COUNTER("Physics impact events", double(stats.impactEvents),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact queries", double(stats.impactQueries),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact search iterations",
                          double(stats.impactSearchIterations), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact sampling fallbacks",
                          double(stats.impactSamplingFallbacks), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact sampling tests", double(stats.impactSamplingTests),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact search limits", double(stats.impactSearchLimit),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact search stalls", double(stats.impactSearchStalls),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact sampling resolution caps",
                          double(stats.impactSamplingResolutionCaps), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD path holds", double(stats.impactSafetyFallback), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD body limit holds", double(stats.impactBodyCapHolds), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD step limit holds", double(stats.impactStepCapHolds), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD response holds", double(stats.impactResponseHolds), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD response attempts", double(stats.impactResponseAttempts), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD responses applied", double(stats.impactResponsesApplied), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD missing manifolds", double(stats.impactMissingManifolds), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD energy rejections", double(stats.impactEnergyRejections), ProfileCounterMode::Latest);
}
} // namespace

// Public fixed-step coordinator. Cohesive implementation modules share one Impl.
bool PhysicsWorld::Init() {
    m_impl = new Impl();
    if(std::getenv("JUDAS_VALIDATION_NATIVE_OVERLAPS"))m_impl->overlapPairs.UseNativeQueries();
    const bool blockNormals=std::getenv("JUDAS_VALIDATION_POINT_CONTACTS")==nullptr;
    m_impl->solver.EnableBlockNormals(blockNormals);
    m_impl->impactSolver.EnableBlockNormals(blockNormals);
    return true;
}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
}

void PhysicsWorld::Shutdown() {
    delete m_impl;
    m_impl = nullptr;
}

void PhysicsWorld::Step(float fixedDeltaTime) {
    Step(fixedDeltaTime, nullptr);
}
void PhysicsWorld::Step(float fixedDeltaTime, const VelocityCoupling *coupling) {
    if (coupling && (!coupling->iterations || coupling->iterations > 64))
        throw std::invalid_argument("invalid velocity coupling quality");
    JUDAS_PROFILE_SCOPE("Rigid physics");
    Impl &w = *m_impl;
    if (w.stepping)
        throw std::logic_error("recursive physics step");
    struct StepGuard {
        bool &flag;
        explicit StepGuard(bool &f) : flag(f) {
            flag = true;
        }
        ~StepGuard() {
            flag = false;
        }
    } guard(w.stepping);
    const Clock::time_point stepStart = Clock::now();
    w.completedObservationsReady = false;
    w.restoredCompletedObservations = false;
    for (auto &joint : w.joints)
        if (joint.restoredContinuationDt > 0) {
            if (joint.restoredContinuationDt != fixedDeltaTime) {
                joint.warm.fill(0);
                joint.state.angularDrive.impulses = {0, 0, 0};
            }
            joint.restoredContinuationDt = 0;
        }

    w.mk2Points.clear();
    w.parallelContactDiagnostics = {};
    w.parallelCookedDiagnostics = {};
    w.stats = StepStats{};
    w.work = {};
    w.work.active = PerformanceProfiler::Get().Active();
    if (w.collectingPreStepQueries) {
        w.stepTouches = std::move(w.preStepQueryTouches);
        w.preStepQueryTouches.clear();
        w.collectingPreStepQueries = false;
        // A safe authority handoff may disable an observer after its motor move.
        for (auto it = w.stepTouches.begin(); it != w.stepTouches.end();) {
            auto *a = w.Get(it->second.a);
            auto *b = w.Get(it->second.b);
            if (!a || !b || !a->enabled || !b->enabled)
                it = w.stepTouches.erase(it);
            else
                ++it;
        }
    } else if (!w.coupledStep)
        w.stepTouches.clear();
    const auto cachesBefore = w.cacheCounters;
    const ContactGeometryDiagnostics geometryBefore = GetContactGeometryDiagnostics();
    const auto collisionBefore = GetCollisionGeometryStats();

    {
        JUDAS_PROFILE_SCOPE("Physics kinematic publication");
        w.PublishKinematics(fixedDeltaTime);
    }

    // Awaken connected state before velocity integration for predicted impacts.
    static ProfileLabel predictionLabel("Physics prediction and wake"),
        integrationLabel("Physics velocity integration");
    ProfileScope predictionScope(predictionLabel);
    for (auto i : w.aliveSlots)
        if (w.bodies[i].HasMotion() && !w.bodies[i].sleeping)
            w.CoverStepReach(w.bodies[i], fixedDeltaTime);
    w.GenerateCandidatePairs();
    // The callback-free interval to contact generation only wakes bodies and
    // integrates velocities. Its candidate policy stays unchanged; only a
    // proxy expansion can change the list. Do not carry this across steps.
    const auto predictionProxyReinsertions = w.reinsertionsSinceStep;
    for (auto [a, b] : w.candidatePairs) {
        auto &aa = w.bodies[a];
        auto &bb = w.bodies[b];
        if (aa.sensor || bb.sensor || aa.queryOnly || bb.queryOnly)
            continue;
        auto moving = [&](const auto &body) {
            return body.isKinematic
                       ? body.rigidBody.linearVelocity != glm::vec3(0) ||
                             body.rigidBody.angularVelocity != glm::vec3(0)
                       : !body.sleeping && !body.rigidBody.IsStatic() &&
                             (glm::length(body.rigidBody.linearVelocity -
                                          body.lastAcceleration * fixedDeltaTime) > .03f ||
                              glm::length(body.rigidBody.angularVelocity) > .03f);
        };
        if (aa.sleeping && moving(bb))
            w.Wake(a);
        if (bb.sleeping && moving(aa))
            w.Wake(b);
    }
    predictionScope.End();
    ProfileScope integrationScope(integrationLabel);
    // 1) Velocity from this step's force/torque accumulators (M12). Gravity
    // is already in linearVelocity via the caller's ApplyLinearAcceleration.
    // The start-of-step pose is kept for player sweeps and presentation.
    std::size_t movable = 0;
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body &body = w.bodies[slot];
        if (!body.rigidBody.IsStatic())
            ++movable;
        if (!body.isDynamic || body.simulationSuspended)
            continue;
        body.motion.Clear();
        body.previousPosition = body.rigidBody.position;
        body.previousOrientation = body.rigidBody.orientation;
        // Capture the old endpoint before integration. CurrentBound itself is
        // keyed by represented pose, so a later correction cannot reuse it.
        w.CurrentBound(body);
        body.previousBound = body.currentBound;
        if (body.sleeping)
            continue;
        if (body.deferredAcceleration) {
            body.rigidBody.linearVelocity += body.lastAcceleration * fixedDeltaTime;
            body.stepVelocityDelta += body.lastAcceleration * fixedDeltaTime;
            body.deferredAcceleration = false;
        }
        IntegrateRigidBodyVelocity(body.rigidBody, fixedDeltaTime);
        w.CoverStepReach(body, fixedDeltaTime);
    }
    w.stats.bodies = w.aliveSlots.size();
    w.stats.dynamicBodies = movable;
    w.stats.possiblePairs =
        movable * (movable - (movable > 0 ? 1 : 0)) / 2 + movable * (w.aliveSlots.size() - movable);
    integrationScope.End();

    // 2) Broadphase: every fat bound contains its body's current pose
    // (refreshed at the end of the previous Step and by ResetBody), so every
    // touching pair is among the candidates. Static-static pairs never are.
    static ProfileLabel broadLabel("Physics broadphase"), narrowLabel("Physics contacts"),
        solverLabel("Physics contact and joint solver"), proxiesLabel("Physics proxy refresh");
    ProfileScope broadScope(broadLabel);
    const Clock::time_point broadphaseStart = Clock::now();
    if (w.reinsertionsSinceStep != predictionProxyReinsertions)
        w.GenerateCandidatePairs();
    w.stats.candidatePairs = w.candidatePairs.size();
    broadScope.End();
    ProfileScope narrowScope(narrowLabel);
    const Clock::time_point narrowphaseStart = Clock::now();

    // 3) Narrowphase at the current poses — the sole authority on contact.
    w.solver.Clear();
    w.lastStepContacts.clear();
    w.pendingCache.clear();
    w.cacheUsed.assign(w.contactCache.size(), 0);
    w.PrepareInitialContactGeometry(fixedDeltaTime);
    std::size_t initialGeometryIndex = 0;
    for (const auto &[slotA, slotB] : w.candidatePairs) {
        Impl::Body &a = w.bodies[slotA];
        Impl::Body &b = w.bodies[slotB];
        if (a.sensor || b.sensor || a.queryOnly || b.queryOnly)
            continue;
        if ((a.sleeping || a.rigidBody.IsStatic()) && (b.sleeping || b.rigidBody.IsStatic())) {
            const auto ha = w.MakeHandle(slotA).id, hb = w.MakeHandle(slotB).id;
            auto key = std::minmax(ha, hb);
            auto old = w.previousTouches.find(key);
            if (old != w.previousTouches.end()) {
                auto observation = old->second;
                observation.normalImpulse = 0;
                observation.impulseAvailable = false;
                observation.relativeVelocity = {0, 0, 0};
                w.stepTouches[key] = observation;
            }
            continue;
        }
        const Impl::InitialContactGeometry *prepared = nullptr;
        if (initialGeometryIndex < w.initialContactGeometry.size()) {
            const auto &candidate = w.initialContactGeometry[initialGeometryIndex];
            if (candidate.a == slotA && candidate.b == slotB) {
                prepared = &candidate;
                ++initialGeometryIndex;
            }
        }
        const float friction = std::sqrt(std::max(a.friction, 0.0f) * std::max(b.friction, 0.0f));
        // Speculative margin: the distance this pair's surfaces can close
        // within the step at their post-force velocities (relative linear
        // motion plus each body's rotation at its bounding radius). Any
        // contact within it is generated now, before it can penetrate; at
        // rest on a support that is g*dt^2 (2.7 mm at 60 Hz, 9.81 m/s^2).
        const float margin =
            prepared ? prepared->margin
                     : w.ContactMargin(
                           (glm::length(a.rigidBody.linearVelocity - b.rigidBody.linearVelocity) +
                            glm::length(a.rigidBody.angularVelocity) * a.boundingRadius +
                            glm::length(b.rigidBody.angularVelocity) * b.boundingRadius) *
                           fixedDeltaTime);
        int pairPoints = 0;
        const auto &orientationA = prepared ? *prepared->orientationA : w.Orientation(a);
        const auto &orientationB = prepared ? *prepared->orientationB : w.Orientation(b);
        const glm::dmat3 inverseA = glm::transpose(orientationA.rotation);
        for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
            std::optional<PrimitivePose> childA;
            if (!prepared) childA.emplace(PrimitiveAt(a.shape, a.rigidBody, partA, &orientationA));
            for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                std::optional<PrimitivePose> childB;
                if (!prepared) childB.emplace(PrimitiveAt(b.shape, b.rigidBody, partB, &orientationB));
                const ContactManifold manifold =
                    prepared
                        ? prepared->manifold
                        : ComputeContacts(*childA, *childB, margin, &orientationA, &orientationB,
                                          true);
                const ContactKey key{slotA, slotB, a.generation, b.generation, partA, partB};
                const auto range = std::equal_range(
                    w.contactCache.begin(), w.contactCache.end(),
                    CachedContact{key, {}, {}, 0.0f, {}},
                    [](const CachedContact &x, const CachedContact &y) { return x.key < y.key; });
                bool touchingManifold = prepared ? prepared->touching : false;
                if (!prepared) {
                    for (int p = 0; p < manifold.count; ++p) {
                        const auto &c = manifold.points[p];
                        touchingManifold =
                            touchingManifold ||
                            (c.hasLocalAnchors ? c.signedSeparation <= 0 : c.penetration >= 0);
                    }
                    if (!touchingManifold && manifold.count)
                        touchingManifold =
                            ComputeContacts(*childA, *childB, 0, &orientationA, &orientationB).count >
                            0;
                }
                for (int p = 0; p < manifold.count; ++p) {
                    Contact contact = manifold.points[p];
                    if (!contact.hit)
                        continue;
                    if ((contact.hasLocalAnchors ? contact.signedSeparation <= 0
                                                 : contact.penetration >= 0) ||
                        touchingManifold)
                        w.Observe(slotA, slotB, contact);
                    // Warm start from the nearest unused cached point of the
                    // same bodies/primitives (same generations: a reused slot
                    // never inherits a previous occupant's impulses).
                    const glm::dvec3 anchor = contact.hasLocalAnchors
                                                  ? contact.localAnchorA
                                                  : inverseA * (glm::dvec3(contact.point) -
                                                                glm::dvec3(a.rigidBody.position));
                    float warmNormal = 0.0f;
                    glm::vec3 warmTangent(0.0f);
                    std::ptrdiff_t best = -1;
                    double bestDistance = kWarmStartAnchorDistance;
                    for (auto it = range.first; it != range.second; ++it) {
                        const std::ptrdiff_t index = it - w.contactCache.begin();
                        if (w.cacheUsed[static_cast<std::size_t>(index)] ||
                            !it->key.SameBodies(key) ||
                            glm::dot(it->normal, contact.normal) < kWarmStartNormalDot)
                            continue;
                        const double distance = glm::distance(it->localAnchorA, anchor);
                        if (distance < bestDistance) {
                            bestDistance = distance;
                            best = index;
                        }
                    }
                    if (best >= 0) {
                        w.cacheUsed[static_cast<std::size_t>(best)] = 1;
                        warmNormal = w.contactCache[static_cast<std::size_t>(best)].normalImpulse;
                        warmTangent = w.contactCache[static_cast<std::size_t>(best)].tangentImpulse;
                    }
                    // Every manifold point, new or persistent, joins one
                    // simultaneous substepped solve (WorldMk2Solver.cpp).
                    w.pendingCache.push_back({key, anchor, contact.normal, 0, glm::vec3(0),
                                              float(contact.hasLocalAnchors
                                                        ? contact.signedSeparation
                                                        : -double(contact.penetration)),
                                              a.contactObservationRevision,
                                              b.contactObservationRevision});
                    w.mk2Points.push_back({slotA, slotB, contact, friction,
                                           std::max(a.restitution, b.restitution),
                                           warmNormal, warmTangent});
                    w.lastStepContacts.push_back(
                        {contact.point, contact.normal, contact.penetration});
                    ++pairPoints;
                    ++pairPoints;
                }
            }
        }
        if (pairPoints > 0)
            ++w.stats.collidingPairs;
    }
    w.stats.contactPoints = w.lastStepContacts.size();
    narrowScope.End();
    ProfileScope solverScope(solverLabel);
    const Clock::time_point solverStart = Clock::now();
    {
        static ProfileLabel mk2Label("Physics MK2 substep solve");
        ProfileScope mk2Scope(mk2Label);
        w.Mk2Solve(fixedDeltaTime, coupling);
    }
    static ProfileLabel settleLabel("Physics sleeping assessment");
    ProfileScope settleScope(settleLabel);
    w.Settle(fixedDeltaTime);
    settleScope.End();
    solverScope.End();
    ProfileScope proxyScope(proxiesLabel);
    const Clock::time_point solverEnd = Clock::now();

    // 5) Keep every dynamic proxy's fat bound around its previous-to-current
    // motion for the player's sweeps and the next step's candidates.
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body &body = w.bodies[slot];
        if (body.HasMotion())
            w.RefreshProxy(body);
    }
    static ProfileLabel touchesLabel("Physics endpoint contact publication");
    ProfileScope touchesScope(touchesLabel);
    w.queryTouchesPending = std::any_of(w.aliveSlots.begin(), w.aliveSlots.end(),
                                        [&](unsigned i) { return w.bodies[i].queryOnly; });
    if (!w.queryTouchesPending && !w.coupledStep)
        w.FinishTouches();
    touchesScope.End();
    for (const auto slot : w.aliveSlots)
        w.bodies[slot].stepVelocityDelta = glm::vec3(0);
    w.stats.proxyReinsertions = w.reinsertionsSinceStep;
    w.reinsertionsSinceStep = 0;
    w.stats.treeHeight = w.tree.Height();
    JUDAS_PROFILE_COUNTER("Physics block normal solves", contact_block::diagnostics.solves, ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics block normal fallbacks", contact_block::diagnostics.fallbacks, ProfileCounterMode::Latest);
    w.stats.orientationCacheHits = w.cacheCounters.orientationHits - cachesBefore.orientationHits;
    w.stats.orientationCacheRebuilds =
        w.cacheCounters.orientationRebuilds - cachesBefore.orientationRebuilds;
    w.stats.boundCacheHits = w.cacheCounters.boundHits - cachesBefore.boundHits;
    w.stats.boundCacheRebuilds = w.cacheCounters.boundRebuilds - cachesBefore.boundRebuilds;
    w.stats.shapeCacheRebuilds = w.cacheCounters.shapeRebuilds - cachesBefore.shapeRebuilds;
    w.stats.solverFrameBuilds = w.solver.FrameBuilds();
    w.stats.geometryCacheAllocations =
        w.cacheCounters.allocations - cachesBefore.allocations + w.solver.FrameAllocations();
    w.stats.geometryCacheAllocatedBytes = w.cacheCounters.allocatedBytes -
                                          cachesBefore.allocatedBytes +
                                          w.solver.FrameAllocatedBytes();
    w.stats.solverFrameNodeRequests = w.solver.FrameNodeRequests();
    w.stats.geometryCacheBytes = w.CacheBytes();
    const Clock::time_point stepEnd = Clock::now();
    w.stats.broadphaseMilliseconds = MillisecondsBetween(broadphaseStart, narrowphaseStart) +
                                     MillisecondsBetween(solverEnd, stepEnd);
    w.stats.narrowphaseMilliseconds = MillisecondsBetween(narrowphaseStart, solverStart);
    JUDAS_PROFILE_COUNTER("Sleeping physics bodies", double(w.stats.sleepingBodies),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Awake physics bodies", double(w.stats.awakeBodies),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics bodies last step", double(w.stats.bodies),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics candidate pairs last step", double(w.stats.candidatePairs),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics contacts last step", double(w.stats.contactPoints),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics joints", double(w.joints.size()), ProfileCounterMode::Latest);
    PublishImpactCounters(w.stats);
    JUDAS_PROFILE_COUNTER("Physics CCD query scratch bytes", double(w.CCDQueryBytes()), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact pose evaluations", double(w.impactPoseEvaluations),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact pose reuses", double(w.impactPoseReuses),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact orientation preparations",
                          double(w.impactOrientationBuilds), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact actual-contact requests", double(w.impactTruthRequests),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact actual-contact reuses", double(w.impactTruthReuses),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact remaining-bound rejections",
                          double(w.impactBoundsRejected), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact actual-bound rejections",
                          double(w.impactActualBoundsRejected), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics broadphase passes", double(w.work.candidatePasses),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics broadphase root queries", double(w.work.candidateQueries),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics broadphase overlapping leaves", double(w.work.candidateLeaves),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics sleep graph edges", double(w.sleepGraphEdges),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics sleep islands", double(w.sleepIslandCount),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics sleep scratch bytes", double(w.sleepScratchBytes),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics persistent contact rows", double(w.solver.Constraints().size()),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics joint rows",
                          double(w.joints.empty() ? 0 : w.jointSolver.RowCount()),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics broadphase pair scratch bytes",
                          double(w.candidateScratch.marks.capacity() * sizeof(std::uint64_t) +
                                 w.candidateScratch.stack.capacity() * sizeof(std::pair<int, int>) +
                                 w.candidateProxies.capacity() * sizeof(int)),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics CCD search inclusive ms", double(w.work.toi.nanoseconds) * 1e-6,
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics CCD geometry inclusive ms",
                          double(w.work.geometry.nanoseconds) * 1e-6, ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics CCD geometry calls", double(w.work.geometry.calls),
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics impact solve ms", double(w.work.impactSolve.nanoseconds) * 1e-6,
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics impact solve calls", double(w.work.impactSolve.calls),
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics joint preparation ms",
                          double(w.work.jointPreparation.nanoseconds) * 1e-6,
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics joint preparation calls", double(w.work.jointPreparation.calls),
                          ProfileCounterMode::Sum);
    const auto collisionAfter = GetCollisionGeometryStats();
    JUDAS_PROFILE_COUNTER(
        "Physics cooked BVH nodes",
        double(collisionAfter.nodes - collisionBefore.nodes + w.parallelCookedDiagnostics.nodes),
        ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics cooked triangles",
                          double(collisionAfter.triangles - collisionBefore.triangles +
                                 w.parallelCookedDiagnostics.triangles),
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics cooked convex tests",
                          double(collisionAfter.convexTests - collisionBefore.convexTests +
                                 w.parallelCookedDiagnostics.convexTests),
                          ProfileCounterMode::Sum);
    w.stats.solverMilliseconds = MillisecondsBetween(solverStart, solverEnd);
    w.stats.totalMilliseconds = MillisecondsBetween(stepStart, stepEnd);
    const ContactGeometryDiagnostics geometryAfter = GetContactGeometryDiagnostics();
    w.stats.geometryPredicates = geometryAfter.predicates - geometryBefore.predicates +
                                 w.parallelContactDiagnostics.predicates;
    w.stats.geometryExactFallbacks = geometryAfter.exactFallbacks - geometryBefore.exactFallbacks +
                                     w.parallelContactDiagnostics.exactFallbacks;
    w.stats.geometryUnresolved = geometryAfter.unresolved - geometryBefore.unresolved +
                                 w.parallelContactDiagnostics.unresolved;
    w.stats.geometryNumericGapFallbacks = geometryAfter.numericGapFallbacks -
                                          geometryBefore.numericGapFallbacks +
                                          w.parallelContactDiagnostics.numericGapFallbacks;
    JUDAS_PROFILE_COUNTER("Physics geometry predicates", double(w.stats.geometryPredicates),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics geometry exact fallbacks",
                          double(w.stats.geometryExactFallbacks), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics geometry unresolved", double(w.stats.geometryUnresolved),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics geometry numeric gap fallbacks",
                          double(w.stats.geometryNumericGapFallbacks), ProfileCounterMode::Latest);
    ++w.completedObservationEpoch;
    w.completedObservationDt = w.completedSolverDt = fixedDeltaTime;
    w.completedObservationsReady = true;
}

void PhysicsWorld::StepCoupled(float dt, unsigned intervals,
                               const std::function<void(float, unsigned)> &before,
                               const std::function<void(float, unsigned)> &after,
                               const VelocityCoupling *coupling) {
    if (!std::isfinite(dt) || dt <= 0 || !intervals || intervals > 32)
        throw std::invalid_argument("invalid coupled physics interval");
    if (coupling && (!coupling->iterations || coupling->iterations > 64))
        throw std::invalid_argument("invalid velocity coupling quality");
    if (intervals == 1) {
        if (before)
            before(dt, 0);
        Step(dt, coupling);
        if (after)
            after(dt, 0);
        return;
    }
    Impl &w = *m_impl;
    if (w.coupledStep)
        throw std::logic_error("nested coupled physics step");
    struct Snapshot {
        BodyHandle handle;
        glm::vec3 position{0}, force{0}, torque{0};
        glm::quat rotation{1, 0, 0, 0};
        std::vector<RigidMotionSegment> motion;
    };
    std::map<unsigned, Snapshot> saved;
    for (auto slot : w.aliveSlots) {
        auto &b = w.bodies[slot];
        if (!b.HasMotion())
            continue;
        const auto handle = w.MakeHandle(slot);
        saved.emplace(handle.id, Snapshot{handle,
                                          b.rigidBody.position,
                                          b.rigidBody.forceAccumulator,
                                          b.rigidBody.torqueAccumulator,
                                          b.rigidBody.orientation,
                                          {}});
        // A next-fixed-step target still finishes at the original macro end.
        if (b.isKinematic && b.kinematic.targetNextStep) {
            b.kinematic.remainingSeconds = dt;
            b.kinematic.targetNextStep = false;
        }
    }
    const auto epoch = w.completedObservationEpoch;
    const float h = dt / float(intervals);
    std::vector<DebugContact> contacts;
    struct Reaction {
        glm::vec3 linear{0}, angular{0};
        float motor = 0;
    };
    std::map<uint64_t, Reaction> jointReactions;
    std::vector<CachedContact> observations;
    StepStats totals{};
    w.coupledStep = true;
    if (!w.collectingPreStepQueries)
        w.stepTouches.clear();
    try {
        for (unsigned interval = 0; interval < intervals; ++interval) {
            // Force producers run once per fixed update. Retain their force
            // (not their impulse) over all subintervals; interval producers such
            // as wheel contact add only their own current-geometry reactions.
            if (interval)
                for (auto &[_, s] : saved)
                    if (auto *b = w.Get(s.handle)) {
                        b->rigidBody.forceAccumulator += s.force;
                        b->rigidBody.torqueAccumulator += s.torque;
                    }
            if (before)
                before(h, interval);
            // These are completed-interval observations, not warm-start
            // inputs. JointInputs deliberately skips sleeping/inactive rows;
            // without clearing them here a prior macro publication would be
            // counted once per interval and grow on every sleeping macro.
            // Keep joint.warm and angularDrive.impulses in their last-h units.
            for (auto &joint : w.joints) {
                joint.state.reactionImpulse = {0, 0, 0};
                joint.state.reactionAngularImpulse = {0, 0, 0};
                joint.state.motorImpulse = 0;
            }
            Step(h, coupling);
            for (const auto &joint : w.joints) {
                auto &reaction = jointReactions[joint.handle.id];
                reaction.linear += joint.state.reactionImpulse;
                reaction.angular += joint.state.reactionAngularImpulse;
                reaction.motor += joint.state.motorImpulse;
            }
            const auto &a = w.stats;
#define SUM(field) totals.field += a.field
            SUM(possiblePairs);
            SUM(candidatePairs);
            SUM(layerRejectedPairs);
            SUM(collidingPairs);
            SUM(contactPoints);
            SUM(proxyReinsertions);
            SUM(geometryPredicates);
            SUM(geometryExactFallbacks);
            SUM(geometryUnresolved);
            SUM(geometryNumericGapFallbacks);
            SUM(orientationCacheHits);
            SUM(orientationCacheRebuilds);
            SUM(boundCacheHits);
            SUM(boundCacheRebuilds);
            SUM(shapeCacheRebuilds);
            SUM(solverFrameBuilds);
            SUM(geometryCacheAllocations);
            SUM(geometryCacheAllocatedBytes);
            SUM(solverFrameNodeRequests);
            SUM(impactEvents);
            SUM(impactSafetyFallback);
            SUM(impactEventCapFallback);
            SUM(impactEnergyRejections);
            SUM(impactMissingManifolds);
            SUM(impactResponsesApplied);
            SUM(impactResponseAttempts);
            SUM(impactResponseHolds);
            SUM(impactStepCapHolds);
            SUM(impactBodyCapHolds);
            SUM(impactQueries);
            SUM(impactSearchIterations);
            SUM(impactSearchLimit);
            SUM(impactUncertifiedAdvances);
            SUM(impactSearchStalls);
            SUM(impactSamplingFallbacks);
            SUM(impactSamplingTests);
            SUM(impactSamplingResolutionCaps);
            SUM(broadphaseMilliseconds);
            SUM(narrowphaseMilliseconds);
            SUM(solverMilliseconds);
            SUM(totalMilliseconds);
#undef SUM
            totals.bodies = a.bodies;
            totals.dynamicBodies = a.dynamicBodies;
            totals.sleepingBodies = a.sleepingBodies;
            totals.sleepingIslands = a.sleepingIslands;
            totals.awakeBodies = a.awakeBodies;
            totals.treeHeight = std::max(totals.treeHeight, a.treeHeight);
            totals.geometryCacheBytes = a.geometryCacheBytes;
            totals.impactPeakIterations =
                std::max(totals.impactPeakIterations, a.impactPeakIterations);
            totals.impactSamplingMaxInterval =
                std::max(totals.impactSamplingMaxInterval, a.impactSamplingMaxInterval);
            contacts.insert(contacts.end(), w.lastStepContacts.begin(), w.lastStepContacts.end());
            const size_t oldRows = observations.size();
            std::vector<bool> matched(oldRows, false);
            for (const auto &c : w.completedContactObservations) {
                size_t best = oldRows;
                double distance = kWarmStartAnchorDistance;
                for (size_t i = 0; i < oldRows; ++i) {
                    const auto &prior = observations[i];
                    if (matched[i] || !prior.key.SameBodies(c.key) ||
                        glm::dot(prior.normal, c.normal) < kWarmStartNormalDot)
                        continue;
                    const double gap = glm::length(prior.localAnchorA - c.localAnchorA);
                    if (gap < distance) {
                        distance = gap;
                        best = i;
                    }
                }
                if (best == oldRows)
                    observations.push_back(c);
                else {
                    matched[best] = true;
                    auto &prior = observations[best];
                    const auto normal = prior.normalImpulse + c.normalImpulse;
                    const auto tangent = prior.tangentImpulse + c.tangentImpulse;
                    prior = c;
                    prior.normalImpulse = normal;
                    prior.tangentImpulse = tangent;
                }
            }
            for (auto &[_, s] : saved)
                if (auto *b = w.Get(s.handle))
                    for (auto segment : b->motion.Segments()) {
                        segment.begin += double(interval) * h;
                        segment.end += double(interval) * h;
                        if (segment.anchoredTranslation)
                            segment.translationTime += double(interval) * h;
                        s.motion.push_back(segment);
                    }
            if (after)
                after(h, interval);
        }
    } catch (...) {
        w.coupledStep = false;
        throw;
    }
    w.coupledStep = false;
    w.lastStepContacts = std::move(contacts);
    std::stable_sort(observations.begin(), observations.end(),
                     [](const auto &a, const auto &b) { return a.key < b.key; });
    w.completedContactObservations = std::move(observations);
    w.completedObservationEpoch = epoch + 1;
    w.completedObservationDt = dt;
    w.stepDuration = double(h) * intervals;
    for (auto &[_, s] : saved)
        if (auto *b = w.Get(s.handle)) {
            b->previousPosition = s.position;
            b->previousOrientation = s.rotation;
            b->previousBound.valid = false;
            b->motion.SetRecorded(std::move(s.motion));
            w.RefreshProxy(*b);
            totals.motionSegments += b->motion.Segments().size();
            totals.motionStorageBytes += b->motion.StorageBytes();
        }
    for (auto &joint : w.joints)
        if (auto it = jointReactions.find(joint.handle.id); it != jointReactions.end()) {
            joint.state.reactionImpulse = it->second.linear;
            joint.state.reactionAngularImpulse = it->second.angular;
            joint.state.motorImpulse = it->second.motor;
        }
    w.stats = totals;
    PublishImpactCounters(w.stats);
    if (!w.queryTouchesPending)
        w.FinishTouches();
}

const PhysicsWorld::StepStats &PhysicsWorld::LastStepStats() const {
    return m_impl->stats;
}

void PhysicsWorld::ShiftOrigin(glm::dvec3 delta) {
    if (!OriginLimits::ValidTranslation(delta))
        throw std::invalid_argument("invalid origin translation");
    auto &w = *m_impl;
    if (w.stepping || w.coupledStep)
        throw std::logic_error("origin translation requires completed fixed boundary");
    const auto shift = [&](glm::vec3 &p) { p = glm::vec3(glm::dvec3(p) - delta); };
    for (auto slot : w.aliveSlots) {
        auto &b = w.bodies[slot];
        shift(b.rigidBody.position);
        shift(b.previousPosition);
        shift(b.kinematic.target.position);
        b.motion.ShiftOrigin(delta);
        b.currentBound.valid = b.previousBound.valid = false;
        w.InvalidateImpactPose(slot);
    }
    for (auto &j : w.joints)
        if (!j.state.settings.bodyB.IsValid())
            shift(j.state.settings.anchorB);
    for (auto &c : w.lastStepContacts)
        shift(c.point);
    for (auto *events : {&w.previousTouches, &w.stepTouches, &w.preStepQueryTouches})
        for (auto &entry : *events)
            shift(entry.second.point);
    for (auto &e : w.touchEvents)
        shift(e.point);
    w.tree.ShiftOrigin(delta);
    w.overlapPairs.Invalidate();
}
