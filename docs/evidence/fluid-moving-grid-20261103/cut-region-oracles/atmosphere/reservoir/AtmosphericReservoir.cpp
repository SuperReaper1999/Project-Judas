#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

constexpr double gravity = 9.81, density = 1000.;

struct Geometry {
    std::array<double, 2> surfaceArea;
    double width, openingHeight, sill, length;
    bool vented = true;
};

struct State {
    std::array<double, 2> water;
    double speed = 0.; // Signed liquid speed in the aperture, left to right.
};

struct Step {
    State next;
    double aperture = 0., flow = 0., transfer = 0.;
    double pressureJump = 0., work = 0., linkKineticChange = 0.;
    double throughflowCfl = 0.;
};

// Two vented constant-section reservoirs with quasi-hydrostatic columns and
// one finite-volume liquid link. The wet rectangular link never changes area
// within an admitted step. This models reservoir heads and link inertia.
bool Advance(const Geometry& g, const State& state, double dt, Step& output, std::string& error) {
    if (!g.vented || !(g.width > 0. && g.openingHeight >= 0. && g.sill >= 0. && g.length > 0.) ||
        !(dt > 0.) || !std::isfinite(dt) || !std::isfinite(state.speed)) {
        error = "liquid link requires vented reservoirs, finite geometry and a positive timestep";
        return false;
    }
    std::array<double, 2> head{};
    for (int i = 0; i < 2; ++i) {
        if (!(g.surfaceArea[i] > 0. && state.water[i] >= 0.) ||
            !std::isfinite(g.surfaceArea[i]) || !std::isfinite(state.water[i])) {
            error = "reservoir storage must be finite and nonnegative";
            return false;
        }
        head[i] = state.water[i] / g.surfaceArea[i];
    }
    const double aperture = g.width * g.openingHeight, portTop = g.sill + g.openingHeight;
    if (!std::isfinite(aperture) || !std::isfinite(portTop) || !std::isfinite(g.length) ||
        !std::isfinite(head[0]) || !std::isfinite(head[1]) || (g.openingHeight > 0. && aperture == 0.)) {
        error = "liquid link geometry is not representable";
        return false;
    }
    Step next;
    next.next = state;
    next.aperture = aperture;
    if (aperture == 0.) {
        if (state.speed != 0.) {
            error = "sealed link has no liquid inertia";
            return false;
        }
        output = next;
        error.clear();
        return true;
    }
    if (!(head[0] >= portTop && head[1] >= portTop)) {
        error = "the bounded liquid link must be fully wet at both ports";
        return false;
    }

    const double compliance = 1. / g.surfaceArea[0] + 1. / g.surfaceArea[1];
    const double headImpulse = dt * gravity / g.length;
    const double k = .25 * headImpulse * compliance * dt * aperture;
    next.next.speed = ((1.-k) * state.speed + headImpulse * (head[0]-head[1])) / (1.+k);
    const double midpoint = .5 * (state.speed + next.next.speed);
    next.flow = aperture * midpoint;
    next.transfer = dt * next.flow;
    next.throughflowCfl = std::abs(midpoint) * dt / g.length;
    if (!std::isfinite(next.throughflowCfl) || next.throughflowCfl > 1.) {
        error = "wet link throughflow exceeds one link inventory; substep required";
        return false;
    }
    next.next.water[0] -= next.transfer;
    next.next.water[1] += next.transfer;
    const double leftMid = .5 * (head[0] + next.next.water[0] / g.surfaceArea[0]);
    const double rightMid = .5 * (head[1] + next.next.water[1] / g.surfaceArea[1]);
    if (!(next.next.water[0] / g.surfaceArea[0] >= portTop &&
          next.next.water[1] / g.surfaceArea[1] >= portTop)) {
        error = "liquid link would expose a port within the step";
        return false;
    }
    next.pressureJump = density * gravity * (leftMid-rightMid);
    next.work = next.pressureJump * next.transfer;
    next.linkKineticChange = density * aperture * g.length * midpoint * (next.next.speed-state.speed);
    const double roundoff = 4096. * std::numeric_limits<double>::epsilon();
    if (!std::isfinite(next.work) || !std::isfinite(next.linkKineticChange) ||
        std::abs(next.work-next.linkKineticChange) > roundoff*(1.+std::abs(next.work))) {
        error = "liquid aperture pressure work does not close";
        return false;
    }
    output = next;
    error.clear();
    return true;
}

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

long double Total(const State& state) { return static_cast<long double>(state.water[0]) + state.water[1]; }

long double Potential(const Geometry& g, const State& state) {
    long double energy = 0.;
    for (int i = 0; i < 2; ++i) {
        const long double volume = state.water[i];
        energy += .5L * density * gravity * volume * volume / g.surfaceArea[i];
    }
    return energy;
}

long double Kinetic(const Geometry& g, const State& state) {
    const long double speed = state.speed;
    return .5L * density * g.width * g.openingHeight * g.length * speed * speed;
}

void StationaryAndThin(Checks& checks) {
    const Geometry original{{1.5, .75}, 2., .001, .2, .125};
    constexpr double dt = 1./60.;
    Step step;
    std::string error;
    const State level{{1.5, .75}, 0.};
    checks.Expect(Advance(original, level, dt, step, error) && step.next.water == level.water &&
                  step.next.speed == 0. && step.flow == 0. && step.pressureJump == 0.,
                  "equal atmospheric heads stay hydrostatic despite unequal storage cross-sections");

    const State unequal{{1.875, .5625}, 0.}; // H_left=1.25 m, H_right=.75 m.
    double previousFlow = 0., previousCfl = 0.;
    for (double gap : {.001, .0005, .00025, std::ldexp(1., -32)}) {
        auto g = original;
        g.openingHeight = gap;
        const bool accepted = Advance(g, unequal, dt, step, error);
        checks.Expect(accepted, accepted ? "pressure advances through a genuine fully wet rectangular link" : error.c_str());
        if (!accepted)
            continue;
        const double exactArea = 2. * gap, exactHeadDifference = .5;
        const double velocityLimit = .5 * dt * gravity * exactHeadDifference / g.length;
        checks.Expect(step.aperture == exactArea && step.transfer > 0. &&
                      std::abs(step.flow/exactArea-velocityLimit) < 1e-5,
                      "flow is positive and approaches zero linearly with measured aperture area", step.flow);
        checks.Expect(std::abs(Total(step.next)-Total(unequal)) < 4e-16L &&
                      std::abs((Potential(g, step.next)-Potential(g, unequal))+step.work) < 2e-12L &&
                      std::abs(Kinetic(g, step.next)-Kinetic(g, unequal)-step.work) < 1e-12L,
                      "volume, reservoir potential and link kinetic work independently balance", step.work);
        if (previousFlow > 0.)
            checks.Expect(step.flow < previousFlow && std::abs(step.throughflowCfl-previousCfl) < 1e-5,
                          "a narrower opening lowers volume flow without inventing a lower transit CFL", step.throughflowCfl);
        previousFlow = step.flow;
        previousCfl = step.throughflowCfl;
    }

    auto sealed = original;
    sealed.openingHeight = 0.;
    checks.Expect(Advance(sealed, unequal, dt, step, error) && step.next.water == unequal.water &&
                  step.next.speed == 0. && step.flow == 0. && step.aperture == 0. && step.work == 0.,
                  "matched sealed geometry retains its unequal head and carries exactly zero flux");
    const auto saved = step;
    auto unvented = original;
    unvented.vented = false;
    checks.Expect(!Advance(unvented, unequal, dt, step, error) && step.next.water == saved.next.water,
                  "unvented head space rejects transactionally");
    auto partial = original;
    partial.sill = .7495;
    checks.Expect(!Advance(partial, unequal, dt, step, error) && step.next.water == saved.next.water,
                  "a partly dry port is outside this fixed-aperture model");
    auto veryNarrow = original;
    veryNarrow.openingHeight = std::ldexp(1., -32);
    auto rapid = unequal;
    rapid.speed = 10.;
    checks.Expect(!Advance(veryNarrow, rapid, dt, step, error) && error.find("throughflow") != std::string::npos &&
                  step.next.water == saved.next.water,
                  "even a tiny wet aperture rejects when liquid travels beyond its link volume");
    const bool shortStep = Advance(veryNarrow, rapid, dt/2, step, error);
    checks.Expect(shortStep && step.throughflowCfl < 1. &&
                  step.transfer > 0., "a resolved shorter interval transports the same fast wet link", step.throughflowCfl);
}

void Convergence(Checks& checks) {
    const Geometry g{{1.5, .75}, 2., .001, .2, .125};
    const State initial{{1.545, .75}, .125};
    constexpr double horizon = 1.;
    const double beta = 1./g.surfaceArea[0]+1./g.surfaceArea[1];
    const double area = g.width*g.openingHeight;
    const double omega = std::sqrt(gravity*beta*area/g.length);
    const double head = initial.water[0]/g.surfaceArea[0]-initial.water[1]/g.surfaceArea[1];
    const double exactSpeed = initial.speed*std::cos(omega*horizon)+
        gravity*head/(g.length*omega)*std::sin(omega*horizon);
    const double exactHead = head*std::cos(omega*horizon)-beta*area*initial.speed/omega*std::sin(omega*horizon);
    const long double energy0 = Potential(g, initial)+Kinetic(g, initial);
    double previousSpeedError = 0., previousHeadError = 0.;
    for (int count : {15, 30, 60, 120, 240}) {
        State state = initial;
        double peakCfl = 0.;
        bool advanced = true;
        int rejectedAt = 0;
        std::string error;
        for (int i = 0; i < count; ++i) {
            Step step;
            if (!Advance(g, state, horizon/count, step, error)) {
                advanced = false;
                rejectedAt = i;
                break;
            }
            state = step.next;
            peakCfl = std::max(peakCfl, step.throughflowCfl);
        }
        if (count == 15) {
            checks.Expect(!advanced && rejectedAt > 0 && error.find("throughflow") != std::string::npos,
                          "coarse trajectory enters an unresolved liquid throughflow regime", rejectedAt);
            continue;
        }
        checks.Expect(advanced, advanced ? "vented thin-link trajectory completes within explicit liquid CFL" : error.c_str(), peakCfl);
        if (!advanced)
            continue;
        const double actualHead = state.water[0]/g.surfaceArea[0]-state.water[1]/g.surfaceArea[1];
        const double speedError = std::abs(state.speed-exactSpeed), headError = std::abs(actualHead-exactHead);
        checks.Expect((previousSpeedError == 0. || previousSpeedError/speedError > 3.9) &&
                      (previousHeadError == 0. || previousHeadError/headError > 3.9) &&
                      std::abs(Total(state)-Total(initial)) < 2e-15L &&
                      std::abs(Potential(g, state)+Kinetic(g, state)-energy0) < 2e-11L,
                      "head and flow converge at second order without volume or mechanical-energy drift", speedError);
        std::printf("TRAJECTORY steps=%d speed_error_m_s=%.17g head_error_m=%.17g cfl=%.17g\n",
                    count, speedError, headError, peakCfl);
        previousSpeedError = speedError;
        previousHeadError = headError;
    }
}

} // namespace

int main() {
    Checks checks;
    StationaryAndThin(checks);
    Convergence(checks);
    std::printf("SUMMARY atmospheric reservoir %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
