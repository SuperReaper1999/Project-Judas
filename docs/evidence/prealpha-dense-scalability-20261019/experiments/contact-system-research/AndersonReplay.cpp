// One bounded full-law acceleration experiment on the captured actual event.
// All normal/friction updates use the original ContactSolver velocity kernel.
#define main JudasOriginalOfflineReplayMain
#include "Replay.cpp"
#undef main

namespace {
using Values = std::vector<double>;
double Dot(const Values& a, const Values& b) {
    double value = 0;
    for (std::size_t i=0; i<a.size(); ++i) value += a[i]*b[i];
    return value;
}
Values Difference(const Values& a, const Values& b) {
    Values result(a.size());
    for (std::size_t i=0; i<a.size(); ++i) result[i] = a[i]-b[i];
    return result;
}
struct Evaluation {
    Values x, mapped, residual;
    double residualSquared = 0, kinetic = 0, work = 0;
    bool energyPass = false;
};
struct FullMap {
    struct BodyState {
        glm::dvec3 linear, angular;
        glm::dmat3 inverseInertia{0};
        double inverseMass;
    };
    struct Pair { std::size_t a,b; };
    System& state;
    ContactSolver& solver;
    std::vector<ContactConstraint>& rows;
    std::vector<BodyState> base;
    std::vector<Pair> pair;
    std::vector<glm::dvec3> linearImpulse, angularImpulse;
    Values weight;
    double initialEnergy, energyTolerance;
    unsigned maps = 0, energyRejects = 0;

    FullMap(System& s, ContactSolver& c, const Json& input)
        : state(s), solver(c), rows(const_cast<std::vector<ContactConstraint>&>(c.Constraints())) {
        for (const auto& encoded : input.at("bodies_before")) {
            const auto b = Body(encoded);
            base.push_back({glm::dvec3(b.linearVelocity),glm::dvec3(b.angularVelocity),
                            glm::dmat3(0),b.inverseMass});
        }
        for (const auto& row : rows) {
            const auto a = s.identities.bodyIndex.at(row.bodyA), b = s.identities.bodyIndex.at(row.bodyB);
            pair.push_back({a,b});
            base[a].inverseInertia = glm::dmat3(row.inverseInertiaA);
            base[b].inverseInertia = glm::dmat3(row.inverseInertiaB);
            const double w = row.normalMass > 0 ? std::sqrt(1.0/double(row.normalMass)) : 1;
            for (int component=0; component<4; ++component) weight.push_back(w);
        }
        linearImpulse.resize(base.size()); angularImpulse.resize(base.size());
        initialEnergy = Double(input.at("result").at("kinetic_before"));
        energyTolerance = Double(input.at("result").at("energy_tolerance"));
    }
    Values Read() const {
        Values result;
        result.reserve(rows.size()*4);
        for (const auto& row : rows) {
            result.push_back(row.normalImpulse);
            for (int axis=0; axis<3; ++axis) result.push_back(row.tangentImpulse[axis]);
        }
        return result;
    }
    void Project(Values& x) const {
        for (std::size_t i=0; i<rows.size(); ++i) {
            x[4*i] = std::max(0.0,x[4*i]);
            const glm::dvec3 n(rows[i].normal);
            glm::dvec3 t(x[4*i+1],x[4*i+2],x[4*i+3]);
            t -= n*(glm::dot(n,t)/glm::dot(n,n));
            const double limit = rows[i].friction*x[4*i], length = glm::length(t);
            if (length > limit) t *= limit/length;
            for (int axis=0; axis<3; ++axis) x[4*i+1+axis] = t[axis];
        }
    }
    void Set(const Values& x) {
        std::fill(linearImpulse.begin(),linearImpulse.end(),glm::dvec3(0));
        std::fill(angularImpulse.begin(),angularImpulse.end(),glm::dvec3(0));
        for (std::size_t i=0; i<rows.size(); ++i) {
            auto& row = rows[i];
            row.normalImpulse = float(x[4*i]);
            row.tangentImpulse = glm::vec3(float(x[4*i+1]),float(x[4*i+2]),float(x[4*i+3]));
            const glm::dvec3 impulse = glm::dvec3(row.normal)*double(row.normalImpulse)+glm::dvec3(row.tangentImpulse);
            const auto a = pair[i].a, b = pair[i].b;
            linearImpulse[a] += impulse; linearImpulse[b] -= impulse;
            angularImpulse[a] += glm::cross(glm::dvec3(row.velocityOffsetA),impulse);
            angularImpulse[b] -= glm::cross(glm::dvec3(row.velocityOffsetB),impulse);
        }
        // Reconstruct from this event's initial physical state, not successive
        // trial velocities. The represented original mass operator is retained.
        // Binary64 accumulation avoids making a trial depend on row summation
        // roundoff; the actual original solver still receives binary32 state.
        for (std::size_t i=0; i<base.size(); ++i) {
            state.bodies[i].linearVelocity = glm::vec3(base[i].linear+linearImpulse[i]*base[i].inverseMass);
            state.bodies[i].angularVelocity = glm::vec3(base[i].angular+base[i].inverseInertia*angularImpulse[i]);
        }
    }
    double Work() const {
        double value = 0;
        for (const auto& row : rows) {
            const glm::dvec3 impulse = glm::dvec3(row.normal)*double(row.normalImpulse)+glm::dvec3(row.tangentImpulse);
            auto velocity = [](const RigidBody& b, glm::vec3 offset) {
                return glm::dvec3(b.linearVelocity)+glm::cross(glm::dvec3(b.angularVelocity),glm::dvec3(offset));
            };
            if (row.bodyA->IsStatic()) value -= glm::dot(impulse,velocity(*row.bodyA,row.velocityOffsetA));
            if (row.bodyB->IsStatic()) value += glm::dot(impulse,velocity(*row.bodyB,row.velocityOffsetB));
        }
        return value;
    }
    Evaluation Evaluate(Values x) {
        Set(x);
        // Quantize the trial to the exact actual solver's accumulated fields.
        x = Read();
        const double kinetic = Energy(state), work = Work();
        solver.SolveVelocities(1);
        ++maps;
        const auto mapped = Read();
        Values residual(mapped.size());
        for (std::size_t i=0; i<residual.size(); ++i) residual[i] = (mapped[i]-x[i])*weight[i];
        const bool pass = std::isfinite(kinetic) && kinetic <= initialEnergy+work+energyTolerance;
        energyRejects += !pass;
        return {std::move(x),mapped,residual,Dot(residual,residual),kinetic,work,pass};
    }
};
bool Extrapolate(const std::vector<Evaluation>& history, Values& candidate) {
    if (history.size()<2) return false;
    const auto count = history.size()-1;
    const auto size = history.back().residual.size();
    std::vector<Values> basis(count), deltaMap(count);
    std::vector<std::vector<double>> upper(count,std::vector<double>(count,0));
    std::vector<double> rhs(count,0), coefficient(count,0);
    double largest = 0;
    for (std::size_t column=0; column<count; ++column) {
        basis[column] = Difference(history[column+1].residual,history[column].residual);
        deltaMap[column] = Difference(history[column+1].mapped,history[column].mapped);
        // Twice-reorthogonalized modified Gram-Schmidt on at most four history
        // directions. Singular history is rejected instead of regularized into
        // a large, unexplained extrapolation.
        for (int repeat=0; repeat<2; ++repeat)
            for (std::size_t row=0; row<column; ++row) {
                const double projection = Dot(basis[row],basis[column]);
                upper[row][column] += projection;
                for (std::size_t i=0; i<size; ++i) basis[column][i] -= projection*basis[row][i];
            }
        const double length = std::sqrt(Dot(basis[column],basis[column]));
        largest = std::max(largest,length);
        if (!(length > std::max(1e-20,largest*1e-10)) || !std::isfinite(length)) return false;
        upper[column][column] = length;
        for (auto& value : basis[column]) value /= length;
        rhs[column] = Dot(basis[column],history.back().residual);
    }
    for (std::size_t reversed=0; reversed<count; ++reversed) {
        const auto i = count-1-reversed;
        double value = rhs[i];
        for (std::size_t j=i+1; j<count; ++j) value -= upper[i][j]*coefficient[j];
        coefficient[i] = value/upper[i][i];
    }
    candidate = history.back().mapped;
    for (std::size_t j=0; j<count; ++j)
        for (std::size_t i=0; i<size; ++i) candidate[i] -= coefficient[j]*deltaMap[j][i];
    return std::all_of(candidate.begin(),candidate.end(),[](double x) { return std::isfinite(x); });
}
}

int main(int argc, char** argv) {
    if (argc<3 || argc>4) throw std::runtime_error("usage: anderson SNAPSHOT OUTPUT [map_budget=40]");
    const unsigned budget = argc>3 ? unsigned(std::stoul(argv[3])) : 40;
    std::ifstream in(argv[1]); Json input; in >> input;
    System system(input);
    ContactSolver solver;
    for (const auto& c : system.contacts) solver.AddContact(*c.a,*c.b,c.geometry,c.friction,0.0f);
    solver.Prepare(0.0f);
    solver.SolveVelocities();
    const auto initialResidual = Residual(solver.Constraints());
    FullMap map(system,solver,input);
    const auto start = std::chrono::steady_clock::now();
    Evaluation current = map.Evaluate(map.Read());
    std::vector<Evaluation> history{current};
    unsigned accepted = 0, rejected = 0, rankRejected = 0;
    Json progress = Json::array();
    auto observe = [&](const char* policy) {
        map.Set(current.x);
        progress.push_back({{"map_evaluations", map.maps}, {"full_fixed_point_residual_norm", std::sqrt(current.residualSquared)},
            {"energy_J", Energy(system)}, {"energy_guard_pass", current.energyPass},
            {"normal", Residual(solver.Constraints())}, {"policy", policy},
            {"elapsed_ms", std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}});
    };
    observe("initial original-PGS10 impulses reconstructed");
    while (map.maps<budget) {
        auto base = map.Evaluate(current.mapped);
        Values candidate;
        bool chose = false;
        if (map.maps<budget && Extrapolate(history,candidate)) {
            map.Project(candidate);
            auto trial = map.Evaluate(std::move(candidate));
            if (trial.energyPass && trial.residualSquared < base.residualSquared) {
                current = std::move(trial); ++accepted; chose = true;
            } else ++rejected;
        } else ++rankRejected;
        if (!chose) current = std::move(base);
        history.push_back(current);
        if (history.size()>5) history.erase(history.begin());
        observe(chose ? "safeguarded Anderson accepted" : "ordinary original full sweep");
    }
    map.Set(current.x);
    Json output = {{"schema", "judas.offline-full-law-anderson.v1"}, {"bodies", system.bodies.size()},
        {"contacts", system.contacts.size()}, {"original_pgs10_normal", initialResidual},
        {"map_budget", budget}, {"map_evaluations", map.maps}, {"accepted_extrapolations", accepted},
        {"rejected_extrapolations", rejected}, {"rank_or_history_rejections", rankRejected},
        {"energy_rejected_trials", map.energyRejects}, {"history_size_limit", 5},
        {"progress", progress}, {"final_velocity_digest", VelocityDigest(system)},
        {"scope", "offline original full Coulomb PGS fixed-point map; cone-projected history acceleration; physical same-event total-impulse reconstruction; no production code change"}};
    std::ofstream(argv[2]) << output.dump(2) << '\n';
    std::cout << output.dump(2) << '\n';
}
