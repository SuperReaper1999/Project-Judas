#include "WorldInternal.h"
#include "WorldMk2Policy.h"

#include <queue>
#include <tuple>
#include <map>

using namespace JudasPhysicsDetail;

void PhysicsWorld::Impl::Mk2Continuous(float dt, const std::vector<char> *required,
                                       std::vector<unsigned> *arrivalCounts, unsigned *eventCount) {
    PhysicsWorkTimer continuousTimer(work.ccdTotal, work.active);
    stepDuration = dt;
    if (work.active)
        ++work.ccdPasses;
    double previousEventTime = -1;
    if (impactGeometryMemo.empty()) {
        impactGeometryMemo.resize(4096);
        ++cacheCounters.allocations;
        cacheCounters.allocatedBytes += impactGeometryMemo.capacity() * sizeof(ImpactGeometryMemo);
    }
    if (impactSearchMemo.empty()) {
        impactSearchMemo.resize(4096);
        ++cacheCounters.allocations;
        cacheCounters.allocatedBytes += impactSearchMemo.capacity() * sizeof(ImpactSearchMemo);
    }
    if (++impactGeometryEpoch == 0) {
        for (auto &memo : impactGeometryMemo)
            memo.epoch = 0;
        for (auto &memo : impactSearchMemo)
            memo.epoch = 0;
        ++impactGeometryEpoch;
    }
    impactGeometryMemoActive = true;
    struct MemoScope {
        bool &active;
        ~MemoScope() {
            active = false;
        }
    } memoScope{impactGeometryMemoActive};
    mk2ContinuousAwakened = false;
    impactQueryAnchorTime = 0;
    for (auto slot : aliveSlots) {
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    }
    std::vector<std::uint64_t> rowTrajectoryRevisions;
    rowTrajectoryRevisions.reserve(bodies.size());
    for (const auto &body : bodies)
        rowTrajectoryRevisions.push_back(body.impactTrajectoryRevision);
    const auto fastReach = [&](unsigned slot) {
        if (required && slot < required->size() && (*required)[slot])
            return true;
        const auto &body = bodies[slot];
        double feature = std::numeric_limits<double>::infinity(), reach = 0;
        for (int part = 0; part < PrimitiveCount(body.shape); ++part)
            feature = std::min(feature, PrimitiveFeature(slot, part));
        for (const auto &segment : body.motion.Segments())
            reach = std::max(reach, (double(glm::length(segment.linearVelocity)) +
                                     double(glm::length(segment.angularVelocity)) *
                                         double(body.boundingRadius)) *
                                        double(dt));
        return reach > kMk2FastFraction * feature;
    };
    const auto movable = [&](unsigned slot) {
        const auto &body = bodies[slot];
        return body.isDynamic && !body.isKinematic && !body.rigidBody.IsStatic() &&
               !body.simulationSuspended && !body.sensor && !body.queryOnly;
    };
    // Continue (or hold) a body's ledger from `at` with its current velocity.
    const auto retime = [&](unsigned slot, double at, bool hold, bool resolved = true,
                            glm::vec3 safeSlide = glm::vec3(0)) {
        PhysicsWorkTimer retimeTimer(work.ccdRetime, work.active);
        auto &body = bodies[slot];
        if (work.active)
            ++work.ccdRetimings;
        if (!resolved && !body.motion.Segments().empty())
            body.rigidBody = body.motion.EvaluateAfterEvents(at);
        std::vector<RigidMotionSegment> kept;
        for (auto segment : body.motion.Segments()) {
            if (segment.begin >= at && !kept.empty())
                break;
            if (segment.end > at) {
                const auto pose = segment.EvaluateGeometry(at);
                segment.end = at;
                segment.endPosition = pose.position;
                segment.endOrientation = pose.orientation;
            }
            kept.push_back(segment);
        }
        if (kept.empty()) {
            // A sleeping participant had a stationary pre-impact history.
            // Its newly published impulse must not rewrite that history.
            RigidMotionSegment rest;
            rest.owner = MakeHandle(slot).id;
            rest.begin = 0;
            rest.end = at;
            rest.position = rest.endPosition = body.rigidBody.position;
            rest.orientation = rest.endOrientation = body.rigidBody.orientation;
            rest.planarInverseIz = body.rigidBody.planarInverseIz;
            rest.planeNormal = body.rigidBody.planeNormal;
            rest.inverseMass = body.rigidBody.inverseMass;
            rest.inverseInertiaLocal = body.rigidBody.inverseInertiaLocal;
            kept.push_back(rest);
        }
        const auto &oldPath = body.motion.Segments();
        auto oldLeaf =
            std::upper_bound(oldPath.begin(), oldPath.end(), at,
                             [](double time, const auto &leaf) { return time < leaf.begin; });
        const auto linearProjection =
            oldLeaf == oldPath.begin() ? glm::vec3(0) : (oldLeaf - 1)->linearProjection;
        const auto angularProjection =
            oldLeaf == oldPath.begin() ? glm::vec3(0) : (oldLeaf - 1)->angularProjection;
        RigidMotionSegment next = kept.back();
        next.begin = at;
        next.end = dt;
        next.position = next.endPosition;
        next.orientation = next.endOrientation;
        if (hold) {
            next.anchoredTranslation = false;
            next.linearProjection = next.angularProjection = glm::vec3(0);
            next.hasPhysicalLinearVelocity = true;
            next.physicalLinearVelocity = body.rigidBody.linearVelocity;
            next.angularVelocity = body.rigidBody.angularVelocity;
            next.linearVelocity = safeSlide;
            next.driftAngularVelocity = glm::vec3(0);
            next.endPosition = next.position + safeSlide * float(dt - at);
            next.endOrientation = next.orientation;
            kept.push_back(next);
            body.rigidBody.position = next.endPosition;
            body.rigidBody.orientation = next.orientation;
        } else {
            std::vector<RigidMotionSegment> tail;
            const auto physical = body.rigidBody;
            auto drift = physical;
            drift.linearVelocity += linearProjection;
            drift.angularVelocity += angularProjection;
            AppendDynamicRigidMotion(tail, next.owner, drift, at, dt);
            for (auto &leaf : tail) {
                leaf.linearProjection = linearProjection;
                leaf.angularProjection = angularProjection;
                leaf.hasPhysicalLinearVelocity = true;
                leaf.physicalLinearVelocity = physical.linearVelocity;
                leaf.angularVelocity = physical.IsPlanar()
                                           ? physical.angularVelocity
                                           : TransportAngularVelocity(
                                                 physical.inverseInertiaLocal, physical.orientation,
                                                 leaf.orientation, physical.angularVelocity);
            }
            const auto endpoint = tail.back().Evaluate(dt);
            kept.insert(kept.end(), tail.begin(), tail.end());
            body.rigidBody.position = endpoint.position;
            body.rigidBody.orientation = endpoint.orientation;
            body.rigidBody.angularVelocity = endpoint.angularVelocity;
        }
        body.motion.SetRecorded(std::move(kept));
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    };
    // Pair-owned chronological frontier. A pair owns its full remaining query
    // horizon until one of its own trajectories changes. Broad-phase padding and
    // unrelated arrivals cannot reset its contact-episode origin.
    {
        using Key = std::array<unsigned, 4>;
        struct PendingPair {
            Key key{};
            std::uint64_t revisionA = 0, revisionB = 0, version = 0;
            double from = 0, hit = 0, lastResolved = -1;
            bool evaluated = false, found = false, queued = false;
        };
        using Entry = std::tuple<double, Key, std::size_t, std::uint64_t>;
        using ArrivalQueue = std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>>;
        ArrivalQueue queue;
        std::vector<PendingPair> pairs;
        std::map<Key, std::size_t> lookup;
        std::vector<char> eligible(bodies.size(), 0);
        std::size_t eligibleCount = 0, queuePeak = 0;
        for (auto slot : aliveSlots) {
            const auto &body = bodies[slot];
            const bool prescribed = body.isKinematic && !body.simulationSuspended && !body.sensor &&
                                    !body.queryOnly && body.HasMotion();
            if ((movable(slot) || prescribed) && !body.sleeping && fastReach(slot)) {
                eligible[slot] = 1;
                ++eligibleCount;
            }
        }
        if (!eligibleCount)
            return;
        std::vector<std::vector<std::size_t>> incident(bodies.size());
        std::vector<char> held(bodies.size(), 0);
        std::vector<unsigned> localArrivals(bodies.size(), 0);
        std::vector<double> lastArrival(bodies.size(), -1);
        const auto active = [&](const PendingPair &pair) {
            return (eligible[pair.key[0]] && !held[pair.key[0]]) ||
                   (eligible[pair.key[1]] && !held[pair.key[1]]);
        };
        DynamicAabbTree futureIndex;
        std::vector<int> futureProxies(bodies.size(), DynamicAabbTree::kNull);
        std::size_t futureProxyPeak = 0;
        const auto publishFuture = [&](unsigned slot, double at) {
            const auto &body = bodies[slot];
            if (!body.enabled || body.simulationSuspended || body.sensor || body.queryOnly ||
                (!body.isDynamic && !body.isKinematic))
                return;
            const auto bound = RemainingImpactBound(slot, at, dt);
            auto &proxy = futureProxies[slot];
            // Reuse the authoritative world proxy whenever it certifiably
            // encloses the actual remaining ledger. Only escaped futures
            // require a delta proxy; this is not a second copy of the world.
            if (body.proxy != DynamicAabbTree::kNull && tree.FatAabb(body.proxy).Contains(bound)) {
                if (proxy != DynamicAabbTree::kNull) {
                    futureIndex.DestroyProxy(proxy);
                    proxy = DynamicAabbTree::kNull;
                }
            } else if (proxy == DynamicAabbTree::kNull)
                proxy = futureIndex.CreateProxy(bound, slot);
            else
                futureIndex.MoveProxy(proxy, bound);
            futureProxyPeak = std::max(futureProxyPeak, std::size_t(futureIndex.ProxyCount()));
        };
        for (auto slot : aliveSlots)
            publishFuture(slot, 0);
        const auto certifySlide = [&](unsigned slot, double at) {
            auto &body = bodies[slot];

            // A centered sphere may keep tangential travel at a static plane
            // even when normal arrival work is exhausted. Certify that path
            // with the ordinary CCD query. Moving/concave candidates retain
            // the conservative full hold; this is not a replacement solver.
            double stop = dt;
            const auto bound = RemainingImpactBound(slot, at, dt);
            const auto found = ImpactCandidates(slot, bound);
            std::vector<unsigned> nearby(found.begin(), found.end());
            futureIndex.Query(bound,
                              [&](int proxy) { nearby.push_back(futureIndex.UserData(proxy)); });
            std::sort(nearby.begin(), nearby.end());
            nearby.erase(std::unique(nearby.begin(), nearby.end()), nearby.end());
            for (auto other : nearby) {
                if (other == slot || !CanRespond(slot, other) || bodies[other].sensor ||
                    bodies[other].queryOnly)
                    continue;
                if (bodies[other].HasMotion() || ConcaveShape(bodies[other])) {
                    stop = at;
                    break;
                }
                const auto a = std::min(slot, other), b = std::max(slot, other);
                for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                    for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb)
                        stop = std::min(stop, ImpactTime(a, b, pa, pb, at, stop));
            }
            if (stop < dt) {
                auto path = body.motion.Segments();
                auto &slide = path.back();
                slide.end = stop;
                slide.endPosition = slide.EvaluateGeometry(stop).position;
                auto rest = slide;
                rest.begin = stop;
                rest.end = dt;
                rest.position = rest.endPosition = slide.endPosition;
                rest.linearVelocity = glm::vec3(0);
                path.push_back(rest);
                body.rigidBody.position = rest.endPosition;
                body.motion.SetRecorded(std::move(path));
                InvalidateImpactPose(slot);
                if (slot < impactPoseSamples.size())
                    impactPoseSamples[slot].tightValid = false;
            }
        };
        const auto evaluate = [&](std::size_t index, double from) {
            auto &pair = pairs[index];
            const auto [a, b, partA, partB] = pair.key;
            if (pair.evaluated && pair.revisionA == bodies[a].impactTrajectoryRevision &&
                pair.revisionB == bodies[b].impactTrajectoryRevision)
                return;
            ++pair.version;
            pair.evaluated = true;
            pair.found = false;
            pair.from = from;
            pair.revisionA = bodies[a].impactTrajectoryRevision;
            pair.revisionB = bodies[b].impactTrajectoryRevision;
            if (!active(pair) || !CanRespond(a, b))
                return;
            PhysicsWorkTimer sweepTimer(work.ccdSweep, work.active);
            if (work.active) {
                ++work.ccdSweeps;
                ++work.ccdPrimitivePairs;
                work.ccdActiveMover = MakeHandle(eligible[a] ? a : b).id;
                work.ccdInitialSweep = from == 0;
            }
            impactQueryAnchorTime = from;
            const bool touching = PairTouches(a, b, int(partA), int(partB), from);
            double hit = double(dt) + 1;
            if (touching) {
                const auto contact = PairAt(a, b, int(partA), int(partB), from, 0);
                const auto aa = PoseAt(a, from), bb = PoseAt(b, from);
                for (int k = 0; k < contact.count; ++k) {
                    const auto &c = contact.points[k];
                    const auto arm = [&](const RigidBody &body, bool first) {
                        if (!c.hasLocalAnchors)
                            return glm::dvec3(c.point) - glm::dvec3(body.position);
                        const auto local = first ? c.localAnchorA : c.localAnchorB;
                        return (first ? c.anchorAInWorldFrame : c.anchorBInWorldFrame)
                                   ? local
                                   : ContactRotation(body.orientation) * local;
                    };
                    const auto va = glm::dvec3(aa.linearVelocity) +
                                    glm::cross(glm::dvec3(aa.angularVelocity), arm(aa, true));
                    const auto vb = glm::dvec3(bb.linearVelocity) +
                                    glm::cross(glm::dvec3(bb.angularVelocity), arm(bb, false));
                    if (glm::dot(va - vb, glm::dvec3(c.normal)) < -kMk2ImpactApproach)
                        hit = from;
                }
            }
            if (hit != from)
                hit = ImpactTime(a, b, int(partA), int(partB), from, dt);
            if (!(hit < double(dt)))
                return;
            const auto row = std::lower_bound(mk2RowPairs.begin(), mk2RowPairs.end(), pair.key);
            const bool solved = row != mk2RowPairs.end() && *row == pair.key &&
                                rowTrajectoryRevisions[a] == pair.revisionA &&
                                rowTrajectoryRevisions[b] == pair.revisionB;
            if (!touching && solved) {
                const double depth = .25 * std::min(PrimitiveFeature(a, int(partA)),
                                                    PrimitiveFeature(b, int(partB)));
                bool passed = -mk2RowEndSeparation[std::size_t(row - mk2RowPairs.begin())] > depth;
                if (eligible[a])
                    passed = passed || Mk2Crossed(a, b, a, b, int(partA), int(partB), hit, dt);
                if (eligible[b])
                    passed = passed || Mk2Crossed(b, a, a, b, int(partA), int(partB), hit, dt);
                if (!passed) {
                    const auto end = PairAt(a, b, int(partA), int(partB), dt, 0);
                    passed = end.uncertain;
                    for (int k = 0; k < end.count; ++k) {
                        const auto &c = end.points[k];
                        passed = passed || -(c.hasLocalAnchors ? c.signedSeparation
                                                               : -double(c.penetration)) > depth;
                    }
                }
                passed = passed || ImpactTime(a, b, int(partA), int(partB), hit, dt) < dt;
                if (!passed)
                    return;
            }
            pair.hit = hit;
            pair.found = true;
            queue.emplace(hit, pair.key, index, pair.version);
            queuePeak = std::max(queuePeak, queue.size());
        };
        std::vector<std::size_t> pending;
        const auto request = [&](std::size_t index) {
            auto &p = pairs[index];
            if (p.queued ||
                (p.evaluated && p.revisionA == bodies[p.key[0]].impactTrajectoryRevision &&
                 p.revisionB == bodies[p.key[1]].impactTrajectoryRevision))
                return;
            p.queued = true;
            pending.push_back(index);
        };
        const auto flush = [&](double from) {
            for (auto index : pending) {
                pairs[index].queued = false;
                evaluate(index, from);
            }
            pending.clear();
        };
        const auto addBodyPair = [&](unsigned slot, unsigned other, double at) {
            if (slot == other || !CanRespond(slot, other) || bodies[slot].sensor ||
                bodies[other].sensor || bodies[slot].queryOnly || bodies[other].queryOnly ||
                (!movable(slot) && !movable(other)))
                return;
            const auto a = std::min(slot, other), b = std::max(slot, other);
            if ((!eligible[a] || held[a]) && (!eligible[b] || held[b]))
                return;
            if (!RemainingImpactBound(a, at, dt).Overlaps(RemainingImpactBound(b, at, dt)))
                return;
            if (work.active)
                ++work.ccdCandidatePairs;
            for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                    const Key key{a, b, unsigned(pa), unsigned(pb)};
                    auto found = lookup.find(key);
                    std::size_t index;
                    if (found == lookup.end()) {
                        index = pairs.size();
                        lookup.emplace(key, index);
                        PendingPair pair;
                        pair.key = key;
                        pairs.push_back(pair);
                        incident[a].push_back(index);
                        incident[b].push_back(index);
                    } else
                        index = found->second;
                    request(index);
                }
        };
        const auto discover = [&](unsigned slot, double at) {
            const auto bound = RemainingImpactBound(slot, at, dt);
            const auto &source = ImpactCandidates(slot, bound);
            std::vector<unsigned> nearby(source.begin(), source.end());
            // The world tree owns static geometry; this interval's motion index
            // owns redirected dynamic futures, updated before discovering pairs.
            futureIndex.Query(bound,
                              [&](int proxy) { nearby.push_back(futureIndex.UserData(proxy)); });
            std::sort(nearby.begin(), nearby.end());
            nearby.erase(std::unique(nearby.begin(), nearby.end()), nearby.end());
            for (auto other : nearby)
                addBodyPair(slot, other, at);
        };
        for (auto slot : aliveSlots)
            if (eligible[slot])
                discover(slot, 0);
        flush(0);
        const auto nextPair = [&]() -> std::optional<std::size_t> {
            while (!queue.empty()) {
                const auto entry = queue.top();
                queue.pop();
                const auto index = std::get<2>(entry);
                const auto &pair = pairs[index];
                if (std::get<3>(entry) == pair.version && pair.found && active(pair))
                    return index;
            }
            return std::nullopt;
        };
        const auto resolve = [&](std::size_t index, bool terminal) {
            const auto key = pairs[index].key;
            const auto a = key[0], b = key[1];
            const auto at = pairs[index].hit;
            pairs[index].found = false;
            ++stats.impactEvents;
            if (work.active) {
                ++work.ccdEvents;
                work.ccdRepeatedEventTimes += at == previousEventTime;
                work.ccdPeakMovers = std::max(work.ccdPeakMovers, eligibleCount);
            }
            previousEventTime = at;
            std::vector<unsigned> changed;
            // Preserve sequential elastic impulse propagation. A newly arriving
            // pair uses the existing binary impact law; only a pair that closes
            // again at this same instant requires a coupled contact closure.
            bool resolved = false;
            if (pairs[index].lastResolved == at)
                resolved = Mk2ResolveArrivalBatch(a, b, int(key[2]), int(key[3]), at, changed,
                                                  futureIndex);
            if (changed.empty()) {
                changed = {a, b};
                resolved = Mk2ResolveArrival(a, b, int(key[2]), int(key[3]), at);
            }
            pairs[index].lastResolved = at;
            std::vector<unsigned> slideCandidates;
            for (auto slot : changed) {
                if (!movable(slot))
                    continue;
                if (bodies[slot].sleeping)
                    Wake(slot);
                if (!eligible[slot]) {
                    eligible[slot] = 1;
                    ++eligibleCount;
                }
                auto &arrivals = arrivalCounts ? (*arrivalCounts)[slot] : localArrivals[slot];
                const bool newInstant = lastArrival[slot] != at;
                const bool stop = held[slot] || terminal || !resolved ||
                                  (newInstant && arrivals >= kMk2ArrivalsPerBody);
                if (stop && !held[slot]) {
                    ++stats.impactSafetyFallback;
                    if (!resolved)
                        ++stats.impactResponseHolds;
                    else {
                        ++stats.impactEventCapFallback;
                        if (terminal)
                            ++stats.impactStepCapHolds;
                        else
                            ++stats.impactBodyCapHolds;
                    }
                } else if (!stop && newInstant) {
                    ++arrivals;
                }
                lastArrival[slot] = at;
                glm::vec3 safeSlide(0);
                const auto boundary = slot == a ? b : a;
                if (stop && !terminal && resolved && (slot == a || slot == b) &&
                    bodies[slot].shape.type == ShapeType::Sphere &&
                    bodies[slot].shape.pivotOffset == glm::vec3(0) &&
                    !bodies[boundary].HasMotion() &&
                    bodies[boundary].shape.type == ShapeType::Box) {
                    const auto contact = PairAt(a, b, int(key[2]), int(key[3]), at, 1e-3f);
                    if (contact.count && !contact.uncertain) {
                        const auto n = glm::normalize(contact.points[0].normal);
                        bool planar = true;
                        for (int k = 1; k < contact.count; ++k)
                            planar = planar && glm::dot(n, contact.points[k].normal) > .99999f;
                        if (planar) {
                            const auto velocity = bodies[slot].rigidBody.linearVelocity;
                            safeSlide = velocity - n * glm::dot(n, velocity);
                        }
                    }
                }
                retime(slot, at, stop, resolved, safeSlide);
                held[slot] = stop;
                publishFuture(slot, at);
                if (stop && safeSlide != glm::vec3(0))
                    slideCandidates.push_back(slot);
            }
            // All participants now own their new ledgers and delta proxies.
            // A fallback slide must not certify itself against stale futures.
            for (auto slot : slideCandidates) {
                certifySlide(slot, at);
                publishFuture(slot, at);
            }
            if (!resolved || terminal)
                for (auto slot : {a, b})
                    if (bodies[slot].isKinematic)
                        held[slot] = 1;
            std::vector<std::size_t> affected;
            for (auto slot : changed)
                if (movable(slot))
                    affected.insert(affected.end(), incident[slot].begin(), incident[slot].end());
            std::sort(affected.begin(), affected.end());
            affected.erase(std::unique(affected.begin(), affected.end()), affected.end());
            if (work.active) {
                work.ccdResweepVisits += affected.size();
                work.ccdAffectedSweeps += affected.size();
            }
            for (auto pair : affected)
                request(pair);
            for (auto slot : changed)
                if (movable(slot))
                    discover(slot, at);
            flush(at);
            // Retire stale heap observations without changing event order. Storage
            // stays proportional to the current pair set, not the event history.
            if (queue.size() > 4 * pairs.size() + 64) {
                ArrivalQueue fresh;
                for (std::size_t n = 0; n < pairs.size(); ++n) {
                    const auto &p = pairs[n];
                    if (p.found && active(p))
                        fresh.emplace(p.hit, p.key, n, p.version);
                }
                queue.swap(fresh);
            }
        };
        unsigned events = eventCount ? *eventCount : 0;
        while (events < kMk2ArrivalsPerStep) {
            const auto next = nextPair();
            if (!next)
                break;
            ++events;
            if (eventCount)
                ++*eventCount;
            resolve(*next, false);
        }
        for (std::size_t retired = 0; retired < aliveSlots.size(); ++retired) {
            const auto next = nextPair();
            if (!next)
                break;
            resolve(*next, true);
        }
        JUDAS_PROFILE_COUNTER("Physics CCD frontier outlier proxies", double(futureProxyPeak),
                              ProfileCounterMode::Maximum);
        JUDAS_PROFILE_COUNTER("Physics CCD frontier pair records", double(pairs.size()),
                              ProfileCounterMode::Maximum);
        JUDAS_PROFILE_COUNTER("Physics CCD frontier queue peak", double(queuePeak),
                              ProfileCounterMode::Maximum);
        JUDAS_PROFILE_COUNTER(
            "Physics CCD frontier record and queue payload bytes",
            double(pairs.capacity() * sizeof(PendingPair) + queuePeak * sizeof(Entry)),
            ProfileCounterMode::Maximum);
    }
}
