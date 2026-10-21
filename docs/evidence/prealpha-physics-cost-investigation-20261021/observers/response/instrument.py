from pathlib import Path
import hashlib
root=Path('/home/conner/Documents/GitHub/Project-Judas')
out=root/'.cache/physics-exhaustive-profile-20261021/response'
fields='''domain_attempts domain_accepted domain_rejected domain_reset_slots domain_reset_flag_bytes
initialize_requests initialized_reuses initialized_bodies initialized_static initialized_dynamic deferred_gravity_bodies
expand_requests expanded_reuses expanded_bodies expanded_static expanded_dynamic
query_slots_calls query_candidates query_candidates_max
pair_attempts pair_unique pair_duplicates manifold_queries manifold_nonempty manifold_uncertain manifold_points
appended_domain_pairs appended_domain_rows continuing_rows new_rows support_capture_visits
callback_rounds callback_activated_inputs callback_already_expanded callback_next_before_unique callback_next_unique callback_next_duplicates
forced_queue_scans forced_queue_already_expanded
visited_node_alloc_lower_count visited_node_payload_lower_bytes
world_vector_growth_count world_vector_growth_bytes next_vector_growth_count next_vector_growth_bytes
row_vector_growth_count row_vector_growth_bytes key_vector_growth_count key_vector_growth_bytes
pair_vector_growth_count pair_vector_growth_bytes
solver_calls_legacy solver_calls_growing solver_legacy_input_rows solver_legacy_safety_fallbacks
solver_append_calls solver_appended_rows solver_append_frame_requests
solver_sweeps solver_ranges solver_rows_per_sweep_sum solver_sweep_fixed_points solver_sweep_after_fixed_point solver_fixed_point_row_visits
solver_contact_closure_checks solver_contact_closure_failures solver_budget_body_measurements solver_budget_rows
range_calls range_iterations range_input_activated range_seen_bytes range_seen_alloc_count range_activated_outputs
activated_vector_growth_count activated_vector_growth_bytes
solver_saved_growth_count solver_saved_growth_bytes constraint_vector_growth_count constraint_vector_growth_bytes
pending_vector_growth_count pending_vector_growth_bytes
frame_vector_growth_count frame_vector_growth_bytes frame_node_requests frame_index_insertions
frame_upstream_alloc_count frame_upstream_alloc_bytes
ordinary_rows ordinary_accumulator_changes ordinary_endpoint_changes ordinary_zero_velocity_changes ordinary_no_accumulator_changes
legacy_rows legacy_accumulator_changes legacy_endpoint_changes legacy_zero_velocity_changes legacy_no_accumulator_changes
growing_rows growing_accumulator_changes growing_endpoint_changes growing_zero_velocity_changes growing_no_accumulator_changes
row_inactive_mass prepare_calls prepare_rows prepare_frames
ordinary_prepare_rows ordinary_warm_normal_nonzero ordinary_warm_tangent_nonzero ordinary_warm_any_nonzero ordinary_warm_zero
legacy_prepare_rows legacy_warm_normal_nonzero legacy_warm_tangent_nonzero legacy_warm_any_nonzero legacy_warm_zero
growing_prepare_rows growing_warm_normal_nonzero growing_warm_tangent_nonzero growing_warm_any_nonzero growing_warm_zero
ns_domain ns_domain_seed ns_domain_growth ns_solver_growing ns_solver_append ns_solver_ranges ns_solver_budget ns_solver_legacy ns_prepare ns_ordinary_rows ns_legacy_rows'''.split()
doubles=['ordinary_warm_normal_sum','legacy_warm_normal_sum','growing_warm_normal_sum']
header='''#pragma once
// Private diagnostic observer. No solver policy or numerical state is modified.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace response_audit {
#define RESPONSE_AUDIT_COUNTERS(X) \\
'''+ (' '+chr(92)+chr(10)).join('    X('+f+')' for f in fields)+'''
#define RESPONSE_AUDIT_SUMS(X) \\
'''+ (' '+chr(92)+chr(10)).join('    X('+f+')' for f in doubles)+'''
struct Stats {
#define DECLARE_COUNTER(name) std::uint64_t name = 0;
    RESPONSE_AUDIT_COUNTERS(DECLARE_COUNTER)
#undef DECLARE_COUNTER
#define DECLARE_SUM(name) double name = 0;
    RESPONSE_AUDIT_SUMS(DECLARE_SUM)
#undef DECLARE_SUM
};
inline thread_local Stats stats{};
inline thread_local bool enabled = false;
enum class Context { Ordinary, LegacyImpact, GrowingImpact };
inline thread_local Context context = Context::Ordinary;
inline void SetEnabled(bool value) { enabled = value; }
inline void Reset() { stats = {}; }
inline void Header(std::ostream& out) {
    out << "step,enabled";
#define WRITE_NAME(name) out << "," #name;
    RESPONSE_AUDIT_COUNTERS(WRITE_NAME)
    RESPONSE_AUDIT_SUMS(WRITE_NAME)
#undef WRITE_NAME
    out << '\\n';
}
inline void WriteStep(std::ostream& out, std::uint64_t step) {
    out << step << ',' << unsigned(enabled);
#define WRITE_VALUE(name) out << ',' << stats.name;
    RESPONSE_AUDIT_COUNTERS(WRITE_VALUE)
    RESPONSE_AUDIT_SUMS(WRITE_VALUE)
#undef WRITE_VALUE
    out << '\\n';
}
inline void Count(std::uint64_t& value, std::uint64_t amount = 1) {
    if (enabled) value += amount;
}
struct ContextScope {
    Context previous;
    explicit ContextScope(Context value) : previous(context) { context = value; }
    ~ContextScope() { context = previous; }
};
struct Timer {
    std::uint64_t* target;
    std::chrono::steady_clock::time_point start;
    explicit Timer(std::uint64_t& value) : target(enabled ? &value : nullptr) {
        if (target) start = std::chrono::steady_clock::now();
    }
    ~Timer() {
        if (target) *target += std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
    }
};
// Counts capacity growth observed across a scope, not allocator calls globally.
// Payload bytes are new capacity, excluding allocator metadata and old storage.
template<class V> struct CapacityProbe {
    const V& vector;
    std::size_t before;
    std::uint64_t* count;
    std::uint64_t* bytes;
    CapacityProbe(const V& value, std::uint64_t& n, std::uint64_t& b)
        : vector(value), before(value.capacity()), count(enabled ? &n : nullptr), bytes(&b) {}
    ~CapacityProbe() {
        if (count && vector.capacity() != before) {
            ++*count;
            *bytes += vector.capacity() * sizeof(typename V::value_type);
        }
    }
};
template<class V> CapacityProbe(const V&, std::uint64_t&, std::uint64_t&) -> CapacityProbe<V>;
struct DomainOutcome {
    const bool& accepted;
    explicit DomainOutcome(const bool& value) : accepted(value) {}
    ~DomainOutcome() {
        if (enabled) ++(accepted ? stats.domain_accepted : stats.domain_rejected);
    }
};
inline void ObserveRow(bool accumulator, bool endpointA, bool endpointB) {
    if (!enabled) return;
#define ROW_CONTEXT(prefix) \\
    ++stats.prefix##_rows; \\
    stats.prefix##_accumulator_changes += accumulator; \\
    stats.prefix##_endpoint_changes += unsigned(endpointA) + unsigned(endpointB); \\
    stats.prefix##_zero_velocity_changes += accumulator && !endpointA && !endpointB; \\
    stats.prefix##_no_accumulator_changes += !accumulator
    switch (context) {
    case Context::Ordinary: { ROW_CONTEXT(ordinary); break; }
    case Context::LegacyImpact: { ROW_CONTEXT(legacy); break; }
    case Context::GrowingImpact: { ROW_CONTEXT(growing); break; }
    }
#undef ROW_CONTEXT
}
inline void ObserveWarm(float normal, bool tangent) {
    if (!enabled) return;
    const bool nonzero = normal != 0;
#define WARM_CONTEXT(prefix) \\
    ++stats.prefix##_prepare_rows; \\
    stats.prefix##_warm_normal_nonzero += nonzero; \\
    stats.prefix##_warm_tangent_nonzero += tangent; \\
    stats.prefix##_warm_any_nonzero += nonzero || tangent; \\
    stats.prefix##_warm_zero += !nonzero && !tangent; \\
    stats.prefix##_warm_normal_sum += double(normal)
    switch (context) {
    case Context::Ordinary: { WARM_CONTEXT(ordinary); break; }
    case Context::LegacyImpact: { WARM_CONTEXT(legacy); break; }
    case Context::GrowingImpact: { WARM_CONTEXT(growing); break; }
    }
#undef WARM_CONTEXT
}
} // namespace response_audit
'''
(out/'ResponseAudit.h').write_text(header)
def source(name):
    path=root/name
    data=path.read_text()
    (out/(path.stem+'.original.cpp')).write_text(data)
    return data

def replace(s,old,new):
    n=s.count(old)
    if n!=1: raise RuntimeError(f'replacement count {n}: {old[:100]}')
    return s.replace(old,new)

s=source('src/physics/WorldIncrementalImpact.cpp')
s=replace(s,'#include "WorldIncrementalImpact.h"','#include "WorldIncrementalImpact.h"\n#include "ResponseAudit.h"')
s=replace(s,'    trial.Reset(bodies.size());','''    response_audit::Count(response_audit::stats.domain_attempts);
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
    }''')
s=replace(s,'    const auto initialize = [&](unsigned slot) {\n        if (trial.initialized[slot])\n            return true;','''    const auto initialize = [&](unsigned slot) {
        response_audit::Count(response_audit::stats.initialize_requests);
        if (trial.initialized[slot]) {
            response_audit::Count(response_audit::stats.initialized_reuses);
            return true;
        }''')
s=replace(s,'        trial.initialized[slot] = 1;\n        trial.slots.push_back(slot);','''        response_audit::Count(response_audit::stats.initialized_bodies);
        response_audit::Count(body.rigidBody.IsStatic() ? response_audit::stats.initialized_static : response_audit::stats.initialized_dynamic);
        trial.initialized[slot] = 1;
        { response_audit::CapacityProbe auditSlots(trial.slots, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
          trial.slots.push_back(slot); }''')
s=replace(s,'            trial.forcedExpansion.push_back(slot);','''            response_audit::Count(response_audit::stats.deferred_gravity_bodies);
            response_audit::CapacityProbe auditForced(trial.forcedExpansion, response_audit::stats.world_vector_growth_count, response_audit::stats.world_vector_growth_bytes);
            trial.forcedExpansion.push_back(slot);''')
s=replace(s,'        const auto key = std::minmax(first, second);\n        if (!trial.visited.emplace(key.first, key.second).second)\n            return true;','''        response_audit::Count(response_audit::stats.pair_attempts);
        const auto key = std::minmax(first, second);
        if (!trial.visited.emplace(key.first, key.second).second) {
            response_audit::Count(response_audit::stats.pair_duplicates);
            return true;
        }
        response_audit::Count(response_audit::stats.pair_unique);
        response_audit::Count(response_audit::stats.visited_node_alloc_lower_count);
        response_audit::Count(response_audit::stats.visited_node_payload_lower_bytes, sizeof(std::pair<unsigned, unsigned>));''')
s=replace(s,'                const auto manifold = PairAt(key.first, key.second, pa, pb, now, 0);','''                response_audit::Count(response_audit::stats.manifold_queries);
                const auto manifold = PairAt(key.first, key.second, pa, pb, now, 0);
                response_audit::Count(response_audit::stats.manifold_points, manifold.count);''')
s=replace(s,'                if (manifold.uncertain) {\n                    return false;','''                if (manifold.uncertain) {
                    response_audit::Count(response_audit::stats.manifold_uncertain);
                    return false;''')
s=replace(s,'                trial.pairs.push_back({key.first, key.second, unsigned(pa), unsigned(pb)});','''                response_audit::Count(response_audit::stats.manifold_nonempty);
                response_audit::Count(response_audit::stats.appended_domain_pairs);
                { response_audit::CapacityProbe auditPairs(trial.pairs, response_audit::stats.pair_vector_growth_count, response_audit::stats.pair_vector_growth_bytes);
                  trial.pairs.push_back({key.first, key.second, unsigned(pa), unsigned(pb)}); }''')
s=replace(s,'                for (int point = 0; point < manifold.count; ++point) {','''                for (int point = 0; point < manifold.count; ++point) {
                    response_audit::Count(response_audit::stats.appended_domain_rows);
                    response_audit::CapacityProbe auditRows(trial.rows, response_audit::stats.row_vector_growth_count, response_audit::stats.row_vector_growth_bytes);
                    response_audit::CapacityProbe auditKeys(trial.keys, response_audit::stats.key_vector_growth_count, response_audit::stats.key_vector_growth_bytes);''')
s=replace(s,'                    trial.rows.back().maxDefectDisplacement = std::min(','                    response_audit::Count(trial.rows.back().continuing ? response_audit::stats.continuing_rows : response_audit::stats.new_rows);\n                    trial.rows.back().maxDefectDisplacement = std::min(')
s=replace(s,'    const auto expand = [&](unsigned slot) {','    const auto expand = [&](unsigned slot) {\n        response_audit::Count(response_audit::stats.expand_requests);')
s=replace(s,'        if (trial.expanded[slot])\n            return true;\n        trial.expanded[slot] = 1;','''        if (trial.expanded[slot]) {
            response_audit::Count(response_audit::stats.expanded_reuses);
            return true;
        }
        response_audit::Count(response_audit::stats.expanded_bodies);
        trial.expanded[slot] = 1;''')
s=replace(s,'        if (body.rigidBody.IsStatic())\n            return true;\n        PhysicsWorkTimer timer','''        if (body.rigidBody.IsStatic()) {
            response_audit::Count(response_audit::stats.expanded_static);
            return true;
        }
        response_audit::Count(response_audit::stats.expanded_dynamic);
        PhysicsWorkTimer timer''')
s=replace(s,'        const auto& candidates = QuerySlots(ActualImpactBound(slot, ImpactPoseAt(slot, now)));','''        response_audit::Count(response_audit::stats.query_slots_calls);
        const auto& candidates = QuerySlots(ActualImpactBound(slot, ImpactPoseAt(slot, now)));
        response_audit::Count(response_audit::stats.query_candidates, candidates.size());
        if (response_audit::enabled) response_audit::stats.query_candidates_max = std::max(response_audit::stats.query_candidates_max, std::uint64_t(candidates.size()));''')
s=replace(s,'    if (!pair(selected[0], selected[1]) || trial.rows.empty() || !expand(selected[0]) ||','''    {
    response_audit::Timer auditSeedTimer(response_audit::stats.ns_domain_seed);
    if (!pair(selected[0], selected[1]) || trial.rows.empty() || !expand(selected[0]) ||''')
s=replace(s,'    for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i)\n        if (!expand(trial.forcedExpansion[i]))\n            return false;','''    for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i) {
        response_audit::Count(response_audit::stats.forced_queue_scans);
        if (trial.expanded[trial.forcedExpansion[i]]) response_audit::Count(response_audit::stats.forced_queue_already_expanded);
        if (!expand(trial.forcedExpansion[i]))
            return false;
    }
    }''')
s=replace(s,'    const auto hasSupport = [&](const auto& p) {','    const auto hasSupport = [&](const auto& p) {\n        response_audit::Count(response_audit::stats.support_capture_visits);')
s=replace(s,'    const auto grow = [&](const std::vector<RigidBody*>& activated) {\n        std::vector<unsigned> next;','''    const auto grow = [&](const std::vector<RigidBody*>& activated) {
        response_audit::Timer auditGrowTimer(response_audit::stats.ns_domain_growth);
        response_audit::Count(response_audit::stats.callback_rounds);
        response_audit::Count(response_audit::stats.callback_activated_inputs, activated.size());
        std::vector<unsigned> next;''')
s=replace(s,'            if (!trial.expanded[slot])\n                next.push_back(slot);','''            if (!trial.expanded[slot]) {
                response_audit::CapacityProbe auditNext(next, response_audit::stats.next_vector_growth_count, response_audit::stats.next_vector_growth_bytes);
                next.push_back(slot);
            } else response_audit::Count(response_audit::stats.callback_already_expanded);''')
s=replace(s,'        std::sort(next.begin(), next.end());\n        next.erase(std::unique(next.begin(), next.end()), next.end());','''        response_audit::Count(response_audit::stats.callback_next_before_unique, next.size());
        const auto auditBeforeUnique = next.size();
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        response_audit::Count(response_audit::stats.callback_next_unique, next.size());
        response_audit::Count(response_audit::stats.callback_next_duplicates, auditBeforeUnique - next.size());''')
s=replace(s,'        for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i)\n            if (!trial.expanded[trial.forcedExpansion[i]] && !expand(trial.forcedExpansion[i]))\n                return false;','''        for (std::size_t i = 0; i < trial.forcedExpansion.size(); ++i) {
            response_audit::Count(response_audit::stats.forced_queue_scans);
            if (trial.expanded[trial.forcedExpansion[i]]) response_audit::Count(response_audit::stats.forced_queue_already_expanded);
            if (!trial.expanded[trial.forcedExpansion[i]] && !expand(trial.forcedExpansion[i]))
                return false;
        }''')
s=replace(s,'    return accepted;','    auditAccepted = accepted;\n    return accepted;')
(out/'WorldIncrementalImpact.cpp').write_text(s)

s=source('src/ImpactSolver.cpp')
s=replace(s,'#include "ImpactSolver.h"','#include "ImpactSolver.h"\n#include "ResponseAudit.h"')
s=replace(s,'Budget Measure(const RigidBody& b) {\n    Budget result;','Budget Measure(const RigidBody& b) {\n    response_audit::Count(response_audit::stats.solver_budget_body_measurements);\n    Budget result;')
s=replace(s,'                                 bool forceInelastic,JointSolver* joints) {','''                                 bool forceInelastic,JointSolver* joints) {
    response_audit::ContextScope auditContext(response_audit::Context::LegacyImpact);
    response_audit::Timer auditTimer(response_audit::stats.ns_solver_legacy);
    response_audit::Count(response_audit::stats.solver_calls_legacy);
    response_audit::Count(response_audit::stats.solver_legacy_input_rows, contacts.size());''')
s=replace(s,'        result.safetyFallback = true;','        response_audit::Count(response_audit::stats.solver_legacy_safety_fallbacks);\n        result.safetyFallback = true;')
s=replace(s,'                                ImpactResult& result) {','''                                ImpactResult& result) {
    response_audit::ContextScope auditContext(response_audit::Context::GrowingImpact);
    response_audit::Timer auditTimer(response_audit::stats.ns_solver_growing);
    response_audit::Count(response_audit::stats.solver_calls_growing);''')
s=replace(s,'    const auto append = [&](std::size_t first) {\n        const auto firstFrame = m_solver.FrameNodeRequests();','''    const auto append = [&](std::size_t first) {
        response_audit::Timer auditAppendTimer(response_audit::stats.ns_solver_append);
        response_audit::Count(response_audit::stats.solver_append_calls);
        response_audit::Count(response_audit::stats.solver_appended_rows, contacts.size() - first);
        const auto firstFrame = m_solver.FrameNodeRequests();''')
s=replace(s,'                m_saved.push_back({body, body->linearVelocity, body->angularVelocity});','''                response_audit::CapacityProbe auditSaved(m_saved, response_audit::stats.solver_saved_growth_count, response_audit::stats.solver_saved_growth_bytes);
                m_saved.push_back({body, body->linearVelocity, body->angularVelocity});''')
s=replace(s,'        m_solver.PrepareAppended(0, first, firstFrame);','''        response_audit::Count(response_audit::stats.solver_append_frame_requests, m_solver.FrameNodeRequests() - firstFrame);
        m_solver.PrepareAppended(0, first, firstFrame);''')
s=replace(s,'    append(0);\n    for (int iteration = 0; iteration < ContactSolver::kVelocityIterations; ++iteration) {\n        std::size_t first = 0;','''    append(0);
    bool auditPreviousFixedPoint = false;
    for (int iteration = 0; iteration < ContactSolver::kVelocityIterations; ++iteration) {
        response_audit::Count(response_audit::stats.solver_sweeps);
        const auto auditChangesBefore = response_audit::stats.growing_accumulator_changes;
        const auto auditRowsBefore = response_audit::stats.growing_rows;
        const auto auditContactsBefore = contacts.size();
        std::size_t first = 0;''')
s=replace(s,'            m_solver.SolveTrackedVelocityRange(first, 1, activated);','''            response_audit::Count(response_audit::stats.solver_ranges);
            m_solver.SolveTrackedVelocityRange(first, 1, activated);''')
s=replace(s,'            first = count;\n        }\n    }\n    result.contacts = contacts.size();','''            first = count;
        }
        if (response_audit::enabled) {
            const auto rowVisits = response_audit::stats.growing_rows - auditRowsBefore;
            response_audit::stats.solver_rows_per_sweep_sum += rowVisits;
            const bool fixed = response_audit::stats.growing_accumulator_changes == auditChangesBefore && contacts.size() == auditContactsBefore;
            response_audit::stats.solver_sweep_fixed_points += fixed;
            if (fixed && auditPreviousFixedPoint) {
                ++response_audit::stats.solver_sweep_after_fixed_point;
                response_audit::stats.solver_fixed_point_row_visits += rowVisits;
            }
            auditPreviousFixedPoint = fixed;
        }
    }
    response_audit::Timer auditBudgetTimer(response_audit::stats.ns_solver_budget);
    result.contacts = contacts.size();''')
s=replace(s,'        const auto& c = m_solver.Constraints()[i];\n        const auto va =\n            c.bodyA->linearVelocity','''        response_audit::Count(response_audit::stats.solver_contact_closure_checks);
        const auto& c = m_solver.Constraints()[i];
        const auto va =
            c.bodyA->linearVelocity''')
s=replace(s,'        if (-double(glm::dot(va - vb, c.normal)) * dt > contacts[i].maxDefectDisplacement)\n            return false;','''        if (-double(glm::dot(va - vb, c.normal)) * dt > contacts[i].maxDefectDisplacement) {
            response_audit::Count(response_audit::stats.solver_contact_closure_failures);
            return false;
        }''')
# Both legacy and growing budget loops are useful to count without numerical changes.
s=s.replace('        for (const auto& c : m_solver.Constraints()) {','        for (const auto& c : m_solver.Constraints()) {\n            response_audit::Count(response_audit::stats.solver_budget_rows);')
s=replace(s,'    for (const auto& c : m_solver.Constraints()) {\n        const glm::dvec3 impulse =','    for (const auto& c : m_solver.Constraints()) {\n        response_audit::Count(response_audit::stats.solver_budget_rows);\n        const glm::dvec3 impulse =')
(out/'ImpactSolver.cpp').write_text(s)

s=source('src/ContactSolver.cpp')
s=replace(s,'#include "ContactSolver.h"','#include "ContactSolver.h"\n#include "ResponseAudit.h"')
s=replace(s,'    void* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);','''    void* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);
    response_audit::Count(response_audit::stats.frame_upstream_alloc_count);
    response_audit::Count(response_audit::stats.frame_upstream_alloc_bytes, bytes);''')
s=replace(s,'std::size_t ContactSolver::RegisterFrame(RigidBody& body, const glm::dmat3* prepared) {','''std::size_t ContactSolver::RegisterFrame(RigidBody& body, const glm::dmat3* prepared) {
    response_audit::Count(response_audit::stats.frame_node_requests);''')
s=replace(s,'    m_frames.push_back(frame);','''    { response_audit::CapacityProbe auditFrames(m_frames, response_audit::stats.frame_vector_growth_count, response_audit::stats.frame_vector_growth_bytes);
      m_frames.push_back(frame); }''')
s=replace(s,'    m_frameIndex.emplace(&body,index);','''    m_frameIndex.emplace(&body,index);
    response_audit::Count(response_audit::stats.frame_index_insertions);''')
s=replace(s,'    m_constraints.push_back(c);\n    m_pending.push_back(Pending{restitution, std::max(warmNormalImpulse, 0.0f), warmTangentImpulse});','''    { response_audit::CapacityProbe auditConstraints(m_constraints, response_audit::stats.constraint_vector_growth_count, response_audit::stats.constraint_vector_growth_bytes);
      m_constraints.push_back(c); }
    { response_audit::CapacityProbe auditPending(m_pending, response_audit::stats.pending_vector_growth_count, response_audit::stats.pending_vector_growth_bytes);
      m_pending.push_back(Pending{restitution, std::max(warmNormalImpulse, 0.0f), warmTangentImpulse}); }''')
s=replace(s,'                                    std::size_t firstFrame) {\n    if (firstRow','''                                    std::size_t firstFrame) {
    response_audit::Timer auditTimer(response_audit::stats.ns_prepare);
    response_audit::Count(response_audit::stats.prepare_calls);
    response_audit::Count(response_audit::stats.prepare_rows, m_constraints.size() - firstRow);
    response_audit::Count(response_audit::stats.prepare_frames, m_frames.size() - firstFrame);
    if (firstRow''')
s=replace(s,'        if (c.normalMass <= 0.0f) continue;\n        c.normalImpulse = m_pending[i - firstRow].warmNormal;','''        response_audit::ObserveWarm(m_pending[i - firstRow].warmNormal,
            glm::any(glm::notEqual(m_pending[i - firstRow].warmTangent, glm::vec3(0.0f))));
        if (c.normalMass <= 0.0f) continue;
        c.normalImpulse = m_pending[i - firstRow].warmNormal;''')
s=replace(s,'bool SolveVelocityConstraint(ContactConstraint& c) {\n    if (c.normalMass <= 0.0f)\n        return false;','''bool SolveVelocityConstraint(ContactConstraint& c) {
    if (c.normalMass <= 0.0f) {
        response_audit::Count(response_audit::stats.row_inactive_mass);
        response_audit::ObserveRow(false, false, false);
        return false;
    }
    glm::vec3 auditLinearA(0), auditAngularA(0), auditLinearB(0), auditAngularB(0);
    if (response_audit::enabled) {
        auditLinearA = c.bodyA->linearVelocity; auditAngularA = c.bodyA->angularVelocity;
        auditLinearB = c.bodyB->linearVelocity; auditAngularB = c.bodyB->angularVelocity;
    }''')
s=replace(s,'    return c.normalImpulse != oldNormal || glm::any(glm::notEqual(c.tangentImpulse, oldTangent));','''    const bool accumulatorChanged = c.normalImpulse != oldNormal || glm::any(glm::notEqual(c.tangentImpulse, oldTangent));
    if (response_audit::enabled) {
        const bool endpointA = auditLinearA != c.bodyA->linearVelocity || auditAngularA != c.bodyA->angularVelocity;
        const bool endpointB = auditLinearB != c.bodyB->linearVelocity || auditAngularB != c.bodyB->angularVelocity;
        response_audit::ObserveRow(accumulatorChanged, endpointA, endpointB);
    }
    return accumulatorChanged;''')
s=replace(s,'void ContactSolver::SolveVelocities(int iterations) {','''void ContactSolver::SolveVelocities(int iterations) {
    response_audit::Timer auditTimer(response_audit::context == response_audit::Context::LegacyImpact ? response_audit::stats.ns_legacy_rows : response_audit::stats.ns_ordinary_rows);''')
s=replace(s,'                                              std::vector<RigidBody*>& activated) {\n    std::vector<unsigned char> seen(m_frames.size(), 0);','''                                              std::vector<RigidBody*>& activated) {
    response_audit::Timer auditTimer(response_audit::stats.ns_solver_ranges);
    response_audit::Count(response_audit::stats.range_calls);
    response_audit::Count(response_audit::stats.range_iterations, iterations);
    response_audit::Count(response_audit::stats.range_input_activated, activated.size());
    response_audit::Count(response_audit::stats.range_seen_bytes, m_frames.size());
    response_audit::Count(response_audit::stats.range_seen_alloc_count, m_frames.empty() ? 0 : 1);
    std::vector<unsigned char> seen(m_frames.size(), 0);''')
s=replace(s,'                activated.push_back(c.bodyA);','''                response_audit::Count(response_audit::stats.range_activated_outputs);
                response_audit::CapacityProbe auditActivated(activated, response_audit::stats.activated_vector_growth_count, response_audit::stats.activated_vector_growth_bytes);
                activated.push_back(c.bodyA);''')
s=replace(s,'                activated.push_back(c.bodyB);','''                response_audit::Count(response_audit::stats.range_activated_outputs);
                response_audit::CapacityProbe auditActivated(activated, response_audit::stats.activated_vector_growth_count, response_audit::stats.activated_vector_growth_bytes);
                activated.push_back(c.bodyB);''')
(out/'ContactSolver.cpp').write_text(s)
(out/'source-sha256.txt').write_text(''.join(hashlib.sha256((root/p).read_bytes()).hexdigest()+'  '+p+'\n' for p in ['src/physics/WorldIncrementalImpact.cpp','src/ImpactSolver.cpp','src/ContactSolver.cpp']))
print('Wrote private observer overlays and preserved source copies; no builds or simulation runs performed.')
