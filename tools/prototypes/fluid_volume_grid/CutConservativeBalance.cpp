#include "CutConservativeBalance.h"
#include <cmath>
namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Finite(Real x) { return std::isfinite(x.hi) && std::isfinite(x.lo); }
}
bool ApplyCutConservativeBalance(const std::vector<double>& capacity,
 const std::vector<CutTransportFace>& faces, const std::vector<CutTransportGasPort>& ports,
 const std::vector<Real>& residual, std::vector<Real>& output, std::string& error) {
    const auto n = capacity.size();
    if (n == 0 || residual.size() != n) { error = "invalid conservative balance dimensions"; return false; }
    std::vector<Real> fraction(n), next = residual;
    for (std::size_t i = 0; i < n; ++i) {
        if (!(capacity[i] > 0. && std::isfinite(capacity[i])) || !Finite(residual[i])) {
            error = "invalid conservative balance row"; return false;
        }
        fraction[i] = residual[i] / Real(capacity[i]);
    }
    for (const auto& face : faces) {
        if (face.left >= n || face.right >= n || face.left == face.right || !Finite(face.volume)) {
            error = "invalid conservative balance face"; return false;
        }
        const bool forward = face.volume.Value() >= 0.;
        const auto from = forward ? face.left : face.right, to = forward ? face.right : face.left;
        const Real amount = forward ? face.volume : Real() - face.volume;
        const Real carried = amount * fraction[from];
        next[from] = next[from] + carried; next[to] = next[to] - carried;
    }
    for (const auto& port : ports) {
        if (port.cell >= n || !Finite(port.volume)) { error = "invalid conservative balance port"; return false; }
        if (port.volume.Value() > 0.) next[port.cell] = next[port.cell] + port.volume * fraction[port.cell];
    }
    output = std::move(next); error.clear(); return true;
}

bool ApplyCutConservativeBalanceTangent(const std::vector<double>& capacity,
 const std::vector<CutTransportFace>& faces, const std::vector<CutTransportGasPort>& ports,
 const std::vector<Real>& residual, const std::vector<Real>& derivative,
 const std::vector<Real>& dq, const std::vector<Real>& dv,
 std::vector<Real>& output, std::string& error) {
    if (residual.size() != capacity.size() || dq.size() != faces.size() || dv.size() != ports.size()) {
        error = "invalid conservative tangent dimensions"; return false;
    }
    for (auto x : residual) if (!Finite(x)) { error = "invalid conservative source residual"; return false; }
    std::vector<Real> next;
    if (!ApplyCutConservativeBalance(capacity, faces, ports, derivative, next, error)) return false;
    for (std::size_t j = 0; j < faces.size(); ++j) {
        if (!Finite(dq[j])) { error = "nonfinite conservative face derivative"; return false; }
        const auto& face = faces[j];
        const bool forward = face.volume.Value() >= 0.;
        const auto from = forward ? face.left : face.right, to = forward ? face.right : face.left;
        Real carried = (forward ? dq[j] : Real() - dq[j]) * residual[from] / Real(capacity[from]);
        if (face.volume.Value() == 0.)
            carried = dq[j] * Real(.5) * (residual[face.left] / Real(capacity[face.left]) + residual[face.right] / Real(capacity[face.right]));
        next[from] = next[from] + carried; next[to] = next[to] - carried;
    }
    for (std::size_t j = 0; j < ports.size(); ++j) {
        if (!Finite(dv[j])) { error = "nonfinite conservative port derivative"; return false; }
        const auto& port = ports[j];
        if (port.volume.Value() >= 0.)
            next[port.cell] = next[port.cell] + dv[j] * residual[port.cell] / Real(capacity[port.cell]) * Real(port.volume.Value() == 0. ? .5 : 1.);
    }
    output = std::move(next); error.clear(); return true;
}
}
