#include "CutMassMomentum.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Finite(Real x) { return std::isfinite(x.hi) && std::isfinite(x.lo); }
bool Fail(std::string& e, const char* m) { e = m; return false; }
Real Squared(const CutMomentumVector& x) {
    Real s;
    for (auto v : x) s = s + v * v;
    return s;
}
CutMomentumVector Difference(const CutMomentumVector& a, const CutMomentumVector& b) {
    CutMomentumVector d{};
    for (int k = 0; k < 3; ++k) d[k] = a[k] - b[k];
    return d;
}
}

bool TransportCutMassMomentum(const std::vector<double>& capacity,
    const std::vector<Real>& startMass, const std::vector<Real>& endMass,
    const std::vector<CutTransportFace>& faces,
    const std::vector<CutMomentumBoundary>& ports,
    const std::vector<CutMomentumVector>& velocity,
    const std::vector<CutMomentumVector>& impulse,
    double massTolerance, double momentumTolerance,
    CutMassMomentumResult& output, std::string& error, CutMomentumBackend backend) {
    const auto n = capacity.size();
    if (n == 0 || startMass.size() != n || endMass.size() != n || velocity.size() != n ||
        impulse.size() != n || !(massTolerance >= 0. && std::isfinite(massTolerance) &&
        momentumTolerance > 0. && std::isfinite(momentumTolerance)))
        return Fail(error, "invalid mass-momentum dimensions or tolerances");
    std::vector<Real> balance(n), density(n), exterior(n);
    Real totalCapacity;
    for (std::size_t i = 0; i < n; ++i) {
        if (!(capacity[i] > 0. && std::isfinite(capacity[i])) || !Finite(startMass[i]) ||
            !Finite(endMass[i]) || !(startMass[i].Value() > 0. && endMass[i].Value() > 0.))
            return Fail(error, "momentum requires positive admitted mass owners");
        for (int k = 0; k < 3; ++k)
            if (!Finite(velocity[i][k]) || !Finite(impulse[i][k]))
                return Fail(error, "nonfinite momentum input");
        balance[i] = endMass[i] - startMass[i];
        density[i] = endMass[i] / Real(capacity[i]);
        totalCapacity = totalCapacity + Real(capacity[i]);
    }
    std::vector<CutDirectedEdge> edges;
    for (const auto& face : faces) {
        if (face.left >= n || face.right >= n || face.left == face.right || !Finite(face.volume))
            return Fail(error, "invalid momentum carrier face");
        const bool forward = face.volume.Value() >= 0.;
        const auto from = forward ? face.left : face.right, to = forward ? face.right : face.left;
        const Real mass = forward ? face.volume : Real() - face.volume;
        balance[from] = balance[from] + mass; balance[to] = balance[to] - mass;
        edges.push_back({from, to, mass / density[from]});
    }
    for (const auto& port : ports) {
        if (port.cell >= n || !Finite(port.mass)) return Fail(error, "invalid momentum boundary");
        for (auto v : port.incomingVelocity) if (!Finite(v)) return Fail(error, "nonfinite incoming velocity");
        balance[port.cell] = balance[port.cell] + port.mass;
        if (port.mass.Value() > 0.) exterior[port.cell] = exterior[port.cell] + port.mass / density[port.cell];
    }
    CutMassMomentumResult next;
    for (auto defect : balance) {
        next.maximumMassResidual = std::max(next.maximumMassResidual, std::abs(defect.Value()));
        if (!Finite(defect) || std::abs(defect.Value()) > massTolerance)
            return Fail(error, "momentum carrier does not close the admitted mass ledger");
    }
    CutFastDirectedTransport solver;
    CutDirectedTransport reference;
    if (backend == CutMomentumBackend::Fast) {
        if (!solver.Build(capacity, edges, exterior, error)) return false;
    } else if (!reference.Build(capacity, edges, exterior, error)) return false;
    const double fractionBudget = momentumTolerance / totalCapacity.Value();
    if (!(fractionBudget > 0. && std::isfinite(fractionBudget)))
        return Fail(error, "momentum error allocation is not representable");
    next.velocity.resize(n); next.momentum.resize(n);
    for (int k = 0; k < 3; ++k) {
        std::vector<Real> rhs(n), solution;
        for (std::size_t i = 0; i < n; ++i) rhs[i] = startMass[i] * velocity[i][k] + impulse[i][k];
        for (const auto& port : ports)
            if (port.mass.Value() < 0.) rhs[port.cell] = rhs[port.cell] - port.mass * port.incomingVelocity[k];
        // Solve for extensive momentum per geometric capacity, so the
        // storage capacity remains exact binary64 without rounding end mass.
        if (backend == CutMomentumBackend::Fast) {
            if (!solver.SolveFraction(rhs, fractionBudget, solution, error, momentumTolerance)) return false;
        } else if (!reference.SolveFraction(rhs, fractionBudget, solution, error)) return false;
        for (std::size_t i = 0; i < n; ++i) {
            next.momentum[i][k] = solution[i] * Real(capacity[i]);
            next.velocity[i][k] = next.momentum[i][k] / endMass[i];
            if (!Finite(next.velocity[i][k])) return Fail(error, "unrepresentable transported velocity");
            next.momentumResidual[k] = next.momentumResidual[k] + next.momentum[i][k] - startMass[i] * velocity[i][k] - impulse[i][k];
        }
    }
    // Independently evaluate EVERY original momentum row. Global cancellation
    // alone is not proof that the carrier actually reached its local owner.
    std::vector<CutMomentumVector> rows(n);
    for (std::size_t i = 0; i < n; ++i) for (int k = 0; k < 3; ++k)
        rows[i][k] = next.momentum[i][k] - startMass[i]*velocity[i][k] - impulse[i][k];
    for (const auto& face : faces) {
        const auto donor = face.volume.Value() >= 0. ? face.left : face.right;
        for (int k = 0; k < 3; ++k) {
            const Real carried = face.volume * next.velocity[donor][k];
            rows[face.left][k] = rows[face.left][k] + carried;
            rows[face.right][k] = rows[face.right][k] - carried;
        }
    }
    for (const auto& port : ports) {
        const auto& v = port.mass.Value() >= 0. ? next.velocity[port.cell] : port.incomingVelocity;
        for (int k = 0; k < 3; ++k) rows[port.cell][k] = rows[port.cell][k] + port.mass*v[k];
    }
    for (const auto& row : rows) for (int k = 0; k < 3; ++k) {
        if (!Finite(row[k])) return Fail(error, "nonfinite momentum row residual");
        const double residual = std::abs(row[k].Value());
        next.maximumLocalMomentumResidual = std::max(next.maximumLocalMomentumResidual, residual);
        next.momentumRowL1[k] = next.momentumRowL1[k] + Real(residual);
    }
    for (auto residual : next.momentumRowL1)
        if (residual.Value() > momentumTolerance) return Fail(error, "original local momentum rows failed");
    Real before, after, work, temporal, mixing, external, defectWork;
    double maximumSpeed = 0.;
    for (std::size_t i = 0; i < n; ++i) {
        before = before + Real(.5) * startMass[i] * Squared(velocity[i]);
        after = after + Real(.5) * endMass[i] * Squared(next.velocity[i]);
        temporal = temporal + Real(.5) * startMass[i] * Squared(Difference(next.velocity[i], velocity[i]));
        defectWork = defectWork - Real(.5) * balance[i] * Squared(next.velocity[i]);
        for (int k = 0; k < 3; ++k) work = work + impulse[i][k] * next.velocity[i][k];
        for (auto v : next.velocity[i]) maximumSpeed = std::max(maximumSpeed, std::abs(v.Value()));
    }
    for (const auto& face : faces) {
        Real mass = face.volume.Value() >= 0. ? face.volume : Real() - face.volume;
        mixing = mixing + Real(.5) * mass * Squared(Difference(next.velocity[face.left], next.velocity[face.right]));
    }
    for (const auto& port : ports) {
        const auto& v = port.mass.Value() >= 0. ? next.velocity[port.cell] : port.incomingVelocity;
        external = external + Real(.5) * port.mass * Squared(v);
        if (port.mass.Value() < 0.)
            mixing = mixing - Real(.5) * port.mass * Squared(Difference(next.velocity[port.cell], v));
        for (int k = 0; k < 3; ++k) next.externalMomentum[k] = next.externalMomentum[k] + port.mass * v[k];
    }
    for (int k = 0; k < 3; ++k) {
        next.momentumResidual[k] = next.momentumResidual[k] + next.externalMomentum[k];
        if (!Finite(next.momentumResidual[k]) || std::abs(next.momentumResidual[k].Value()) > momentumTolerance)
            return Fail(error, "transported momentum ledger failed");
    }
    Real energyResidual = after - before - work + temporal + mixing + external - defectWork;
    const double scale = std::abs(before.Value()) + std::abs(after.Value()) + std::abs(work.Value()) +
        std::abs(temporal.Value()) + std::abs(mixing.Value()) + std::abs(external.Value());
    // This admission is independent of the mass/momentum solve certificates.
    // A mass defect's kinetic work is exposed, never used to adjust energy.
    if (!Finite(energyResidual) || !std::isfinite(scale) ||
        std::abs(energyResidual.Value()) > 512.*std::numeric_limits<double>::epsilon()*scale +
            3.*momentumTolerance*maximumSpeed)
        return Fail(error, "transported momentum does not close its discrete work identity");
    next.beforeEnergy = before.Value(); next.afterEnergy = after.Value(); next.impulseWork = work.Value();
    next.externalEnergy = external.Value(); next.unsteadyLoss = temporal.Value(); next.mixingLoss = mixing.Value();
    next.energyResidual = energyResidual.Value(); next.massDefectWork = defectWork.Value();
    output = std::move(next); error.clear(); return true;
}
} // namespace judas::volume_grid
