#include "WaterPressureILU.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char *why) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << why << '\n';
    }
}
} // namespace
int main() {
    using Real = CutCompensated;
    std::string error;
    WaterPressureILU factor;
    std::vector<std::map<std::size_t, Real>> rows(4);
    rows[0] = {{0, 4.}, {1, -1.}};
    rows[1] = {{0, -2.}, {1, 5.}, {2, -1.}};
    rows[2] = {{1, -1.}, {2, 4.}, {3, -1.}};
    rows[3] = {{2, -1.}, {3, 3.}};
    Check(factor.Build(rows, error), "positive nonsymmetric pressure factor prepares");
    std::vector<double> wanted{1., -.2, .7, 3.}, rhs(4), answer;
    for (std::size_t i = 0; i < 4; ++i)
        for (auto e : rows[i])
            rhs[i] += e.second.Value() * wanted[e.first];
    Check(factor.Apply(rhs, answer, error), "finite-air search direction solves");
    double maximum = 0.;
    for (std::size_t i = 0; i < 4; ++i)
        maximum = std::max(maximum, std::abs(answer[i] - wanted[i]));
    Check(maximum < 1e-14, "tridiagonal no-fill case reproduces independent known solution");
    rows[0].erase(0);
    Check(!factor.Build(rows, error), "missing diagonal rejects instead of inserting a stabilizer");
    std::vector<double> again;
    Check(factor.Apply(rhs, again, error) && again == answer,
          "failed replacement preserves previous prepared factor");
    std::vector<std::map<std::size_t, Real>> bad(2);
    bad[0] = {{0, 1.}, {1, 2.}};
    bad[1] = {{0, 2.}, {1, 1.}};
    Check(!factor.Build(bad, error),
          "indefinite incomplete factor rejects rather than shifting physical rows");
    Check(!factor.Apply({1.}, again, error), "invalid search-vector dimensions reject");
    std::vector<std::map<std::size_t, Real>> cycle(4);
    cycle[0] = {{0, 4.}, {1, -1.}, {3, -1.}};
    cycle[1] = {{0, -1.}, {1, 4.}, {2, -1.}};
    cycle[2] = {{1, -1.}, {2, 4.}, {3, -1.}};
    cycle[3] = {{0, -1.}, {2, -1.}, {3, 4.}};
    std::vector<double> target{1., 2., -1., .5}, load(4), filled, unfilled;
    for (std::size_t i = 0; i < 4; ++i)
        for (auto e : cycle[i])
            load[i] += e.second.Value() * target[e.first];
    WaterPressureILU full, noFill;
    Check(noFill.Build(cycle, error) && noFill.Apply(load, unfilled, error),
          "zero-fill cycle negative control prepares");
    maximum = 0.;
    for (std::size_t i = 0; i < 4; ++i)
        maximum = std::max(maximum, std::abs(unfilled[i] - target[i]));
    Check(maximum > 1e-5, "cycle genuinely needs an elimination fill edge");
    Check(full.Build(cycle, error, 1) && full.Apply(load, filled, error),
          "bounded one-level fill factor prepares");
    maximum = 0.;
    for (std::size_t i = 0; i < 4; ++i)
        maximum = std::max(maximum, std::abs(filled[i] - target[i]));
    Check(maximum < 1e-13, "one-level fill reproduces independent cycle solution");
    Check(!full.Build(cycle, error, 2), "unsupported fill levels reject explicitly");
    Check(full.Apply(load, again, error) && again == filled,
          "failed fill replacement preserves prepared factor");
    std::vector<std::map<std::size_t, Real>> star(520);
    star[0][0] = 1040.;
    for (std::size_t i = 1; i < star.size(); ++i) {
        star[0][i] = -1.;
        star[i][0] = -1.;
        star[i][i] = 2.;
    }
    Check(!full.Build(star, error, 1), "fill work budget rejects a pathological elimination star");
    Check(full.Apply(load, again, error) && again == filled,
          "fill work exhaustion preserves the previous factor");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
