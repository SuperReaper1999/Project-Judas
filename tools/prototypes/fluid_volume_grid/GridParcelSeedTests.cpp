#include "GridParcelSeed.h"

#include <cstdio>
#include <limits>
#include <numeric>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

bool Same(const GridParcelSeed& a, const GridParcelSeed& b) {
    return a.water == b.water && a.movedVolume == b.movedVolume &&
           a.maximumGraphRadius == b.maximumGraphRadius;
}

void Reachability(Checks& checks) {
    const GridLayout layout{{5, 5, 5}, 1.};
    const std::vector<BoxMotion> boxes{{1, {1.75, 1., 1.}, {2.25, 2., 2.}, {}}};
    MovingGridGeometry geometry;
    std::string error;
    const auto source = layout.Cell(1, 1, 1), across = layout.Cell(2, 1, 1);
    const auto blocked = layout.Face(0, 2, 1, 1);
    const bool built = BuildMovingGridGeometry(layout, boxes, geometry, error);
    checks.Expect(built, built ? "reference stationary cut geometry builds" : error.c_str());
    if (!built)
        return;
    checks.Expect(geometry.startAperture[0][blocked] == 0. && geometry.startCapacity[source] == .75 &&
                  geometry.startCapacity[across] == .75,
                  "adjacent positive cut capacities are separated by a completely blocked face");

    auto authored = geometry.startCapacity;
    GridParcelSeed output;
    const bool unchanged = ImportGridParcelSeed(layout, boxes, authored, 0, output, error);
    checks.Expect(unchanged && output.water == authored && output.movedVolume == 0. &&
                  output.maximumGraphRadius == 0, "a valid seed is preserved bit for bit");
    const auto saved = output;
    authored[source] = 1.;
    authored[across] = .5;
    checks.Expect(!ImportGridParcelSeed(layout, boxes, authored, 2, output, error) && Same(output, saved),
                  "blocked direct face and too-short graph radius reject without changing output");
    const bool routed = ImportGridParcelSeed(layout, boxes, authored, 3, output, error);
    checks.Expect(routed && output.maximumGraphRadius == 3 && output.water == geometry.startCapacity &&
                  output.movedVolume == .25,
                  "nearest real face path takes three hops around the obstruction");
    if (routed) {
        const auto sum = [](const auto& field) { return std::accumulate(field.begin(), field.end(), 0.); };
        checks.Expect(sum(output.water) == sum(authored),
                      "no source volume is created or discarded on the detour", sum(output.water) - sum(authored));
    }
    const auto retained = output;
    authored[across] = geometry.startCapacity[across];
    checks.Expect(!ImportGridParcelSeed(layout, boxes, authored, -1, output, error) && Same(output, retained),
                  "even an unbounded graph search rejects exhausted free capacity transactionally");

    // Equal-distance candidates have different capacities; only the first
    // two in canonical index order should receive the displaced amount.
    authored.assign(layout.CellCount(), 0.);
    authored[source] = 1.;
    const auto first = layout.Cell(1, 1, 0), second = layout.Cell(1, 0, 1);
    authored[first] = .875;
    authored[second] = .25;
    auto expected = authored;
    expected[source] = .75;
    expected[first] = 1.;
    expected[second] = .375;
    const bool ordered = ImportGridParcelSeed(layout, boxes, authored, 1, output, error);
    checks.Expect(ordered && output.water == expected && output.maximumGraphRadius == 1 &&
                  output.movedVolume == .25,
                  "nearest recipients are deterministic, with preexisting seed only added to");
}

void TinyResidual(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const std::vector<BoxMotion> boxes{{2, {1., 1., 1.}, {2., 2., 1.75}, {}}};
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, boxes, geometry, error)) {
        checks.Expect(false, error.c_str());
        return;
    }
    const auto source = layout.Cell(1, 1, 1), coarse = layout.Cell(1, 0, 1),
               exact = layout.Cell(0, 1, 1);
    auto authored = geometry.startCapacity;
    authored[source] = std::nextafter(.25, 1.);
    authored[coarse] = std::nextafter(.75, 1.);
    authored[exact] = 0.;
    GridParcelSeed output;
    const double excess = std::ldexp(1., -54);
    const bool routed = ImportGridParcelSeed(layout, boxes, authored, 1, output, error);
    checks.Expect(routed && output.water[source] == .25 && output.water[coarse] == authored[coarse] &&
                  output.water[exact] == excess && output.movedVolume == excess,
                  "sub-ulp excess bypasses a recipient that would round it away or create volume", excess);
    const auto saved = output;
    authored[exact] = geometry.startCapacity[exact];
    checks.Expect(!ImportGridParcelSeed(layout, boxes, authored, -1, output, error) && Same(output, saved),
                  "reachable space alone is insufficient when it cannot represent the exact excess");
}

void InvalidSeed(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const std::vector<BoxMotion> boxes{{3, {1., 1., 1.}, {2., 2., 2.}, {}}};
    std::vector<double> authored(layout.CellCount());
    GridParcelSeed output{{.125}, .25, 2};
    const auto saved = output;
    std::string error;
    const auto solid = layout.Cell(1, 1, 1);
    authored[solid] = .125;
    checks.Expect(!ImportGridParcelSeed(layout, boxes, authored, -1, output, error) && Same(output, saved) &&
                  error.find("completely solid") != std::string::npos,
                  "fully solid seed rejects even with free surrounding cells");
    authored[solid] = std::numeric_limits<double>::quiet_NaN();
    checks.Expect(!ImportGridParcelSeed(layout, boxes, authored, 1, output, error) && Same(output, saved),
                  "nonfinite authored volume rejects transactionally");
    authored[solid] = 0.;
    auto moving = boxes;
    moving[0].translation[0] = .125;
    checks.Expect(!ImportGridParcelSeed(layout, moving, authored, 1, output, error) && Same(output, saved),
                  "moving solids cannot be substituted for the stationary seed geometry");
}

} // namespace

int main() {
    Checks checks;
    Reachability(checks);
    TinyResidual(checks);
    InvalidSeed(checks);
    std::printf("SUMMARY parcel seed import %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
