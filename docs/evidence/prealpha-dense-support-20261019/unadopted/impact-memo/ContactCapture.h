#pragma once

// Isolated one-shot observer. Integer bit representations preserve the actual
// solver inputs exactly; the observed solver and all its budgets are untouched.
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include "third_party/nlohmann/json.hpp"

namespace contact_capture {
using Json = nlohmann::json;
inline std::uint32_t Bits(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
inline std::uint64_t Bits(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
inline Json Bits(const glm::vec3& v) { return {Bits(v.x), Bits(v.y), Bits(v.z)}; }
inline Json Bits(const glm::dvec3& v) { return {Bits(v.x), Bits(v.y), Bits(v.z)}; }
inline Json Bits(const glm::quat& q) { return {Bits(q.w), Bits(q.x), Bits(q.y), Bits(q.z)}; }
inline Json Bits(const glm::mat3& m) { return {Bits(m[0]), Bits(m[1]), Bits(m[2])}; }
inline Json Body(const RigidBody& b) {
    return {{"position", Bits(b.position)}, {"orientation_wxyz", Bits(b.orientation)},
            {"linear_velocity", Bits(b.linearVelocity)}, {"angular_velocity", Bits(b.angularVelocity)},
            {"inverse_mass", Bits(b.inverseMass)}, {"inverse_inertia_local_columns", Bits(b.inverseInertiaLocal)},
            {"force_accumulator", Bits(b.forceAccumulator)}, {"torque_accumulator", Bits(b.torqueAccumulator)}};
}
inline Json Geometry(const Contact& c) {
    return {{"hit", c.hit}, {"point", Bits(c.point)}, {"normal", Bits(c.normal)},
            {"penetration", Bits(c.penetration)}, {"signed_separation", Bits(c.signedSeparation)},
            {"precise_normal", Bits(c.preciseNormal)}, {"local_anchor_a", Bits(c.localAnchorA)},
            {"local_anchor_b", Bits(c.localAnchorB)}, {"local_witness_a", Bits(c.localWitnessA)},
            {"local_witness_b", Bits(c.localWitnessB)}, {"has_local_anchors", c.hasLocalAnchors},
            {"anchor_a_world_frame", c.anchorAInWorldFrame}, {"anchor_b_world_frame", c.anchorBInWorldFrame},
            {"separation_state", int(c.separationState)}};
}
struct Record {
    Json state;
    std::vector<RigidBody*> bodies;
    std::unordered_map<RigidBody*, std::size_t> bodyIndex;
    std::string path;
    explicit operator bool() const { return !path.empty(); }
};
inline Record Begin(const std::vector<ImpactContact>& contacts, float dt,
                    bool forceInelastic, bool hardJoints) {
    Record record;
    const char* path = std::getenv("JUDAS_CONTACT_CAPTURE_PATH");
    const char* selection = std::getenv("JUDAS_CONTACT_CAPTURE_EVENT");
    if (!path || !selection) return record;
    static std::uint64_t sequence = 0;
    ++sequence;
    if (sequence != std::strtoull(selection, nullptr, 10)) return record;
    record.path = path;
    record.state = {{"schema", "judas.contact-system.bits.v1"}, {"sequence", sequence},
                    {"encoding", "IEEE-754 scalar bits as unsigned JSON integers; matrices column-major"},
                    {"dt_float_bits", Bits(dt)}, {"force_inelastic", forceInelastic},
                    {"hard_joints", hardJoints}, {"contacts", Json::array()},
                    {"bodies_before", Json::array()}};
    auto index = [&](RigidBody* body) {
        auto found = record.bodyIndex.find(body);
        if (found != record.bodyIndex.end()) return found->second;
        const std::size_t i = record.bodies.size();
        record.bodies.push_back(body);
        record.bodyIndex.emplace(body, i);
        record.state["bodies_before"].push_back(Body(*body));
        return i;
    };
    for (const auto& c : contacts) {
        if (c.a->IsStatic() && c.b->IsStatic()) continue;
        const auto a = index(c.a);
        const auto b = index(c.b);
        record.state["contacts"].push_back({{"body_a", a}, {"body_b", b},
            {"geometry", Geometry(c.geometry)}, {"friction", Bits(c.friction)},
            {"restitution", Bits(c.restitution)}, {"persistent", c.persistent}});
    }
    return record;
}
inline Json Constraint(const ContactConstraint& c, const Record& record) {
    return {{"body_a", record.bodyIndex.at(c.bodyA)}, {"body_b", record.bodyIndex.at(c.bodyB)},
            {"frame_a", c.frameA}, {"frame_b", c.frameB}, {"point", Bits(c.point)},
            {"normal", Bits(c.normal)}, {"penetration", Bits(c.penetration)},
            {"friction", Bits(c.friction)}, {"restitution_bias", Bits(c.restitutionBias)},
            {"normal_mass", Bits(c.normalMass)}, {"normal_impulse", Bits(c.normalImpulse)},
            {"tangent_impulse", Bits(c.tangentImpulse)}, {"local_anchor_a", Bits(c.localAnchorA)},
            {"local_anchor_b", Bits(c.localAnchorB)}, {"precise_normal", Bits(c.preciseNormal)},
            {"initial_anchor_difference", Bits(c.initialAnchorDifference)},
            {"position_offset_difference", Bits(c.positionOffsetDifference)},
            {"signed_separation", Bits(c.signedSeparation)}, {"has_local_anchors", c.hasLocalAnchors},
            {"anchor_a_world_frame", c.anchorAInWorldFrame}, {"anchor_b_world_frame", c.anchorBInWorldFrame},
            {"velocity_offset_a", Bits(c.velocityOffsetA)}, {"velocity_offset_b", Bits(c.velocityOffsetB)},
            {"inverse_inertia_a_columns", Bits(c.inverseInertiaA)},
            {"inverse_inertia_b_columns", Bits(c.inverseInertiaB)}};
}
inline Json Result(const ImpactResult& r) {
    return {{"contacts", r.contacts}, {"material_closing_contacts", r.materialClosingContacts},
            {"ambiguous", r.ambiguous}, {"safety_fallback", r.safetyFallback},
            {"energy_budget_satisfied", r.energyBudgetSatisfied},
            {"effective_restitution", Bits(r.effectiveRestitution)},
            {"kinetic_before", Bits(r.kineticBefore)}, {"kinetic_after", Bits(r.kineticAfter)},
            {"prescribed_boundary_work", Bits(r.prescribedBoundaryWork)},
            {"energy_tolerance", Bits(r.energyTolerance)},
            {"momentum_before", Bits(r.momentumBefore)}, {"momentum_after", Bits(r.momentumAfter)},
            {"angular_momentum_before", Bits(r.angularMomentumBefore)},
            {"angular_momentum_after", Bits(r.angularMomentumAfter)}};
}
inline void End(Record& record, const std::vector<ContactConstraint>& constraints,
                const ImpactResult& result) {
    if (!record) return;
    record.state["bodies_after"] = Json::array();
    for (const auto* body : record.bodies) record.state["bodies_after"].push_back(Body(*body));
    record.state["constraints_after"] = Json::array();
    for (const auto& row : constraints) record.state["constraints_after"].push_back(Constraint(row, record));
    record.state["result"] = Result(result);
    std::ofstream out(record.path, std::ios::binary);
    if (!out) throw std::runtime_error("isolated contact capture output unavailable");
    out << record.state.dump(2) << '\n';
    if (!out) throw std::runtime_error("isolated contact capture output failed");
}
} // namespace contact_capture
