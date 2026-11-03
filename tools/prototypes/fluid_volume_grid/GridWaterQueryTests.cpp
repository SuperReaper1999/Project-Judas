#include "GridWaterQuery.h"

#include <iomanip>
#include <iostream>
#include <limits>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int count = 0;
    int failures = 0;

    void Check(bool passed, const char* description) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << description << '\n';
        }
    }
};

double Difference(const GridVec& first, const GridVec& second) {
    double maximum = 0.;
    for (int axis = 0; axis < 3; ++axis)
        maximum = std::max(maximum, std::abs(first[axis] - second[axis]));
    return maximum;
}

bool FlatPhase(const GridLayout& layout, const std::vector<BoxMotion>& boxes, int axis,
               double level, GridCellPhase& phase, std::string& error) {
    GridPhaseGeometry flat;
    return BuildGridPhaseGeometry(layout, boxes, axis, level, flat, error) &&
           ReconstructGridCellPhase(layout, boxes, axis, flat.water, phase, error);
}

void AnalyticVolumeAndCentroid(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    constexpr double waterHeight = .42;
    const GridVec lo{.16, .18, .22}, hi{.66, .54, .57};
    const double rawVolume = (.66 - .16) * (.42 - .18) * (.57 - .22);
    const GridVec rawCentroid{.41, .30, .395};
    std::string error;
    GridCellPhase phase;
    GridWaterSample sample;
    checks.Check(FlatPhase(layout, {}, 1, waterHeight, phase, error) &&
                     QueryGridWaterVolume(layout, {}, 1, phase.waterPlane, lo, hi, sample, error),
                 "query intersects a partial flat surface across several cells");
    checks.Check(std::abs(sample.wetVolume - rawVolume) < 2e-15 &&
                     Difference(sample.centroid, rawCentroid) < 2e-15,
                 "open volume and first moment match analytic rectangular water");

    const std::vector<BoxMotion> boxes{{1, {.29, .21, .28}, {.56, .49, .56}, {}}};
    checks.Check(FlatPhase(layout, boxes, 1, waterHeight, phase, error) &&
                     QueryGridWaterVolume(layout, boxes, 1, phase.waterPlane, lo, hi, sample, error),
                 "partly submerged box crosses all three grid axes");
    const double excluded = (.56 - .29) * (.42 - .21) * (.56 - .28);
    const GridVec excludedCentroid{.425, .315, .42};
    GridVec expectedCentroid{};
    for (int axis = 0; axis < 3; ++axis)
        expectedCentroid[axis] = (rawCentroid[axis] * rawVolume - excludedCentroid[axis] * excluded) /
                                 (rawVolume - excluded);
    checks.Check(std::abs(sample.wetVolume - (rawVolume - excluded)) < 2e-15 &&
                     Difference(sample.centroid, expectedCentroid) < 2e-15,
                 "cross-cell cut removes only the actual submerged solid and moment");

    checks.Check(QueryGridWaterVolume(layout, boxes, 1, phase.waterPlane,
                                     {.12, .424, .19}, {.37, .48, .41}, sample, error) &&
                     sample.wetVolume == 0. && sample.centroid == GridVec{},
                 "4 mm dry gap remains dry in a cell that also owns water");
    checks.Check(QueryGridWaterVolume(layout, boxes, 1, phase.waterPlane,
                                     {.31, .24, .29}, {.55, .41, .55}, sample, error) &&
                     sample.wetVolume == 0. && sample.centroid == GridVec{},
                 "sample wholly within submerged solid excludes all wet volume");

    const double almostDry = std::nextafter(waterHeight, 0.);
    checks.Check(QueryGridWaterVolume(layout, boxes, 1, phase.waterPlane,
                                     {.05, almostDry, .05}, {.20, .48, .20}, sample, error) &&
                     std::abs(sample.wetVolume - (.20 - .05) * (.42 - almostDry) * (.20 - .05)) <
                         1e-29 && sample.wetVolume > 0.,
                 "positive one-representable-step water sliver is not clipped away");

    const double secondWall = std::nextafter(.50, 1.);
    const std::vector<BoxMotion> narrow{
        {1, {.28, .25, .26}, {.50, .65, .74}, {}},
        {2, {secondWall, .25, .26}, {.72, .65, .74}, {}},
    };
    checks.Check(FlatPhase(layout, narrow, 1, .60, phase, error) &&
                     QueryGridWaterVolume(layout, narrow, 1, phase.waterPlane,
                                          {.30, .30, .30}, {.70, .50, .60}, sample, error) &&
                     std::abs(sample.wetVolume - (secondWall - .50) * .20 * .30) < 1e-29 &&
                     Difference(sample.centroid, {.50, .40, .45}) < 2e-15,
                 "one-ulp wet gap between distinct solids survives exact subtraction");
}

void ClippedAndLocalPlanes(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    std::string error;
    GridCellPhase phase;
    GridWaterSample sample;
    checks.Check(FlatPhase(layout, {}, 0, .7, phase, error) &&
                     QueryGridWaterVolume(layout, {}, 0, phase.waterPlane,
                                          {.29, .32, .30}, {.55, .54, .57}, sample, error),
                 "full immersion works on a non-y water axis");
    checks.Check(std::abs(sample.wetVolume - .26 * .22 * .27) < 2e-15 &&
                     Difference(sample.centroid, {.42, .43, .435}) < 2e-15,
                 "fully wet AABB retains its physical volume and center");

    checks.Check(QueryGridWaterVolume(layout, {}, 0, phase.waterPlane,
                                     {-.1, -.02, .1}, {.08, .10, .2}, sample, error) &&
                     std::abs(sample.wetVolume - .08 * .10 * .10) < 2e-15 &&
                     Difference(sample.centroid, {.04, .05, .15}) < 2e-15,
                 "partly exterior AABB contributes only in-domain water");
    checks.Check(QueryGridWaterVolume(layout, {}, 0, phase.waterPlane,
                                     {-1., 0., 0.}, {-.5, .25, .25}, sample, error) &&
                     sample.wetVolume == 0. && sample.centroid == GridVec{},
                 "wholly exterior AABB is dry");

    std::vector<double> water(layout.CellCount());
    const auto a = layout.Cell(0, 1, 1), b = layout.Cell(1, 1, 1);
    water[a] = .25 * .25 * .05;
    water[b] = .25 * .25 * .19;
    checks.Check(ReconstructGridCellPhase(layout, {}, 1, water, phase, error) &&
                     QueryGridWaterVolume(layout, {}, 1, phase.waterPlane,
                                          {.20, .25, .30}, {.30, .50, .42}, sample, error),
                 "query samples independent adjacent cell heights");
    const double leftVolume = .05 * .05 * .12, rightVolume = .05 * .19 * .12;
    const double total = leftVolume + rightVolume;
    const GridVec expected{(.225 * leftVolume + .275 * rightVolume) / total,
                           (.275 * leftVolume + .345 * rightVolume) / total, .36};
    checks.Check(std::abs(sample.wetVolume - total) < 2e-15 &&
                     Difference(sample.centroid, expected) < 2e-15,
                 "local jump yields an independent composite centroid and occupied volume");

    const auto before = sample;
    phase.waterPlane[a] = std::numeric_limits<double>::quiet_NaN();
    checks.Check(!QueryGridWaterVolume(layout, {}, 1, phase.waterPlane,
                                      {.20, .25, .30}, {.30, .50, .42}, sample, error) &&
                     sample.wetVolume == before.wetVolume && sample.centroid == before.centroid,
                 "invalid visited phase plane preserves the previous sample");
    checks.Check(!QueryGridWaterVolume(layout, {}, 1, phase.waterPlane,
                                      {.2, .3, .4}, {.2, .5, .6}, sample, error) &&
                     sample.wetVolume == before.wetVolume && sample.centroid == before.centroid,
                 "nonpositive sample bounds are rejected transactionally");
}

} // namespace

int main() {
    Checks checks;
    AnalyticVolumeAndCentroid(checks);
    ClippedAndLocalPlanes(checks);
    std::cout << checks.count << " water geometry query checks, " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
