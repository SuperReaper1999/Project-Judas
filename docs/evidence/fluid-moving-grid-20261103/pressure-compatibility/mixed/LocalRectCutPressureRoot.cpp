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
        thin[i] = frozenGrid.volumes[i].hi[0] - frozenGrid.volumes[i].lo[0] <= 6e-6 ||
                  frozenGrid.volumes[i].hi[2] - frozenGrid.volumes[i].lo[2] <= 6e-6;
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

bool SolveTangent(std::array<GridVec, 3> matrix, GridVec rhs, GridVec& direction) {
    for (int col = 0; col < 3; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 3; ++row)
            if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col]))
                pivot = row;
        if (!(std::abs(matrix[pivot][col]) > std::numeric_limits<double>::epsilon()))
            return false;
        std::swap(matrix[col], matrix[pivot]);
        std::swap(rhs[col], rhs[pivot]);
        for (int row = col + 1; row < 3; ++row) {
            const double scale = matrix[row][col] / matrix[col][col];
            for (int k = col + 1; k < 3; ++k)
                matrix[row][k] -= scale * matrix[col][k];
            rhs[row] -= scale * rhs[col];
        }
    }
    for (int row = 2; row >= 0; --row) {
        for (int col = row + 1; col < 3; ++col)
            rhs[row] -= matrix[row][col] * direction[col];
        direction[row] = rhs[row] / matrix[row][row];
        if (!std::isfinite(direction[row]))
            return false;
    }
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
    GridVec free;
    for (int axis = 0; axis < 3; ++axis)
        free[axis] = dt * (velocity[axis] + dt * gravity[axis]);
    rect_cut::LocalChart frozen;
    const GridVec minimum{-3e-6, free[1] - .001, -3e-6};
    const GridVec maximum{3e-6, free[1] + .001, 3e-6};
    if (!rect_cut::PrepareLocalChart(chart, body, minimum, maximum, frozen))
        return 2;
    std::cout << std::setprecision(17);
    unsigned calls = 0;
    std::string error;
    const auto norm = [](const GridVec& v) {
        return std::max({std::abs(v[0]), std::abs(v[1]), std::abs(v[2])});
    };
    const auto evaluate = [&](const GridVec& path, GridVec& residual) {
        for (int axis = 0; axis < 3; ++axis)
            if (path[axis] < minimum[axis] || path[axis] > maximum[axis])
                return false;
        body.translation = path;
        Sample result;
        ++calls;
        if (!Solve(chart, frozen, body, old, dt, result, error) || result.closure > 1e-17 ||
            result.patchClosure > 1e-17 || result.fluxClosure > 1e-14 || std::abs(result.workError) > 1e-8)
            return false;
        for (int axis = 0; axis < 3; ++axis)
            residual[axis] = path[axis] - free[axis] - dt * dt * result.force[axis] / mass;
        std::cout << "TRIAL path " << path[0] << ' ' << path[1] << ' ' << path[2]
                  << " residual " << residual[0] << ' ' << residual[1] << ' ' << residual[2]
                  << " force " << result.force[0] << ' ' << result.force[1] << ' ' << result.force[2] << '\n';
        return true;
    };
    GridVec path = free, residual;
    if (!evaluate(path, residual))
        return 3;
    const double tolerance = 1e-11 * chart.h;
    for (int iteration = 0; norm(residual) > tolerance && iteration < 12; ++iteration) {
        std::array<GridVec, 3> jacobian{};
        for (int axis = 0; axis < 3; ++axis) {
            auto trial = path;
            const double epsilon = std::cbrt(std::numeric_limits<double>::epsilon()) *
                (chart.h + std::abs(path[axis]));
            trial[axis] += epsilon;
            GridVec change;
            if (!evaluate(trial, change))
                return 4;
            for (int row = 0; row < 3; ++row)
                jacobian[row][axis] = (change[row] - residual[row]) / epsilon;
        }
        GridVec rhs{-residual[0], -residual[1], -residual[2]}, direction;
        if (!SolveTangent(jacobian, rhs, direction))
            return 5;
        bool improved = false;
        for (int i = 0; i < 12; ++i) {
            GridVec trial, after;
            for (int axis = 0; axis < 3; ++axis)
                trial[axis] = path[axis] + std::ldexp(direction[axis], -i);
            if (evaluate(trial, after) && norm(after) < norm(residual)) {
                path = trial;
                residual = after;
                improved = true;
                break;
            }
        }
        if (!improved)
            break;
    }
    std::cout << "RESULT residual " << norm(residual) << " tolerance " << tolerance
              << " evaluations " << calls << " final path " << path[0] << ' ' << path[1] << ' ' << path[2] << '\n';
    return norm(residual) <= tolerance ? 0 : 6;
}
