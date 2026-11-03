#include "RectCutGeometry.h"

// Reuse the isolated pressure graph for this experiment. The network has
// nonuniform cells; no engine target or prototype public API is changed.
#include "MovingVolumeGrid.cpp"

#include <fstream>
#include <iomanip>
#include <iostream>

namespace {

using namespace judas::volume_grid;

struct Sample {
    GridVec force{};
    double closure = 0., patchClosure = 0., workError = 0., fluxClosure = 0.;
    double throatArea = 0., throatFlux = 0., capFlux = 0.;
    std::size_t cells = 0, links = 0;
    int iterations = 0;
};

bool Solve(const GridLayout& chart, const rect_cut::Grid& frozenGrid, const BoxMotion& body, double dt, Sample& out,
           std::string& error) {
    rect_cut::Geometry geometry;
    if (!rect_cut::Build(frozenGrid, body, geometry)) {
        error = "invalid rectilinear cut";
        return false;
    }
    const auto& grid = geometry.grid;
    Sample result;
    result.cells = grid.Count();
    std::vector<double> rhs(grid.Count()), solidDiv(grid.Count()), patchSweep(grid.Count());
    std::vector<double> pressure(grid.Count()), fluxDiv(grid.Count());
    std::vector<FaceFlux> graph;
    std::vector<const rect_cut::Connection*> physical;
    for (const auto& face : geometry.links) {
        solidDiv[face.left] += face.sweptVolume;
        solidDiv[face.right] -= face.sweptVolume;
        if (face.meanArea == 0.)
            continue;
        const auto knot = std::lower_bound(grid.planes[face.axis].begin(), grid.planes[face.axis].end(),
                                           face.lower[face.axis]);
        const auto& planes = grid.planes[face.axis];
        if (knot == planes.begin() || knot == planes.end() || knot + 1 == planes.end()) {
            error = "invalid interior cut connection";
            return false;
        }
        const double length = .5 * (knot[1] - knot[-1]);
        const double density = 1000.;
        const double mass = density * face.meanArea * length;
        const double conductance = (dt * face.meanArea) * (dt * face.meanArea) / mass;
        if (!(conductance > 0.) || !std::isfinite(conductance)) {
            error = "invalid cut connection pressure inertia";
            return false;
        }
        graph.push_back({face.left, face.right, graph.size(), face.axis, face.meanArea, mass,
                         conductance, 0., face.lower[face.axis]});
        physical.push_back(&face);
    }
    result.links = graph.size();
    for (const auto& patch : geometry.patches)
        patchSweep[patch.cell] -= patch.sign * patch.area * body.translation[patch.axis];
    double maxSource = 0.;
    for (std::size_t cell = 0; cell < grid.Count(); ++cell) {
        rhs[cell] = geometry.startCapacity[cell] - geometry.endCapacity[cell];
        maxSource = std::max(maxSource, std::abs(rhs[cell]));
        result.closure = std::max(result.closure, std::abs(rhs[cell] + solidDiv[cell]));
        result.patchClosure = std::max(result.patchClosure, std::abs(rhs[cell] + patchSweep[cell]));
    }
    const double cellVolume = chart.h * chart.h * chart.h;
    if (!SolvePressure(graph, rhs, maxSource * 1e-12, cellVolume, pressure,
                       result.iterations, error))
        return false;
    double fluidWork = 0.;
    for (std::size_t index = 0; index < graph.size(); ++index) {
        const auto& f = graph[index];
        const auto& face = *physical[index];
        const double p = pressure[f.left] - pressure[f.right];
        const double flux = f.conductance * p;
        fluxDiv[f.left] += flux;
        fluxDiv[f.right] -= flux;
        fluidWork += p * flux;
        const double wallX = body.translation[0] < 0. ? body.hi[0] : body.lo[0];
        const double wallEnd = std::fma(body.translation[0], 1., wallX);
        const double slitLo = std::min(wallX, wallEnd), slitHi = std::max(wallX, wallEnd);
        if (face.axis == 1 && (body.translation[0] < 0. ? face.upper[0] == slitHi : face.lower[0] == slitLo) &&
            face.upper[0] - face.lower[0] < 2e-6 &&
            face.lower[1] == .75 && face.lower[2] == .5) {
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
    for (std::size_t cell = 0; cell < grid.Count(); ++cell)
        result.fluxClosure = std::max(result.fluxClosure, std::abs(fluxDiv[cell] -
            (geometry.startCapacity[cell] - geometry.endCapacity[cell])));
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
    rect_cut::Grid frozen;
    auto envelope = body;
    envelope.translation = {1e-6, -.0036349140061924913, 0.};
    if (!rect_cut::ClipGrid(chart, envelope, frozen))
        return 2;
    for (double wall : {body.lo[0], body.hi[0]})
        frozen.planes[0].push_back(wall - 1e-6);
    std::sort(frozen.planes[0].begin(), frozen.planes[0].end());
    frozen.planes[0].erase(std::unique(frozen.planes[0].begin(), frozen.planes[0].end()),
                           frozen.planes[0].end());
    frozen.cells[0] = static_cast<int>(frozen.planes[0].size() - 1);
    const std::array<double, 9> samples{-1e-6, -1e-8, -1e-10, -1e-14, 0., 1e-14, 1e-10, 1e-8, 1e-6};
    std::cout << std::setprecision(17)
              << "dx,cells,links,pcg,fx,fy,sweep_error,patch_error,volume_error,work_error,neck_area,neck_flux,side_flux\n";
    std::string error;
    for (const double dx : samples) {
        body.translation = {dx, -.0036349140061924913, 0.};
        Sample row;
        if (!Solve(chart, frozen, body, dt, row, error)) {
            std::cout << dx << ",FAIL," << error << '\n';
            return 3;
        }
        std::cout << dx << ',' << row.cells << ',' << row.links << ',' << row.iterations << ','
                  << row.force[0] << ',' << row.force[1] << ',' << row.closure << ',' << row.patchClosure
                  << ',' << row.fluxClosure << ',' << row.workError << ',' << row.throatArea << ','
                  << row.throatFlux << ',' << row.capFlux << '\n';
    }
}
