#include "WaterBulkProjection.h"
#include "CutCoupledKrylov.h"
#include "WaterBulkCondensation.h"
#include "WaterBulkReconcile.h"
#include "WaterGasGeometry.h"
#include "WaterPressureILU.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Fail(std::string &e, const char *why) {
    e = why;
    return false;
}
struct Link {
    std::size_t l, r;
    double c;
};
struct Vent {
    std::size_t i;
    double c;
};
struct Local {
    Real water, gas, mass, pressure;
    double wh = 0., wm = 0., ph = 0., pm = 0., md = 1., mh = 0.;
};
Real Root(Real a) {
    Real x(std::sqrt(a.Value()));
    if (x.Value() > 0.)
        x = x + (a - x * x) / (Real(2.) * x);
    return x;
}
bool PositiveRatio(Real x, Real &result) {
    if (!std::isfinite(x.Value()) || std::abs(x.Value()) > 700.)
        return false;
    unsigned halves = 0;
    while (std::abs(x.Value()) > .01) {
        x = x * Real(.5);
        ++halves;
    }
    Real sum(1.), term(1.);
    for (unsigned i = 1; i <= 16; ++i) {
        term = term * x / Real(double(i));
        sum = sum + term;
    }
    for (; halves > 0; --halves)
        sum = sum * sum;
    result = sum;
    return std::isfinite(sum.Value()) && sum.Value() > 0.;
}
bool LogRatio(Real ratio, Real &result) {
    if (!(ratio.Value() > 0. && std::isfinite(ratio.Value())))
        return false;
    Real x(std::log(ratio.Value()));
    for (int i = 0; i < 2; ++i) {
        Real e;
        if (!PositiveRatio(x, e))
            return false;
        x = x + ratio / e - Real(1.);
    }
    result = x;
    return true;
}
struct System {
    const WaterBulkGeometry &geometry;
    const WaterBulkState &source;
    const WaterGasSettings &settings;
    std::vector<Real> head, sourcePressure;
    std::vector<bool> pressureCoordinates;
    std::vector<double> inertance, predictor;
    std::vector<Link> liquid, air;
    std::vector<Vent> vents;
    std::vector<bool> activeWater, activeAir;
    std::vector<Local> local;
    std::vector<Real> q, z, vent;
    std::vector<double> residual;
    std::vector<std::array<double, 4>> airDerivative;
    std::vector<std::array<double, 2>> ventDerivative;
    System(const WaterBulkGeometry &g, const WaterBulkState &s, const WaterGasSettings &c)
        : geometry(g), source(s), settings(c) {}

    bool Evaluate(const std::vector<Real> &x, std::string &error) {
        const auto n = geometry.capacity.size();
        const double rt = settings.referencePressure / settings.referenceDensity;
        local.resize(n);
        std::vector<Real> water(n), mass(n);
        for (std::size_t i = 0; i < n; ++i) {
            auto &l = local[i];
            const Real c(geometry.capacity[i]), k(geometry.potentialSlope[i]);
            l.mh = 0.;
            if (pressureCoordinates[i] && activeWater[i]) {
                l.pressure = sourcePressure[i] + x[n + i];
                if (l.pressure.Value() < 0.)
                    return Fail(error, "negative bulk air-pressure iterate");
                Real b = head[i] + x[i] + Real(settings.referencePressure - geometry.potentialBase[i]) -
                         k * c;
                l.gas = (l.pressure - b) / k;
                if (l.gas.Value() < 0.)
                    return Fail(error, "bulk pressure coordinate crossed its positive-air region");
                l.wh = l.wm = l.ph = 0.;
                l.pm = 1.;
                if ((l.gas - c).Value() >= 0.) {
                    l.gas = c;
                    l.water = Real();
                    l.md = (c / Real(rt) / Real(settings.referenceDensity)).Value();
                } else {
                    l.water = c - l.gas;
                    l.wh = 1. / k.Value();
                    l.wm = -l.wh;
                    l.md =
                        ((l.gas + l.pressure / k) / Real(rt) / Real(settings.referenceDensity)).Value();
                    l.mh =
                        (Real() - l.pressure / k / Real(rt) / Real(settings.referenceDensity)).Value();
                }
                l.mass = l.pressure * l.gas / Real(rt);
                if (source.gasMass[i].Value() > 0. && l.mass.Value() == 0.)
                    return Fail(error, "positive stored air reached a degenerate pressure coordinate");
                water[i] = l.water - source.water[i];
                mass[i] = (l.mass - source.gasMass[i]) / Real(settings.referenceDensity);
                continue;
            }
            bool pressureCoordinate = pressureCoordinates[i];
            Real sourcePressure;
            if (pressureCoordinate) {
                sourcePressure = Real(rt) * source.gasMass[i] / source.gas[i];
                l.mass = source.gasMass[i] + (source.gas[i] / Real(rt)) * x[n + i];
                l.md = (source.gas[i] / Real(rt) / Real(settings.referenceDensity)).Value();
            } else if (source.gasMass[i].Value() > 0.) {
                Real ratio;
                if (!PositiveRatio(x[n + i], ratio))
                    return Fail(error, "positive stored air ratio is unrepresentable");
                l.mass = source.gasMass[i] * ratio;
                if (!(l.mass.Value() > 0. && std::isfinite(l.mass.Value())))
                    return Fail(error, "positive subgrid air outside representable mass range");
                l.md = l.mass.Value() / settings.referenceDensity;
            } else {
                l.mass = Real(settings.referenceDensity) * x[n + i];
                l.md = 1.;
            }
            if (l.mass.Value() < 0. || !std::isfinite(l.mass.Value()))
                return Fail(error, "negative or nonfinite bulk air mass iterate");
            Real b = head[i] + Real(x[i]) +
                     Real(settings.referencePressure - geometry.potentialBase[i]) - k * c;
            l.wh = l.wm = l.ph = l.pm = 0.;
            if (!activeWater[i]) {
                l.water = source.water[i];
                l.gas = source.gas[i];
                l.pressure = l.gas.Value() > 0. ? Real(rt) * l.mass / l.gas : Real();
                if (l.gas.Value() > 0.)
                    l.pm = settings.referencePressure / l.gas.Value();
            } else {
                // Solve the local EOS for positive gas volume directly from
                // independently owned mass. No subtraction of large pressure
                // heads to reconstruct a microscopic air layer is required.
                Real discriminant = Root(b * b + Real(4.) * k * Real(rt) * l.mass);
                if (l.mass.Value() > 0.) {
                    l.gas = b.Value() >= 0. ? Real(2. * rt) * l.mass / (b + discriminant)
                                            : (discriminant - b) / (Real(2.) * k);
                    l.pressure = Real(rt) * l.mass / l.gas;
                } else {
                    l.gas = b.Value() < 0. ? Real() - b / k : Real();
                    l.pressure = b.Value() > 0. ? b : Real();
                }
                if ((l.gas - c).Value() >= 0.) {
                    l.gas = c;
                    l.water = Real();
                    l.pressure = Real(rt) * l.mass / c;
                    l.pm = settings.referencePressure / c.Value();
                } else {
                    l.water = c - l.gas;
                    double d = discriminant.Value();
                    if (!(d > 0. && std::isfinite(d)))
                        return Fail(error, "degenerate bulk vacuum/front tangent");
                    l.wh = l.gas.Value() / d;
                    l.wm = -settings.referencePressure / d;
                    l.ph = l.pressure.Value() / d;
                    l.pm = geometry.potentialSlope[i] * settings.referencePressure / d;
                }
            }
            l.wm *= l.md;
            l.pm *= l.md;
            if (pressureCoordinate) {
                l.pressure = sourcePressure + x[n + i];
                l.pm = 1.;
            }
            water[i] = l.water - source.water[i];
            mass[i] = (l.mass - source.gasMass[i]) / Real(settings.referenceDensity);
        }
        q.resize(liquid.size());
        z.resize(air.size());
        vent.resize(vents.size());
        airDerivative.resize(air.size());
        ventDerivative.resize(vents.size());
        for (std::size_t f = 0; f < liquid.size(); ++f) {
            auto e = liquid[f];
            q[f] =
                Real(predictor[f]) + Real(e.c) * (head[e.l] - head[e.r] + Real(x[e.l]) - Real(x[e.r]));
            water[e.l] = water[e.l] + q[f];
            water[e.r] = water[e.r] - q[f];
        }
        for (std::size_t f = 0; f < air.size(); ++f) {
            auto e = air[f];
            double left = (local[e.l].gas / source.gas[e.l]).Value(),
                   right = (local[e.r].gas / source.gas[e.r]).Value();
            Real difference = local[e.l].pressure - local[e.r].pressure;
            bool forward = difference.Value() >= 0.;
            double factor = std::min(1., forward ? left : right);
            double dl = forward && left < 1. ? e.c * difference.Value() / source.gas[e.l].Value() : 0.;
            double dr =
                !forward && right < 1. ? e.c * difference.Value() / source.gas[e.r].Value() : 0.;
            z[f] = Real(e.c * factor) * difference;
            airDerivative[f] = {e.c * factor * local[e.l].ph - dl * local[e.l].wh,
                                e.c * factor * local[e.l].pm - dl * local[e.l].wm,
                                -e.c * factor * local[e.r].ph - dr * local[e.r].wh,
                                -e.c * factor * local[e.r].pm - dr * local[e.r].wm};
            mass[e.l] = mass[e.l] + z[f];
            mass[e.r] = mass[e.r] - z[f];
        }
        for (std::size_t f = 0; f < vents.size(); ++f) {
            auto e = vents[f];
            double ratio = (local[e.i].gas / source.gas[e.i]).Value();
            Real difference = local[e.i].pressure - Real(settings.referencePressure);
            bool outward = difference.Value() >= 0.;
            double factor = outward ? std::min(1., ratio) : 1.;
            double d = outward && ratio < 1. ? e.c * difference.Value() / source.gas[e.i].Value() : 0.;
            vent[f] = Real(e.c * factor) * difference;
            ventDerivative[f] = {e.c * factor * local[e.i].ph - d * local[e.i].wh,
                                 e.c * factor * local[e.i].pm - d * local[e.i].wm};
            mass[e.i] = mass[e.i] + vent[f];
        }
        residual.resize(2 * n);
        for (std::size_t i = 0; i < n; ++i) {
            residual[i] = water[i].Value() / geometry.capacity[i];
            residual[n + i] = mass[i].Value() / geometry.capacity[i];
            if (!std::isfinite(residual[i]) || !std::isfinite(residual[n + i]))
                return Fail(error, "nonfinite bulk pressure residual");
        }
        return true;
    }
    bool Reparameterize(std::vector<Real> &x, std::string &error) {
        const auto n = geometry.capacity.size();
        for (std::size_t i = 0; i < n; ++i)
            if (activeWater[i]) {
                if (pressureCoordinates[i] && local[i].gas.Value() < geometry.capacity[i] / 32.) {
                    Real coordinate;
                    if (source.gasMass[i].Value() > 0.) {
                        if (!LogRatio(local[i].mass / source.gasMass[i], coordinate))
                            return Fail(error, "positive-air coordinate conversion failed");
                    } else
                        coordinate = local[i].mass / Real(settings.referenceDensity);
                    x[n + i] = coordinate;
                    pressureCoordinates[i] = false;
                } else if (!pressureCoordinates[i] &&
                           local[i].gas.Value() > geometry.capacity[i] / 8.) {
                    x[n + i] = local[i].pressure - sourcePressure[i];
                    pressureCoordinates[i] = true;
                }
            }
        return true;
    }
    double Norm() const {
        double result = 0.;
        for (double r : residual)
            result = std::max(result, std::abs(r));
        return result;
    }
    std::string Worst() const {
        std::size_t row = 0;
        for (std::size_t i = 1; i < residual.size(); ++i)
            if (std::abs(residual[i]) > std::abs(residual[row]))
                row = i;
        auto n = geometry.capacity.size(), i = row % n;
        std::ostringstream report;
        report << std::setprecision(17) << " row=" << (row < n ? "water" : "air") << " cell=" << i
               << " capacity=" << geometry.capacity[i] << " normalized=" << residual[row]
               << " sourceWater=" << source.water[i].Value() << " sourceGas=" << source.gas[i].Value()
               << " candidateWater=" << local[i].water.Value()
               << " candidateGas=" << local[i].gas.Value() << " candidateMass=" << local[i].mass.Value()
               << " candidatePressure=" << local[i].pressure.Value();
        return report.str();
    }
    double Merit() const {
        Real sum;
        for (double r : residual)
            sum = sum + Real(r) * Real(r);
        return sum.Value();
    }
    bool Linear(std::vector<double> &dx, std::size_t &applications, std::string &error) const {
        const auto n = geometry.capacity.size();
        const auto count = 2 * n;
        std::vector<std::size_t> component(n);
        std::iota(component.begin(), component.end(), 0);
        auto root = [&](std::size_t i) {
            while (component[i] != i) {
                component[i] = component[component[i]];
                i = component[i];
            }
            return i;
        };
        for (auto e : liquid)
            if (e.c > 0.) {
                auto a = root(e.l), b = root(e.r);
                if (a != b)
                    component[b] = a;
            }
        std::vector<Real> rowSum(n), capacitySum(n), sourceSum(n), endSum(n);
        std::vector<double> storage(n);
        for (std::size_t i = 0; i < n; ++i) {
            auto c = root(i);
            rowSum[c] = rowSum[c] + Real(residual[i]) * Real(geometry.capacity[i]);
            capacitySum[c] = capacitySum[c] + Real(geometry.capacity[i]);
            sourceSum[c] = sourceSum[c] + source.water[i];
            endSum[c] = endSum[c] + local[i].water;
            storage[c] += std::abs(local[i].wh) + std::abs(local[i].wm);
        }
        for (std::size_t i = 0; i < n; ++i)
            if (storage[i] == 0. && std::abs(rowSum[i].Value()) > 1e-14 * capacitySum[i].Value()) {
                std::ostringstream report;
                report << std::setprecision(17)
                       << "incompatible dry-component linearization component=" << i
                       << " sourceWater=" << sourceSum[i].Value()
                       << " candidateWater=" << endSum[i].Value()
                       << " waterRowSum=" << rowSum[i].Value();
                error = report.str();
                return false;
            }
        std::vector<std::map<std::size_t, Real>> matrix(count);
        auto index = [&](std::size_t i) { return i < n ? 2 * i : 2 * (i - n) + 1; };
        auto add = [&](std::size_t row, std::size_t column, Real value) {
            auto &entry = matrix[index(row)][index(column)];
            entry = entry + value / Real(geometry.capacity[row % n]);
        };
        for (std::size_t i = 0; i < n; ++i) {
            add(i, i, Real(local[i].wh));
            add(i, n + i, Real(local[i].wm));
            add(n + i, i, Real(local[i].mh));
            add(n + i, n + i, Real(local[i].md));
        }
        for (auto e : liquid)
            if (e.c > 0.) {
                add(e.l, e.l, Real(e.c));
                add(e.r, e.r, Real(e.c));
                add(e.l, e.r, Real(-e.c));
                add(e.r, e.l, Real(-e.c));
            }
        auto gasRow = [&](std::size_t row, std::size_t cell, double c) {
            add(n + row, cell, Real(c * local[cell].ph));
            add(n + row, n + cell, Real(c * local[cell].pm));
        };
        // Freeze opening area only in the accelerator. The Krylov operator
        // below differentiates the actual changing aperture and checks J*x.
        for (auto e : air) {
            auto donor = local[e.l].pressure.Value() >= local[e.r].pressure.Value() ? e.l : e.r;
            double factor = std::min(1., (local[donor].gas / source.gas[donor]).Value());
            double c = e.c * factor;
            gasRow(e.l, e.l, c);
            gasRow(e.r, e.r, c);
            gasRow(e.l, e.r, -c);
            gasRow(e.r, e.l, -c);
        }
        for (auto e : vents) {
            double factor = local[e.i].pressure.Value() >= settings.referencePressure
                                ? std::min(1., (local[e.i].gas / source.gas[e.i]).Value())
                                : 1.;
            gasRow(e.i, e.i, e.c * factor);
        }
        for (std::size_t i = 0; i < count; ++i)
            if (matrix[i][i].Value() == 0.)
                matrix[i][i] = Real(1.); // accelerator gauge only
        WaterBulkCondensation preconditioner;
        std::vector<bool> condense(count);
        for (std::size_t i = 0; i < n; ++i)
            if (!activeWater[i])
                condense[index(i)] = true;
        for (std::size_t i = 0; i < n; ++i)
            condense[index(n + i)] = pressureCoordinates[i] && local[i].wh == 0. && local[i].wm == 0.;
        if (!preconditioner.Build(matrix, condense, error))
            return false;
        if (settings.trace)
            std::cerr << "BULK_ACCELERATOR eliminated=" << preconditioner.Eliminated()
                      << " total=" << count << std::endl;
        auto precondition = [&](const std::vector<double> &rhs, std::vector<double> &result) {
            std::vector<double> ordered(count), solved;
            for (std::size_t i = 0; i < count; ++i)
                ordered[index(i)] = rhs[i];
            if (!preconditioner.Apply(ordered, solved, error))
                return false;
            result.resize(count);
            for (std::size_t i = 0; i < count; ++i)
                result[i] = solved[index(i)];
            return true;
        };
        auto multiply = [&](const std::vector<double> &x, std::vector<double> &y) {
            y.assign(count, 0.);
            for (std::size_t i = 0; i < n; ++i) {
                y[i] = local[i].wh * x[i] + local[i].wm * x[n + i];
                y[n + i] = local[i].mh * x[i] + local[i].md * x[n + i];
            }
            for (auto e : liquid) {
                double q = e.c * (x[e.l] - x[e.r]);
                y[e.l] += q;
                y[e.r] -= q;
            }
            for (std::size_t f = 0; f < air.size(); ++f) {
                auto e = air[f];
                auto d = airDerivative[f];
                double q = d[0] * x[e.l] + d[1] * x[n + e.l] + d[2] * x[e.r] + d[3] * x[n + e.r];
                y[n + e.l] += q;
                y[n + e.r] -= q;
            }
            for (std::size_t f = 0; f < vents.size(); ++f) {
                auto e = vents[f];
                auto d = ventDerivative[f];
                y[n + e.i] += d[0] * x[e.i] + d[1] * x[n + e.i];
            }
            for (std::size_t i = 0; i < count; ++i)
                y[i] /= geometry.capacity[i % n];
            return true;
        };
        std::vector<double> rhs = residual;
        for (double &r : rhs)
            r = -r;
        std::vector<Real> solution;
        CutKrylovStats stats;
        bool ok = SolveCutCoupledKrylov(rhs, multiply, precondition, solution, stats, error, 40, 32,
                                        std::min(1e-6, std::max(1e-10, .01 * std::sqrt(Norm()))));
        applications += stats.applications;
        if (!ok) {
            std::ostringstream detail;
            detail << std::setprecision(17) << error
                   << " originalLinearRelativeResidual=" << stats.relativeResidual;
            error = detail.str();
            return false;
        }
        dx.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            dx[i] = solution[i].Value();
            if (i >= n && !activeAir[i - n])
                dx[i] = 0.;
        }
        return true;
    }
};
enum class Admission { Accepted, Refine, Rejected };
Admission PreparePublication(const System &s, const WaterBulkAudit &before, WaterBulkState &candidate,
                             WaterBulkProjectionStats &work, std::string &error) {
    const auto &geometry = s.geometry;
    const auto &source = s.source;
    const auto &settings = s.settings;
    const auto n = geometry.capacity.size(), nf = geometry.field.geometry.faces.size();
    WaterBulkState next = source;
    next.faceInertance = s.inertance;
    std::vector<WaterGasLink> waterLinks, gasLinks;
    std::vector<WaterBulkMassReceipt> receipts;
    for (std::size_t i = 0; i < n; ++i) {
        next.water[i] = s.local[i].water;
        next.gas[i] = s.local[i].gas;
        next.gasMass[i] = s.local[i].mass;
    }
    for (std::size_t f = 0; f < nf; ++f) {
        auto e = s.liquid[f];
        next.faceRate[f] = s.q[f].Value() / settings.dt;
        if (e.c > 0.)
            waterLinks.push_back({e.l, e.r, e.c});
    }
    for (auto e : s.air)
        gasLinks.push_back({e.l, e.r, e.c});
    for (std::size_t f = 0; f < s.vents.size(); ++f) {
        Real m = Real(settings.referenceDensity) * s.vent[f];
        receipts.push_back({s.vents[f].i, m});
        next.externalGasMass = next.externalGasMass + m;
    }
    WaterBulkReconcileStats reconciled;
    if (!ReconcileWaterBulk(geometry, source, waterLinks, gasLinks, receipts, settings, next,
                            reconciled, error))
        return reconciled.needsRefinement ? Admission::Refine : Admission::Rejected;
    std::vector<Real> waterRows(n), airRows(n);
    for (std::size_t i = 0; i < n; ++i) {
        waterRows[i] = next.water[i] - source.water[i];
        airRows[i] = (next.gasMass[i] - source.gasMass[i]) / Real(settings.referenceDensity);
    }
    for (std::size_t f = 0; f < nf; ++f) {
        auto e = s.liquid[f];
        waterRows[e.l] = waterRows[e.l] + s.q[f];
        waterRows[e.r] = waterRows[e.r] - s.q[f];
    }
    for (std::size_t f = 0; f < s.air.size(); ++f) {
        auto e = s.air[f];
        airRows[e.l] = airRows[e.l] + s.z[f];
        airRows[e.r] = airRows[e.r] - s.z[f];
    }
    for (std::size_t f = 0; f < s.vents.size(); ++f)
        airRows[s.vents[f].i] = airRows[s.vents[f].i] + s.vent[f];
    work.maximumVolumeResidual = 0.;
    for (std::size_t i = 0; i < n; ++i)
        work.maximumVolumeResidual =
            std::max({work.maximumVolumeResidual, std::abs(waterRows[i].Value()) / geometry.capacity[i],
                      std::abs(airRows[i].Value()) / geometry.capacity[i]});
    if (work.maximumVolumeResidual > settings.relativeTolerance) {
        error = "reconciled bulk state fails original local material equations";
        return Admission::Refine;
    }
    work.maximumWaterReconciliation = reconciled.maximumWaterCorrection;
    work.maximumMassReconciliation = reconciled.maximumMassCorrection;
    WaterBulkAudit after;
    if (!AuditWaterBulkStorage(geometry, next, settings, after, error))
        return Admission::Rejected;
    work.sourceEnergy = before.potentialEnergy + before.kineticEnergy;
    work.endEnergy = after.potentialEnergy + after.kineticEnergy;
    for (std::size_t f = 0; f < nf; ++f) {
        double delta = next.faceRate[f] - source.faceRate[f];
        work.inertialLoss += .5 * s.inertance[f] * delta * delta;
    }
    if (work.endEnergy > work.sourceEnergy + 1e-10 * std::max(std::abs(work.sourceEnergy), 1.)) {
        error = "bulk cold energy increased; variable-inertia transport is not qualified";
        return Admission::Rejected;
    }
    candidate = std::move(next);
    error.clear();
    return Admission::Accepted;
}
} // namespace
bool ProjectWaterBulk(const WaterBulkGeometry &geometry, const WaterBulkState &source,
                      const WaterGasSettings &settings, WaterBulkState &output,
                      WaterBulkProjectionStats &stats, std::string &error) {
    auto started = std::chrono::steady_clock::now();
    WaterBulkProjectionStats work;
    struct Diagnostic {
        WaterBulkProjectionStats &out;
        WaterBulkProjectionStats &work;
        std::chrono::steady_clock::time_point start;
        ~Diagnostic() {
            work.elapsedMs =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                    .count();
            out = work;
        }
    } diagnostic{stats, work, started};
    WaterBulkAudit before;
    if (!(settings.dt > 0. && std::isfinite(settings.dt)) ||
        !AuditWaterBulkStorage(geometry, source, settings, before, error))
        return false;
    if (!(settings.massTolerance > 0. && std::isfinite(settings.massTolerance) &&
          settings.relativeTolerance > 0. && settings.relativeTolerance < 1.))
        return Fail(error, "invalid bulk material tolerance");
    const auto n = geometry.capacity.size();
    const auto nf = geometry.field.geometry.faces.size();
    System s(geometry, source, settings);
    s.head.resize(n);
    s.sourcePressure.resize(n);
    s.pressureCoordinates.resize(n);
    s.inertance.resize(nf);
    s.predictor.resize(nf);
    s.activeWater.assign(n, false);
    s.activeAir.assign(n, false);
    auto field = geometry.field;
    for (std::size_t i = 0; i < n; ++i) {
        field.water[i] = source.water[i].Value();
        field.gas[i] = source.gas[i].Value();
        field.gasMass[i] = source.gasMass[i].Value();
    }
    std::vector<CutRegionWaterPlane> planes;
    WaterGasGeometry gas;
    if (!ReconstructCutRegionPhasePlanes(field.chart, field.geometry, geometry.upAxis, field.water,
                                         field.gas, {}, planes, error) ||
        !PrepareWaterGasGeometry(field, planes, settings, 1.8e-5, gas, error))
        return false;
    for (std::size_t f = 0; f < nf; ++f) {
        auto face = field.geometry.faces[f];
        auto l = face.left, r = face.right;
        int a = face.axis;
        double length = .5 * (field.chart.regions[l].hi[a] - field.chart.regions[l].lo[a] +
                              field.chart.regions[r].hi[a] - field.chart.regions[r].lo[a]);
        auto phase = MeasureCutRegionFacePhase(face, {}, planes[l], planes[r]);
        double area = .5 * (phase.wet[0].start + phase.wet[1].start);
        double inertance = area > 0. ? geometry.density * length / area : 0.;
        s.inertance[f] = inertance;
        s.liquid.push_back({l, r, inertance > 0. ? settings.dt * settings.dt / inertance : 0.});
        s.predictor[f] = source.faceRate[f] * settings.dt;
        if (inertance > 0.)
            s.activeWater[l] = s.activeWater[r] = true;
    }
    for (auto link : gas.links) {
        auto l = gas.nodeCell[link.left], r = gas.nodeCell[link.right];
        s.air.push_back({l, r, settings.dt * link.massConductance / settings.referenceDensity});
        s.activeAir[l] = s.activeAir[r] = true;
    }
    for (auto vent : gas.vents) {
        auto i = gas.nodeCell[vent.node];
        s.vents.push_back({i, settings.dt * vent.massConductance / settings.referenceDensity});
        s.activeAir[i] = true;
    }
    std::vector<Real> x(2 * n);
    double wetHead = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < n; ++i) {
        Real p = source.gas[i].Value() > 0.
                     ? Real(settings.referencePressure / settings.referenceDensity) *
                           source.gasMass[i] / source.gas[i]
                     : Real();
        s.sourcePressure[i] = p;
        s.pressureCoordinates[i] =
            source.gas[i].Value() > 0. &&
            (!s.activeWater[i] || source.gas[i].Value() >= geometry.capacity[i] / 16.);
        s.head[i] = Real(geometry.potentialBase[i]) +
                    Real(geometry.potentialSlope[i]) * (Real(geometry.capacity[i]) - source.gas[i]) +
                    p - Real(settings.referencePressure);
        if (source.water[i].Value() > 0.)
            wetHead = std::max(wetHead, geometry.potentialBase[i] +
                                            geometry.potentialSlope[i] * source.water[i].Value());
    }
    for (std::size_t i = 0; i < n; ++i)
        if (source.gas[i].Value() == 0. && source.water[i].Value() > 0.)
            x[i] = Real(wetHead) - s.head[i];
    for (std::size_t iteration = 0; iteration < 48; ++iteration) {
        if (!s.Evaluate(x, error))
            return false;
        double norm = s.Norm();
        work.maximumVolumeResidual = norm;
        if (settings.trace)
            std::cerr << "BULK_MASS_ITER iteration=" << iteration << " residual=" << norm
                      << " linear=" << work.linearApplications << s.Worst() << std::endl;
        if (norm <= settings.relativeTolerance) {
            WaterBulkState candidate;
            auto admission = PreparePublication(s, before, candidate, work, error);
            if (admission == Admission::Accepted) {
                output = std::move(candidate);
                return true;
            }
            if (admission == Admission::Rejected)
                return false;
            if (settings.trace)
                std::cerr << "BULK_ADMISSION_REFINE " << error << std::endl;
            error.clear();
        }
        std::vector<double> dx;
        if (!s.Linear(dx, work.linearApplications, error)) {
            error += s.Worst();
            return false;
        }
        auto previous = x;
        double merit = s.Merit();
        bool accepted = false;
        for (int j = 0; j < 48; ++j) {
            double fraction = std::ldexp(1., -j);
            for (std::size_t i = 0; i < x.size(); ++i)
                x[i] = previous[i] + Real(fraction) * Real(dx[i]);
            std::string trial;
            if (s.Evaluate(x, trial) && s.Merit() < merit * (1. - 1e-4 * fraction)) {
                accepted = true;
                break;
            }
        }
        if (!accepted) {
            error = "positive-mass bulk line search stagnated" + s.Worst();
            return false;
        }
        if (!s.Reparameterize(x, error))
            return false;
        ++work.newtonIterations;
        if (iteration == 47)
            return Fail(error, "bulk mass/head nonlinear budget exhausted");
    }
    return Fail(error, "bulk nonlinear admission budget exhausted");
}
} // namespace judas::volume_grid
