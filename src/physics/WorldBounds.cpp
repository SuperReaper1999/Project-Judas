#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::Orientation(Body &body) -> const ContactPreparedOrientation & {
    if (body.orientationCache && body.orientationCache->Matches(body.rigidBody.orientation)) {
        ++cacheCounters.orientationHits;
    } else {
        body.orientationCache.emplace(body.rigidBody.orientation);
        body.boundPreparationValid = false;
        ++cacheCounters.orientationRebuilds;
    }
    return *body.orientationCache;
}

auto PhysicsWorld::Impl::CurrentBound(Body &body) -> const Aabb & {
    const auto &p = body.rigidBody.position;
    const auto &q = body.rigidBody.orientation;
    if (Matches(body.currentBound, p, q)) {
        ++cacheCounters.boundHits;
        return body.currentBound.bound;
    }
    const auto &orientation = Orientation(body);
    if (!body.boundPreparationValid) {
        if (!body.boundPreparation)
            body.boundPreparation.emplace();
        const auto capacity = body.boundPreparation->children.capacity();
        PrepareShapeBounds(body.shape, orientation, *body.boundPreparation);
        if (body.boundPreparation->children.capacity() != capacity) {
            ++cacheCounters.allocations;
            cacheCounters.allocatedBytes +=
                body.boundPreparation->children.capacity() * sizeof(PreparedBoxBound);
        }
        body.boundPreparationValid = true;
    }
    body.currentBound = {p, q, ShapeAabb(*body.boundPreparation, p), true};
    ++cacheCounters.boundRebuilds;
    return body.currentBound.bound;
}

auto PhysicsWorld::Impl::PreviousBound(Body &body) -> const Aabb & {
    const auto &p = body.previousPosition;
    const auto &q = body.previousOrientation;
    if (Matches(body.previousBound, p, q)) {
        ++cacheCounters.boundHits;
    } else if (Matches(body.currentBound, p, q)) {
        body.previousBound = body.currentBound;
        ++cacheCounters.boundHits;
    } else {
        // Normally captured before integration. This independent fallback
        // preserves the old pose if a caller introduces another transition.
        body.previousBound = {p, q, ShapeAabb(body.shape, p, q), true};
        ++cacheCounters.boundRebuilds;
    }
    return body.previousBound.bound;
}

auto PhysicsWorld::Impl::CacheBytes() const -> std::size_t {
    std::size_t bytes =
        bodies.capacity() *
        (sizeof(float) + sizeof(std::optional<ContactPreparedOrientation>) +
         sizeof(std::optional<PreparedShapeBounds>) + sizeof(bool) + 2 * sizeof(PoseBound));
    for (const Body &body : bodies)
        if (body.boundPreparation)
            bytes += body.boundPreparation->children.capacity() * sizeof(PreparedBoxBound);
    bytes += initialContactGeometry.capacity() * sizeof(InitialContactGeometry);
    bytes += CCDQueryBytes() + overlapPairs.StorageBytes();
    return bytes +
           (impactPoseSamples.capacity() + impactAnchorSamples.capacity()) *
               sizeof(ImpactPoseSample) +
           solver.FrameStorageBytes();
}

auto PhysicsWorld::Impl::CCDQueryBytes() const -> std::size_t {
    std::size_t bytes = ccdQueryStorage.capacity() * sizeof(std::unique_ptr<CCDQueryStorage>);
    for (const auto& scratch : ccdQueryStorage)
        if (scratch)
            bytes += sizeof(CCDQueryStorage) +
                     (scratch->impactPoseSamples.capacity() + scratch->impactAnchorSamples.capacity()) *
                         sizeof(ImpactPoseSample);
    return bytes;
}

auto PhysicsWorld::Impl::TightBound(Body &body) -> Aabb {
    const Aabb current = CurrentBound(body);
    if (!body.HasMotion()) {
        body.previousBound = body.currentBound;
        return current;
    }
    Aabb bound = current.Union(PreviousBound(body));
    const float turn = std::abs(glm::dot(body.previousOrientation, body.rigidBody.orientation));
    if (turn < 1.0f - 1.0e-7f) {
        const float r = body.boundingRadius;
        bound = bound.Union(
            Aabb{body.previousPosition - glm::vec3(r), body.previousPosition + glm::vec3(r)});
        bound = bound.Union(
            Aabb{body.rigidBody.position - glm::vec3(r), body.rigidBody.position + glm::vec3(r)});
    }
    if (body.motion.Segments().size() > 1)
        for (const auto &segment : body.motion.Segments()) {
            const float r = body.boundingRadius;
            bound =
                bound.Union(Aabb{glm::min(segment.position, segment.endPosition) - glm::vec3(r),
                                 glm::max(segment.position, segment.endPosition) + glm::vec3(r)});
        }
    return bound;
}

auto PhysicsWorld::Impl::StepReach(const Body &body, float fixedDeltaTime) const -> float {
    return (glm::length(body.rigidBody.linearVelocity) +
            glm::length(body.rigidBody.angularVelocity) * body.boundingRadius) *
           fixedDeltaTime;
}

auto PhysicsWorld::Impl::CoverStepReach(Body &body, float fixedDeltaTime) -> void {
    const Aabb reach = TightBound(body).Expanded(StepReach(body, fixedDeltaTime));
    if (tree.FatAabb(body.proxy).Contains(reach))
        return;
    tree.MoveProxy(body.proxy, reach.Expanded(kBroadphaseMargin));
    overlapPairs.Moved(unsigned(&body - bodies.data()));
    ++reinsertionsSinceStep;
}

auto PhysicsWorld::Impl::RefreshProxy(Body &body) -> void {
    const Aabb tight = TightBound(body);
    if (tree.FatAabb(body.proxy).Contains(tight))
        return;
    tree.MoveProxy(body.proxy, tight.Expanded(kBroadphaseMargin));
    overlapPairs.Moved(unsigned(&body - bodies.data()));
    ++reinsertionsSinceStep;
}

auto PhysicsWorld::Impl::QuerySlots(const Aabb &box) const -> const std::vector<unsigned int> & {
    queryScratch.clear();
    tree.Query(box, [&](int proxy) { queryScratch.push_back(tree.UserData(proxy)); });
    std::sort(queryScratch.begin(), queryScratch.end());
    return queryScratch;
}

auto PhysicsWorld::Impl::GenerateCandidatePairs() -> void {
    JUDAS_PROFILE_SCOPE("Physics candidate generation");
    if (work.active)
        ++work.candidatePasses;
    candidatePairs.clear();
    candidateProxies.clear();
    for (const unsigned slot : aliveSlots) {
        const auto& body = bodies[slot];
        if (body.enabled && (body.HasMotion() || body.sensor || body.queryOnly))
            candidateProxies.push_back(body.proxy);
    }
    for (auto slot : aliveSlots) {
        const auto& body = bodies[slot];
        overlapPairs.SetBody(slot, body.proxy, body.generation,
                             body.enabled && (body.HasMotion() || body.sensor || body.queryOnly));
    }
    const auto emit = [&](unsigned a, unsigned b) {
        if (work.active)
            work.candidateLeaves += 2;
        if (!CanCollide(a, b)) {
            ++stats.layerRejectedPairs;
            return;
        }
        candidatePairs.emplace_back(std::min(a,b), std::max(a,b));
    };
    if (overlapPairs.Synchronize(tree)) {
        overlapPairs.Pairs(emit);
    } else {
        tree.QueryPairs(candidateProxies, candidateScratch, [&](int first, int second) {
            emit(tree.UserData(first), tree.UserData(second));
        });
        std::sort(candidatePairs.begin(),candidatePairs.end());
    }
    JUDAS_PROFILE_COUNTER("Physics overlap index bytes", overlapPairs.StorageBytes(), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics overlap index pairs", overlapPairs.EdgeCount(), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics overlap index rebuilds", overlapPairs.rebuilds, ProfileCounterMode::Latest);
}

const std::vector<unsigned>& PhysicsWorld::Impl::ImpactCandidates(unsigned slot, const Aabb& query) {
    if (!tree.FatAabb(bodies[slot].proxy).Contains(query) || !overlapPairs.Synchronize(tree))
        return QuerySlots(query);
    const auto* neighbours = overlapPairs.Neighbours(slot);
    if (!neighbours)
        return QuerySlots(query);
    queryScratch.clear();
    for (auto other : *neighbours)
        if (tree.FatAabb(bodies[other].proxy).Overlaps(query))
            queryScratch.push_back(other);
    return queryScratch;
}
