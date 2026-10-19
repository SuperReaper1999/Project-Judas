// Shares only snapshot decoding and exact observation helpers with the baseline
// replay. This separate executable is not linked into the engine or lab.
#define main JudasOriginalOfflineReplayMain
#include "Replay.cpp"
#undef main
#include "NormalSchur.h"

namespace {
double NormalSpeedPrecise(const ContactConstraint& row) {
    const auto va = glm::dvec3(row.bodyA->linearVelocity) +
        glm::cross(glm::dvec3(row.bodyA->angularVelocity), glm::dvec3(row.velocityOffsetA));
    const auto vb = glm::dvec3(row.bodyB->linearVelocity) +
        glm::cross(glm::dvec3(row.bodyB->angularVelocity), glm::dvec3(row.velocityOffsetB));
    return glm::dot(va-vb, glm::dvec3(row.normal)) - double(row.restitutionBias);
}
void NormalDelta(ContactConstraint& row, float delta) {
    const glm::vec3 impulse = row.normal*delta;
    if (!row.bodyA->IsStatic()) {
        row.bodyA->linearVelocity += impulse*row.bodyA->inverseMass;
        row.bodyA->angularVelocity += row.inverseInertiaA*glm::cross(row.velocityOffsetA, impulse);
    }
    if (!row.bodyB->IsStatic()) {
        row.bodyB->linearVelocity -= impulse*row.bodyB->inverseMass;
        row.bodyB->angularVelocity -= row.inverseInertiaB*glm::cross(row.velocityOffsetB, impulse);
    }
}
Json MatrixChecks(normal_schur::System& a) {
    double inertiaAsymmetry = 0, diagonalDifference = 0;
    for (const auto& body : a.bodies)
        for (int column=0; column<3; ++column) for (int row=0; row<3; ++row)
            inertiaAsymmetry = std::max(inertiaAsymmetry,
                std::abs(body.inverseInertia[column][row]-body.inverseInertia[row][column]));
    std::vector<double> x(a.rows.size()), y(a.rows.size());
    for (std::size_t i=0; i<x.size(); ++i) { x[i] = std::sin(double(i+1)); y[i] = std::cos(double(i+7)); }
    const auto ax = a.Multiply(x), ay = a.Multiply(y);
    const auto xay = normal_schur::Dot(x,ay), yax = normal_schur::Dot(y,ax);
    for (std::size_t i=0; i<std::min<std::size_t>(a.rows.size(),8); ++i) {
        std::vector<double> unit(a.rows.size(),0);
        unit[i] = 1;
        const auto column = a.Multiply(unit);
        diagonalDifference = std::max(diagonalDifference,std::abs(column[i]-a.diagonal[i]));
    }
    return {{"inverse_inertia_max_asymmetry", inertiaAsymmetry},
            {"schur_xAy_minus_yAx", xay-yax},
            {"schur_relative_symmetry_error", std::abs(xay-yax)/std::max({1.0,std::abs(xay),std::abs(yax)})},
            {"schur_xAx", normal_schur::Dot(x,ax)},
            {"schur_yAy", normal_schur::Dot(y,ay)},
            {"sampled_diagonal_difference", diagonalDifference},
            {"operator", "original represented inverse mass and cached float inverse inertia, converted to binary64 without symmetrization"}};
}
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) throw std::runtime_error("usage: schur-replay SNAPSHOT OUTPUT [cg|fista]");
    const bool fista = argc > 3 && std::string(argv[3]) == "fista";
    std::ifstream in(argv[1]); Json input; in >> input;
    if (input.at("hard_joints").get<bool>()) throw std::runtime_error("no hard-joint state present");
    System system(input);
    ContactSolver solver;
    for (const auto& c : system.contacts) solver.AddContact(*c.a,*c.b,c.geometry,c.friction,0.0f);
    solver.Prepare(0.0f);
    solver.SolveVelocities();
    // Non-const owned diagnostic object; public engine API remains unchanged.
    auto& rows = const_cast<std::vector<ContactConstraint>&>(solver.Constraints());
    normal_schur::System matrix(rows);
    Json output = {{"schema", "judas.offline-normal-schur.v1"},
        {"scope", "actual event, frozen friction normal QP then original full-law recoupling; diagnostic only"},
        {"bodies", system.bodies.size()}, {"rows", rows.size()}, {"algorithm", fista ? "diagonally scaled monotone restarted FISTA" : "projected activation plus active CG"}, {"operator_checks", MatrixChecks(matrix)},
        {"pgs10_residual", Residual(rows)}, {"pgs10_energy_J", Energy(system)},
        {"passes", Json::array()}};
    double allMs = 0;
    for (unsigned outer=0; outer<5; ++outer) {
        std::vector<double> initial(rows.size()), rhs(rows.size());
        for (std::size_t i=0; i<rows.size(); ++i) initial[i] = rows[i].normalImpulse;
        const auto applied = matrix.Multiply(initial);
        for (std::size_t i=0; i<rows.size(); ++i) rhs[i] = NormalSpeedPrecise(rows[i])-applied[i];
        const auto begin = std::chrono::steady_clock::now();
        const auto solved = fista ? normal_schur::SolveFista(matrix,rhs,initial) : normal_schur::Solve(matrix,rhs,initial);
        const double solveMs = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        allMs += solveMs;
        std::size_t negativeDeltas = 0, coneViolations = 0;
        for (std::size_t i=0; i<rows.size(); ++i) {
            const float next = float(solved.impulse[i]);
            const float delta = next-rows[i].normalImpulse;
            negativeDeltas += delta < 0;
            NormalDelta(rows[i],delta);
            rows[i].normalImpulse = next;
            coneViolations += glm::length(rows[i].tangentImpulse) > rows[i].friction*next+1e-7f;
        }
        Json pass = {{"pass", outer}, {"qp_ms", solveMs}, {"qp_converged", solved.converged},
            {"qp_outer_steps", solved.outer}, {"qp_cg_iterations", solved.cgIterations},
            {"qp_bound_hits", solved.blocked}, {"qp_backtracks", solved.backtracks},
            {"qp_projected_normal_residual_mps", solved.projectedResidual},
            {"qp_objective_before", solved.objectiveBefore}, {"qp_objective_after", solved.objectiveAfter},
            {"negative_within_event_normal_delta_count", negativeDeltas},
            {"frozen_friction_cone_violations", coneViolations},
            {"after_float_application_residual", Residual(rows)}, {"after_qp_energy_J", Energy(system)}};
        // This is deliberately the unchanged engine law. Re-solving normals
        // alone cannot be labelled a Coulomb solution while these rows violate.
        const auto recouple = std::chrono::steady_clock::now();
        solver.SolveVelocities(1);
        const double recoupleMs = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-recouple).count();
        allMs += recoupleMs;
        pass["original_full_kernel_recoupling_ms"] = recoupleMs;
        pass["after_original_full_kernel_residual"] = Residual(rows);
        pass["after_original_full_kernel_energy_J"] = Energy(system);
        output["passes"].push_back(pass);
    }
    output["cumulative_diagnostic_ms"] = allMs;
    output["matrix_vector_products"] = matrix.multiplies;
    output["final_velocity_digest"] = VelocityDigest(system);
    std::ofstream(argv[2]) << output.dump(2) << '\n';
    std::cout << output.dump(2) << '\n';
}
