#pragma once
// Private exact-row dependency prototype. No production/public header changes.
#include "ContactSolver.h"
#include "RigidBody.h"
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace row_dependency {
struct Stats {
    std::uint64_t considered = 0, evaluated = 0, skipped = 0;
    std::uint64_t cleanCertificates = 0, accumulatorOnlyChanges = 0;
    std::uint64_t frameChecks = 0, frameChanges = 0, dirtyMarks = 0;
    std::uint64_t appendedRows = 0, targetInvalidations = 0;
};
inline thread_local Stats stats{};
inline thread_local bool enabled = true, collectStats = false;
inline void SetEnabled(bool value) { enabled = value; }
inline void SetStatsEnabled(bool value) { collectStats = value; }
inline void ResetStats() { stats = {}; }
inline void Count(std::uint64_t& count, std::uint64_t amount = 1) {
    if (collectStats) count += amount;
}
inline bool SameFloat(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }
inline bool SameVector(const glm::vec3& a, const glm::vec3& b) {
    return SameFloat(a.x,b.x) && SameFloat(a.y,b.y) && SameFloat(a.z,b.z);
}

// Broken mechanisms are private negative controls, never production policies.
enum class Control { Exact, ForgetEndpointInvalidation, ForgetOwnAccumulator, ForgetTargetInvalidation };
inline thread_local Control control = Control::Exact;
inline void SetControl(Control value) { control = value; }

class Graph {
    static constexpr std::size_t None = std::numeric_limits<std::size_t>::max();
    struct Row {
        std::size_t nextA = None, nextB = None;
        bool dirty = true;
    };
    struct Frame {
        RigidBody* body = nullptr;
        std::size_t first = None;
        glm::vec3 linear{0}, angular{0};
        float inverseMass = 0;
    };
    const ContactSolver* owner;
    std::vector<Row> rows;
    std::vector<Frame> frames;
    void Mark(std::size_t index) {
        rows[index].dirty = true;
        Count(stats.dirtyMarks);
    }
    void MarkFrame(std::size_t frame) {
        if (control == Control::ForgetEndpointInvalidation) return;
        for (auto link = frames[frame].first; link != None;) {
            const auto index = link / 2;
            const auto endpoint = link % 2;
            Mark(index);
            link = endpoint ? rows[index].nextB : rows[index].nextA;
        }
    }
    bool Observe(std::size_t frame) {
        Count(stats.frameChecks);
        auto& observed = frames[frame];
        const auto& body = *observed.body;
        if (SameVector(observed.linear,body.linearVelocity) &&
            SameVector(observed.angular,body.angularVelocity) &&
            SameFloat(observed.inverseMass,body.inverseMass)) return false;
        observed.linear = body.linearVelocity;
        observed.angular = body.angularVelocity;
        observed.inverseMass = body.inverseMass;
        Count(stats.frameChanges);
        MarkFrame(frame);
        return true;
    }
    void Attach(std::size_t row, std::size_t frame, bool second, RigidBody* body) {
        auto& endpoint = frames[frame];
        if (!endpoint.body) {
            endpoint.body = body;
            endpoint.linear = body->linearVelocity;
            endpoint.angular = body->angularVelocity;
            endpoint.inverseMass = body->inverseMass;
        }
        if (second) rows[row].nextB = endpoint.first;
        else rows[row].nextA = endpoint.first;
        endpoint.first = row * 2 + unsigned(second);
    }
public:
    explicit Graph(const ContactSolver* solver) : owner(solver) {}
    bool Owns(const ContactSolver* solver) const { return owner == solver; }
    void Clear() { rows.clear(); frames.clear(); }
    void Prepare(const std::vector<ContactConstraint>& constraints,
                 std::size_t firstRow, std::size_t frameCount) {
        // Re-preparation changes geometry/masses/targets and accumulated state.
        // A non-appending call deliberately invalidates/rebuilds all certificates.
        if (!firstRow || rows.size() != firstRow) {
            Clear();
            firstRow = 0;
        }
        frames.resize(frameCount);
        rows.resize(constraints.size());
        for (std::size_t index = firstRow; index < constraints.size(); ++index) {
            const auto& c = constraints[index];
            Attach(index,c.frameA,false,c.bodyA);
            Attach(index,c.frameB,true,c.bodyB);
            Mark(index);
            Count(stats.appendedRows);
        }
    }
    void InvalidateAll() {
        for (auto& row : rows) row.dirty = true;
    }
    void Retarget(std::size_t index) {
        Count(stats.targetInvalidations);
        if (control != Control::ForgetTargetInvalidation && index < rows.size()) Mark(index);
    }
    bool NeedsEvaluation(std::size_t index, const ContactConstraint& constraint) {
        Count(stats.considered);
        if (index >= rows.size()) { Count(stats.evaluated); return true; }
        // Lazy exact checks also detect caller edits between tracked ranges,
        // without a global scan of all frames before every appended range.
        Observe(constraint.frameA);
        Observe(constraint.frameB);
        if (!rows[index].dirty) { Count(stats.skipped); return false; }
        Count(stats.evaluated);
        return true;
    }
    void Evaluated(std::size_t index, const ContactConstraint& constraint, bool accumulatorChanged) {
        if (index >= rows.size()) return;
        // An accumulator change is itself a dependency even if body updates
        // round away. Only an idempotent evaluation establishes a certificate.
        rows[index].dirty = accumulatorChanged;
        const bool a = Observe(constraint.frameA), b = Observe(constraint.frameB);
        if (control == Control::ForgetOwnAccumulator && !a && !b) rows[index].dirty = false;
        Count(stats.accumulatorOnlyChanges, accumulatorChanged && !a && !b);
        Count(stats.cleanCertificates, !accumulatorChanged && !a && !b);
    }
};
inline thread_local Graph* active = nullptr;
inline Graph* ActiveFor(const ContactSolver* owner) {
    return enabled && active && active->Owns(owner) ? active : nullptr;
}
struct Scope {
    Graph* previous;
    explicit Scope(Graph& graph) : previous(active) { active = &graph; }
    ~Scope() { active = previous; }
};
} // namespace row_dependency
