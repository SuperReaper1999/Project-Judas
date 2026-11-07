#include "WaterBulkStorage.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Fail(std::string &e, const char *why) {
    e = why;
    return false;
}
bool Finite(Real v) {
    return std::isfinite(v.hi) && std::isfinite(v.lo);
}
} // namespace
bool PrepareWaterBulkStorage(const CutSpatialField &f, double density, const GridVec &gravity,
                             WaterBulkGeometry &output, WaterBulkState &state, std::string &error) {
    const auto n = f.chart.regions.size();
    if (!n || n > 262144 || f.water.size() != n || f.gas.size() != n || f.gasMass.size() != n ||
        f.geometry.startCapacity.size() != n || f.geometry.endCapacity.size() != n ||
        f.parent.size() != n || f.flux.size() != f.geometry.faces.size() ||
        !f.chart.layout.CheckedCellCount() || !(density > 0. && std::isfinite(density)))
        return Fail(error, "invalid bulk storage source");
    if (f.geometry.motion)
        return Fail(error, "bulk storage requires a frozen solid-free box chart");
    for (double rate : f.flux)
        if (rate != 0.)
            return Fail(error, "bulk import cannot discard an existing velocity field");
    int up = -1;
    for (int a = 0; a < 3; ++a) {
        if (!std::isfinite(gravity[a]))
            return Fail(error, "nonfinite bulk gravity");
        if (gravity[a] != 0.) {
            if (up != -1 || gravity[a] > 0.)
                return Fail(error, "bulk prototype requires one negative chart gravity axis");
            up = a;
        }
    }
    if (up == -1)
        return Fail(error, "bulk prototype requires nonzero gravity");
    WaterBulkGeometry next;
    WaterBulkState imported;
    next.field = f;
    next.density = density;
    next.upAxis = up;
    next.stamp = 1469598103934665603ull;
    auto hash = [&](double v) {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        next.stamp = (next.stamp ^ bits) * 1099511628211ull;
    };
    hash(density);
    for (double v : gravity)
        hash(v);
    for (std::size_t i = 0; i < n; ++i) {
        const auto &r = f.chart.regions[i];
        double volume = 1.;
        for (int a = 0; a < 3; ++a) {
            if (!(std::isfinite(r.lo[a]) && std::isfinite(r.hi[a]) && r.hi[a] > r.lo[a]))
                return Fail(error, "invalid bulk cell bounds");
            volume *= r.hi[a] - r.lo[a];
            hash(r.lo[a]);
            hash(r.hi[a]);
        }
        double c = f.geometry.startCapacity[i];
        if (!(c > 0.) || c != f.geometry.endCapacity[i] || std::abs(volume - c) > 1e-12 * c)
            return Fail(error, "bulk import needs a fixed orthogonal free-space chart");
        Real w(f.water[i]), g(f.gas[i]), m(f.gasMass[i]);
        if (!Finite(w) || !Finite(g) || !Finite(m) || w.Value() < 0. || g.Value() < 0. ||
            m.Value() < 0. || std::abs((w + g - Real(c)).Value()) > 1e-12 * c ||
            (g.Value() == 0. && m.Value() != 0.))
            return Fail(error, "bulk source violates independently owned phase storage");
        // Keep measured water and air separate, and audit their import partition
        // error. Deriving gas by binary64 subtraction would erase small pockets.
        next.capacity.push_back(c);
        imported.water.push_back(w);
        imported.gas.push_back(g);
        imported.gasMass.push_back(m);
        double area = volume / (r.hi[up] - r.lo[up]);
        next.potentialBase.push_back(-density * gravity[up] * r.lo[up]);
        next.potentialSlope.push_back(-density * gravity[up] / area);
        hash(c);
        hash(next.potentialBase.back());
        hash(next.potentialSlope.back());
    }
    for (const auto &face : f.geometry.faces) {
        if (face.left >= n || face.right >= n || face.left == face.right || face.axis < 0 ||
            face.axis > 2 || !(face.meanArea > 0. && std::isfinite(face.meanArea)))
            return Fail(error, "invalid bulk connection");
        hash(double(face.left));
        hash(double(face.right));
        hash(double(face.axis));
        hash(face.meanArea);
        hash(face.length);
        for (int a = 0; a < 3; ++a) {
            hash(face.lo[a]);
            hash(face.hi[a]);
        }
    }
    for (const auto &b : f.boundaries) {
        if (b.fragment >= n || !b.gasOnly || b.axis < 0 || b.axis > 2 ||
            (b.sign != 1 && b.sign != -1) || b.volume != 0. || b.incomingWater != 0.)
            return Fail(error, "bulk prototype accepts only stationary gas vents");
        hash(double(b.fragment));
        hash(double(b.axis));
        hash(double(b.sign));
    }
    imported.faceRate.resize(f.geometry.faces.size());
    imported.faceInertance.resize(f.geometry.faces.size());
    imported.geometryStamp = next.stamp;
    output = std::move(next);
    state = std::move(imported);
    error.clear();
    return true;
}
bool AuditWaterBulkStorage(const WaterBulkGeometry &g, const WaterBulkState &s,
                           const WaterGasSettings &settings, WaterBulkAudit &output,
                           std::string &error) {
    const auto n = g.capacity.size();
    const auto nf = g.field.geometry.faces.size();
    if (!n || g.potentialBase.size() != n || g.potentialSlope.size() != n ||
        !Finite(s.externalGasMass) || s.geometryStamp != g.stamp || s.water.size() != n ||
        s.gas.size() != n || s.gasMass.size() != n || s.faceRate.size() != nf ||
        s.faceInertance.size() != nf ||
        !(settings.referenceDensity > 0. && settings.referencePressure > 0.))
        return Fail(error, "bulk state does not belong to this chart");
    Real water, mass, energy, kinetic;
    WaterBulkAudit next;
    const double rt = settings.referencePressure / settings.referenceDensity;
    for (std::size_t i = 0; i < n; ++i) {
        auto w = s.water[i], v = s.gas[i], m = s.gasMass[i];
        if (!Finite(w) || !Finite(v) || !Finite(m) || w.Value() < 0. || v.Value() < 0. ||
            m.Value() < 0. || std::abs((w + v - Real(g.capacity[i])).Value()) > 1e-12 * g.capacity[i] ||
            (v.Value() == 0. && m.Value() != 0.))
            return Fail(error, "bulk phase or capacity ledger failed");
        next.maximumPartitionResidual =
            std::max(next.maximumPartitionResidual,
                     std::abs((w + v - Real(g.capacity[i])).Value()) / g.capacity[i]);
        water = water + w;
        mass = mass + m;
        energy = energy + Real(g.potentialBase[i]) * w + Real(.5 * g.potentialSlope[i]) * w * w +
                 Real(settings.referencePressure) * v;
        if (m.Value() > 0.)
            energy = energy +
                     Real(rt) * m * Real(std::log((m / v).Value() / settings.referenceDensity) - 1.);
    }
    for (std::size_t f = 0; f < nf; ++f) {
        if (!(s.faceInertance[f] >= 0. && std::isfinite(s.faceInertance[f]) &&
              std::isfinite(s.faceRate[f])) ||
            (s.faceInertance[f] == 0. && s.faceRate[f] != 0.))
            return Fail(error, "invalid bulk face momentum");
        kinetic = kinetic + Real(.5 * s.faceInertance[f]) * Real(s.faceRate[f]) * Real(s.faceRate[f]);
    }
    next.water = water.Value();
    next.air = (mass + s.externalGasMass).Value();
    next.potentialEnergy = energy.Value();
    next.kineticEnergy = kinetic.Value();
    if (!std::isfinite(next.water) || !std::isfinite(next.air) ||
        !std::isfinite(next.potentialEnergy) || !std::isfinite(next.kineticEnergy))
        return Fail(error, "nonfinite bulk energy");
    output = next;
    error.clear();
    return true;
}
} // namespace judas::volume_grid
