#include "WorldIncrementalImpact.h"
#include "ResponseAudit.h"
#include "WorldInternal.h"
using namespace JudasPhysicsDetail;

bool PhysicsWorld::Impl::TryIncrementalImpact(const std::array<unsigned, 4>& selected, double now,
                                              float dt,
                                              const std::vector<unsigned char>& jointBodies,
                                              IncrementalImpact& trial) {
    response_audit::Count(response_audit::stats.domain_attempts);
    response_audit::Timer auditDomainTimer(response_audit::stats.ns_domain);
    bool auditAccepted = false;
    response_audit::DomainOutcome auditOutcome(auditAccepted);
    response_audit::Count(response_audit::stats.domain_reset_slots, bodies.size());
    response_audit::Count(response_audit::stats.domain_reset_flag_bytes, 3 * bodies.size());
    {
        response_audit::CapacityProbe auditBodies(trial.bodies, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
        response_audit::CapacityProbe auditIncoming(trial.incoming, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
        response_audit::CapacityProbe auditOriginal(trial.original, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
        response_audit::CapacityProbe auditInitialized(trial.initialized, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
        response_audit::CapacityProbe auditExpanded(trial.expanded, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
        response_audit::CapacityProbe auditGravity(trial.gravityConsumed, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
        trial.Reset(bodies.size());
    }
    const auto initialize = [&](unsigned slot) {
        response_audit::Count(response_audit::stats.initialize_requests);
        if (trial.initialized[slot]) {
            response_audit::Count(response_audit::stats.initialized_reuses);
            return true;
        }
        const auto& body = bodies[slot];
        if (!ContinuationPrimitive(body) || (jointBodies[slot] && !body.rigidBody.IsStatic()) ||
            (body.isKinematic && !body.rigidBody.IsStatic())) {
            return false;
        }
        response_audit::Count(response_audit::stats.initialized_bodies);
        response_audit::Count(body.rigidBody.IsStatic() ? response_audit::stats.initialized_static : response_audit::stats.initialized_dynamic);
        trial.initialized[slot] = 1;
        { response_audit::CapacityProbe auditSlots(trial.slots, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
          trial.slots.push_back(slot); }
        trial.bodies[slot] = trial.incoming[slot] = PoseAt(slot, now);
        trial.original[slot] = {body.rigidBody.linearVelocity,
                                trial.incoming[slot].angularVelocity};
        if (body.isDynamic && body.deferredAcceleration) {
            trial.incoming[slot].linearVelocity += body.lastAcceleration * float(dt - now);
            trial.bodies[slot] = trial.incoming[slot];
            trial.gravityConsumed[slot] = 1;
            response_audit::Count(response_audit::stats.deferred_gravity_bodies);
            response_audit::CapacityProbe auditForced(trial.forcedExpansion, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
            trial.forcedExpansion.push_back(slot);
        }
        return true;
    };
    const auto pair = [&](unsigned first, unsigned second) {
        response_audit::Count(response_audit::stats.pair_attempts);
        const auto key = std::minmax(first, second);
        if (!trial.visited.emplace(key.first, key.second).second) {
            response_audit::Count(response_audit::stats.pair_duplicates);
            return true;
        }
        response_audit::Count(response_audit::stats.pair_unique);
        response_audit::Count(response_audit::stats.visited_node_alloc_lower_count);
        response_audit::Count(response_audit::stats.visited_node_payload_lower_bytes, sizeof(std::pair<unsigned, unsigned>));
        auto& a = bodies[key.first];
        auto& b = bodies[key.second];
        for (int pa = 0; pa < PrimitiveCount(a.shape); ++pa)
            for (int pb = 0; pb < PrimitiveCount(b.shape); ++pb) {
                response_audit::Count(response_audit::stats.manifold_queries);
                const auto manifold = PairAt(key.first, key.second, pa, pb, now, 0);
                response_audit::Count(response_audit::stats.manifold_points, manifold.count);
                if (manifold.uncertain) {
                    response_audit::Count(response_audit::stats.manifold_uncertain);
                    return false;
                }
                if (!manifold.count)
                    continue;
                if (!initialize(key.first) || !initialize(key.second))
                    return false;
                response_audit::Count(response_audit::stats.manifold_nonempty);
                response_audit::Count(response_audit::stats.appended_domain_pairs);
                { response_audit::CapacityProbe auditPairs(trial.pairs, response_audit::stats.pair_vector_growth_count, response_audit::stats.pair_vector_growth_bytes);
                  trial.pairs.push_back({key.first, key.second, unsigned(pa), unsigned(pb)}); }
                for (int point = 0; point < manifold.count; ++point) {
                    response_audit::Count(response_audit::stats.appended_domain_rows);
                    response_audit::CapacityProbe auditRows(trial.rows, response_audit::stats.row_vector_growth_count, response_audit::stats.row_vector_growth_bytes);
                    response_audit::CapacityProbe auditKeys(trial.keys, response_audit::stats.key_vector_growth_count, response_audit::stats.key_vector_growth_bytes);
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
                    response_audit::Count(trial.rows.back().continuing ? response_audit::stats.continuing_rows : response_audit::stats.new_rows);
                    trial.rows.back().maxDefectDisplacement = std::min(
                        ContactSolver::kPenetrationSlop, float(SpinPenetrationTarget(identity)));
                }
            }
        return true;
    };
    const auto expand = [&](unsigned slot) {
        response_audit::Count(response_audit::stats.expand_requests);
        if (!initialize(slot))
            return false;
        if (trial.expanded[slot]) {
            response_audit::Count(response_audit::stats.expanded_reuses);
            return true;
        }
        response_audit::Count(response_audit::stats.expanded_bodies);
        trial.expanded[slot] = 1;
        const auto& body = bodies[slot];
        if (body.rigidBody.IsStatic()) {
            response_audit::Count(response_audit::stats.expanded_static);
            return true;
        }
        response_audit::Count(response_audit::stats.expanded_dynamic);
        PhysicsWorkTimer timer(work.impactIslands, work.active);
        response_audit::Count(response_audit::stats.query_slots_calls);
        const auto& candidates = QuerySlots(ActualImpactBound(slot, ImpactPoseAt(slot, now)));
        response_audit::Count(response_audit::stats.query_candidates, candidates.size());
        if (response_audit::enabled) response_audit::stats.query_candidates_max = std::max(response_audit::stats.query_candidates_max, std::uint64_t(candidates.size()));
        for (auto other : candidates) {
            if (other == slot || !CanCollide(slot, other))
                continue;
            const auto& b = bodies[other];
            if (b.sensor || b.queryOnly || body.sensor || body.queryOnly ||
                (body.rigidBody.IsStatic() && b.rigidBody.IsStatic()))
                continue;
            // This participant is provisionally awake in the transaction.
            // World sleep flags must not hide its still-sleeping neighbours.
            if (!pair(slot, other))
                return false;
        }
        return true;
    };
    {
    response_audit::Timer auditSeedTimer(response_audit::stats.ns_domain_seed);
    if (!pair(selected[0], selected[1]) || trial.rows.empty() || !expand(selected[0]) ||
        !expand(selected[1]))
        return false;
    for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i) {
        response_audit::Count(response_audit::stats.forced_queue_scans);
        if (trial.expanded[trial.forcedExpansion[i]]) response_audit::Count(response_audit::stats.forced_queue_already_expanded);
        if (!expand(trial.forcedExpansion[i]))
            return false;
    }
    }
    const auto hasSupport = [&](const auto& p) {
        response_audit::Count(response_audit::stats.support_capture_visits);
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
        response_audit::Timer auditGrowTimer(response_audit::stats.ns_domain_growth);
        response_audit::Count(response_audit::stats.callback_rounds);
        response_audit::Count(response_audit::stats.callback_activated_inputs, activated.size());
        std::vector<unsigned> next;
        for (auto* body : activated) {
            const auto slot = unsigned(body - trial.bodies.data());
            if (!trial.expanded[slot]) {
                response_audit::CapacityProbe auditNext(next, response_audit::stats.next_vector_growth_count, response_audit::stats.next_vector_growth_bytes);
                next.push_back(slot);
            } else response_audit::Count(response_audit::stats.callback_already_expanded);
        }
        response_audit::Count(response_audit::stats.callback_next_before_unique, next.size());
        const auto auditBeforeUnique = next.size();
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        response_audit::Count(response_audit::stats.callback_next_unique, next.size());
        response_audit::Count(response_audit::stats.callback_next_duplicates, auditBeforeUnique - next.size());
        for (auto slot : next)
            if (!expand(slot))
                return false;
        for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i) {
            response_audit::Count(response_audit::stats.forced_queue_scans);
            if (trial.expanded[trial.forcedExpansion[i]]) response_audit::Count(response_audit::stats.forced_queue_already_expanded);
            if (!trial.expanded[trial.forcedExpansion[i]] && !expand(trial.forcedExpansion[i]))
                return false;
        }
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
    auditAccepted = accepted;
    return accepted;
}
