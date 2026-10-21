#pragma once
// Private diagnostic observer. No solver policy or numerical state is modified.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace response_audit {
#define RESPONSE_AUDIT_COUNTERS(X) \
    X(domain_attempts) \
    X(domain_accepted) \
    X(domain_rejected) \
    X(domain_reset_slots) \
    X(domain_reset_flag_bytes) \
    X(initialize_requests) \
    X(initialized_reuses) \
    X(initialized_bodies) \
    X(initialized_static) \
    X(initialized_dynamic) \
    X(deferred_gravity_bodies) \
    X(expand_requests) \
    X(expanded_reuses) \
    X(expanded_bodies) \
    X(expanded_static) \
    X(expanded_dynamic) \
    X(query_slots_calls) \
    X(query_candidates) \
    X(query_candidates_max) \
    X(pair_attempts) \
    X(pair_unique) \
    X(pair_duplicates) \
    X(manifold_queries) \
    X(manifold_nonempty) \
    X(manifold_uncertain) \
    X(manifold_points) \
    X(appended_domain_pairs) \
    X(appended_domain_rows) \
    X(continuing_rows) \
    X(new_rows) \
    X(support_capture_visits) \
    X(callback_rounds) \
    X(callback_activated_inputs) \
    X(callback_already_expanded) \
    X(callback_next_before_unique) \
    X(callback_next_unique) \
    X(callback_next_duplicates) \
    X(forced_queue_scans) \
    X(forced_queue_already_expanded) \
    X(visited_node_alloc_lower_count) \
    X(visited_node_payload_lower_bytes) \
    X(world_vector_growth_count) \
    X(world_vector_growth_bytes) \
    X(next_vector_growth_count) \
    X(next_vector_growth_bytes) \
    X(row_vector_growth_count) \
    X(row_vector_growth_bytes) \
    X(key_vector_growth_count) \
    X(key_vector_growth_bytes) \
    X(pair_vector_growth_count) \
    X(pair_vector_growth_bytes) \
    X(solver_calls_legacy) \
    X(solver_calls_growing) \
    X(solver_legacy_input_rows) \
    X(solver_legacy_safety_fallbacks) \
    X(solver_append_calls) \
    X(solver_appended_rows) \
    X(solver_append_frame_requests) \
    X(solver_sweeps) \
    X(solver_ranges) \
    X(solver_rows_per_sweep_sum) \
    X(solver_sweep_fixed_points) \
    X(solver_sweep_after_fixed_point) \
    X(solver_fixed_point_row_visits) \
    X(solver_contact_closure_checks) \
    X(solver_contact_closure_failures) \
    X(solver_budget_body_measurements) \
    X(solver_budget_rows) \
    X(range_calls) \
    X(range_iterations) \
    X(range_input_activated) \
    X(range_seen_bytes) \
    X(range_seen_alloc_count) \
    X(range_activated_outputs) \
    X(activated_vector_growth_count) \
    X(activated_vector_growth_bytes) \
    X(solver_saved_growth_count) \
    X(solver_saved_growth_bytes) \
    X(constraint_vector_growth_count) \
    X(constraint_vector_growth_bytes) \
    X(pending_vector_growth_count) \
    X(pending_vector_growth_bytes) \
    X(frame_vector_growth_count) \
    X(frame_vector_growth_bytes) \
    X(frame_node_requests) \
    X(frame_index_insertions) \
    X(frame_upstream_alloc_count) \
    X(frame_upstream_alloc_bytes) \
    X(ordinary_rows) \
    X(ordinary_accumulator_changes) \
    X(ordinary_endpoint_changes) \
    X(ordinary_zero_velocity_changes) \
    X(ordinary_no_accumulator_changes) \
    X(legacy_rows) \
    X(legacy_accumulator_changes) \
    X(legacy_endpoint_changes) \
    X(legacy_zero_velocity_changes) \
    X(legacy_no_accumulator_changes) \
    X(growing_rows) \
    X(growing_accumulator_changes) \
    X(growing_endpoint_changes) \
    X(growing_zero_velocity_changes) \
    X(growing_no_accumulator_changes) \
    X(row_inactive_mass) \
    X(prepare_calls) \
    X(prepare_rows) \
    X(prepare_frames) \
    X(ordinary_prepare_rows) \
    X(ordinary_warm_normal_nonzero) \
    X(ordinary_warm_tangent_nonzero) \
    X(ordinary_warm_any_nonzero) \
    X(ordinary_warm_zero) \
    X(legacy_prepare_rows) \
    X(legacy_warm_normal_nonzero) \
    X(legacy_warm_tangent_nonzero) \
    X(legacy_warm_any_nonzero) \
    X(legacy_warm_zero) \
    X(growing_prepare_rows) \
    X(growing_warm_normal_nonzero) \
    X(growing_warm_tangent_nonzero) \
    X(growing_warm_any_nonzero) \
    X(growing_warm_zero) \
    X(ns_domain) \
    X(ns_domain_seed) \
    X(ns_domain_growth) \
    X(ns_solver_growing) \
    X(ns_solver_append) \
    X(ns_solver_ranges) \
    X(ns_solver_budget) \
    X(ns_solver_legacy) \
    X(ns_prepare) \
    X(ns_ordinary_rows) \
    X(ns_legacy_rows)
#define RESPONSE_AUDIT_SUMS(X) \
    X(ordinary_warm_normal_sum) \
    X(legacy_warm_normal_sum) \
    X(growing_warm_normal_sum)
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
    out << '\n';
}
inline void WriteStep(std::ostream& out, std::uint64_t step) {
    out << step << ',' << unsigned(enabled);
#define WRITE_VALUE(name) out << ',' << stats.name;
    RESPONSE_AUDIT_COUNTERS(WRITE_VALUE)
    RESPONSE_AUDIT_SUMS(WRITE_VALUE)
#undef WRITE_VALUE
    out << '\n';
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
#define ROW_CONTEXT(prefix) \
    ++stats.prefix##_rows; \
    stats.prefix##_accumulator_changes += accumulator; \
    stats.prefix##_endpoint_changes += unsigned(endpointA) + unsigned(endpointB); \
    stats.prefix##_zero_velocity_changes += accumulator && !endpointA && !endpointB; \
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
#define WARM_CONTEXT(prefix) \
    ++stats.prefix##_prepare_rows; \
    stats.prefix##_warm_normal_nonzero += nonzero; \
    stats.prefix##_warm_tangent_nonzero += tangent; \
    stats.prefix##_warm_any_nonzero += nonzero || tangent; \
    stats.prefix##_warm_zero += !nonzero && !tangent; \
    stats.prefix##_warm_normal_sum += double(normal)
    switch (context) {
    case Context::Ordinary: { WARM_CONTEXT(ordinary); break; }
    case Context::LegacyImpact: { WARM_CONTEXT(legacy); break; }
    case Context::GrowingImpact: { WARM_CONTEXT(growing); break; }
    }
#undef WARM_CONTEXT
}
} // namespace response_audit
