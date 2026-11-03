#include "CutRegionGasTopology.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

using namespace judas::volume_grid;

namespace {

unsigned checks = 0, failures = 0;

void Check(bool result, const std::string& label) {
    ++checks;
    failures += !result;
    std::cout << (result ? "PASS " : "FAIL ") << label << '\n';
}

double Sum(const std::vector<double>& values) {
    return static_cast<double>(std::accumulate(values.begin(), values.end(), 0.L));
}

bool Prepare(const GridLayout& layout, const std::vector<BoxMotion>& walls,
             const std::optional<BoxMotion>& moving, CutRegionChart& chart,
             CutRegionGeometry& geometry, std::string& error) {
    return PrepareCutRegionChart(layout, walls, moving, {}, {}, chart, error) &&
        BuildCutRegionGeometry(chart, {}, geometry, error);
}

bool Build(const CutRegionChart& chart, const CutRegionGeometry& geometry, int up,
           const std::vector<double>& water, const std::vector<CutRegionGasVent>& apertures,
           CutRegionGasTopology& graph, std::string& error,
           const std::vector<CutRegionInterface>& orientation = {}) {
    std::vector<CutRegionWaterPlane> interfaces;
    return ReconstructCutRegionWaterPlanes(chart, geometry, up, water, orientation, interfaces, error) &&
        BuildCutRegionGasTopology(chart, geometry, interfaces, apertures, graph, error);
}

void PartialPort() {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutRegionGasTopology vents;
    std::string error;
    if (!Prepare({{2, 1, 1}, 1.}, {}, {}, chart, geometry, error)) {
        Check(false, "partial chart " + error);
        return;
    }
    const bool built = Build(chart, geometry, 1, {.2, .6}, {}, vents, error);
    Check(built && vents.components.size() == 2 && vents.sharedDryArea.size() == 1 &&
          std::abs(vents.sharedDryArea.front() - .4) < 1e-14 &&
          std::abs(vents.components[1].volume - 1.2) < 1e-14 &&
          vents.regionOwner == std::vector<std::size_t>{1, 1},
          "overlapping one-sided dry traces own only their common area " + error);
}

void CoordinateResolution() {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutRegionGasTopology vents;
    std::string error;
    if (!Prepare({{1, 2, 1}, 1.}, {}, {}, chart, geometry, error)) {
        Check(false, "coordinate resolution chart " + error);
        return;
    }
    const auto top = FullCutRegionGasVent(chart, 1, 1);
    const double nearFull = std::nextafter(1., 0.);
    const bool built = Build(chart, geometry, 1, {1., nearFull}, {top}, vents, error);
    Check(built && vents.regionOwner == std::vector<std::size_t>{NoCutRegionGas, 0} &&
          vents.components.size() == 1 && vents.components[0].volume == 1. - nearFull &&
          Sum(vents.ventArea) == 1. && Sum(vents.sharedDryArea) == 0.,
          "sub-coordinate upper gas retains its real top boundary " + error);
    const std::vector<CutRegionInterface> lower{{1, false}, {1, true}};
    const bool oriented = Build(chart, geometry, 1, {1., nearFull}, {top}, vents, error, lower);
    Check(oriented && vents.regionOwner == std::vector<std::size_t>{NoCutRegionGas, 1} &&
          vents.components.size() == 2 && vents.components[1].volume == 1. - nearFull &&
          Sum(vents.ventArea) == 0. && Sum(vents.sharedDryArea) == 0.,
          "the same gas depth below the interface is sealed by its actual wet top " + error);
}

void WallAndBoundary() {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutRegionGasTopology vents;
    std::string error;
    const BoxMotion wall{1, {1., 0., 0.}, {2., 2., 2.}, {}};
    if (!Prepare({{3, 2, 2}, 1.}, {wall}, {}, chart, geometry, error)) {
        Check(false, "partitioned chart " + error);
        return;
    }
    auto top = FullCutRegionGasVent(chart, 1, 1);
    top.hi[0] = 1.5; // Half a meter sits over static solid and offers no vent.
    const std::vector<double> dry(chart.regions.size());
    const bool built = Build(chart, geometry, 1, dry, {top}, vents, error);
    Check(built && vents.components.size() == 2 && vents.components[0].volume == 4. &&
          vents.components[1].volume == 4. && Sum(vents.ventArea) == 2.,
          "explicit partial top vent leaves the wall's far side closed " + error);
    const auto previous = vents.regionOwner;
    Check(!Build(chart, geometry, 1, dry, {top, top}, vents, error) &&
          vents.regionOwner == previous,
          "overlapping vent rectangles reject without publication");

    auto capped = dry;
    for (std::size_t i = 0; i < chart.regions.size(); ++i)
        if (chart.regions[i].lo[1] == 1.)
            capped[i] = geometry.startCapacity[i];
    top = FullCutRegionGasVent(chart, 1, 1);
    const bool cappedBuilt = Build(chart, geometry, 1, capped, {top}, vents, error);
    Check(cappedBuilt && vents.components.size() == 3 && vents.components[0].volume == 0. &&
          vents.components[1].volume == 2. && vents.components[2].volume == 2. &&
          Sum(vents.ventArea) == 0.,
          "wet top prevents a geometrically open boundary from venting gas " + error);
}

void ThinPhysicalOpening() {
    const double apertureWidth = double(1.1f) - double(.1f) - 1.;
    for (const bool open : {false, true}) {
        CutRegionChart chart;
        CutRegionGeometry geometry;
        CutRegionGasTopology vents;
        std::string error;
        const double wallEnd = open ? 1. - apertureWidth : 1.;
        const BoxMotion wall{1, {.9, 0., 0.}, {1.1, 1., wallEnd}, {}};
        if (!Prepare({{2, 1, 1}, 1.}, {wall}, {}, chart, geometry, error)) {
            Check(false, "thin-opening chart " + error);
            continue;
        }
        const auto left = FullCutRegionGasVent(chart, 0, -1);
        const bool built = Build(chart, geometry, 1,
            std::vector<double>(chart.regions.size()), {left}, vents, error);
        double portalArea = 0.;
        if (built)
            for (std::size_t f = 0; f < geometry.faces.size(); ++f) {
                const auto& face = geometry.faces[f];
                if (face.axis == 0 && face.lo[0] == 1.)
                    portalArea += vents.sharedDryArea[f];
            }
        Check(built && portalArea == 1. - wallEnd && vents.components.size() == (open ? 1u : 2u),
              open ? "native-scale 22 nm aperture connects gas without widening"
                   : "exact contact keeps far-side gas sealed");
    }
}

void MovingBoundaryAndDrySplit() {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutRegionGasTopology vents;
    std::string error;
    const BoxMotion body{1, {.2, .5, .2}, {.8, 1., .8}, {}};
    chart.layout = {{1, 1, 1}, 1.};
    chart.regions.push_back({{}, {1., 1., 1.}, 0});
    geometry.motion = body;
    geometry.startCapacity = {CutRegionClippedVolume(chart.regions[0], body, {}, {1., 1., 1.})};
    const std::vector<double> water(chart.regions.size());
    const CutRegionGasVent covered{1, 1, {.3, 1., .3}, {.7, 1., .7}};
    const bool coveredBuilt = Build(chart, geometry, 1, water, {covered}, vents, error);
    Check(coveredBuilt && vents.components.size() == 2 && vents.components[0].volume == 0. &&
          Sum(vents.ventArea) == 0.,
          "moving body on chart boundary physically seals the explicit vent " + error);
    const CutRegionGasVent partlyOpen{1, 1, {.1, 1., .3}, {.7, 1., .7}};
    const bool openBuilt = Build(chart, geometry, 1, water, {partlyOpen}, vents, error);
    Check(openBuilt && vents.components.size() == 1 &&
          std::abs(Sum(vents.ventArea) - .04) < 1e-14,
          "uncovered portion of an obstructed vent retains exact support " + error);

    // Deliberately hold one larger retained control volume, as can happen
    // away from chart-construction positions with a translating obstacle.
    chart = {};
    chart.layout = {{1, 1, 1}, 1.};
    chart.regions.push_back({{}, {1., 1., 1.}, 0});
    geometry = {};
    geometry.motion = BoxMotion{1, {.45, .5, 0.}, {.55, 1., 1.}, {}};
    geometry.startCapacity = {.95};
    Check(!Build(chart, geometry, 1, {.5}, {}, vents, error) &&
          error == "moving solid divides dry material inside one cut region",
          "a connected full volume cannot silently connect a split dry complement");

    geometry.motion = BoxMotion{1, {.45, .4, 0.}, {.55, .95, 1.}, {0., .2, 0.}};
    geometry.startCapacity = {CutRegionClippedVolume(chart.regions[0], geometry.motion, {}, {1., 1., 1.})};
    const std::vector<CutRegionWaterPlane> planes{{{1, false}, .5}};
    Check(!BuildCutRegionGasTopology(chart, geometry, planes, {}, vents, error) &&
          error == "moving solid divides dry material inside one cut region",
          "a transient split between connected endpoint slabs rejects");
}

void DifferentOrientations() {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutRegionGasTopology graph, repeated;
    std::string error;
    if (!Prepare({{1, 2, 1}, 1.}, {}, {}, chart, geometry, error)) {
        Check(false, "mixed orientation chart " + error);
        return;
    }
    const std::vector<CutRegionInterface> orientation{{0, false}, {1, true}};
    const auto top = FullCutRegionGasVent(chart, 1, 1);
    const std::vector<double> water{.6, .9};
    const bool built = Build(chart, geometry, 1, water, {top}, graph, error, orientation);
    Check(built && graph.components.size() == 2 && graph.regionOwner == std::vector<std::size_t>{1, 1} &&
          std::abs(graph.sharedDryArea[0] - .4) < 1e-15 &&
          std::abs(graph.components[1].volume - .5) < 1e-15 && Sum(graph.ventArea) == 0.,
          "upper cut's dry inlet joins the original pocket without creating a top vent " + error);
    Check(Build(chart, geometry, 1, water, {top}, repeated, error, orientation) &&
          graph.regionOwner == repeated.regionOwner && graph.sharedDryArea == repeated.sharedDryArea,
          "oriented gas labels and physical apertures are deterministic " + error);

    std::vector<CutRegionWaterPlane> planes;
    const auto saved = graph.regionOwner;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, 1, water, orientation, planes, error)) {
        Check(false, "prepare malformed control " + error);
        return;
    }
    planes[1].coordinate = 3.;
    Check(!BuildCutRegionGasTopology(chart, geometry, planes, {top}, graph, error) &&
          graph.regionOwner == saved, "invalid phase coordinate rejects transactionally");

    const bool defaultBuilt = Build(chart, geometry, 1, water, {top}, graph, error);
    Check(defaultBuilt && graph.components.size() == 2 &&
          graph.regionOwner == std::vector<std::size_t>{1, 0} &&
          graph.sharedDryArea[0] == 0. && Sum(graph.ventArea) == 1.,
          "independent gravity-top slabs create a disconnected gas label on matched inventory " + error);
}

void VentOpeningMotion() {
    CutRegionChart chart;
    chart.layout = {{1, 1, 1}, 1.};
    chart.regions.push_back({{}, {1., 1., 1.}, 0});
    CutRegionGeometry geometry;
    geometry.motion = BoxMotion{1, {.2, .5, .2}, {.8, 1., .8}, {.5, 0., 0.}};
    geometry.startCapacity = {CutRegionClippedVolume(chart.regions[0], geometry.motion, {}, {1., 1., 1.})};
    const CutRegionGasVent vent{1, 1, {.3, 1., .3}, {.7, 1., .7}};
    CutRegionGasTopology graph;
    std::string error;
    Check(Build(chart, geometry, 1, {0.}, {vent}, graph, error) &&
          graph.regionOwner == std::vector<std::size_t>{1} && graph.ventArea[0] == 0.,
          "interval-average opening cannot vent a chamber before its solid moves " + error);
}

} // namespace

int main() {
    PartialPort();
    CoordinateResolution();
    WallAndBoundary();
    ThinPhysicalOpening();
    MovingBoundaryAndDrySplit();
    DifferentOrientations();
    VentOpeningMotion();
    std::cout << "CHECKS " << checks << " FAILURES " << failures << '\n';
    return failures != 0;
}
