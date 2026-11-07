#include "WaterBulkProjection.cpp"
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
std::vector<CutCompensated> OriginalRows(const System &s) {
  using R = CutCompensated;
  std::vector<R> r(4);
  for (std::size_t i = 0; i < 2; ++i) {
    r[i] = s.local[i].water - s.source.water[i];
    r[2 + i] = (s.local[i].mass - s.source.gasMass[i]) /
               R(s.settings.referenceDensity);
  }
  for (std::size_t f = 0; f < s.liquid.size(); ++f) {
    auto e = s.liquid[f];
    r[e.l] = r[e.l] + s.q[f];
    r[e.r] = r[e.r] - s.q[f];
  }
  for (std::size_t f = 0; f < s.air.size(); ++f) {
    auto e = s.air[f];
    r[2 + e.l] = r[2 + e.l] + s.z[f];
    r[2 + e.r] = r[2 + e.r] - s.z[f];
  }
  for (std::size_t f = 0; f < s.vents.size(); ++f)
    r[2 + s.vents[f].i] = r[2 + s.vents[f].i] + s.vent[f];
  for (std::size_t i = 0; i < 4; ++i)
    r[i] = r[i] / R(s.geometry.capacity[i % 2]);
  return r;
}
int main() {
  using R = CutCompensated;
  int checks = 0, failures = 0;
  double maximum = 0.;
  for (double capacity : {1., 1e-12})
    for (bool leftPressure : {false, true})
      for (bool rightPressure : {false, true}) {
        WaterBulkGeometry g;
        g.capacity = {1., capacity};
        g.potentialBase = {0., 0.};
        g.potentialSlope = {9810., 9810. / capacity};
        WaterBulkState source;
        source.water = {R(.6), R(.4 * capacity)};
        source.gas = {R(.4), R(.6 * capacity)};
        source.gasMass = {R(.48), R(.72 * capacity)};
        WaterGasSettings settings;
        System s(g, source, settings);
        s.head = {R(5886.), R(3924.)};
        s.sourcePressure = {R(101325.), R(101325.)};
        s.pressureCoordinates = {leftPressure, rightPressure};
        s.activeWater = {true, true};
        s.activeAir = {true, true};
        s.liquid = {{0, 1, 1e-7}};
        s.predictor = {0.};
        s.air = {{0, 1, 1e-7}};
        s.vents = {{1, 1e-8}};
        std::vector<R> x = {R(1000.), R(-500.), R(leftPressure ? 200. : .002),
                            R(rightPressure ? -100. : -.001)};
        std::string error;
        if (!s.Evaluate(x, error)) {
          std::cerr << error;
          return 2;
        }
        auto local = s.local;
        auto ad = s.airDerivative;
        auto vd = s.ventDerivative;
        for (std::size_t j = 0; j < 4; ++j) {
          double h =
              j < 2 || (j == 2 ? leftPressure : rightPressure) ? .01 : 1e-7;
          auto plus = x, minus = x;
          plus[j] = plus[j] + R(h);
          minus[j] = minus[j] - R(h);
          if (!s.Evaluate(plus, error)) {
            std::cerr << error;
            return 2;
          }
          auto pr = OriginalRows(s);
          if (!s.Evaluate(minus, error)) {
            std::cerr << error;
            return 2;
          }
          auto mr = OriginalRows(s);
          std::vector<double> direction(4), wanted(4);
          direction[j] = 1.;
          for (std::size_t i = 0; i < 2; ++i) {
            wanted[i] =
                local[i].wh * direction[i] + local[i].wm * direction[2 + i];
            wanted[2 + i] =
                local[i].mh * direction[i] + local[i].md * direction[2 + i];
          }
          double q = 1e-7 * (direction[0] - direction[1]);
          wanted[0] += q;
          wanted[1] -= q;
          auto d = ad[0];
          q = d[0] * direction[0] + d[1] * direction[2] + d[2] * direction[1] +
              d[3] * direction[3];
          wanted[2] += q;
          wanted[3] -= q;
          wanted[3] += vd[0][0] * direction[1] + vd[0][1] * direction[3];
          for (std::size_t i = 0; i < 4; ++i) {
            wanted[i] /= g.capacity[i % 2];
            double fd = ((pr[i] - mr[i]) / R(2 * h)).Value();
            double relative =
                std::abs(fd - wanted[i]) /
                std::max({1e-9, std::abs(fd), std::abs(wanted[i])});
            maximum = std::max(maximum, relative);
            ++checks;
            if (relative > 2e-5) {
              ++failures;
              std::cerr << "BAD capacity=" << capacity
                        << " modes=" << leftPressure << rightPressure
                        << " row=" << i << " col=" << j << " FD=" << fd
                        << " analytic=" << wanted[i] << " relative=" << relative
                        << '\n';
            }
          }
        }
      }
  std::cout << std::setprecision(17) << checks << " checks " << failures
            << " failures maximumRelative=" << maximum << '\n';
  return failures ? 1 : 0;
}
