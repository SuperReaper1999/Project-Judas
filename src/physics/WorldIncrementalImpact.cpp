#include "WorldIncrementalImpact.h"
#include "WorldInternal.h"
using namespace JudasPhysicsDetail;

void PhysicsWorld::Impl::IncrementalImpactDeleter::operator()(
    IncrementalImpact* workspace) const noexcept {
    delete workspace;
}

bool PhysicsWorld::Impl::ImpactDiscoverySeparated(unsigned a, unsigned b, const Aabb& query,
                                                  double start, double finish) {
    const auto supported = [](const Body& body) {
        return ContinuationPrimitive(body) && std::isfinite(body.boundingRadius) &&
               body.boundingRadius > 0;
    };
    if (!supported(bodies[a]) || !supported(bodies[b]) ||
        !rigid_motion_bounds::FiniteBound(query))
        return false;
    // An existing remaining-trajectory enclosure contains the other's geometry
    // at this instant too. Reject only disjoint finite enclosures, before
    // preparing that other's event-time orientation and tight shape bounds.
    // This is not contact truth or a retained neighbor set. Published velocity
    // changes invalidate RemainingImpactBound through InvalidateImpactPose;
    // an earlier unchanged interval's enclosure remains a safe superset.
    const auto& remaining = RemainingImpactBound(b, start, finish);
    return rigid_motion_bounds::FiniteBound(remaining) && !query.Overlaps(remaining);
}

bool PhysicsWorld::Impl::TryIncrementalImpact(const std::array<unsigned, 4>& selected, double now,
                                              float dt,
                                              const std::vector<unsigned char>& jointBodies,
                                              IncrementalImpact& trial) {
    trial.Reset(bodies.size());
    // Growing shocks retain the same normal-patch response as ordinary
    // support. Point-only sweeps left quiet piles rocking and prevented sleep.
    // The private trial still validates displacement and energy before publish.
    trial.solver.EnableBlockNormals(solver.BlockNormalsEnabled());
    const auto initialize = [&](unsigned slot) {
        if (trial.initialized[slot])
            return true;
        const auto& body = bodies[slot];
        if (!ContinuationPrimitive(body) || (jointBodies[slot] && !body.rigidBody.IsStatic()) ||
            (body.isKinematic && !body.rigidBody.IsStatic())) {
            return false;
        }
        trial.initialized[slot] = 1;
        trial.slots.push_back(slot);
        trial.bodies[slot] = trial.incoming[slot] = PoseAt(slot, now);
        trial.original[slot] = {body.rigidBody.linearVelocity,
                                trial.incoming[slot].angularVelocity};
        if (body.isDynamic && body.deferredAcceleration) {
            trial.incoming[slot].linearVelocity += body.lastAcceleration * float(dt - now);
            trial.bodies[slot] = trial.incoming[slot];
            trial.gravityConsumed[slot] = 1;
            trial.forcedExpansion.push_back(slot);
        }
        return true;
    };
    const auto pair = [&](unsigned first, unsigned second) {
        const auto key = std::minmax(first, second);
        if (!trial.Visit(key.first, key.second))
            return true;
        auto& a = bodies[key.first];
        auto& b = bodies[key.second];
        for (int pa = 0; pa < PrimitiveCount(a.shape); ++pa)
            for (int pb = 0; pb < PrimitiveCount(b.shape); ++pb) {
                const auto manifold = PairAt(key.first, key.second, pa, pb, now, 0);
                if (manifold.uncertain) {
                    return false;
                }
                if (!manifold.count)
                    continue;
                if (!initialize(key.first) || !initialize(key.second))
                    return false;
                trial.pairs.push_back({key.first, key.second, unsigned(pa), unsigned(pb)});
                for (int point = 0; point < manifold.count; ++point) {
                    trial.bodies[key.first].position = trial.incoming[key.first].position;
                    trial.bodies[key.second].position = trial.incoming[key.second].position;
                    trial.rows.push_back(
                        {&trial.bodies[key.first], &trial.bodies[key.second],
                         manifold.points[point],
                         std::sqrt(std::max(0.f, a.friction) * std::max(0.f, b.friction)),
                         std::max(a.restitution, b.restitution), false});
                    trial.keys.push_back(
                        {key.first, key.second, a.generation, b.generation, pa, pb});
                    const std::array<unsigned, 4> identity{key.first, key.second, unsigned(pa),
                                                           unsigned(pb)};
                    trial.rows.back().continuing =
                        identity != selected &&
                        std::binary_search(supportPairs.begin(), supportPairs.end(), identity);
                    trial.rows.back().maxDefectDisplacement = std::min(
                        ContactSolver::kPenetrationSlop, float(SpinPenetrationTarget(identity)));
                }
            }
        return true;
    };
    const auto expand = [&](unsigned slot) {
        if (!initialize(slot))
            return false;
        if (trial.expanded[slot])
            return true;
        trial.expanded[slot] = 1;
        const auto& body = bodies[slot];
        if (body.rigidBody.IsStatic())
            return true;
        PhysicsWorkTimer timer(work.impactIslands, work.active);
        // Copy: PairAt and remaining-bound preparation share per-body scratch.
        const Aabb query = ActualImpactBound(slot, ImpactPoseAt(slot, now));
        const auto& candidates = ImpactCandidates(slot, query);
        for (auto other : candidates) {
            if (other == slot || !CanCollide(slot, other))
                continue;
            const auto& b = bodies[other];
            if (b.sensor || b.queryOnly || body.sensor || body.queryOnly ||
                (body.rigidBody.IsStatic() && b.rigidBody.IsStatic()))
                continue;
            // This participant is provisionally awake in the transaction.
            // World sleep flags must not hide its still-sleeping neighbours.
            if (ImpactDiscoverySeparated(slot, other, query, now, dt)) {
                if (work.active)
                    ++work.incrementalBoundsRejected;
                continue;
            }
            if (!pair(slot, other))
                return false;
        }
        return true;
    };
    if (!pair(selected[0], selected[1]) || trial.rows.empty() || !expand(selected[0]) ||
        !expand(selected[1]))
        return false;
    for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i)
        if (!expand(trial.forcedExpansion[i]))
            return false;
    const auto hasSupport = [&](const auto& p) {
        return (trial.initialized[p[0]] && !bodies[p[0]].rigidBody.IsStatic()) ||
               (trial.initialized[p[1]] && !bodies[p[1]].rigidBody.IsStatic());
    };
    trial.capture = std::any_of(supportPairs.begin(), supportPairs.end(), hasSupport) ||
                    std::any_of(spinningPairs.begin(), spinningPairs.end(), hasSupport);
    if (trial.rows.size() == 1 && !trial.capture && trial.rows.front().restitution != 0) {
        return false;
    }
    for (auto slot : trial.slots)
        trial.bodies[slot] = trial.incoming[slot];
    const auto grow = [&](const std::vector<RigidBody*>& activated) {
        std::vector<unsigned> next;
        for (auto* body : activated) {
            const auto slot = unsigned(body - trial.bodies.data());
            if (!trial.expanded[slot])
                next.push_back(slot);
        }
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        for (auto slot : next)
            if (!expand(slot))
                return false;
        for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i)
            if (!trial.expanded[trial.forcedExpansion[i]] && !expand(trial.forcedExpansion[i]))
                return false;
        return true;
    };
    const auto geometryBefore = work.impactIslands.nanoseconds;
    bool accepted;
    {
        PhysicsWorkTimer timer(work.impactSolve, work.active);
        accepted = trial.solver.SolveGrowing(trial.rows, dt, grow, trial.result);
    }
    // Domain growth occurs inside the solver callback. Publish exclusive stage
    // costs, rather than counting the same geometry work as solve work as well.
    if (work.active)
        work.impactSolve.nanoseconds -= work.impactIslands.nanoseconds - geometryBefore;
    return accepted;
}
