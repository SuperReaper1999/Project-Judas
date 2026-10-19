#include "ImpactSolver.h"
#include "RigidBody.h"
#include "ContactCapture.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using contact_capture::Json;
float Float(const Json& j) {
    const auto bits = j.get<std::uint32_t>();
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
double Double(const Json& j) {
    const auto bits = j.get<std::uint64_t>();
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
glm::vec3 Vector(const Json& j) { return {Float(j[0]), Float(j[1]), Float(j[2])}; }
glm::dvec3 Precise(const Json& j) { return {Double(j[0]), Double(j[1]), Double(j[2])}; }
glm::mat3 Matrix(const Json& j) { return {Vector(j[0]), Vector(j[1]), Vector(j[2])}; }
RigidBody Body(const Json& j) {
    RigidBody b;
    b.position = Vector(j.at("position"));
    const auto& q = j.at("orientation_wxyz");
    b.orientation = {Float(q[0]), Float(q[1]), Float(q[2]), Float(q[3])};
    b.linearVelocity = Vector(j.at("linear_velocity"));
    b.angularVelocity = Vector(j.at("angular_velocity"));
    b.inverseMass = Float(j.at("inverse_mass"));
    b.inverseInertiaLocal = Matrix(j.at("inverse_inertia_local_columns"));
    b.forceAccumulator = Vector(j.at("force_accumulator"));
    b.torqueAccumulator = Vector(j.at("torque_accumulator"));
    return b;
}
Contact Geometry(const Json& j) {
    Contact c;
    c.hit = j.at("hit");
    c.point = Vector(j.at("point"));
    c.normal = Vector(j.at("normal"));
    c.penetration = Float(j.at("penetration"));
    c.signedSeparation = Double(j.at("signed_separation"));
    c.preciseNormal = Precise(j.at("precise_normal"));
    c.localAnchorA = Precise(j.at("local_anchor_a"));
    c.localAnchorB = Precise(j.at("local_anchor_b"));
    c.localWitnessA = Precise(j.at("local_witness_a"));
    c.localWitnessB = Precise(j.at("local_witness_b"));
    c.hasLocalAnchors = j.at("has_local_anchors");
    c.anchorAInWorldFrame = j.at("anchor_a_world_frame");
    c.anchorBInWorldFrame = j.at("anchor_b_world_frame");
    c.separationState = SeparationState(j.at("separation_state").get<int>());
    return c;
}
struct System {
    std::vector<RigidBody> bodies;
    std::vector<ImpactContact> contacts;
    contact_capture::Record identities;
    explicit System(const Json& input) {
        bodies.reserve(input.at("bodies_before").size());
        for (const auto& j : input.at("bodies_before")) bodies.push_back(Body(j));
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            identities.bodies.push_back(&bodies[i]);
            identities.bodyIndex.emplace(&bodies[i], i);
        }
        for (const auto& j : input.at("contacts")) {
            contacts.push_back({&bodies[j.at("body_a").get<std::size_t>()],
                &bodies[j.at("body_b").get<std::size_t>()], Geometry(j.at("geometry")),
                Float(j.at("friction")), Float(j.at("restitution")), j.at("persistent").get<bool>()});
        }
    }
};
std::string VelocityDigest(const System& s) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto& b : s.bodies) for (auto v : {b.linearVelocity, b.angularVelocity})
        for (int axis = 0; axis < 3; ++axis) {
            const auto bits = contact_capture::Bits(v[axis]);
            for (unsigned byte = 0; byte < 4; ++byte) {
                hash ^= (bits >> (byte * 8)) & 255u;
                hash *= 1099511628211ull;
            }
        }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}
Json Residual(const std::vector<ContactConstraint>& rows) {
    double worstClosing = 0, worstActive = 0, complementarity = 0, squared = 0;
    std::size_t active = 0, closing = 0;
    for (const auto& row : rows) {
        const auto va = row.bodyA->linearVelocity + glm::cross(row.bodyA->angularVelocity, row.velocityOffsetA);
        const auto vb = row.bodyB->linearVelocity + glm::cross(row.bodyB->angularVelocity, row.velocityOffsetB);
        const double vn = glm::dot(va-vb, row.normal) - row.restitutionBias;
        worstClosing = std::max(worstClosing, -vn);
        closing += vn < 0;
        if (row.normalImpulse > 0) {
            ++active;
            worstActive = std::max(worstActive, std::abs(vn));
        }
        const double residual = row.normalImpulse > 0 ? std::abs(vn) : std::max(-vn, 0.0);
        squared += residual*residual;
        complementarity = std::max(complementarity, std::abs(double(row.normalImpulse)*vn));
    }
    return {{"rows", rows.size()}, {"active", active}, {"closing", closing},
            {"maximum_closing_mps", worstClosing}, {"maximum_active_residual_mps", worstActive},
            {"normal_kkt_l2_mps", std::sqrt(squared)},
            {"maximum_complementarity_product_Ns_mps", complementarity}};
}
}

int main(int argc, char** argv) {
    if (argc != 3) throw std::runtime_error("usage: replay SNAPSHOT_JSON OUTPUT_JSON");
    std::ifstream in(argv[1]);
    Json input;
    in >> input;
    if (input.at("hard_joints").get<bool>()) throw std::runtime_error("joint state not present; cannot replay hard-joint event");
    System actual(input);
    ImpactSolver impact;
    const auto beforeDigest = VelocityDigest(actual);
    const auto start = std::chrono::steady_clock::now();
    const auto result = impact.Solve(actual.contacts, Float(input.at("dt_float_bits")), input.at("force_inelastic"));
    const auto elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    Json afterBodies = Json::array(), afterRows = Json::array();
    for (const auto& b : actual.bodies) afterBodies.push_back(contact_capture::Body(b));
    for (const auto& row : impact.Constraints()) afterRows.push_back(contact_capture::Constraint(row, actual.identities));
    const bool bodiesExact = afterBodies == input.at("bodies_after");
    const bool rowsExact = afterRows == input.at("constraints_after");
    const bool resultExact = contact_capture::Result(result) == input.at("result");
    Json output = {{"schema", "judas.contact-replay.v1"}, {"sequence", input.at("sequence")},
        {"bodies", actual.bodies.size()}, {"contacts", actual.contacts.size()},
        {"dt", Float(input.at("dt_float_bits"))}, {"bodies_exact_bit_parity", bodiesExact},
        {"all_prepared_and_solved_rows_exact_bit_parity", rowsExact}, {"result_exact_bit_parity", resultExact},
        {"before_velocity_digest", beforeDigest}, {"original_10_pgs_after_velocity_digest", VelocityDigest(actual)},
        {"original_10_pgs_ms", elapsed}, {"original_10_pgs_residual", Residual(impact.Constraints())},
        {"kinetic_before_J", Double(input.at("result").at("kinetic_before"))},
        {"kinetic_after_J", result.kineticAfter}, {"constant_geometry_pgs_reference", Json::array()}};
    if (!bodiesExact) output["body_bit_difference"] = Json::diff(input.at("bodies_after"), afterBodies);
    if (!rowsExact) output["row_bit_difference"] = Json::diff(input.at("constraints_after"), afterRows);
    if (!resultExact) output["result_bit_difference"] = Json::diff(input.at("result"), contact_capture::Result(result));

    System repeated(input);
    const auto initialBodies = repeated.bodies;
    ImpactSolver repeatedSolver;
    Json samples = Json::array();
    bool repeatedExact = true;
    for (int trial = 0; trial < 32; ++trial) {
        std::copy(initialBodies.begin(), initialBodies.end(), repeated.bodies.begin());
        const auto begin = std::chrono::steady_clock::now();
        const auto repeatedResult = repeatedSolver.Solve(repeated.contacts,
            Float(input.at("dt_float_bits")), input.at("force_inelastic"));
        samples.push_back(std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-begin).count());
        Json observedBodies=Json::array(), observedRows=Json::array();
        for (const auto& body : repeated.bodies) observedBodies.push_back(contact_capture::Body(body));
        for (const auto& row : repeatedSolver.Constraints())
            observedRows.push_back(contact_capture::Constraint(row, repeated.identities));
        repeatedExact = repeatedExact && observedBodies==input.at("bodies_after") &&
            observedRows==input.at("constraints_after") &&
            contact_capture::Result(repeatedResult)==input.at("result");
    }
    output["same_captured_event_32_timings_ms"] = samples;
    output["all_32_repetitions_exact_body_row_result_bits"] = repeatedExact;
    output["reference_scope"] = "Actual unchanged ten-iteration ImpactSolver replay; JSON and input restoration outside timed spans. Warm repeated captured event timings are a local cost comparison, not whole-world FPS.";
    std::ofstream(argv[2]) << output.dump(2) << '\n';
    std::cout << output.dump(2) << '\n';
    return bodiesExact && rowsExact && resultExact && repeatedExact ? 0 : 1;
}
