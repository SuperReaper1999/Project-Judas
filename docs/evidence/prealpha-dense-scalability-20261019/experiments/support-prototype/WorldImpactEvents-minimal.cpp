#include "WorldInternal.h"
#include "ImpactCandidateCache.h"
#include "ImpactSupportSets.h"

#include <unordered_map>

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::ResolveInitialImpacts(float dt) -> void {
    if (std::none_of(startContacts.begin(), startContacts.end(),
                     [](const auto &c) { return c.newImpact; }))
        return;
    std::vector<Contact> geometry;
    geometry.reserve(solver.Constraints().size());
    for (const auto &c : solver.Constraints()) {
        Contact g;
        g.hit = true;
        g.point = c.point;
        g.normal = c.normal;
        g.preciseNormal = c.preciseNormal;
        g.penetration = c.penetration;
        g.signedSeparation = c.signedSeparation;
        g.localAnchorA = c.localAnchorA;
        g.localAnchorB = c.localAnchorB;
        g.hasLocalAnchors = c.hasLocalAnchors;
        g.anchorAInWorldFrame = c.anchorAInWorldFrame;
        g.anchorBInWorldFrame = c.anchorBInWorldFrame;
        geometry.push_back(g);
    }
    std::vector<unsigned> parent(bodies.size());
    for (unsigned i = 0; i < parent.size(); ++i)
        parent[i] = i;
    auto root = [&](unsigned i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    for (const auto &c : startContacts)
        if (!bodies[c.a].rigidBody.IsStatic() && !bodies[c.b].rigidBody.IsStatic())
            parent[root(c.b)] = root(c.a);
    for (const auto &joint : joints)
        if (JointActive(joint) && joint.state.settings.bodyB.IsValid()) {
            const auto a = joint.state.settings.bodyA.id & kSlotMask,
                       b = joint.state.settings.bodyB.id & kSlotMask;
            if (!bodies[a].rigidBody.IsStatic() && !bodies[b].rigidBody.IsStatic())
                parent[root(b)] = root(a);
        }
    std::vector<std::pair<unsigned, std::size_t>> order;
    for (std::size_t i = 0; i < startContacts.size(); ++i) {
        const auto &c = startContacts[i];
        order.emplace_back(root(bodies[c.a].rigidBody.IsStatic() ? c.b : c.a), i);
    }
    std::sort(order.begin(), order.end());
    for (std::size_t begin = 0; begin < order.size();) {
        std::size_t end = begin + 1;
        bool impact = startContacts[order[begin].second].newImpact;
        while (end < order.size() && order[end].first == order[begin].first) {
            impact = impact || startContacts[order[end].second].newImpact;
            ++end;
        }
        if (impact) {
            std::vector<ImpactContact> rows;
            for (auto k = begin; k < end; ++k) {
                auto &c = startContacts[order[k].second];
                const bool persistent = c.warmNormal > 0;
                c.warmNormal = 0;
                c.warmTangent = glm::vec3(0);
                if (geometry[c.constraint].signedSeparation > 0)
                    continue;
                auto &a = bodies[c.a];
                auto &b = bodies[c.b];
                rows.push_back({&a.rigidBody, &b.rigidBody, geometry[c.constraint], c.friction,
                                std::max(a.restitution, b.restitution), persistent});
            }
            std::vector<unsigned char> included(bodies.size(), 0);
            for (unsigned slot : aliveSlots)
                if (root(slot) == order[begin].first)
                    included[slot] = 1;
            PrepareJoints(eventJointSolver, JointInputs(&included), dt, false, true);
            const auto result = [&] {
                PhysicsWorkTimer timer(work.impactSolve, work.active);
                return impactSolver.Solve(rows, dt, false,
                                          eventJointSolver.Empty() ? nullptr : &eventJointSolver);
            }();
            if (!result.energyBudgetSatisfied)
                throw std::runtime_error("inelastic impact energy guard failed");
            if (result.effectiveRestitution == 0)
                for (auto k = begin; k < end; ++k)
                    startContacts[order[k].second].newImpact = false;
            ++stats.impactEvents;
            stats.impactSafetyFallback += result.safetyFallback;
        }
        begin = end;
    }
    // Only the rare time-zero impact path needs rebuilt warm-start inputs.
    // Persistent contact solver arithmetic and APIs remain unchanged.
    solver.Clear();
    for (const auto &c : startContacts) {
        auto &a = bodies[c.a];
        auto &b = bodies[c.b];
        const auto &oa = Orientation(a);
        const auto &ob = Orientation(b);
        solver.AddContact(a.rigidBody, b.rigidBody, geometry[c.constraint], c.friction, 0,
                          c.warmNormal, c.warmTangent, &oa.rotation, &ob.rotation);
    }
}

auto PhysicsWorld::Impl::AdvanceImpacts(float dt, bool coupledVelocityChanged) -> void {
    // Keep published contact storage ordered even if a later physical
    // query throws. Successful steps sort once; failure observers retain
    // the same lower_bound invariant as the former per-event publication.
    struct ObservationPublication {
        std::vector<CachedContact> &rows;
        PhysicsWorkSample &work;
        bool active, dirty = false;
        void Publish() {
            if (!dirty)
                return;
            PhysicsWorkTimer timer(work, active);
            std::stable_sort(
                rows.begin(), rows.end(),
                [](const CachedContact &a, const CachedContact &b) { return a.key < b.key; });
            dirty = false;
        }
        ~ObservationPublication() {
            Publish();
        }
    } observationPublication{completedContactObservations, work.impactObservations, work.active};
    // The incoming cache is already ordered. Only newly appended TOI
    // identities require publication; erasure and impulse changes preserve
    // order. Keep that invariant on failure as well as successful return.
    ObservationPublication cachePublication{contactCache, work.continuationCache, work.active};
    stepDuration = dt;
    impactQueryAnchorTime = 0;
    impactPoseEvaluations = impactPoseReuses = impactOrientationBuilds = impactTruthRequests =
        impactTruthReuses = impactBoundsRejected = 0;
    impactActualBoundsRejected = 0;
    for (auto slot : aliveSlots) {
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    }
    impactCounts.assign(bodies.size(), 0);
    impactCapped.assign(bodies.size(), 0);
    // A sleeping body still needs its stationary pre-impact trajectory.
    // An actual TOI may wake it and append a new segment during this step.
    for (const auto slot : aliveSlots)
        if (bodies[slot].isDynamic) {
            bodies[slot].motion.Begin(MakeHandle(slot).id, bodies[slot].rigidBody, dt);
            InvalidateImpactPose(slot);
        }
    // Initial touching impacts can launch a previously stationary body.
    // Refresh those trajectories before searching for a separated event.
    if (stats.impactEvents || coupledVelocityChanged) {
        for (auto slot : aliveSlots)
            if (bodies[slot].isDynamic)
                CoverStepReach(bodies[slot], dt);
        GenerateCandidatePairs();
        separatedPairs.clear();
        for (const auto &pair : candidatePairs)
            for (int pa = 0; pa < PrimitiveCount(bodies[pair.first].shape); ++pa)
                for (int pb = 0; pb < PrimitiveCount(bodies[pair.second].shape); ++pb)
                    separatedPairs.push_back({pair.first, pair.second, unsigned(pa), unsigned(pb)});
    }
    // Support pairs whose rotation over the step could carry a feature
    // through the support remain under continuous search.
    spinningPairs.clear();
    for (const auto &key : supportPairs)
        if (SpinsThroughSupport(key, dt))
            spinningPairs.push_back(key);
    if (!spinningPairs.empty())
        supportPairs.erase(std::remove_if(supportPairs.begin(), supportPairs.end(),
                                          [&](const auto &key) {
                                              return std::binary_search(spinningPairs.begin(),
                                                                        spinningPairs.end(), key);
                                          }),
                           supportPairs.end());
    // Preserve joint encounter order, but visit only incident joints while
    // growing an impact island. Scanning every unrelated joint for every
    // reached body makes independent machinery multiply dense-pile cost.
    // Allocate/build only once an actual event needs island discovery.
    // Free flight and fully quiet worlds must not pay this scratch cost.
    std::vector<std::vector<unsigned>> impactJointNeighbours;
    bool jointNeighboursReady = false;
    double now = 0;
    scheduledImpacts.clear();
    // This index owns no encounter order. The event vector and sorted dirty
    // keys retain the existing chronological and lexicographic policy.
    struct ScheduledKeyHash {
        std::size_t operator()(const std::array<unsigned, 4> &key) const noexcept {
            std::uint64_t value = (std::uint64_t(key[0]) << 32) | key[1];
            const std::uint64_t parts = (std::uint64_t(key[2]) << 32) | key[3];
            value += parts * 0x9e3779b97f4a7c15ULL;
            value ^= value >> 30;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27;
            value *= 0x94d049bb133111ebULL;
            return std::size_t(value ^ (value >> 31));
        }
    };
    std::unordered_map<std::array<unsigned, 4>, std::size_t, ScheduledKeyHash> scheduledIndex;
    auto schedule = [&](const std::array<unsigned, 4> &key) {
        if (!CanRespond(key[0], key[1]))
            return;
        const double time = StaticMeshPolyPair(key[0], key[1], int(key[2]), int(key[3]))
                                ? ImpactTime(key[0], key[1], int(key[2]), int(key[3]), now, dt)
                            : std::binary_search(supportPairs.begin(), supportPairs.end(), key)
                                ? double(dt) + 1
                            : std::binary_search(spinningPairs.begin(), spinningPairs.end(), key)
                                ? ImpactTime(key[0], key[1], int(key[2]), int(key[3]), now, dt,
                                             SpinPenetrationTarget(key))
                                : ImpactTime(key[0], key[1], int(key[2]), int(key[3]), now, dt);
        const auto found = scheduledIndex.find(key);
        if (found != scheduledIndex.end())
            scheduledImpacts[found->second].time = time;
        else {
            scheduledIndex.emplace(key, scheduledImpacts.size());
            scheduledImpacts.push_back({key, time});
        }
    };
    // These three initial lists may overlap after time-zero impulses.
    // They share the same anchored trajectories and query interval; reuse
    // an already scheduled key. Actual event-induced dirty keys below are
    // still rescheduled unconditionally after their trajectories change.
    const auto scheduleInitial = [&](const std::array<unsigned, 4>& key) {
        if (scheduledIndex.find(key) == scheduledIndex.end())
            schedule(key);
    };
    for (const auto &key : separatedPairs)
        scheduleInitial(key);
    for (const auto &key : spinningPairs)
        scheduleInitial(key);
    for (const auto &key : supportPairs)
        if (StaticMeshPolyPair(key[0], key[1], int(key[2]), int(key[3])))
            scheduleInitial(key);
    // Capacities live through this event pass; values and membership are
    // rebuilt for each event. No cross-time contact truth is cached.
    // All proxy changes in this event pass go through CoverStepReach, which
    // advances reinsertionsSinceStep. Wake changes only sleep/island state;
    // callers cannot create bodies or run gameplay callbacks inside this pass.
    ImpactCandidateCache candidateCache;
    const auto impactCandidates = [&](unsigned slot,
                                      const Aabb &query) -> const std::vector<unsigned> & {
        return candidateCache.Get(
            slot, tree.FatAabb(bodies[slot].proxy), query, reinsertionsSinceStep,
            [&](const Aabb &box) -> const std::vector<unsigned> & { return QuerySlots(box); },
            [&](unsigned other) -> const Aabb & { return tree.FatAabb(bodies[other].proxy); });
    };
    ImpactPairVisitSet visitedPairs;
    struct DiscoveredManifold {
        std::size_t touchingIndex;
        int partA, partB;
        ContactManifold geometry;
    };
    std::vector<unsigned> island;
    std::vector<unsigned char> included;
    std::vector<std::pair<unsigned, unsigned>> touching;
    std::vector<DiscoveredManifold> discoveredManifolds;
    std::vector<std::pair<glm::vec3, glm::vec3>> oldVelocities;
    std::vector<ImpactContact> rows;
    std::vector<ContactKey> observationKeys;
    std::vector<std::array<unsigned, 4>> eventPairs;
    std::vector<std::array<unsigned, 4>> eventPairMembership;
    ImpactPairVisitSet dirtyBodyPairs;
    std::vector<unsigned char> dirtyScheduled;
    std::vector<std::array<unsigned, 4>> dirty;
    for (;;) {
        double earliest = double(dt) + 1;
        unsigned seedA = 0, seedB = 0;
        std::array<unsigned, 4> selected{};
        for (const auto &event : scheduledImpacts) {
            if (event.time < earliest || (event.time == earliest && event.key < selected)) {
                earliest = event.time;
                selected = event.key;
                seedA = event.key[0];
                seedB = event.key[1];
            }
        }
        if (earliest > dt)
            break;
        if (!jointNeighboursReady) {
            impactJointNeighbours.resize(bodies.size());
            for (const auto &joint : joints)
                if (JointActive(joint) && joint.state.settings.bodyB.IsValid()) {
                    const auto a = joint.state.settings.bodyA.id & kSlotMask,
                               b = joint.state.settings.bodyB.id & kSlotMask;
                    impactJointNeighbours[a].push_back(b);
                    if (a != b)
                        impactJointNeighbours[b].push_back(a);
                }
            jointNeighboursReady = true;
        }
        now = earliest;
        impactQueryAnchorTime = now;
        // Infinite-mass boundaries do not join the velocity-changing
        // island, but solver lever arms and observed point velocities
        // must use their actual event-time COM/orientation.
        for (auto slot : aliveSlots)
            if (bodies[slot].isKinematic) {
                auto &b = bodies[slot];
                auto p = PoseAt(slot, now);
                b.rigidBody.position = p.position;
                b.rigidBody.orientation = p.orientation;
                b.rigidBody.linearVelocity = p.linearVelocity;
                b.rigidBody.angularVelocity = p.angularVelocity;
            }
        island.clear();
        included.assign(bodies.size(), 0);
        auto include = [&](unsigned slot) {
            if (!included[slot] && !bodies[slot].rigidBody.IsStatic()) {
                if (bodies[slot].sleeping)
                    Wake(slot);
                included[slot] = 1;
                island.push_back(slot);
            }
        };
        include(seedA);
        include(seedB);
        touching.clear();
        discoveredManifolds.clear();
        visitedPairs.Begin(bodies.size());
        // Membership removes only previously resolved identical queries;
        // encounter order and uncertainty handling remain unchanged.
        const auto simplePart = [&](unsigned slot, int part) {
            const auto &shape = bodies[slot].shape;
            const auto type = shape.type == ShapeType::CompoundBoxes
                                  ? shape.boxes[std::size_t(part)].type
                                  : shape.type;
            return type == ShapeType::Box || type == ShapeType::Sphere;
        };
        {
            PhysicsWorkTimer timer(work.impactIslands, work.active);
            for (std::size_t next = 0; next < island.size(); ++next) {
                const unsigned a = island[next];
                for (unsigned neighbour : impactJointNeighbours[a])
                    include(neighbour);
                // Nested geometry and wake paths do not query the tree, so this
                // scratch remains valid until the next outer body query.
                const auto &candidates =
                    impactCandidates(a, ActualImpactBound(a, ImpactPoseAt(a, now)));
                for (unsigned b : candidates) {
                    if (a == b || !CanRespond(a, b))
                        continue;
                    const auto pair = std::minmax(a, b);
                    const uint64_t pairKey = (uint64_t(pair.first) << 32) | pair.second;
                    if (visitedPairs.Contains(pairKey))
                        continue;
                    bool hit = false, reliableMiss = true;
                    std::optional<DiscoveredManifold> discovered;
                    for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                        for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                            if (hit)
                                continue; // Preserve the existing first-hit short circuit.
                            if (simplePart(a, pa) && simplePart(b, pb)) {
                                auto geometry = PairAt(a, b, pa, pb, now, 0);
                                hit = geometry.count > 0;
                                reliableMiss &= !geometry.uncertain;
                                // Response is always in ascending body order.
                                // A reversed discovery must retain the original
                                // canonical response query, not flip its rows.
                                if (hit && a == pair.first)
                                    discovered = DiscoveredManifold{touching.size(), pa, pb,
                                                                    std::move(geometry)};
                            } else {
                                hit = PairTouches(a, b, pa, pb, now);
                                // Specialized Boolean paths stay unchanged;
                                // a miss carries no public uncertainty proof.
                                reliableMiss = false;
                            }
                        }
                    if (hit) {
                        visitedPairs.Insert(pairKey);
                        if (discovered)
                            discoveredManifolds.push_back(std::move(*discovered));
                        touching.emplace_back(pair);
                        include(b);
                    } else if (reliableMiss) {
                        // Only a resolved primitive separation may suppress
                        // the reverse traversal's identical existence query.
                        visitedPairs.Insert(pairKey);
                    }
                }
            }
        }
        // A supported body's incoming impact is mechanically coupled even
        // when a cached support point currently has a small positive gap.
        // This only chooses restitution zero: separated support geometry
        // is never inserted into the actual-contact impulse rows below.
        // Spinning support pairs are still supports.
        const auto supports = [&](const auto &pair) {
            return included[pair[0]] || included[pair[1]];
        };
        const bool supportedIsland =
            std::any_of(supportPairs.begin(), supportPairs.end(), supports) ||
            std::any_of(spinningPairs.begin(), spinningPairs.end(), supports);
        bool capture = supportedIsland;
        oldVelocities.clear();
        oldVelocities.reserve(island.size());
        for (unsigned slot : island) {
            auto &body = bodies[slot];
            const auto pose = PoseAt(slot, now);
            body.rigidBody.angularVelocity = pose.angularVelocity;
            oldVelocities.emplace_back(body.rigidBody.linearVelocity,
                                       body.rigidBody.angularVelocity);
            body.rigidBody.position = pose.position;
            body.rigidBody.orientation = pose.orientation;
            // Gravity was deferred while stationary. Integrate only the
            // remaining interval; never drift the frozen pre-impact pose.
            if (body.deferredAcceleration) {
                body.rigidBody.linearVelocity +=
                    body.lastAcceleration * static_cast<float>(dt - now);
                body.deferredAcceleration = false;
            }
            if (++impactCounts[slot] > 16) {
                if (!impactCapped[slot]) {
                    impactCapped[slot] = 1;
                    ++stats.impactEventCapFallback;
                }
                capture = true;
            }
        }
        rows.clear();
        observationKeys.clear();
        eventPairs.clear();
        std::size_t discoveredIndex = 0;
        for (std::size_t touchingIndex = 0; touchingIndex < touching.size(); ++touchingIndex) {
            const auto &pair = touching[touchingIndex];
            const DiscoveredManifold *discovered =
                discoveredIndex < discoveredManifolds.size() &&
                        discoveredManifolds[discoveredIndex].touchingIndex == touchingIndex
                    ? &discoveredManifolds[discoveredIndex++]
                    : nullptr;
            auto &a = bodies[pair.first];
            auto &b = bodies[pair.second];
            for (int pa = 0; pa < PrimitiveCount(a.shape); ++pa)
                for (int pb = 0; pb < PrimitiveCount(b.shape); ++pb) {
                    const auto m = discovered && discovered->partA == pa && discovered->partB == pb
                                       ? discovered->geometry
                                       : PairAt(pair.first, pair.second, pa, pb, now, 0);
                    if (m.count)
                        eventPairs.push_back({pair.first, pair.second, unsigned(pa), unsigned(pb)});
                    for (int k = 0; k < m.count; ++k) {
                        Observe(pair.first, pair.second, m.points[k]);
                        observationKeys.push_back(
                            {pair.first, pair.second, a.generation, b.generation, pa, pb});
                        rows.push_back(
                            {&a.rigidBody, &b.rigidBody, m.points[k],
                             std::sqrt(std::max(0.f, a.friction) * std::max(0.f, b.friction)),
                             std::max(a.restitution, b.restitution), false});
                        lastStepContacts.push_back(
                            {m.points[k].point, m.points[k].normal, m.points[k].penetration});
                    }
                }
        }
        if (rows.empty())
            throw std::runtime_error("impact bracket lost contact geometry");
        if (work.active) {
            work.peakImpactBodies = std::max(work.peakImpactBodies, island.size());
            work.peakImpactRows = std::max(work.peakImpactRows, rows.size());
        }
        PrepareJoints(eventJointSolver, JointInputs(&included), dt, false, true);
        const auto result = [&] {
            PhysicsWorkTimer timer(work.impactSolve, work.active);
            return impactSolver.Solve(rows, dt, capture,
                                      eventJointSolver.Empty() ? nullptr : &eventJointSolver);
        }();
        if (!result.energyBudgetSatisfied)
            throw std::runtime_error("inelastic impact energy guard failed");
        // Replace only primitive groups for which actual TOI geometry
        // was solved. Never replay warm impulses or duplicate repeated
        // TOI loads. Unaffected stance groups retain their completed rows.
        {
            PhysicsWorkTimer timer(work.impactObservations, work.active);
            auto replacedGroups = observationKeys;
            const auto groupLess = [](const ContactKey &a, const ContactKey &b) {
                return std::tie(a.slotA, a.slotB, a.partA, a.partB, a.generationA, a.generationB) <
                       std::tie(b.slotA, b.slotB, b.partA, b.partB, b.generationA, b.generationB);
            };
            std::sort(replacedGroups.begin(), replacedGroups.end(), groupLess);
            replacedGroups.erase(std::unique(replacedGroups.begin(), replacedGroups.end(),
                                             [](const ContactKey &a, const ContactKey &b) {
                                                 return a.SameBodies(b);
                                             }),
                                 replacedGroups.end());
            observationPublication.dirty = true;
            completedContactObservations.erase(
                std::remove_if(
                    completedContactObservations.begin(), completedContactObservations.end(),
                    [&](const CachedContact &cached) {
                        return std::binary_search(replacedGroups.begin(), replacedGroups.end(),
                                                  cached.key, groupLess);
                    }),
                completedContactObservations.end());
            const auto &observed = impactSolver.Constraints();
            if (observed.size() != observationKeys.size())
                throw std::runtime_error("impact observation mapping mismatch");
            for (std::size_t i = 0; i < observed.size(); ++i) {
                const auto &row = observed[i];
                const auto &key = observationKeys[i];
                if (row.normalImpulse <= 0)
                    continue;
                completedContactObservations.push_back(
                    {key, row.localAnchorA, row.normal, row.normalImpulse, row.tangentImpulse,
                     float(row.signedSeparation), bodies[key.slotA].contactObservationRevision,
                     bodies[key.slotB].contactObservationRevision});
            }
        }
        if (result.effectiveRestitution == 0) {
            JUDAS_PROFILE_SCOPE("Physics support set publication");
            // An inelastic event leaves the pair touching. It becomes a
            // support pair unless its new spin can still turn a feature
            // through the support before the step ends.
            for (const auto &key : eventPairs) {
                const bool spinning = SpinsThroughSupport(key, double(dt) - now, now, true);
                SetImpactSupport(supportPairs,spinningPairs,key,spinning);
            }
        }
        ++stats.impactEvents;
        stats.impactSafetyFallback += result.safetyFallback;
        // A geometric overlap enclosure alone does not certify the reaction
        // history of a coupled mechanical system. Keep the original event
        // and cache policy for hard joints, external couplers and elastic
        // flights. The fast continuation is for free inelastic primitives.
        const bool continuationEligible =
            result.effectiveRestitution == 0 && eventJointSolver.Empty() &&
            !coupledVelocityChanged &&
            std::all_of(eventPairs.begin(), eventPairs.end(), [&](const auto &pair) {
                return ContinuationPrimitive(bodies[pair[0]]) &&
                       ContinuationPrimitive(bodies[pair[1]]);
            });
        if (work.active) {
            if (!eventJointSolver.Empty() || coupledVelocityChanged)
                ++work.coupledFallbackEvents;
            else if (result.effectiveRestitution != 0)
                ++work.elasticFallbackEvents;
            else if (!continuationEligible)
                ++work.geometryFallbackEvents;
        }
        {
            PhysicsWorkTimer timer(work.continuationCache, work.active);
            if (continuationEligible) {
                cachePublication.dirty = true;
                // A velocity-changing island invalidates its old load guesses,
                // not contact identity. New actual TOI features also need zero-load
                // identity for next step's ordinary geometric support matching.
                // Neither old warm impulses nor impact impulses are replayed here.
                for (auto &cached : contactCache)
                    if (included[cached.key.slotA] || included[cached.key.slotB]) {
                        cached.normalImpulse = 0;
                        cached.tangentImpulse = glm::vec3(0);
                    }
                auto replaced = observationKeys;
                std::sort(replaced.begin(), replaced.end());
                replaced.erase(std::unique(replaced.begin(), replaced.end(),
                                           [](const ContactKey &a, const ContactKey &b) {
                                               return a.SameBodies(b);
                                           }),
                               replaced.end());
                contactCache.erase(std::remove_if(contactCache.begin(), contactCache.end(),
                                                  [&](const CachedContact &c) {
                                                      return std::binary_search(
                                                          replaced.begin(), replaced.end(), c.key);
                                                  }),
                                   contactCache.end());
                // An elastic departure is a new flight, not support. In particular,
                // a short ballistic return must still receive its own restitution.
                if (result.effectiveRestitution == 0) {
                    const auto &contactRows = impactSolver.Constraints();
                    for (std::size_t i = 0; i < contactRows.size(); ++i) {
                        const auto &row = contactRows[i];
                        const auto &key = observationKeys[i];
                        contactCache.push_back({key, row.localAnchorA, row.normal, 0, glm::vec3(0),
                                                float(row.signedSeparation),
                                                bodies[key.slotA].contactObservationRevision,
                                                bodies[key.slotB].contactObservationRevision});
                    }
                }
            } else {
                contactCache.erase(std::remove_if(contactCache.begin(), contactCache.end(),
                                                  [&](const CachedContact &c) {
                                                      return included[c.key.slotA] ||
                                                             included[c.key.slotB];
                                                  }),
                                   contactCache.end());
            }
        }
        std::vector<unsigned> changed;
        for (std::size_t i = 0; i < island.size(); ++i) {
            const auto slot = island[i];
            auto &b = bodies[slot];
            if (SamePosition(oldVelocities[i].first, b.rigidBody.linearVelocity) &&
                SamePosition(oldVelocities[i].second, b.rigidBody.angularVelocity))
                continue;
            changed.push_back(slot);
            // Membership remains nonzero; value two also marks the
            // changed subset without scanning it for every scheduled pair.
            included[slot] = 2;
            b.motion.ChangeVelocity(b.rigidBody, now);
            InvalidateImpactPose(slot);
            CoverStepReach(b, static_cast<float>(dt - now));
        }
        // Solver rows and event pairs retain discovery order. Membership
        // needs no such order: avoid rescanning every actual event pair
        // for every incident support in a large connected assembly.
        eventPairMembership.assign(eventPairs.begin(), eventPairs.end());
        std::sort(eventPairMembership.begin(), eventPairMembership.end());
        supportPairs.erase(
            std::remove_if(supportPairs.begin(), supportPairs.end(),
                           [&](const auto &pair) {
                               return (included[pair[0]] == 2 || included[pair[1]] == 2) &&
                                      !std::binary_search(eventPairMembership.begin(),
                                                          eventPairMembership.end(), pair) &&
                                      (!continuationEligible ||
                                       !SupportContinuationSafe(pair, now, dt));
                           }),
            supportPairs.end());
        // Cached event times depend only on the two anchored trajectories.
        // Recompute incident queries after an actual velocity change; retain
        // every unaffected event (including disjoint simultaneous events).
        dirty.clear();
        dirtyScheduled.assign(scheduledImpacts.size(), 0);
        auto markDirty = [&](const std::array<unsigned, 4> &key) {
            const auto found = scheduledIndex.find(key);
            if (found != scheduledIndex.end()) {
                auto &marked = dirtyScheduled[found->second];
                if (marked)
                    return;
                marked = 1;
            }
            dirty.push_back(key);
        };
        markDirty(selected); // retire the event even if its impulse is zero
        for (const auto &event : scheduledImpacts)
            if (included[event.key[0]] == 2 || included[event.key[1]] == 2)
                markDirty(event.key);
        dirtyBodyPairs.Begin(bodies.size());
        for (unsigned slot : changed) {
            const auto &candidates = impactCandidates(slot, tree.FatAabb(bodies[slot].proxy));
            for (unsigned other : candidates)
                if (other != slot && CanRespond(slot, other)) {
                    const auto a = std::min(slot, other), b = std::max(slot, other);
                    const std::uint64_t pairKey = (std::uint64_t(a) << 32) | b;
                    // Both changed endpoints can discover the same pair. Expand
                    // its children once; no contact or TOI result is reused.
                    if (dirtyBodyPairs.Contains(pairKey))
                        continue;
                    dirtyBodyPairs.Insert(pairKey);
                    for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                        for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb)
                            markDirty({a, b, unsigned(pa), unsigned(pb)});
                }
        }
        // Known keys were marked once; previously unscheduled keys arose from
        // one canonical body-pair expansion. Retain the original unique order.
        std::sort(dirty.begin(), dirty.end());
        for (const auto &key : dirty)
            schedule(key);
        if (now >= dt)
            break;
    }
    // No solver reads completed observations during this event pass. Each
    // event still replaces exactly its actual primitive groups and keeps
    // row insertion order. One stable publication sort has the same final
    // key/row order as sorting the entire history after every impact.
    observationPublication.Publish();
    cachePublication.Publish();
    for (const auto slot : aliveSlots)
        if (bodies[slot].HasMotion() && !bodies[slot].sleeping) {
            auto &body = bodies[slot];
            const auto pose = body.motion.EvaluateAfterEvents(dt);
            stats.motionSegments += body.motion.Segments().size();
            stats.motionStorageBytes += body.motion.StorageBytes();
            body.rigidBody.position = pose.position;
            body.rigidBody.orientation = pose.orientation;
            if (body.isDynamic)
                body.rigidBody.angularVelocity = pose.angularVelocity;
            if (body.isKinematic) {
                body.rigidBody.linearVelocity = pose.linearVelocity;
                body.rigidBody.angularVelocity = pose.angularVelocity;
                if (body.kinematic.control == KinematicControl::Target) {
                    body.kinematic.remainingSeconds = std::max(
                        0.f, body.kinematic.remainingSeconds - float(body.kinematicActiveDuration));
                    if (body.kinematic.remainingSeconds == 0)
                        body.kinematic.control = KinematicControl::Stopped;
                }
            }
        }
}
