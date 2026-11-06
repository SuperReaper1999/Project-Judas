#include "CutSelectiveTransport.h"
#include <cmath>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Finite(Real x) { return std::isfinite(x.hi) && std::isfinite(x.lo); }
bool Fail(std::string& error, const char* message) { error = message; return false; }
}

bool CutSelectiveTransport::Build(const std::vector<double>& capacity,
    const std::vector<CutTransportFace>& faces, const std::vector<CutTransportGasPort>& ports,
    const std::vector<Real>& water, const std::vector<Real>& mass,
    double density, double budget, std::string& error) {
    CutSelectiveTransport next;
    if (!next.Prepare(capacity, faces, ports, water, mass, density, budget, error)) return false;
    *this = std::move(next);
    return true;
}

bool CutSelectiveTransport::Prepare(const std::vector<double>& capacity,
    const std::vector<CutTransportFace>& faces, const std::vector<CutTransportGasPort>& ports,
    const std::vector<Real>& water, const std::vector<Real>& mass,
    double density, double budget, std::string& error) {
    const auto n = capacity.size();
    if (n == 0 || water.size() != n || mass.size() != n ||
        !(density > 0. && std::isfinite(density) && budget > 0. && std::isfinite(budget)))
        return Fail(error, "invalid selective transport dimensions or settings");
    for (std::size_t i = 0; i < n; ++i)
        if (!Finite(water[i]) || !Finite(mass[i]) || water[i].Value() < 0. || mass[i].Value() < 0.)
            return Fail(error, "invalid selective source quantity");
    std::vector<CutDirectedEdge> edges;
    for (const auto& face : faces) {
        if (face.left >= n || face.right >= n || face.left == face.right || !Finite(face.volume))
            return Fail(error, "invalid selective interior face");
        if (face.volume.Value() >= 0.) edges.push_back({face.left, face.right, face.volume});
        else edges.push_back({face.right, face.left, Real() - face.volume});
    }
    for (const auto& port : ports)
        if (port.cell >= n || !Finite(port.volume)) return Fail(error, "invalid selective gas port");
    std::vector<Real> zero(n);
    if (!waterSolver_.Build(capacity, edges, zero, error) ||
        !waterSolver_.SolveFraction(water, 1e-12, alpha_, error, budget / 8.)) return false;
    capacity_ = capacity; faces_ = faces; ports_ = ports; ambientDensity_ = density;
    water_.resize(n);
    for (std::size_t i = 0; i < n; ++i) water_[i] = alpha_[i] * Real(capacity[i]);
    std::vector<Real> exterior(n), rhs = mass;
    sharedOperator_ = true;
    for (const auto& port : ports) {
        const auto i = port.cell;
        if (port.volume.Value() > 0.) {
            const Real gap = Real(capacity[i]) - water_[i];
            if (!(gap.Value() > 0.)) return Fail(error, "gas-only port has no gas capacity");
            exterior[i] = exterior[i] + port.volume * Real(capacity[i]) / gap;
            sharedOperator_ = false;
        } else rhs[i] = rhs[i] - port.volume * Real(density);
    }
    if (!sharedOperator_ && !gasSolver_.Build(capacity, edges, exterior, error)) return false;
    auto& solver = sharedOperator_ ? waterSolver_ : gasSolver_;
    if (!solver.SolveFraction(rhs, density * 1e-12, gamma_, error, density * budget / 8.)) return false;
    mass_.resize(n);
    for (std::size_t i = 0; i < n; ++i) mass_[i] = gamma_[i] * Real(capacity[i]);
    error.clear();
    return true;
}

bool CutSelectiveTransport::Tangent(const std::vector<Real>& dq,
    const std::vector<Real>& dv, std::vector<Real>& waterDerivative,
    std::vector<Real>& massDerivative, std::string& error) {
    const auto n = capacity_.size();
    if (n == 0 || dq.size() != faces_.size() || dv.size() != ports_.size() || &waterDerivative == &massDerivative)
        return Fail(error, "invalid selective tangent dimensions");
    for (auto x : dq) if (!Finite(x)) return Fail(error, "nonfinite selective face tangent");
    for (auto x : dv) if (!Finite(x)) return Fail(error, "nonfinite selective port tangent");
    std::vector<Real> dw(n), dm(n), aw, am;
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        const auto& face = faces_[f];
        const bool forward = face.volume.Value() >= 0.;
        const auto from = forward ? face.left : face.right, to = forward ? face.right : face.left;
        const Real delta = forward ? dq[f] : Real() - dq[f];
        const Real a = face.volume.Value() == 0. ? (alpha_[face.left] + alpha_[face.right]) * Real(.5) : alpha_[from];
        const Real g = face.volume.Value() == 0. ? (gamma_[face.left] + gamma_[face.right]) * Real(.5) : gamma_[from];
        dw[from] = dw[from] - delta * a; dw[to] = dw[to] + delta * a;
        dm[from] = dm[from] - delta * g; dm[to] = dm[to] + delta * g;
    }
    if (!waterSolver_.SolveFraction(dw, 1e-12, aw, error)) return false;
    for (std::size_t j = 0; j < ports_.size(); ++j) {
        const auto& port = ports_[j]; const auto i = port.cell;
        if(port.volume.Value()==0. && dv[j].Value()==0.) continue;
        if (port.volume.Value() >= 0.) {
            const Real capacity = capacity_[i], gap = capacity - water_[i];
            if (!(gap.Value() > 0.)) return Fail(error, "gas-port tangent has no gas capacity");
            const Real delta = capacity / gap * dv[j] + port.volume * capacity / (gap * gap) * (capacity * aw[i]);
            if (port.volume.Value() == 0.)
                dm[i] = dm[i] - dv[j] * (gamma_[i] * capacity / gap + Real(ambientDensity_)) * Real(.5);
            else dm[i] = dm[i] - delta * gamma_[i];
        } else dm[i] = dm[i] - dv[j] * Real(ambientDensity_);
    }
    auto& solver = sharedOperator_ ? waterSolver_ : gasSolver_;
    if (!solver.SolveFraction(dm, ambientDensity_ * 1e-12, am, error)) return false;
    std::vector<Real> nextWater(n), nextMass(n);
    for (std::size_t i = 0; i < n; ++i) {
        nextWater[i] = aw[i] * Real(capacity_[i]);
        nextMass[i] = am[i] * Real(capacity_[i]);
    }
    waterDerivative = std::move(nextWater); massDerivative = std::move(nextMass);
    error.clear();
    return true;
}

std::vector<Real> CutSelectiveTransport::GasPortMass() const {
    std::vector<Real> result;
    for (const auto& port : ports_) {
        if (port.volume.Value() > 0.)
            result.push_back(port.volume * mass_[port.cell] / (Real(capacity_[port.cell]) - water_[port.cell]));
        else result.push_back(port.volume * Real(ambientDensity_));
    }
    return result;
}

CutFastTransportStats CutSelectiveTransport::Stats() const {
    const auto& a = waterSolver_.Stats(); const auto& b = gasSolver_.Stats();
    return {a.solves + b.solves, a.corrections + b.corrections,
            a.sweeps + b.sweeps, a.fallbacks + b.fallbacks};
}
}
