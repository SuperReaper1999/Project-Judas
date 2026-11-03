#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr double density = 1000., gravity = 9.81, h = .125, dt = 1. / 60.;

struct Column {
    double bottom, surface, area, wallSpeed;
    bool vented = true;
};

struct Projection {
    std::vector<double> position, pressure, velocity;
    double bottom = 0., surface = 0., wallForce = 0.;
    double beforeVolume = 0., afterVolume = 0., continuity = 0.;
    double pressureWork = 0., kineticChange = 0., correctionEnergy = 0.;
};

std::vector<double> Centres(double bottom, double surface) {
    std::vector<double> centres;
    for (int cell = int(std::floor(bottom/h)); cell*h < surface; ++cell) {
        const double lo = std::max(bottom, cell*h), hi = std::min(surface, (cell+1)*h);
        if (hi > lo)
            centres.push_back(lo + .5*(hi-lo));
    }
    return centres;
}

double ClippedVolume(double bottom, double surface, double area) {
    double volume = 0.;
    for (int cell = 0; cell < 32; ++cell) {
        const double lo = std::max(bottom, cell*h), hi = std::min(surface, (cell+1)*h);
        volume += std::max(0., hi-lo)*area;
    }
    return volume;
}

// Liquid-only pressure with impermeable prescribed piston flux below and
// an atmospheric-pressure Dirichlet trace at the moving upper interface.
// The top flux advects the interface. It is not discarded as water outflow.
bool Project(const Column& c, const std::vector<double>& predicted,
             Projection& output, std::string& error) {
    if (!c.vented) {
        error = "atmospheric pressure requires a known vented free surface";
        return false;
    }
    if (!(c.bottom >= 0. && c.surface <= 32*h && c.surface > c.bottom && c.area > 0.) ||
        !std::isfinite(c.area) || !std::isfinite(c.wallSpeed)) {
        error = "invalid atmospheric column";
        return false;
    }
    Projection next;
    next.position = Centres(c.bottom, c.surface);
    next.position.insert(next.position.begin(), c.bottom);
    next.position.push_back(c.surface);
    const auto cells = next.position.size() - 2;
    if (cells == 0 || predicted.size() != cells + 1 ||
        std::any_of(predicted.begin(), predicted.end(), [](double v) { return !std::isfinite(v); })) {
        error = "liquid predictor does not match the wet pressure graph";
        return false;
    }

    // Face f joins position[f] and position[f+1]. The positive pressure
    // jump drives upward flow. Interior and atmospheric links contribute to
    // the liquid divergence equation. The piston velocity is prescribed.
    std::vector<double> mobility(cells+1), diagonal(cells), upper(cells), rhs(cells);
    for (std::size_t f = 0; f <= cells; ++f) {
        const double length = next.position[f+1]-next.position[f];
        if (!(length > 0.)) {
            error = "wet pressure links are unresolved at chart precision";
            return false;
        }
        mobility[f] = dt/(density*length);
    }
    for (std::size_t i = 0; i < cells; ++i) {
        diagonal[i] = mobility[i+1] + (i == 0 ? 0. : mobility[i]);
        upper[i] = i+1 < cells ? -mobility[i+1] : 0.;
        rhs[i] = (i == 0 ? c.wallSpeed : predicted[i]) - predicted[i+1];
    }
    for (std::size_t i = 1; i < cells; ++i) {
        if (!(diagonal[i-1] > 0.) || !std::isfinite(diagonal[i-1])) {
            error = "liquid pressure solve lost its positive pivot";
            return false;
        }
        const double factor = -mobility[i] / diagonal[i-1];
        diagonal[i] -= factor*upper[i-1];
        rhs[i] -= factor*rhs[i-1];
    }
    next.pressure.resize(cells+2);
    for (std::size_t i = cells; i-- > 0; ) {
        if (!(diagonal[i] > 0.) || !std::isfinite(diagonal[i])) {
            error = "liquid pressure solve lost its positive pivot";
            return false;
        }
        next.pressure[i+1] = (rhs[i] - upper[i]*next.pressure[i+2])/diagonal[i];
    }
    next.pressure.front() = next.pressure[1] + (c.wallSpeed-predicted.front())/mobility.front();
    if (!(next.pressure.front() >= 0.) || !std::isfinite(next.pressure.front())) {
        error = "piston contact would require unsupported liquid tension";
        return false;
    }
    next.velocity.resize(cells+1);
    next.velocity.front() = c.wallSpeed;
    for (std::size_t f = 1; f <= cells; ++f)
        next.velocity[f] = predicted[f] + mobility[f]*(next.pressure[f]-next.pressure[f+1]);
    for (std::size_t f = 0; f <= cells; ++f) {
        const double mass = density*c.area*(next.position[f+1]-next.position[f]);
        const double change = next.velocity[f]-predicted[f];
        next.kineticChange += mass*(predicted[f]+.5*change)*change;
        next.correctionEnergy += .5*mass*change*change;
        if (f > 0)
            next.continuity = std::max(next.continuity, std::abs(next.velocity[f]-next.velocity[f-1]));
    }
    next.bottom = c.bottom + dt*next.velocity.front();
    next.surface = c.surface + dt*next.velocity.back();
    if (!(next.bottom >= 0. && next.surface <= 32*h && next.surface > next.bottom) ||
        !std::isfinite(next.kineticChange) || !std::isfinite(next.correctionEnergy)) {
        error = "liquid projection cannot publish its moving boundaries";
        return false;
    }
    next.beforeVolume = ClippedVolume(c.bottom, c.surface, c.area);
    next.afterVolume = ClippedVolume(next.bottom, next.surface, c.area);
    next.wallForce = -next.pressure.front()*c.area;
    next.pressureWork = -next.wallForce*dt*c.wallSpeed;
    const double roundoff = 4096.*std::numeric_limits<double>::epsilon();
    double speedScale = 1.+std::abs(c.wallSpeed);
    for (double speed : predicted)
        speedScale = std::max(speedScale, std::abs(speed));
    const double workScale = 1.+std::abs(next.pressureWork)+std::abs(next.kineticChange)+next.correctionEnergy;
    if (next.continuity > roundoff*speedScale ||
        std::abs(next.afterVolume-next.beforeVolume) > roundoff*(1.+next.beforeVolume) ||
        !std::isfinite(workScale) ||
        std::abs(next.pressureWork-next.kineticChange-next.correctionEnergy) > roundoff*workScale) {
        error = "liquid projection did not close continuity, storage or pressure work";
        return false;
    }
    output = std::move(next);
    error.clear();
    return true;
}

struct PatchLoad { double area = 0., force = 0.; };

PatchLoad VerticalPatch(const Projection& p, double bottom, double top, double width) {
    PatchLoad result;
    for (std::size_t i = 1; i < p.position.size(); ++i) {
        const double lo = std::max(bottom, p.position[i-1]), hi = std::min(top, p.position[i]);
        if (hi <= lo)
            continue;
        const auto trace = [&](double y) {
            const double fraction = (y-p.position[i-1])/(p.position[i]-p.position[i-1]);
            return p.pressure[i-1] + fraction*(p.pressure[i]-p.pressure[i-1]);
        };
        const double area = (hi-lo)*width;
        result.area += area;
        result.force += area*.5*(trace(lo)+trace(hi));
    }
    return result;
}

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

std::vector<double> UniformPredictor(const Column& c, double oldVelocity) {
    return std::vector<double>(Centres(c.bottom, c.surface).size()+1, oldVelocity-gravity*dt);
}

void Hydrostatic(Checks& checks) {
    const Column c{.03125, .587, .375, 0.};
    Projection p;
    std::string error;
    const bool accepted = Project(c, UniformPredictor(c, 0.), p, error);
    checks.Expect(accepted, accepted ? "solve a vented liquid column with a cut top cell" : error.c_str());
    if (!accepted)
        return;
    double pressureError = 0.;
    for (std::size_t i = 0; i < p.pressure.size(); ++i)
        pressureError = std::max(pressureError, std::abs(p.pressure[i]-density*gravity*(c.surface-p.position[i])));
    checks.Expect(pressureError < 1e-10 && p.pressure.back() == 0. && p.continuity < 1e-12,
                  "static pressure matches analytic hydrostatics and atmospheric trace", pressureError);
    const double force = -density*gravity*c.area*(c.surface-c.bottom);
    checks.Expect(std::abs(p.wallForce-force) < 1e-10 && p.beforeVolume == p.afterVolume &&
                  std::abs(p.pressureWork-p.kineticChange-p.correctionEnergy) < 1e-12,
                  "stationary piston load and work agree with independent column mass", p.wallForce);

    // Both probes share the top coarse cell. The second starts 4 mm above
    // the actual surface. Positive coarse wet occupancy is not wet ownership.
    const auto wet = VerticalPatch(p, .55, .61, .25);
    const auto dry = VerticalPatch(p, .591, .61, .25);
    const double expectedWet = .125*density*gravity*(c.surface-.55)*(c.surface-.55);
    checks.Expect(std::abs(wet.force-expectedWet) < 1e-11 && wet.area > 0.,
                  "partially wet wall patch integrates actual affine traction", wet.force);
    checks.Expect(dry.area == 0. && dry.force == 0. && int(.591/h) == int(c.surface/h),
                  "four-millimetre dry gap leaves zero force in a partly wet cell", dry.force);
    const double wrongForce = (.61-.591)*.25*p.pressure[p.pressure.size()-2];
    checks.Expect(wrongForce > .01, "coarse wet-cell classification would falsely load the dry wall", wrongForce);

    auto closed = c;
    closed.vented = false;
    const auto saved = p.pressure;
    checks.Expect(!Project(closed, UniformPredictor(closed, 0.), p, error) && p.pressure == saved,
                  "unvented air pocket rejects instead of silently assuming atmosphere");
}

void MovingPiston(Checks& checks) {
    const Column c{.03125, .62, .375, 1.2};
    constexpr double oldSpeed = 1.;
    Projection p;
    std::string error;
    const auto predictor = UniformPredictor(c, oldSpeed);
    const bool accepted = Project(c, predictor, p, error);
    checks.Expect(accepted, accepted ? "solve a moving piston with an atmospheric free surface" : error.c_str());
    if (!accepted)
        return;
    const double mass = density*c.area*(c.surface-c.bottom), acceleration = (c.wallSpeed-oldSpeed)/dt;
    const double expectedForce = -mass*(gravity+acceleration);
    checks.Expect(std::abs(p.wallForce-expectedForce) < 1e-9 && p.continuity < 1e-12,
                  "piston traction recovers independent liquid added mass and gravity", p.wallForce);
    const double birth = ClippedVolume(.625, p.surface, c.area);
    const double exactBirth = (.62+dt*c.wallSpeed-.625)*c.area;
    checks.Expect(std::abs(p.afterVolume-p.beforeVolume) < 1e-14 &&
                  std::abs(p.surface-(c.surface+dt*c.wallSpeed)) < 1e-14 &&
                  std::abs(birth-exactBirth) < 1e-14 && birth > 0.,
                  "surface flux opens real wet storage in the next coarse cell without losing water", birth);
    const double frozenError = ClippedVolume(p.bottom, c.surface, c.area)-p.beforeVolume;
    checks.Expect(std::abs(frozenError+c.area*c.wallSpeed*dt) < 1e-14 && std::abs(frozenError) > .001,
                  "discarding the atmospheric-boundary flux would destroy water", frozenError);
    const double workError = p.pressureWork-p.kineticChange-p.correctionEnergy;
    checks.Expect(std::abs(workError) < 1e-10, "moving-wall impulse closes discrete projection work", workError);

    // Exercise a genuine nonuniform predictor. An analytical pressure
    // difference follows each link's required change, independent of Thomas.
    auto disturbed = predictor;
    for (std::size_t i = 0; i < disturbed.size(); ++i)
        disturbed[i] += .06*std::sin(double(i));
    const bool corrected = Project(c, disturbed, p, error);
    double forceError = 1.;
    if (corrected) {
        double expectedPressure = 0.;
        for (std::size_t i = 0; i < disturbed.size(); ++i)
            expectedPressure += density*(p.position[i+1]-p.position[i])*(c.wallSpeed-disturbed[i])/dt;
        forceError = std::abs(p.wallForce+c.area*expectedPressure);
    }
    checks.Expect(corrected && forceError < 1e-9 && p.continuity < 1e-12,
                  "nonuniform pressure predictor solves the same independent flux/traction constraints", forceError);

    auto tensile = c;
    tensile.wallSpeed = 0.;
    const auto safe = p.pressure;
    checks.Expect(!Project(tensile, UniformPredictor(tensile, oldSpeed), p, error) && p.pressure == safe,
                  "piston separation requiring unsupported tension rejects transactionally");
}

} // namespace

int main() {
    Checks checks;
    Hydrostatic(checks);
    MovingPiston(checks);
    std::printf("SUMMARY atmospheric liquid column %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
