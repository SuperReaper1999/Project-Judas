#include "LocalRectCutGeometry.h"

// Reuse the isolated pressure graph for this experiment. The network has
// nonuniform cells; no engine target or prototype public API is changed.
#include "MovingVolumeGrid.cpp"
#include "GridHydrostaticReaction.h"
#include "RectCutCondense.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace {

using namespace judas::volume_grid;

struct Sample {
    GridVec force{};
    double closure = 0., patchClosure = 0., workError = 0., fluxClosure = 0.;
    double throatArea = 0., throatFlux = 0., capFlux = 0.;
    std::size_t cells = 0, links = 0, retained = 0;
    int iterations = 0;
};

struct ImportedState {
    MovingVolumeState fluid;
    GridCellPhase phase;
    GridVec wallVelocity;
    double referenceHeight;
    double gravity;
};

double SampleCoarseFace(const GridLayout& chart, const ImportedState& old,
                        const rect_cut::LocalConnection& face) {
    std::array<int, 3> index{};
    for (int d = 0; d < 3; ++d) {
        const double middle = .5 * (face.lower[d] + face.upper[d]);
        index[d] = std::clamp(int(std::floor(middle / chart.h)), 0, chart.cells[d] - 1);
    }
    const auto a = chart.Face(face.axis, index[0], index[1], index[2]);
    const double weight = std::clamp(face.lower[face.axis] / chart.h - index[face.axis], 0., 1.);
    ++index[face.axis];
    const auto b = chart.Face(face.axis, index[0], index[1], index[2]);
    const auto& velocity = old.fluid.velocity[face.axis];
    return velocity[a] + weight * (velocity[b] - velocity[a]);
}

double WetArea(const BoxMotion& body, const rect_cut::LocalConnection& face, double plane, bool left) {
    using moving_geometry_detail::AverageBlockedFace;
    auto upper = face.upper;
    if (face.axis == 1) {
        if (left ? plane < face.lower[1] : plane <= face.lower[1])
            return 0.;
        return face.meanArea;
    }
    upper[1] = std::clamp(plane, face.lower[1], upper[1]);
    const int tangent = face.axis == 0 ? 2 : 0;
    const double area = (upper[1] - face.lower[1]) * (upper[tangent] - face.lower[tangent]);
    return std::clamp(area - AverageBlockedFace(body, face.axis, face.lower, upper), 0., face.meanArea);
}

bool Solve(const GridLayout& chart, const rect_cut::LocalChart& frozenGrid, const BoxMotion& body, const ImportedState& old, double dt, Sample& out,
           std::string& error) {
    rect_cut::LocalGeometry geometry;
    if (!rect_cut::BuildLocalGeometry(frozenGrid, body, geometry)) {
        error = "invalid rectilinear cut";
        return false;
    }
    Sample result;
    result.cells = frozenGrid.volumes.size();
    std::vector<double> rhs(frozenGrid.volumes.size()), solidDiv(frozenGrid.volumes.size()), patchSweep(frozenGrid.volumes.size());
    std::vector<double> pressure(frozenGrid.volumes.size()), fluxDiv(frozenGrid.volumes.size());
    std::vector<FaceFlux> graph;
    std::vector<const rect_cut::LocalConnection*> physical;
    for (const auto& face : geometry.links) {
        solidDiv[face.left] += face.sweptVolume;
        solidDiv[face.right] -= face.sweptVolume;
        if (face.meanArea == 0.)
            continue;
        const double length = face.length;
        const auto& low = frozenGrid.volumes[face.left];
        const auto& high = frozenGrid.volumes[face.right];
        const double planeLow = old.phase.waterPlane[low.parent];
        const double planeHigh = old.phase.waterPlane[high.parent];
        const double mid = face.lower[1];
        double wetLength = 0., gravityIncrement = 0.;
        if (face.axis == 1) {
            const double lo = .5 * (low.lo[1] + low.hi[1]);
            const double hi = .5 * (high.lo[1] + high.hi[1]);
            wetLength = LiquidLinkLength(lo, mid, planeLow) + LiquidLinkLength(mid, hi, planeHigh);
            const double referenceWet = LiquidLinkLength(lo, hi, old.referenceHeight);
            gravityIncrement = dt * old.gravity * 999. * (wetLength - referenceWet);
        } else {
            wetLength = .5 * length * (WetArea(body, face, planeLow, true) +
                         WetArea(body, face, planeHigh, false)) / face.meanArea;
        }
        const double inertia = length + 999. * wetLength;
        const double mass = face.meanArea * inertia;
        const double conductance = (dt * face.meanArea) * (dt * face.meanArea) / mass;
        if (!(conductance > 0.) || !std::isfinite(conductance)) {
            error = "invalid cut connection pressure inertia";
            return false;
        }
        const double oldSpeed = SampleCoarseFace(chart, old, face);
        const double oldShare = std::min(1., face.startArea / face.meanArea);
        const double predictor = oldShare * oldSpeed + (1. - oldShare) * old.wallVelocity[face.axis] +
            gravityIncrement / inertia;
        graph.push_back({face.left, face.right, graph.size(), face.axis, face.meanArea, mass,
                         conductance, predictor, face.lower[face.axis]});
        physical.push_back(&face);
    }
    result.links = graph.size();
    for (const auto& patch : geometry.patches)
        patchSweep[patch.cell] -= patch.sign * patch.area * body.translation[patch.axis];
    double maxSource = 0.;
    for (std::size_t cell = 0; cell < frozenGrid.volumes.size(); ++cell) {
        rhs[cell] = geometry.startCapacity[cell] - geometry.endCapacity[cell];
        maxSource = std::max(maxSource, std::abs(rhs[cell]));
        result.closure = std::max(result.closure, std::abs(rhs[cell] + solidDiv[cell]));
        result.patchClosure = std::max(result.patchClosure, std::abs(rhs[cell] + patchSweep[cell]));
    }
    for (const auto& edge : graph) {
        const double q = dt * edge.aperture * edge.predictor;
        rhs[edge.left] -= q;
        rhs[edge.right] += q;
        maxSource = std::max({maxSource, std::abs(rhs[edge.left]), std::abs(rhs[edge.right])});
    }
    const double cellVolume = chart.h * chart.h * chart.h;
    std::vector<bool> thin(frozenGrid.volumes.size());
    for (std::size_t i = 0; i < thin.size(); ++i)
        thin[i] = frozenGrid.volumes[i].hi[0] - frozenGrid.volumes[i].lo[0] <= 2e-6;
    std::vector<PressureValue> values;
    if (!SolveCondensed(graph, rhs, thin, maxSource * 1e-12, cellVolume, values,
                        result.iterations, result.retained, error))
        return false;
    for (std::size_t i = 0; i < values.size(); ++i)
        pressure[i] = values[i].base + values[i].correction;
    double fluidWork = 0.;
    for (std::size_t index = 0; index < graph.size(); ++index) {
        const auto& f = graph[index];
        const auto& face = *physical[index];
        const double p = Difference(values[f.left], values[f.right]);
        const double flux = dt * f.aperture * f.predictor + f.conductance * p;
        fluxDiv[f.left] += flux;
        fluxDiv[f.right] -= flux;
        fluidWork += p * flux;
        const double wallX = body.translation[0] < 0. ? body.hi[0] : body.lo[0];
        const double wallEnd = std::fma(body.translation[0], 1., wallX);
        const double slitLo = std::min(wallX, wallEnd), slitHi = std::max(wallX, wallEnd);
        if (face.axis == 1 && (body.translation[0] < 0. ? face.upper[0] == slitHi : face.lower[0] == slitLo) &&
            face.upper[0] - face.lower[0] < 2e-6 &&
            face.lower[1] == std::fma(body.translation[1], 1., body.hi[1]) &&
            face.lower[2] == .5) {
            result.throatArea = face.meanArea;
            result.throatFlux = flux;
        }
        if (face.axis == 0 && face.lower[0] == wallX && face.lower[1] >= .625 &&
            face.upper[1] <= .75 && face.lower[2] == .5)
            result.capFlux += flux;
    }
    const auto loads = AccumulateGridPressureReaction(geometry.patches, pressure);
    if (loads.size() != 1 || loads.front().owner != body.owner) {
        error = "lost rectilinear wall owner";
        return false;
    }
    result.force = loads.front().force;
    result.workError = fluidWork + std::inner_product(result.force.begin(), result.force.end(),
                                                    body.translation.begin(), 0.);
    for (std::size_t cell = 0; cell < frozenGrid.volumes.size(); ++cell)
        result.fluxClosure = std::max(result.fluxClosure, std::abs(fluxDiv[cell] -
            (geometry.startCapacity[cell] - geometry.endCapacity[cell])));
    const auto hydro = MeasurePlanarHydrostaticMotion(body, 1, old.referenceHeight, old.gravity, 1000., 1.);
    result.force[1] += hydro.force[1];
    out = result;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    using namespace judas::volume_grid;
    if (argc != 2)
        return 1;
    std::ifstream input(argv[1], std::ios::binary);
    BoxMotion body{4};
    GridVec velocity;
    double mass = 0., dt = 0.;
    const auto read = [&](double* values, std::size_t count) {
        input.read(reinterpret_cast<char*>(values), count * sizeof(double));
    };
    read(body.lo.data(), body.lo.size());
    read(body.hi.data(), body.hi.size());
    read(velocity.data(), velocity.size());
    read(&mass, 1);
    read(&dt, 1);
    if (!input || !(dt > 0.) || !(mass > 0.))
        return 2;

    constexpr GridLayout chart{{8, 8, 8}, .125};
    rect_cut::LocalChart frozen;
    if (!rect_cut::PrepareLocalChart(chart, body, {-1e-6, -.0036349140061924913, 0.},
                                    {1e-6, -.0036349140061924913, 0.}, frozen))
        return 2;
    ImportedState old;
    old.wallVelocity = velocity;
    GridVec gravity;
    read(gravity.data(), gravity.size());
    old.gravity = gravity[1];
    old.fluid.water.resize(chart.CellCount());
    read(old.fluid.water.data(), old.fluid.water.size());
    for (int axis = 0; axis < 3; ++axis) {
        auto& field = old.fluid.velocity[axis];
        field.resize(chart.FaceCount(axis));
        read(field.data(), field.size());
    }
    std::string setupError;
    const double volume = std::accumulate(old.fluid.water.begin(), old.fluid.water.end(), 0.);
    if (!input || !ReconstructGridCellPhase(chart, {body}, 1, old.fluid.water, old.phase, setupError) ||
        !FlatWaterHeight(chart, {body}, 1, volume, old.referenceHeight, setupError)) {
        std::cout << "setup " << setupError << '\n';
        return 2;
    }
    const std::array<double, 9> samples{-1e-6, -1e-8, -1e-10, -1e-14, 0., 1e-14, 1e-10, 1e-8, 1e-6};
    std::cout << std::setprecision(17)
              << "dx,cells,links,condensed_links,pcg,fx,fy,sweep_error,patch_error,volume_error,work_error,neck_area,neck_flux,side_flux,elapsed_ms\n";
    std::string error;
    std::vector<Sample> results;
    std::vector<double> times;
    unsigned failures = 0;
    for (const double dx : samples) {
        body.translation = {dx, -.0036349140061924913, 0.};
        Sample row;
        const auto begin = std::chrono::steady_clock::now();
        if (!Solve(chart, frozen, body, old, dt, row, error)) {
            std::cout << dx << ",FAIL," << error << '\n';
            return 3;
        }
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        times.push_back(elapsed);
        results.push_back(row);
        if (!(row.closure < 1e-17 && row.patchClosure < 1e-17 && row.fluxClosure < 1e-14 &&
              std::abs(row.workError) < 1e-8))
            ++failures;
        std::cout << dx << ',' << row.cells << ',' << row.links << ',' << row.retained << ',' << row.iterations << ','
                  << row.force[0] << ',' << row.force[1] << ',' << row.closure << ',' << row.patchClosure
                  << ',' << row.fluxClosure << ',' << row.workError << ',' << row.throatArea << ','
                  << row.throatFlux << ',' << row.capFlux << ',' << elapsed << '\n';
    }
    const auto& zero = results[4];
    const auto& minus = results[3];
    const auto& plus = results[5];
    if (!(std::abs(minus.force[0] - zero.force[0]) < .01 &&
          std::abs(plus.force[0] - zero.force[0]) < .01 &&
          std::abs(minus.force[1] - zero.force[1]) < .01 &&
          std::abs(plus.force[1] - zero.force[1]) < .01))
        ++failures;
    if (!(zero.throatArea == 0. && zero.throatFlux == 0. &&
          minus.throatArea > 0. && minus.throatArea < 1e-14 &&
          plus.throatArea > 0. && plus.throatArea < 1e-14 &&
          std::abs(minus.throatFlux) < 1e-14 && std::abs(plus.throatFlux) < 1e-14 &&
          std::abs(plus.capFlux - zero.capFlux) < 1e-10))
        ++failures;
    std::sort(times.begin(), times.end());
    std::cout << "SUMMARY 11 checks, " << failures << " failures, median ms "
              << times[times.size() / 2] << ", worst ms " << times.back() << '\n';
    return failures ? 4 : 0;
}
