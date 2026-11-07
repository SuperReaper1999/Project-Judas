#include "WaterBulkCondensation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
namespace {
using Real = CutCompensated;
using Matrix = std::vector<std::map<std::size_t, Real>>;
int checks = 0, failures = 0;
void Check(bool ok, const char *text) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << text << '\n';
    }
}
std::vector<double> Product(const Matrix &a, const std::vector<double> &x) {
    std::vector<double> y(x.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        for (auto [j, v] : a[i])
            y[i] += v.Value() * x[j];
    return y;
}
double Error(const std::vector<double> &a, const std::vector<double> &b) {
    double result = 0.;
    for (std::size_t i = 0; i < a.size(); ++i)
        result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}
} // namespace
int main() {
    Matrix a(4);
    a[0] = {{0, 4.}, {1, -1.}};
    a[1] = {{0, -2.}, {1, 5.}, {2, -1.}};
    a[2] = {{1, -1.}, {2, 4.}, {3, -1.}};
    a[3] = {{2, -1.}, {3, 3.}};
    std::vector<double> wanted{1., -.2, .7, 3.}, answer;
    auto rhs = Product(a, wanted);
    std::string error;
    WaterBulkCondensation factor;
    Check(factor.Build(a, {true, true, true, true}, error),
          "full positive nonsymmetric storage elimination prepares");
    Check(factor.Eliminated() == 4, "all-selected fixture has an empty reduced system");
    Check(factor.Apply(rhs, answer, error) && Error(answer, wanted) < 1e-13,
          "empty-reduced solve recovers the independent known solution");
    Check(Error(Product(a, answer), rhs) < 1e-13,
          "all original equations hold after back substitution");
    auto saved = answer;
    Check(!factor.Build(a, {true}, error), "invalid selection dimensions reject");
    Check(factor.Apply(rhs, answer, error) && answer == saved,
          "failed replacement preserves prior owned factors");
    Matrix cycle(4);
    cycle[0] = {{0, 4.}, {1, -1.}, {3, -1.}};
    cycle[1] = {{0, -1.}, {1, 4.}, {2, -1.}};
    cycle[2] = {{1, -1.}, {2, 4.}, {3, -1.}};
    cycle[3] = {{0, -1.}, {2, -1.}, {3, 4.}};
    rhs = Product(cycle, wanted);
    Check(factor.Build(cycle, {true, false, false, false}, error) && factor.Eliminated() == 1,
          "one gas row condenses without removing its connections");
    Check(factor.Apply(rhs, answer, error) && Error(answer, wanted) < 1e-13 &&
              Error(Product(cycle, answer), rhs) < 1e-13,
          "partial Schur solve satisfies independent original rows");
    saved = answer;
    auto bad = cycle;
    bad[0][0] = -1.;
    Check(!factor.Build(bad, {true, false, false, false}, error),
          "nonpositive pivot rejects instead of changing physics");
    Check(factor.Apply(rhs, answer, error) && answer == saved, "pivot failure is transactional");
    Check(!factor.Apply({1.}, answer, error) && answer == saved,
          "RHS dimension failure leaves previous output intact");
    auto invalid = rhs;
    invalid[0] = std::numeric_limits<double>::quiet_NaN();
    Check(!factor.Apply(invalid, answer, error) && answer == saved, "nonfinite RHS rejects atomically");
    Matrix star(20);
    star[0][0] = 40.;
    for (std::size_t i = 1; i < star.size(); ++i) {
        star[0][i] = -1.;
        star[i][0] = -1.;
        star[i][i] = 2.;
    }
    std::vector<bool> selection(20);
    selection[0] = true;
    std::vector<double> target(20, 1.);
    rhs = Product(star, target);
    Check(factor.Build(star, selection, error) && factor.Eliminated() == 0,
          "high-degree storage stays in reduced operator rather than losing edges");
    Check(factor.Apply(rhs, answer, error) && Error(Product(star, answer), rhs) < 1e-12,
          "bounded elimination retains the complete star equations");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
