#include "PhysicsWorld.h"
#include "FluidSettlingContract.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

// Independent calm-reservoir controls. The three recorded release surfaces
// come from the unchanged production fixtures, pinned at 90a97547. No PBF or
// hydrostatic sampler supplies the force oracle. Original four-second results
// remain recorded alongside the separately authorized window/decay contract.
namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cout << "FAIL " << label << '\n'; }
}
struct Parameters { const char* name; double half, mass, surface; };
constexpr Parameters cases[] = {
    {"half_020", .30000001192092896, 108.00000762939453, 1.7942163720726967},
    {"half_025", .30000001192092896, 108.00000762939453, 1.7094379663467407},
    {"light_80kg", .2714417576789856, 80, 1.7985037341713905}
};
constexpr double gravity = 9.8100004196166992, release = .69999998807907104;
constexpr double density = 1000, dragRate = 2; // unchanged authored coefficient
struct State { double y, v, dissipated; };
struct Reference { State state; double energy, initialEnergy, budgetError, maxEnergyIncrease; };
double Volume(const Parameters& p) { return 8 * p.half * p.half * p.half; }
double Fraction(const Parameters& p, double y) {
    return std::clamp((p.surface - y + p.half) / (2 * p.half), 0., 1.);
}
double Potential(const Parameters& p, double y) {
    const double area = 4 * p.half * p.half, stiffness = density * gravity * area;
    const double equilibrium = p.surface + p.half - p.mass / (density * area);
    const double low = p.surface - p.half, high = p.surface + p.half;
    const auto partial = [&](double height) { return .5 * stiffness * (height - equilibrium) * (height - equilibrium); };
    if (y < low) return partial(low) + (p.mass * gravity - density * gravity * Volume(p)) * (y - low);
    if (y > high) return partial(high) + p.mass * gravity * (y - high);
    return partial(y);
}
State Derivative(const Parameters& p, State s) {
    const double fraction = Fraction(p, s.y);
    return {s.v, -gravity + density * gravity * Volume(p) * fraction / p.mass - dragRate * fraction * s.v,
            p.mass * dragRate * fraction * s.v * s.v};
}
State Advance(State a, State derivative, double dt) {
    return {a.y + dt * derivative.y, a.v + dt * derivative.v, a.dissipated + dt * derivative.dissipated};
}
Reference IntegrateReference(const Parameters& p, int steps) {
    const double dt = 4. / steps;
    State s{release, 0, 0};
    const double initial = Potential(p, s.y);
    double previous = initial, maximumIncrease = 0;
    for (int i = 0; i < steps; ++i) {
        const auto a = Derivative(p, s), b = Derivative(p, Advance(s, a, dt * .5));
        const auto c = Derivative(p, Advance(s, b, dt * .5)), d = Derivative(p, Advance(s, c, dt));
        s.y += dt * (a.y + 2 * b.y + 2 * c.y + d.y) / 6;
        s.v += dt * (a.v + 2 * b.v + 2 * c.v + d.v) / 6;
        s.dissipated += dt * (a.dissipated + 2 * b.dissipated + 2 * c.dissipated + d.dissipated) / 6;
        const double energy = .5 * p.mass * s.v * s.v + Potential(p, s.y);
        maximumIncrease = std::max(maximumIncrease, energy - previous);
        previous = energy;
    }
    return {s, previous, initial, previous + s.dissipated - initial, maximumIncrease};
}
double ApplyCalmStep(PhysicsWorld& world, BodyHandle body, const Parameters& p, float dt,
                     bool buoyancy, bool drag) {
    const float mass=world.GetMass(body);
    const float fraction=float(Fraction(p,world.GetTransform(body).position.y));
    world.ApplyLinearAcceleration(body,{0,-float(gravity),0},dt);
    const float force=buoyancy?float(density*gravity*Volume(p)*fraction):0;
    const float relative=world.GetLinearVelocity(body).y+force*(dt/mass);
    const float alpha=drag?-std::expm1(-float(dragRate)*fraction*dt):0;
    const float dragForce=-alpha*mass*relative/dt;
    const float predicted=world.GetLinearVelocity(body).y+(force+dragForce)*(dt/mass);
    world.ApplyForce(body,{0,force+dragForce,0});
    world.Step(dt);
    return std::abs(world.GetLinearVelocity(body).y-predicted);
}
struct NativeResult { double y, v, kickResidual, maxSpeed; };
NativeResult IntegrateNative(const Parameters& p, int hz, bool buoyancy = true, bool drag = true) {
    PhysicsWorld world;
    Check(world.Init(), "native reference world initializes");
    const float dt = 1.f / hz;
    const auto body = world.CreateDynamicBox({0, float(release), 0}, glm::vec3(float(p.half)), float(p.mass), 0, 0);
    double residual = 0, maxSpeed = 0;
    for (int step = 0; step < 4 * hz; ++step) {
        residual=std::max(residual,ApplyCalmStep(world,body,p,dt,buoyancy,drag));
        const float actual=world.GetLinearVelocity(body).y;
        maxSpeed = std::max(maxSpeed, double(std::abs(actual)));
    }
    return {world.GetTransform(body).position.y, world.GetLinearVelocity(body).y, residual, maxSpeed};
}
FluidTest::SettlingAssessment CalmWindow(const Parameters& p, bool drag) {
    PhysicsWorld world;
    Check(world.Init(), "window reference world initializes");
    const float dt=1.f/60;
    const auto body=world.CreateDynamicBox({0,float(release),0},glm::vec3(float(p.half)),float(p.mass),0,0);
    const auto contract=FluidTest::SettlingContract::Make(dragRate,.5,dt);
    FluidTest::WindowMotion early,late;
    for(std::size_t step=0;step<contract.late.end;++step) {
        ApplyCalmStep(world,body,p,dt,true,drag);
        const double y=world.GetTransform(body).position.y,v=world.GetLinearVelocity(body).y;
        const double fraction=Fraction(p,y); // independent exact plane/box volume
        if(contract.early.Contains(step))early.Add(y,v,fraction);
        if(contract.late.Contains(step))late.Add(y,v,fraction,true,fraction);
    }
    return FluidTest::Assess(contract,early,late,std::sqrt(gravity*p.half));
}
} // namespace
int main() {
    std::cout << std::setprecision(17);
    for (const auto& p : cases) {
        std::array<Reference, 4> references;
        const int referenceSteps[] = {960, 3840, 15360, 61440};
        for (std::size_t i = 0; i < references.size(); ++i) references[i] = IntegrateReference(p, referenceSteps[i]);
        const auto& fine = references.back();
        Check(std::abs(fine.state.v - references[2].state.v) < 2e-6, "independent reference velocity converges");
        Check(std::abs(fine.state.y - references[2].state.y) < 2e-6, "independent reference position converges");
        Check(std::abs(fine.budgetError) < 1e-8 * fine.initialEnergy, "reference work and potential budget closes");
        Check(fine.maxEnergyIncrease < 1e-8 * fine.initialEnergy, "reference energy decays within integration error");
        Check(fine.energy < fine.initialEnergy && fine.state.dissipated > 0, "existing drag dissipates energy");
        Check(std::abs(fine.state.v) > .5, "correct calm model disproves universal four-second speed deadline");
        double previousError = std::numeric_limits<double>::infinity();
        double native60 = 0;
        for (int hz : {60, 120, 240, 960}) {
            const auto actual = IntegrateNative(p, hz);
            const double error = std::abs(actual.v - fine.state.v);
            Check(error < previousError, "native rigid response converges under timestep refinement");
            Check(actual.kickResidual <= 64 * std::numeric_limits<float>::epsilon() * (1 + actual.maxSpeed),
                  "native force kick agrees with prescribed buoyancy and drag");
            previousError = error;
            if (hz == 60) native60 = actual.v;
        }
        Check(std::abs(native60) > .5, "native calm control also exceeds original terminal-speed gate");
        const auto noDrag = IntegrateNative(p, 60, true, false);
        const auto noBuoyancy = IntegrateNative(p, 60, false, true);
        Check(std::abs(noDrag.v - fine.state.v) > 10 * std::abs(native60 - fine.state.v), "reference detects missing drag");
        Check(std::abs(noBuoyancy.v - fine.state.v) > 10 * std::abs(native60 - fine.state.v), "reference detects missing buoyancy");
        Check(CalmWindow(p,true).Passed(), "actual native calm-water motion passes window/decay contract");
        Check(!CalmWindow(p,false).Passed(), "window contract rejects actual native undamped bobbing");
        std::cout << p.name << " reference_v4=" << fine.state.v << " native_v4=" << native60
                  << " energy4=" << fine.energy << " initial_energy=" << fine.initialEnergy
                  << " energy_budget_error=" << fine.budgetError << '\n';
    }
    std::cout << "Fluid settling reference: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
