#include "CutRegionGeometry.h"

#include <iomanip>
#include <iostream>

using namespace judas::volume_grid;

namespace {

struct Checks {
    int total = 0, failures = 0;

    void Expect(bool pass, const std::string& label, double measurement = 0.) {
        ++total;
        failures += !pass;
        std::cout << (pass ? "PASS " : "FAIL ") << label << " value=" << measurement << '\n';
    }
};

std::vector<BoxMotion> BasinWalls(bool sealed) {
    constexpr GridVec origin{-1.5, -.5, -1.5};
    std::vector<BoxMotion> boxes;
    const auto box = [&](std::array<float, 3> center, std::array<float, 3> half) {
        BoxMotion shape{static_cast<std::uint32_t>(boxes.size() + 1)};
        for (int axis = 0; axis < 3; ++axis) {
            shape.lo[axis] = double(center[axis]) - half[axis] - origin[axis];
            shape.hi[axis] = double(center[axis]) + half[axis] - origin[axis];
        }
        boxes.push_back(shape);
    };
    box({0.f, -.1f, 0.f}, {1.1f, .1f, 1.1f});
    box({-1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    box({1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    const float height = sealed ? 1.5f : 1.499f;
    box({0.f, 1.5f, -1.1f}, {1.f, height, .1f});
    box({0.f, 1.5f, 1.1f}, {1.f, height, .1f});
    return boxes;
}

double BasinSeam(const CutRegionGeometry& geometry, double plane) {
    double area = 0.;
    for (const auto& face : geometry.faces) {
        if (face.axis == 2 && face.lo[2] == plane)
            area += MeasureCutRegionAperture(face, geometry.motion, {.5, .5, plane},
                                           {2.5, .625, plane}).mean;
    }
    return area;
}

void Stationary(Checks& checks) {
    constexpr GridLayout one{{1, 1, 1}, 1.};
    const std::vector<BoxMotion> barrier{{1, {.25, 0., 0.}, {.75, 1., 1.}, {}},
        {2, {.4, 0., 0.}, {.7, 1., 1.}, {}}};
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(one, barrier, {}, {}, {}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) {
        checks.Expect(false, "overlapping wall chart builds " + error);
        return;
    }
    checks.Expect(geometry.startCapacity == std::vector<double>{.25, .25} && geometry.faces.empty(),
                  "overlapping wall union preserves two separate free components");

    constexpr GridLayout basin{{24, 32, 24}, .125};
    const double analytic = 2. * (1.5 - double(1.499f));
    const double half = double(.3f), y = double(.7f) + .5;
    const BoxMotion body{6, {1.5 - half, y - half, 1.5 - half},
        {1.5 + half, y + half, 1.5 + half}, {}};
    for (const bool sealed : {false, true}) {
        if (!PrepareCutRegionChart(basin, BasinWalls(sealed), body, {0., -.02, 0.}, {0., .02, 0.},
                                   chart, error) ||
            !BuildCutRegionGeometry(chart, {0., .01, 0.}, geometry, error)) {
            checks.Expect(false, "exact basin chart builds " + error);
            continue;
        }
        const double a = BasinSeam(geometry, 1.5 - 1.125);
        const double b = BasinSeam(geometry, 1.5 + 1.125);
        const bool expected = sealed ? a == 0. && b == 0. :
            std::abs(a - analytic) < 2e-17 && std::abs(b - analytic) < 2e-17;
        checks.Expect(expected, sealed ? "matched sealed faces retain exactly zero seam" :
                      "authored 1 mm seams retain exact shared aperture", std::max(a, b));
        checks.Expect(geometry.maxSweepError < 1e-16 && geometry.maxPatchError < 1e-16,
                      "stationary union and translating body close separate volume ledgers",
                      std::max(geometry.maxSweepError, geometry.maxPatchError));
    }
}

void Moving(Checks& checks) {
    constexpr GridLayout layout{{8, 8, 8}, .125};
    constexpr double dy = -.0036349140061924913;
    const BoxMotion body{4, {.25, .3768848776817322, .25}, {.75, .7518848776817322, .75}, {}};
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, body, {-1e-6, dy, 0.}, {1e-6, .01, 0.}, chart, error)) {
        checks.Expect(false, "fixed local cut chart builds " + error);
        return;
    }
    bool correct = true;
    double maximum = 0.;
    for (double dx : {-1e-14, -1e-18, 0., 1e-18, 1e-14}) {
        if (!BuildCutRegionGeometry(chart, {dx, dy, 0.}, geometry, error)) {
            checks.Expect(false, "local cut sweep builds " + error);
            return;
        }
        double area = 0.;
        const double wall = dx < 0. ? body.hi[0] : body.lo[0];
        for (const auto& face : geometry.faces) {
            if (face.axis == 1 && face.lo[1] == body.hi[1] + dy &&
                face.lo[2] == .5 && face.hi[2] == .625 &&
                face.hi[0] - face.lo[0] < 2e-6 &&
                (dx < 0. ? face.hi[0] == wall : face.lo[0] == wall))
                area += face.meanArea;
        }
        const double analytic = std::abs(dx) * layout.h / 2.;
        correct = correct && std::abs(area - analytic) <= 1e-10 * analytic &&
            (dx == 0. ? area == 0. : area > 0.);
        maximum = std::max(maximum, area);
        std::array<std::array<double, 2>, 3> patchArea{};
        for (const auto& patch : geometry.patches)
            patchArea[patch.axis][patch.sign > 0] += patch.area;
        for (int axis = 0; axis < 3; ++axis) {
            const int b = (axis + 1) % 3, c = (axis + 2) % 3;
            const double expected = (body.hi[b] - body.lo[b]) * (body.hi[c] - body.lo[c]);
            for (const double actual : patchArea[axis])
                correct = correct && std::abs(actual - expected) < 2e-15;
        }
        correct = correct && geometry.maxSweepError < 1e-17 && geometry.maxPatchError < 1e-17;
    }
    checks.Expect(correct, "native cap slit shrinks with motion and retains complete wall patches", maximum);

    CutRegionGeometry next;
    if (!BuildCutRegionGeometry(chart, {1e-14, dy, 0.}, {0., .001, 0.}, next, error)) {
        checks.Expect(false, "second motion interval retains the chart " + error);
        return;
    }
    double discrepancy = 0.;
    for (std::size_t i = 0; i < geometry.endCapacity.size(); ++i)
        discrepancy = std::max(discrepancy, std::abs(geometry.endCapacity[i] - next.startCapacity[i]));
    bool sameFaces = geometry.faces.size() == next.faces.size();
    for (std::size_t i = 0; sameFaces && i < geometry.faces.size(); ++i)
        sameFaces = geometry.faces[i].endArea == next.faces[i].startArea;
    checks.Expect(discrepancy == 0. && sameFaces,
                  "consecutive sweeps share region and face ownership", discrepancy);
    std::vector<double> water;
    for (const double capacity : next.startCapacity)
        water.push_back(.3 * capacity);
    std::vector<double> planes;
    const bool reconstructed = ReconstructCutRegionWaterPlanes(chart, next, 1, water, planes, error);
    bool partial = false;
    double reconstruction = 0.;
    if (reconstructed) {
        for (std::size_t i = 0; i < chart.regions.size(); ++i) {
            const auto& region = chart.regions[i];
            auto upper = region.hi;
            upper[1] = planes[i];
            const double full = CutRegionClippedVolume(region, {}, region.lo, region.hi);
            partial = partial || (next.startCapacity[i] > 0. && next.startCapacity[i] < full);
            const double clipped = CutRegionClippedVolume(region, next.motion, region.lo, upper);
            reconstruction = std::max(reconstruction, std::abs(clipped - water[i]));
        }
    }
    checks.Expect(reconstructed && partial && reconstruction < 1e-17,
                  "partly occupied regions reconstruct the same geometric wet storage", reconstruction);
    checks.Expect(!BuildCutRegionGeometry(chart, {2e-6, 0., 0.}, geometry, error) &&
                  geometry.motion && geometry.motion->translation == GridVec{1e-14, dy, 0.},
                  "rejected sweep keeps the previous geometry");

    constexpr GridLayout dividerLayout{{3, 3, 3}, 1.};
    const BoxMotion divider{9, {.2, 1.2, 1.2}, {.35, 1.8, 1.8}, {}};
    if (!PrepareCutRegionChart(dividerLayout, {}, divider, {}, {1.3, 0., 0.}, chart, error) ||
        !BuildCutRegionGeometry(chart, {.1, 0., 0.}, geometry, error)) {
        checks.Expect(false, "short travel in a divider chart " + error);
        return;
    }
    checks.Expect(!BuildCutRegionGeometry(chart, {1.3, 0., 0.}, geometry, error) &&
                  error.find("separate pockets") != std::string::npos &&
                  geometry.motion && geometry.motion->translation == GridVec{.1, 0., 0.},
                  "transient divider rejects rather than joining isolated pressure pockets");
}

void Phase(Checks& checks) {
    constexpr GridLayout layout{{2, 1, 1}, .5};
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, {}, {}, {}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) {
        checks.Expect(false, "phase chart builds " + error);
        return;
    }
    std::vector<double> planes;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, 1, {.03125, .09375}, planes, error)) {
        checks.Expect(false, "phase planes reconstruct " + error);
        return;
    }
    const auto traces = MeasureCutRegionFacePhase(geometry.faces.front(), geometry.motion, 1,
                                                planes[0], planes[1]);
    checks.Expect(planes == std::vector<double>{.125, .375} &&
                  traces.wet[0].mean == .0625 && traces.wet[1].mean == .1875 &&
                  traces.dry[0].mean == .1875 && traces.dry[1].mean == .0625,
                  "shared face keeps independent wet and dry traces on either side");
    const double plane = std::nextafter(.5, 0.);
    const auto thin = MeasureCutRegionFacePhase(geometry.faces.front(), geometry.motion, 1, plane, .5);
    checks.Expect(thin.dry[0].mean == .5 * (.5 - plane) && thin.dry[1].mean == 0.,
                  "independent clipping preserves a one-ULP dry layer", thin.dry[0].mean);
    const auto axial = MeasureCutRegionFacePhase(geometry.faces.front(), geometry.motion, 0, .5, .5);
    checks.Expect(axial.wet[0].mean == .25 && axial.dry[0].mean == 0. &&
                  axial.wet[1].mean == 0. && axial.dry[1].mean == .25,
                  "normal face at the waterline sees the correct one-sided phase");
}

void PhaseNearSaturation(Checks& checks) {
    const CutRegion region{{0., .5, 0.}, {.125, .501, .125}, 0};
    CutRegionChart chart;
    chart.regions = {region, region};
    CutRegionGeometry geometry;
    std::vector<CutRegionWaterPlane> planes;
    std::string error;
    const double capacity = CutRegionClippedVolume(region, {}, region.lo, region.hi);
    const double water = capacity - 6.74e-19;
    geometry.startCapacity = {capacity, capacity};
    const bool subcoordinate = ReconstructCutRegionWaterPlanes(chart, geometry, 1,
        {water, capacity}, {{1, true}, {1, true}}, planes, error);
    if (!subcoordinate) {
        checks.Expect(false, "sub-coordinate phase reconstruction " + error);
        return;
    }
    const auto [wet, dry] = MeasureCutRegionPhaseVolumes(region, {}, region.lo, region.hi, planes[0]);
    checks.Expect(planes[0].coordinate == .5 && planes[0].remainder > 0. &&
                  std::abs(wet - water) < 3e-21 && std::abs(dry - (capacity - water)) < 1e-31,
                  "a local plane retains the air inventory below a chart-coordinate step", dry);
    CutRegionFace face{0, 1, 0, {.125, .5, 0.}, {.125, .501, .125},
                       .125 * (.501 - .5), .125 * (.501 - .5), .125 * (.501 - .5), 0., .125};
    const auto thin = MeasureCutRegionFacePhase(face, {}, planes[0], planes[1]);
    checks.Expect(thin.dry[0].start == dry / .125 && thin.dry[1].start == 0.,
                  "only the positive thin phase has a physical aperture", thin.dry[0].start);
    const auto connected = MeasureCutRegionSharedPhase(face, {}, planes[0], planes[0], false, false);
    const auto disconnected = MeasureCutRegionSharedPhase(face, {}, planes[0], planes[1], false, false);
    checks.Expect(connected.start == thin.dry[0].start && disconnected.start == 0.,
                  "the same tiny shared phase opening connects only when both sides are dry", connected.start);

    geometry.motion = BoxMotion{7, {-1., .5006, -1.}, {1., .52, 1.}, {0., .0001, 0.}};
    const double clipped = CutRegionClippedVolume(region, geometry.motion, region.lo, region.hi);
    geometry.startCapacity = {clipped, clipped};
    const double almost = std::nextafter(clipped, 0.);
    const bool saturated = ReconstructCutRegionWaterPlanes(chart, geometry, 1,
        {clipped, almost}, {}, planes, error);
    if (!saturated) {
        checks.Expect(false, "swept saturation limit " + error);
        return;
    }
    const auto aperture = MeasureCutRegionAperture(face, geometry.motion, face.lo, face.hi);
    face.startArea = aperture.start;
    face.meanArea = aperture.mean;
    const auto phase = MeasureCutRegionFacePhase(face, geometry.motion, planes[0], planes[1]);
    checks.Expect(planes[0].coordinate == geometry.motion->lo[1] &&
                  std::abs(planes[1].RelativeTo(planes[0].coordinate)) < 2e-18 &&
                  phase.dry[0].start == 0. && phase.dry[0].mean > 1e-7 &&
                  std::abs(phase.wet[0].mean - phase.wet[1].mean) < 1e-18,
                  "swept aperture reaches the same saturation limit on the actual free slab",
                  phase.dry[0].mean);
    const CutRegionWaterPlane crossed{{2, true}, .0625};
    double partition = 0.;
    for (bool leftWet : {false, true})
        for (bool rightWet : {false, true})
            partition += MeasureCutRegionSharedPhase(face, geometry.motion, planes[0], crossed,
                                                     leftWet, rightWet).mean;
    checks.Expect(std::abs(partition - face.meanArea) < 1e-19,
                  "crossed interfaces partition the swept aperture without subtractive phase support", partition);

    // A box that retreats from the lower half must never expose wet area
    // when the adjacent region is entirely dry.
    geometry.motion = BoxMotion{7, {-1., .48, -1.}, {1., .5006, 1.}, {0., -.0001, 0.}};
    const double dryCapacity = CutRegionClippedVolume(region, geometry.motion, region.lo, region.hi);
    geometry.startCapacity = {dryCapacity, dryCapacity};
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, 1, {0., 0.}, {}, planes, error)) {
        checks.Expect(false, "dry swept aperture " + error);
        return;
    }
    const auto opening = MeasureCutRegionAperture(face, geometry.motion, face.lo, face.hi);
    face.startArea = opening.start;
    face.meanArea = opening.mean;
    const auto dryTrace = MeasureCutRegionFacePhase(face, geometry.motion, planes[0], planes[1]);
    checks.Expect(opening.mean > opening.start && dryTrace.wet[0].mean == 0. &&
                  dryTrace.wet[1].mean == 0. && dryTrace.dry[0].mean == opening.mean,
                  "a retreating solid cannot expose water from an entirely dry cut", opening.mean);
}

void ConnectedInertia(Checks& checks) {
    const CutRegion region{{0., .5, 0.}, {.25, .75, .25}, 0};
    const CutRegionFace face{0, 1, 1, {0., .5, 0.}, {.25, .5, .25},
                             .0625, .0625, .0625, 0., .25};
    const BoxMotion solid{5, {0., .515625, 0.}, {.25, .59375, .25}, {}};
    const CutRegionWaterPlane vertical{{1, false}, .515625};
    const CutRegionWaterPlane lateral{{0, false}, .25, -1e-17};
    const auto water = MeasureCutRegionFaceInertiaVolumes(face, solid, region, false, vertical);
    const auto trace = MeasureCutRegionFaceInertiaVolumes(face, solid, region, false, lateral);
    const double connected = .0625 * .015625;
    checks.Expect(water[0] == connected && water[1] == 0. &&
                  std::abs(trace[0] - connected) < 1e-18 &&
                  std::abs(trace[1] - (.25 * .015625 * 1e-17)) < 1e-33,
                  "vertical inertia excludes fluid behind a solid and is continuous across phase axes",
                  water[0]);

    auto moving = solid;
    moving.translation[1] = .015625;
    const auto swept = MeasureCutRegionFaceInertiaVolumes(face, moving, region, false, vertical);
    checks.Expect(swept[0] == connected && swept[1] == .5 * connected,
                  "swept inertia integrates the actual wet and newly opened dry depths", swept[1]);

    const BoxMotion entering{5, {0., .4375, 0.}, {.25, .484375, .25}, {0., .03125, 0.}};
    const CutRegionWaterPlane allWet{{1, false}, .75};
    const auto blocked = MeasureCutRegionFaceInertiaVolumes(face, entering, region, false, allWet);
    checks.Expect(blocked[0] == .5 * .0625 * .125 && blocked[1] == 0.,
                  "a moving occluder removes column inertia only after it covers the aperture", blocked[0]);
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    Stationary(checks);
    Moving(checks);
    Phase(checks);
    PhaseNearSaturation(checks);
    ConnectedInertia(checks);
    std::cout << "SUMMARY cut region geometry " << checks.total << " checks "
              << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
