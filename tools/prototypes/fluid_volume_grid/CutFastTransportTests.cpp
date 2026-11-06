#include "CutFastDirectedTransport.h"
#include "CutFastResidual.h"
#include <cmath>
#include <iostream>
#include <random>

using namespace judas::volume_grid;
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* name) {
        ++checks; failures += !ok;
        if (!ok) std::cerr << "FAIL " << name << '\n';
    };
    std::mt19937_64 random(761106);
    for (int sample = 0; sample < 100; ++sample) {
        const std::size_t n = 7;
        std::vector<double> capacity(n);
        std::vector<CutCompensated> exterior(n), rhs(n), state(n);
        std::vector<CutDirectedEdge> edges;
        auto value = [&]() { return std::ldexp(.5 + double(random() % 1000) / 2000., int(random() % 1800) - 900); };
        for (std::size_t i = 0; i < n; ++i) {
            capacity[i] = value(); exterior[i] = value();
            rhs[i] = value(); state[i] = value(); state[i].lo = std::ldexp(state[i].hi, -54);
        }
        for (int j = 0; j < 80; ++j) edges.push_back({random() % n, random() % n, value()});
        std::vector<CutResidualInterval> reference, fast;
        std::string a, b;
        bool ra = BoundCutDirectedResidual(capacity, edges, exterior, rhs, state, reference, a);
        bool rb = BoundCutDirectedResidualFast(capacity, edges, exterior, rhs, state, fast, b);
        check(ra == rb, "enclosure range rejection matches reference");
        bool same = reference.size() == fast.size();
        for (std::size_t i = 0; ra && rb && i < reference.size(); ++i)
            same = same && reference[i].lo == fast[i].lo && reference[i].hi == fast[i].hi;
        check(same, "enclosures exactly match reference across cancellation and underflow");
    }
    for (int sample = 0; sample < 40; ++sample) {
        const std::size_t n = 8;
        std::vector<double> capacity(n);
        std::vector<CutCompensated> exterior(n), rhs(n), reference, fast;
        std::vector<CutDirectedEdge> edges;
        for (std::size_t i = 0; i < n; ++i) {
            capacity[i] = std::ldexp(1., int(random() % 24) - 18);
            rhs[i] = capacity[i] * double(random() % 1000) / 1000.;
            if (i + 1 < n) edges.push_back({i, i + 1, .01});
            if (i > 0) edges.push_back({i, i - 1, .009});
        }
        CutDirectedTransport oracle; CutFastDirectedTransport candidate;
        std::string error;
        check(oracle.Build(capacity, edges, exterior, error) && candidate.Build(capacity, edges, exterior, error), "same operator prepares");
        bool a = oracle.SolveFraction(rhs, 1e-12, reference, error);
        bool b = candidate.SolveFraction(rhs, 1e-12, fast, error);
        check(a && b, "positive recirculating transport admits");
        bool close = a && b;
        for (std::size_t i = 0; a && b && i < n; ++i)
            close = close && fast[i].Value() >= 0. && std::abs((fast[i] - reference[i]).Value()) <= 2e-12;
        check(close, "candidate matches reference within combined certified budgets");
    }
    CutFastDirectedTransport candidate; std::string error;
    check(candidate.Build({1.,1.}, {{0,1,2.}}, {0.,1.}, error), "prepare directed channel");
    std::vector<CutCompensated> result;
    check(candidate.SolveFraction({1e-100,0.},1e-12,result,error) && result[0].Value()>0. && result[1].Value()>0., "minority material remains present");
    check(!candidate.Build({-1.}, {}, {0.}, error), "invalid rebuild rejects");
    check(candidate.SolveFraction({1.,0.},1e-12,result,error) && std::abs(result[0].Value()-1./3.)<1e-12, "failed rebuild preserves prior factorization");
    auto old = result;
    check(!candidate.SolveFraction({1.},1e-12,result,error) && result[0].hi == old[0].hi, "failed solve preserves output");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
