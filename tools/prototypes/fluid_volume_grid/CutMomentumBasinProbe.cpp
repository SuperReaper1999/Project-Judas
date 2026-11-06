#include "BasinFixture.h"
#include "CutLocalPressure.h"
#include "CutMassMomentum.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
namespace {
using Real = CutCompensated;

// Retained numerical format only. This is not the native game-save format.
bool Load(const std::string& path, CutLocalPressureResult& state) {
    std::ifstream file(path, std::ios::binary);
    auto scalar = [&](auto& x) { file.read(reinterpret_cast<char*>(&x), sizeof(x)); };
    scalar(state.dt); scalar(state.waterDensity); scalar(state.gasDensity);
    scalar(state.soundSpeed); scalar(state.frozenGeometryStamp);
    for (auto* values : {&state.pressure, &state.water, &state.gasMass, &state.flux, &state.portFlux}) {
        std::uint64_t count = 0;
        scalar(count);
        if (count > 1'000'000) return false;
        values->resize(count);
        file.read(reinterpret_cast<char*>(values->data()), values->size() * sizeof(Real));
    }
    return bool(file);
}

bool BuildFixture(bool rotated, CutSpatialField& field, std::string& error) {
    const auto fixture = Fixture::Authored(rotated);
    CutRegionChart chart;
    CutRegionGeometry geometry;
    if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(), fixture.InitialBox(),
        {-.005,-.015,-.005}, {.005,.025,.005}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) return false;
    CutDynamicState source;
    SeedImport imported;
    if (!WaterByRegion(fixture, chart, geometry, rotated, source.water, imported, error)) return false;
    std::vector<double> gas;
    for (std::size_t i = 0; i < source.water.size(); ++i) {
        gas.push_back(geometry.startCapacity[i] - source.water[i]);
        source.gasMass.push_back(1.2 * gas.back());
    }
    CutConnectedOwners owners;
    if (!BuildCutConnectedOwners(chart, geometry, .125, owners, error)) return false;
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionPhasePlanes(chart, geometry, 1, source.water, gas, {}, planes, error)) return false;
    auto vent = FullCutRegionGasVent(chart, 1, 1);
    std::vector<CutPhaseBoundary> ports;
    for (std::size_t i = 0; i < chart.regions.size(); ++i)
        if (MeasureCutRegionGasVent(chart.regions[i], geometry.motion, vent, planes[i]).mean > 0.)
            ports.push_back({i,1,1,0.,0.,true});
    return BuildCutSpatialField(chart, geometry, owners, source,
        std::vector<double>(geometry.faces.size()), ports, field, error, false);
}

bool Planes(const CutSpatialField& field, const CutLocalPressureResult& state,
    std::vector<CutRegionWaterPlane>& planes, std::string& error) {
    const auto n = state.water.size();
    if (n != field.parent.size() || state.gasMass.size() != n || state.pressure.size() != n ||
        state.flux.size() != field.flux.size() || state.portFlux.size() != field.boundaries.size() ||
        !(state.dt > 0. && std::isfinite(state.dt))) { error = "diagnostic state dimensions mismatch"; return false; }
    std::vector<double> water(n), gas(n);
    for (std::size_t i = 0; i < n; ++i) {
        Real rho = Real(state.gasDensity) + state.pressure[i] / Real(state.soundSpeed * state.soundSpeed);
        if (!(rho.Value() > 0.)) { error = "diagnostic density outside EOS"; return false; }
        Real g = state.gasMass[i] / rho, capacity = field.geometry.startCapacity[i];
        if (state.water[i].Value() <= g.Value()) {
            water[i] = state.water[i].Value(); gas[i] = (capacity - state.water[i]).Value();
        } else {
            gas[i] = g.Value(); water[i] = (capacity - g).Value();
        }
    }
    return ReconstructCutRegionPhasePlanes(field.chart, field.geometry, 1, water, gas, {}, planes, error);
}

std::array<double,2> Column(const CutSpatialField& field, const CutRegionFace& face,
    const std::vector<CutRegionWaterPlane>& planes, bool left) {
    const auto region = left ? face.left : face.right;
    return MeasureCutRegionFaceInertiaVolumes(face, field.geometry.motion,
        field.chart.regions[region], left, planes[region]);
}

double MassMetric(const CutSpatialField& field, const CutRegionFace& face,
    const std::vector<CutRegionWaterPlane>& planes) {
    const auto l = Column(field, face, planes, true), r = Column(field, face, planes, false);
    // Exactly the current pressure metric, including its reference gas density.
    return 1000. * (l[0] + r[0]) + 1.2 * (l[1] + r[1]);
}

void AuditDualOwnership(const CutSpatialField& field, const std::vector<CutRegionWaterPlane>& planes,
    bool rotated) {
    const auto n = field.parent.size();
    std::vector<std::array<int,6>> faces(n);
    std::vector<std::array<double,6>> columns(n);
    for (const auto& face : field.geometry.faces) if (face.meanArea > 0.) {
        const auto l = Column(field, face, planes, true), r = Column(field, face, planes, false);
        ++faces[face.left][2*face.axis+1]; ++faces[face.right][2*face.axis];
        columns[face.left][2*face.axis+1] += l[0] + l[1];
        columns[face.right][2*face.axis] += r[0] + r[1];
    }
    std::size_t multiple = 0, partial = 0, overlap = 0;
    for (std::size_t i = 0; i < n; ++i) for (int axis = 0; axis < 3; ++axis) for (int side = 0; side < 2; ++side) {
        auto region = field.chart.regions[i];
        auto solid = field.geometry.motion;
        const double anchor = region.lo[axis], width = region.hi[axis]-anchor;
        region.lo[axis] = 0.; region.hi[axis] = width;
        if (solid) { solid->lo[axis] -= anchor; solid->hi[axis] -= anchor; }
        auto lo = region.lo, hi = region.hi;
        // The pressure columns also use face-local coordinates. Adding half
        // a nanometre-scale width to a metre coordinate would corrupt this
        // independent coverage audit before it even measures any geometry.
        const double mid = .5 * width;
        (side ? lo : hi)[axis] = mid;
        const double capacity = CutRegionClippedVolume(region, solid, lo, hi);
        const int slot = 2*axis+side;
        multiple += faces[i][slot] > 1;
        partial += faces[i][slot] > 0 && columns[i][slot] < capacity * (1.-1e-12);
        overlap += columns[i][slot] > capacity * (1.+1e-12);
    }
    std::cout << "DUAL_OWNERSHIP rotated=" << rotated << " multipleOpenFaces=" << multiple
        << " partialConnectedCoverage=" << partial << " overlappingCoverage=" << overlap << '\n';
}

bool MeasureKineticStep(const CutSpatialField& field, const CutLocalPressureResult& previous,
    const CutLocalPressureResult& state, bool rotated, int step, std::string& error) {
    std::vector<CutRegionWaterPlane> startPlanes, endPlanes;
    if (!Planes(field, previous, startPlanes, error) || !Planes(field, state, endPlanes, error)) return false;
    if (step == 1) AuditDualOwnership(field, startPlanes, rotated);
    long double sourceK = 0., fixedMetricK = 0., endK = 0., kickWork = 0., metricWork = 0.;
    double maxSpeed = 0., maxLawError = 0., maxRatio = 0.;
    double fastStartMass = 0., fastEndMass = 0., fastOldSpeed = 0., fastPressure = 0.;
    std::size_t fastest = 0;
    for (std::size_t f = 0; f < field.geometry.faces.size(); ++f) {
        const auto& face = field.geometry.faces[f];
        if (face.meanArea == 0.) continue;
        const double m0 = MassMetric(field, face, startPlanes), m1 = MassMetric(field, face, endPlanes);
        const double u0 = previous.flux[f].Value() / (previous.dt * face.meanArea);
        const double u1 = state.flux[f].Value() / (state.dt * face.meanArea);
        const double jump = (state.pressure[face.left] - state.pressure[face.right]).Value();
        const double push = state.dt * face.meanArea * jump +
            m0 * state.dt * (face.axis == 1 ? -double(gravity) : 0.);
        const double expected = u0 + push / m0;
        maxLawError = std::max(maxLawError, std::abs(expected-u1) / std::max(1.,std::abs(u1)));
        sourceK += .5L*m0*u0*u0; fixedMetricK += .5L*m0*u1*u1; endK += .5L*m1*u1*u1;
        kickWork += push*.5L*(u0+u1); metricWork += .5L*(m1-m0)*u1*u1;
        maxRatio = std::max(maxRatio, m1/m0);
        if (std::abs(u1) > maxSpeed) {
            maxSpeed = std::abs(u1); fastest = f; fastStartMass = m0; fastEndMass = m1;
            fastOldSpeed = u0; fastPressure = jump;
        }
    }
    std::cout << "METRIC rotated=" << rotated << " step=" << step << " sourceK=" << double(sourceK)
        << " pressureMetricK=" << double(fixedMetricK) << " endpointK=" << double(endK)
        << " pressureGravityWork=" << double(kickWork) << " massMetricWork=" << double(metricWork)
        << " fixedMetricWorkError=" << double(fixedMetricK-sourceK-kickWork)
        << " maxVelocityLawRelativeError=" << maxLawError << " maxMassRatio=" << maxRatio << '\n';
    std::cout << "FASTEST rotated=" << rotated << " step=" << step << " face=" << fastest
        << " speed=" << maxSpeed << " oldSpeed=" << fastOldSpeed << " sourceMass=" << fastStartMass
        << " endpointMass=" << fastEndMass << " pressureJump=" << fastPressure << std::endl;
    return maxLawError <= 1e-9;
}

bool CheckMaterialCarrier(const CutSpatialField& field, const CutLocalPressureResult& previous,
    const CutLocalPressureResult& state, bool rotated, int step, CutMomentumBackend backend,
    bool patterned, const std::string& dumpPrefix, std::string& error) {
    std::vector<CutTransportFace> faces;
    for (std::size_t f = 0; f < field.flux.size(); ++f)
        faces.push_back({field.geometry.faces[f].left, field.geometry.faces[f].right, state.flux[f]});
    std::vector<CutTransportGasPort> ports;
    for (std::size_t j = 0; j < field.boundaries.size(); ++j)
        ports.push_back({field.boundaries[j].fragment, state.portFlux[j]});
    CutSelectiveTransport material;
    if (!material.Build(field.geometry.startCapacity, faces, ports, previous.water, previous.gasMass,
        1.2, 1e-18, error)) return false;
    std::vector<Real> massFlux, m0, m1;
    if (!material.InteriorMassFlux(1000., massFlux, error)) return false;
    for (std::size_t i = 0; i < field.parent.size(); ++i) {
        m0.push_back(Real(1000.)*previous.water[i]+previous.gasMass[i]);
        m1.push_back(Real(1000.)*material.Water()[i]+material.Mass()[i]);
    }
    for (std::size_t f = 0; f < faces.size(); ++f) faces[f].volume = massFlux[f];
    const CutMomentumVector uniform{Real(2.), Real(-.5), Real(1.)};
    std::vector<CutMomentumVector> velocity(m0.size(), uniform), impulse(m0.size());
    if (patterned) for (std::size_t i = 0; i < velocity.size(); ++i)
        velocity[i] = {Real(.2*double(i%7)), Real(1.-.3*double(i%5)), Real(.1*double(i%3))};
    std::vector<CutMomentumBoundary> boundaries;
    const auto external = material.GasPortMass();
    for (std::size_t j = 0; j < field.boundaries.size(); ++j)
        boundaries.push_back({field.boundaries[j].fragment, external[j], patterned ? CutMomentumVector{} : uniform});
    CutMassMomentumResult result;
    const auto started = std::chrono::steady_clock::now();
    const bool ok = TransportCutMassMomentum(field.geometry.startCapacity, m0, m1, faces, boundaries,
        velocity, impulse, 1e-11, 1e-12, result, error, backend);
    double maxVelocityError = 0.;
    if (ok && !patterned) for (const auto& v : result.velocity) for (int k = 0; k < 3; ++k)
        maxVelocityError = std::max(maxVelocityError, std::abs((v[k]-uniform[k]).Value()));
    if (ok && !dumpPrefix.empty()) {
        const auto path = dumpPrefix+"-"+std::to_string(rotated)+"-"+std::to_string(step)+"-"+std::to_string(patterned)+".bin";
        std::ofstream file(path,std::ios::binary);
        file.write(reinterpret_cast<const char*>(result.velocity.data()),result.velocity.size()*sizeof(CutMomentumVector));
        if (!file) { error="momentum diagnostic dump failed"; return false; }
    }
    std::cout << "MOMENTUM rotated=" << rotated << " step=" << step << " patterned=" << patterned << " admitted=" << ok
        << " ms=" << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
        << " uniformVelocityError=" << maxVelocityError << " massResidual=" << result.maximumMassResidual
        << " momentumXResidual=" << result.momentumResidual[0].Value()
        << " energyBefore=" << result.beforeEnergy << " energyAfter=" << result.afterEnergy
        << " externalEnergy=" << result.externalEnergy << " mixingLoss=" << result.mixingLoss
        << " unsteadyLoss=" << result.unsteadyLoss
        << " localMomentumResidual=" << result.maximumLocalMomentumResidual
        << " energyResidual=" << result.energyResidual << " massDefectWork=" << result.massDefectWork
        << " reason=" << error << std::endl;
    return ok && maxVelocityError <= 1e-10 &&
        (!patterned || result.afterEnergy+result.externalEnergy <= result.beforeEnergy+1e-10);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 5) {
        std::cerr << "Usage: CutMomentumBasinProbe STATE_PREFIX [INTERVALS] [--fast|--reference] [DUMP_PREFIX]\n";
        return 2;
    }
    std::cout << std::setprecision(17);
    int failures = 0;
    int steps = 3;
    if (argc > 2) {
        std::size_t consumed = 0;
        try { steps = std::stoi(argv[2], &consumed); }
        catch (const std::exception&) { std::cerr << "interval count must be a positive integer\n"; return 2; }
        if (steps < 1 || consumed != std::string(argv[2]).size()) {
            std::cerr << "interval count must be a positive integer\n"; return 2;
        }
    }
    if (argc > 3 && std::string(argv[3]) != "--fast" && std::string(argv[3]) != "--reference") {
        std::cerr << "momentum backend must be --fast or --reference\n"; return 2;
    }
    const auto backend = argc > 3 && std::string(argv[3]) == "--reference"
        ? CutMomentumBackend::Reference : CutMomentumBackend::Fast;
    const std::string dumpPrefix=argc>4?argv[4]:"";
    for (bool rotated : {false,true}) {
        CutSpatialField field;
        std::string error;
        if (!BuildFixture(rotated, field, error)) { std::cerr << error; return 2; }
        CutLocalPressureResult previous;
        previous.dt = double(tick); previous.gasDensity = 1.2;
        previous.waterDensity = 1000.; previous.soundSpeed = CutDynamicSettings{}.soundSpeed;
        previous.water.assign(field.water.begin(), field.water.end());
        previous.gasMass.assign(field.gasMass.begin(), field.gasMass.end());
        previous.pressure.resize(field.parent.size()); previous.flux.resize(field.flux.size());
        previous.portFlux.resize(field.boundaries.size());
        for (int step = 1; step <= steps; ++step) {
            CutLocalPressureResult state;
            const auto file = std::string(argv[1])+"-"+std::to_string(rotated)+"-"+std::to_string(step)+".bin";
            if (!Load(file, state)) { std::cerr << "state unavailable: " << file; return 2; }
            if (!MeasureKineticStep(field, previous, state, rotated, step, error)) {
                ++failures; std::cerr << error << '\n';
            }
            for (bool patterned : {false,true})
                if (!CheckMaterialCarrier(field, previous, state, rotated, step, backend, patterned, dumpPrefix, error)) {
                    ++failures; std::cerr << error << '\n';
                }
            previous = std::move(state);
        }
    }
    return failures ? 1 : 0;
}
