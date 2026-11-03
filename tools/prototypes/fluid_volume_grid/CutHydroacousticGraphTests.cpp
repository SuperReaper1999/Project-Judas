#include "CutHydroacousticGraph.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main() {
    using namespace judas::volume_grid;
    std::string error;
    CutHydroacousticGraphResult result;
    int failed = 0, checked = 0;
    const auto check = [&](bool ok, const char* label) {
        ++checked;
        failed += !ok;
        std::cout << (ok ? "PASS " : "FAIL ") << label << ' ' << error << '\n';
    };
    const auto near = [](double a, double b) { return std::abs(a - b) <= 1e-12; };
    check(SolveCutHydroacousticGraph({2., 0.}, {{0, 1, 3.}}, {-1., 3.}, 1e-13, result, error) &&
          near(result.pressure[0], 1.) && near(result.pressure[1], 2.) && result.maximumResidual <= 1e-13,
          "grounded known endpoint");
    check(SolveCutHydroacousticGraph({0., 0., 0., 7., 0.},
          {{0, 1, 3.}, {0, 1, 1.}, {1, 2, 2.}}, {-4., 2., 2., 14., 0.}, 1e-13, result, error) &&
          near(result.pressure[0], 0.) && near(result.pressure[1], 1.) &&
          near(result.pressure[2], 2.) && near(result.pressure[3], 2.) &&
          near(result.pressure[4], 0.) && result.maximumResidual <= 1e-13,
          "closed gauge, parallel edges, independent ground, and isolated row");

    const auto before = result;
    check(!SolveCutHydroacousticGraph({0., 0.}, {{0, 1, 2.}}, {1., 0.}, 1e-13, result, error) &&
          result.pressure == before.pressure && result.iterations == before.iterations &&
          result.maximumResidual == before.maximumResidual, "incompatible closed source and rollback");
    check(!SolveCutHydroacousticGraph({0.}, {}, {1e-30}, 1e-13, result, error) &&
          result.pressure == before.pressure, "isolated ownerless volume rejects even below tolerance");
    check(!SolveCutHydroacousticGraph({0., 0.}, {{0, 1, 2.}}, {1., -1. + 1e-14}, 1e-15, result, error) &&
          result.pressure == before.pressure, "roundoff-eligible pin still checked on original row");
    check(!SolveCutHydroacousticGraph({1., 1.}, {{0, 0, 2.}}, {1., 2.}, 1e-13, result, error) &&
          result.pressure == before.pressure, "self link rejects");
    check(!SolveCutHydroacousticGraph({1., 1.}, {{0, 2, 2.}}, {1., 2.}, 1e-13, result, error) &&
          result.pressure == before.pressure, "invalid endpoint rejects");
    check(!SolveCutHydroacousticGraph({1., 1.}, {{0, 1, 0.}}, {1., 2.}, 1e-13, result, error) &&
          result.pressure == before.pressure, "zero link rejects");
    check(!SolveCutHydroacousticGraph({1., -1.}, {}, {1., 2.}, 1e-13, result, error) &&
          result.pressure == before.pressure, "negative storage rejects");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    check(!SolveCutHydroacousticGraph({1., 1.}, {}, {1., nan}, 1e-13, result, error) &&
          result.pressure == before.pressure, "NaN source rejects");
    check(!SolveCutHydroacousticGraph({1., 1.}, {{0, 1, infinity}}, {1., 2.}, 1e-13, result, error) &&
          result.pressure == before.pressure, "infinite conductance rejects");
    check(!SolveCutHydroacousticGraph({1., 1.}, {}, {1., 2.}, 0., result, error) &&
          result.pressure == before.pressure, "invalid tolerance rejects");
    check(SolveCutHydroacousticGraph({2e-10, 3.}, {{0, 1, 1e-19}}, {8e-10, 0.}, 1e-24, result, error) &&
          near(result.pressure[0], 4. - 2e-9) && std::abs(result.pressure[1] - 4e-19 / 3.) < 1e-27 &&
          result.maximumResidual <= 1e-24, "weak physical connection preserved");
    check(SolveCutHydroacousticGraph({0., 0.}, {}, {0., 0.}, 1e-13, result, error) &&
          result.pressure == std::vector<double>{0., 0.} && result.iterations == 0, "empty active matrix");
    std::cout << checked - failed << '/' << checked << " passed\n";
    return failed != 0;
}
