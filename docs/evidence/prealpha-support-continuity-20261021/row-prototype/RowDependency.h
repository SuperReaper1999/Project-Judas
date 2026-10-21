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
    struct Row {
        std::uint64_t consumedA = 0, consumedB = 0;
        bool ownDirty = true;
    };
    struct Frame {
        RigidBody* body = nullptr;
        glm::vec3 linear{0}, angular{0};
        float inverseMass = 0;
        std::uint64_t revision = 0;
    };
    const ContactSolver* owner;
    std::vector<Row> rows;
    std::vector<Frame> frames;
    void Bump(std::size_t index) {
        if (frames[index].revision == std::numeric_limits<std::uint64_t>::max()) {
            // Prevent generation wrap from making an old certificate current.
            InvalidateAll();
            for (auto& frame : frames) frame.revision = 0;
        }
        ++frames[index].revision;
    }
    bool Observe(std::size_t index) {
        Count(stats.frameChecks);
        auto& observed = frames[index];
        const auto& body = *observed.body;
        if (SameVector(observed.linear,body.linearVelocity) &&
            SameVector(observed.angular,body.angularVelocity) &&
            SameFloat(observed.inverseMass,body.inverseMass)) return false;
        observed.linear = body.linearVelocity;
        observed.angular = body.angularVelocity;
        observed.inverseMass = body.inverseMass;
        Count(stats.frameChanges);
        if (control != Control::ForgetEndpointInvalidation) Bump(index);
        return true;
    }
    void Attach(std::size_t index, RigidBody* body) {
        auto& frame = frames[index];
        if (!frame.body) {
            frame.body = body;
            frame.linear = body->linearVelocity;
            frame.angular = body->angularVelocity;
            frame.inverseMass = body->inverseMass;
        }
    }
public:
    explicit Graph(const ContactSolver* solver) : owner(solver) {}
    bool Owns(const ContactSolver* solver) const { return owner == solver; }
    void Clear() { rows.clear(); frames.clear(); }
    void Prepare(const std::vector<ContactConstraint>& constraints,
                 std::size_t firstRow, std::size_t frameCount) {
        // Re-preparation changes geometry/masses/targets and accumulated state.
        // A non-appending call invalidates/rebuilds every certificate.
        if (!firstRow || rows.size() != firstRow) {
            Clear();
            firstRow = 0;
        }
        frames.resize(frameCount);
        rows.resize(constraints.size());
        for (std::size_t index = firstRow; index < constraints.size(); ++index) {
            const auto& c = constraints[index];
            Attach(c.frameA,c.bodyA);
            Attach(c.frameB,c.bodyB);
            rows[index].ownDirty = true;
            Count(stats.appendedRows);
        }
    }
    void InvalidateAll() {
        for (auto& row : rows) row.ownDirty = true;
    }
    void Retarget(std::size_t index) {
        Count(stats.targetInvalidations);
        if (control != Control::ForgetTargetInvalidation && index < rows.size())
            rows[index].ownDirty = true;
    }
    bool NeedsEvaluation(std::size_t index, const ContactConstraint& constraint) {
        Count(stats.considered);
        if (index >= rows.size()) { Count(stats.evaluated); return true; }
        // Exact lazy checks detect external body edits without scanning all
        // frames before every appended range. No incident-row walk is needed.
        Observe(constraint.frameA);
        Observe(constraint.frameB);
        const auto& row = rows[index];
        if (!row.ownDirty && row.consumedA == frames[constraint.frameA].revision &&
            row.consumedB == frames[constraint.frameB].revision) {
            Count(stats.skipped);
            return false;
        }
        Count(stats.evaluated);
        return true;
    }
    void Evaluated(std::size_t index, const ContactConstraint& constraint, bool accumulatorChanged) {
        if (index >= rows.size()) return;
        const bool a = Observe(constraint.frameA), b = Observe(constraint.frameB);
        auto& row = rows[index];
        row.consumedA = frames[constraint.frameA].revision;
        row.consumedB = frames[constraint.frameB].revision;
        // Accumulator changes remain dirty even when body updates round away.
        // Only a no-change evaluation at these endpoint versions is reusable.
        row.ownDirty = accumulatorChanged;
        if (control == Control::ForgetOwnAccumulator && !a && !b) row.ownDirty = false;
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
