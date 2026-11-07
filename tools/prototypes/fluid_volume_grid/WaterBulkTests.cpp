#include "WaterBulkProjection.h"
#include "WaterBulkReconcile.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

using namespace judas::volume_grid;
namespace {
using Real = CutCompensated;
int checks = 0, failures = 0;
void Check(bool ok, const std::string &why) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << why << '\n';
    }
}
CutSpatialField Columns(double a, double b, double width = 1.) {
    CutSpatialField f;
    f.chart.layout = {{2, 1, 1}, 1.};
    f.parent = {0, 1};
    f.chart.regions = {{{0., 0., 0.}, {1., 1., width}, 0}, {{1., 0., 0.}, {2., 1., width}, 1}};
    f.geometry.startCapacity = f.geometry.endCapacity = {width, width};
    f.geometry.faces = {{0, 1, 0, {1., 0., 0.}, {1., 1., width}, width, width, width, 0., 1.}};
    f.water = {a * width, b * width};
    f.gas = {(1. - a) * width, (1. - b) * width};
    f.gasMass = {1.2 * f.gas[0], 1.2 * f.gas[1]};
    f.flux = {0.};
    return f;
}
bool Same(Real a, Real b) {
    return a.hi == b.hi && a.lo == b.lo;
}
bool Same(const WaterBulkState &a, const WaterBulkState &b) {
    if (a.geometryStamp != b.geometryStamp || a.faceRate != b.faceRate ||
        a.faceInertance != b.faceInertance || !Same(a.externalGasMass, b.externalGasMass))
        return false;
    auto sameVector = [](const std::vector<Real> &x, const std::vector<Real> &y) {
        if (x.size() != y.size())
            return false;
        for (std::size_t i = 0; i < x.size(); ++i)
            if (!Same(x[i], y[i]))
                return false;
        return true;
    };
    if (!sameVector(a.water, b.water) || !sameVector(a.gas, b.gas) || !sameVector(a.gasMass, b.gasMass))
        return false;
    return true;
}
void Flow(int upAxis) {
    auto f = Columns(.6, .4);
    GridVec gravity{};
    gravity[upAxis] = -9.81;
    if (upAxis != 1) {
        for (auto &region : f.chart.regions) {
            std::swap(region.lo[1], region.lo[upAxis]);
            std::swap(region.hi[1], region.hi[upAxis]);
        }
        auto &face = f.geometry.faces[0];
        std::swap(face.lo[1], face.lo[upAxis]);
        std::swap(face.hi[1], face.hi[upAxis]);
        if (face.axis == upAxis)
            face.axis = 1;
    }
    WaterBulkGeometry g;
    WaterBulkState state;
    WaterGasSettings settings;
    std::string error;
    if (!PrepareWaterBulkStorage(f, 1000., gravity, g, state, error)) {
        Check(false, "flow import: " + error);
        return;
    }
    WaterBulkAudit initial;
    if (!AuditWaterBulkStorage(g, state, settings, initial, error)) {
        Check(false, error);
        return;
    }
    bool complete = true, quantity = true, passive = true, bounded = true, deterministic = true;
    double previous = initial.potentialEnergy + initial.kineticEnergy;
    std::vector<double> elapsed;
    for (int step = 0; step < 240; ++step) {
        WaterBulkState next;
        WaterBulkProjectionStats stats;
        if (!ProjectWaterBulk(g, state, settings, next, stats, error)) {
            std::cerr << "flow axis=" << upAxis << " step=" << step << " " << error << '\n';
            complete = false;
            break;
        }
        if (step == 0) {
            WaterBulkState repeat;
            WaterBulkProjectionStats again;
            deterministic =
                ProjectWaterBulk(g, state, settings, repeat, again, error) && Same(next, repeat);
        }
        WaterBulkAudit audit;
        if (!AuditWaterBulkStorage(g, next, settings, audit, error)) {
            complete = false;
            break;
        }
        quantity &=
            std::abs(audit.water - initial.water) < 1e-12 && std::abs(audit.air - initial.air) < 1e-12;
        double energy = audit.potentialEnergy + audit.kineticEnergy;
        passive &= energy <= previous + 1e-8 * std::max(std::abs(previous), 1.);
        previous = energy;
        bounded &= next.water.size() == 2 && next.gas.size() == 2 && next.gasMass.size() == 2 &&
                   next.faceRate.size() == 1;
        elapsed.push_back(stats.elapsedMs);
        state = std::move(next);
    }
    Check(complete, "240 complete intervals with rotated chart axis " + std::to_string(upAxis));
    Check(complete && quantity, "independent cold water and finite-air totals");
    Check(complete && passive, "adjacent-step total energy cannot grow");
    Check(complete && bounded, "storage and pressure face counts cannot grow with interface history");
    Check(complete && deterministic, "identical source produces identical physical state");
    Check(complete && state.water[0].Value() < .6, "surface head drives real water transfer");
    if (complete) {
        std::sort(elapsed.begin(), elapsed.end());
        std::cout << "FLOW axis=" << upAxis << " finalLeft=" << state.water[0].Value()
                  << " energy=" << previous << " medianMs=" << elapsed[120] << " p95Ms=" << elapsed[227]
                  << " worstMs=" << elapsed.back() << '\n';
    }
}
void Gas(double width) {
    auto f = Columns(0., 0., width);
    f.gasMass[0] *= 2.;
    WaterBulkGeometry g;
    WaterBulkState source, next;
    WaterGasSettings settings;
    std::string error;
    if (!PrepareWaterBulkStorage(f, 1000., {0., -9.81, 0.}, g, source, error)) {
        Check(false, error);
        return;
    }
    WaterBulkProjectionStats stats;
    bool ok = ProjectWaterBulk(g, source, settings, next, stats, error);
    Check(ok, "pure finite gas flow: " + error);
    if (!ok)
        return;
    double k = 0.;
    Check(RectangularGasMassConductance(1., width, 1., settings.referenceDensity,
                                        settings.referencePressure, 1.8e-5, k, error),
          "analytic rectangular pipe");
    double dimensionless =
        settings.dt * k * settings.referencePressure / (settings.referenceDensity * width);
    double sum = 3.6 * width, difference = 1.2 * width / (1. + 2. * dimensionless);
    Check(std::abs((next.gasMass[0] - Real(.5 * (sum + difference))).Value()) < 1e-12 * width,
          "independent two-pocket backward Euler known answer");
    Check((next.gasMass[0] - next.gasMass[1]).Value() > 0.,
          "finite openings do not instantaneously equalize air owners");
    Real total = next.gasMass[0] + next.gasMass[1];
    Check(std::abs((total - source.gasMass[0] - source.gasMass[1]).Value()) <
              1e-28 * std::max(1., width),
          "cold paired air ledger");
    Check(next.water[0].Value() == 0. && next.water[1].Value() == 0., "gas transport creates no water");
}
void Storage() {
    auto f = Columns(1., 0.);
    f.geometry.faces.clear();
    f.flux.clear();
    f.gas[0] = 1e-24;
    f.gasMass[0] = 1.2e-24;
    f.water[1] = 1e-24;
    WaterBulkGeometry g;
    WaterBulkState source, next;
    WaterGasSettings settings;
    std::string error;
    Check(PrepareWaterBulkStorage(f, 1000., {0., -9.81, 0.}, g, source, error),
          "import independently owned subgrid phases: " + error);
    WaterBulkProjectionStats stats;
    Check(ProjectWaterBulk(g, source, settings, next, stats, error),
          "sealed subgrid state survives a pressure interval: " + error);
    Check(!next.gas.empty() && Same(source.gas[0], next.gas[0]) &&
              Same(source.gasMass[0], next.gasMass[0]),
          "sub-coordinate-ULP sealed air and its mass cannot disappear");
    Check(!next.water.empty() && Same(source.water[1], next.water[1]),
          "isolated subgrid water quantity cannot disappear");
    auto saved = next;
    settings.dt = 0.;
    Check(!ProjectWaterBulk(g, source, settings, next, stats, error) && Same(next, saved),
          "failed pressure publication is atomic");
    settings.dt = 1. / 60.;
    f.gasMass[0] = -1.;
    auto old = source;
    Check(!PrepareWaterBulkStorage(f, 1000., {0., -9.81, 0.}, g, source, error) && Same(source, old),
          "invalid import leaves published quantities intact");
}
void Reconcile() {
    auto f = Columns(.5, .5);
    WaterBulkGeometry g;
    WaterBulkState source;
    std::string error;
    WaterGasSettings settings;
    Check(PrepareWaterBulkStorage(f, 1000., {0., -9.81, 0.}, g, source, error),
          "reconciliation fixture");
    auto next = source;
    next.water[0] = next.water[0] + Real(1e-15);
    next.gas[0] = next.gas[0] - Real(1e-15);
    WaterBulkReconcileStats stats;
    std::vector<WaterGasLink> links{{0, 1, 1.}};
    Check(ReconcileWaterBulk(g, source, links, links, {}, settings, next, stats, error),
          "conservative solver-defect reconciliation");
    Check(std::abs((next.water[0] + next.water[1] - Real(1.)).Value()) < 1e-30,
          "reconciliation restores quantity without clipping a phase");
    next = source;
    next.water[0] = next.water[0] + Real(1e-5);
    next.gas[0] = next.gas[0] - Real(1e-5);
    auto saved = next;
    Check(!ReconcileWaterBulk(g, source, links, links, {}, settings, next, stats, error) &&
              Same(next, saved),
          "a real physical defect cannot be hidden by reconciliation");
    next = source;
    next.water[0] = next.water[0] + Real(1e-15);
    next.gas[0] = next.gas[0] - Real(1e-15);
    saved = next;
    Check(ReconcileWaterBulk(g, source, {}, links, {}, settings, next, stats, error) &&
              Same(next.water[0], source.water[0]) && Same(next.water[1], source.water[1]),
          "disconnected water controls retain their own independent source quantities");
}
void RetreatedSupport() {
    auto f = Columns(0., 0.);
    WaterBulkGeometry geometry;
    WaterBulkState source, output;
    WaterGasSettings settings;
    std::string error;
    if (!PrepareWaterBulkStorage(f, 1000., {0., -9.81, 0.}, geometry, source, error)) {
        Check(false, "retreated-support fixture: " + error);
        return;
    }
    // A preceding wet interval can retain a face momentum after its material
    // leaves. This pressure-only stage has no dual-volume transport capable of
    // relocating that momentum. It must reject, rather than reset the rate.
    source.faceRate[0] = .001;
    source.faceInertance[0] = 1000.;
    WaterBulkAudit audit;
    Check(AuditWaterBulkStorage(geometry, source, settings, audit, error),
          "stored preceding-interval momentum remains independently auditable");
    output = source;
    const auto saved = output;
    WaterBulkProjectionStats stats;
    Check(!ProjectWaterBulk(geometry, source, settings, output, stats, error) && !error.empty(),
          "unsupported dry-face momentum cannot silently disappear");
    Check(Same(output, saved) && Same(source, saved),
          "failed support transition preserves both source and published state");
}
} // namespace
int main() {
    Flow(1);
    Flow(0);
    Flow(2);
    Gas(1.);
    Gas(.01);
    Gas(.001);
    Storage();
    Reconcile();
    RetreatedSupport();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
