#include "FluidWorld.h"
#include "GravityField.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

struct ConstantGravity final : GravityField {
    explicit ConstantGravity(glm::vec3 g) : acceleration(g) {}
    glm::vec3 Sample(const glm::vec3&) const override { return acceleration; }
    glm::vec3 acceleration;
};

int main(int argc, char** argv) {
    const std::size_t count = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 8;
    const bool rotated = argc > 2 && std::string(argv[2]) == "rotated";
    const bool supported = argc < 4 || std::string(argv[3]) != "free";
    if (count < 2 || count > 32) return 2;
    const glm::quat turn = rotated
        ? glm::angleAxis(.72f, glm::normalize(glm::vec3(1, 2, 3)))
        : glm::quat(1, 0, 0, 0);
    const glm::vec3 up = turn * glm::vec3(0, 1, 0);
    const glm::vec3 origin = rotated ? glm::vec3(.24f, -.13f, .36f) : glm::vec3(0);
    FluidSettings settings;
    settings.substeps = 1;
    settings.smoothingRadius = .095f;
    constexpr float spacing = .05f;
    constexpr float dt = 1.f / 60;
    constexpr float g = 9.81f;
    const double h = settings.smoothingRadius;
    const double pi = std::acos(-1.);
    const double coefficient = 315 / (64 * pi * std::pow(h, 9));
    const auto kernel = [&](double distance) {
        return coefficient * std::pow(h*h-distance*distance, 3);
    };
    const double neighbor = kernel(spacing);
    const double slope = 6 * coefficient * spacing * std::pow(h*h-spacing*spacing, 2);

    // h < 2s makes the kernel system tridiagonal. Choose positive parcel masses
    // so that every rest density, including the two ends, is exactly saturated.
    std::vector<double> diagonal(count, kernel(0));
    std::vector<double> mass(count, settings.restDensity);
    for (std::size_t i = 1; i < count; ++i) {
        const double factor = neighbor / diagonal[i-1];
        diagonal[i] -= factor * neighbor;
        mass[i] -= factor * mass[i-1];
    }
    for (std::size_t i = count; i-- > 0;) {
        if (i+1 < count) mass[i] -= neighbor*mass[i+1];
        mass[i] /= diagonal[i];
        if (!(mass[i] > 0)) return 3;
    }

    // For each interior gap k, the required outward pair impulse is the
    // total upper weight × dt. The density multipliers share that impulse.
    // Propagate their affine dependence on lambda[0] and intersect its
    // admissible interval. If feasible, v=0 is the unique minimum-energy
    // velocity projection for a resting supported column in this model.
    double lower = 0, upper = INFINITY, a = 1, b = 0, load = 0;
    std::vector<double> pairImpulse(count-1);
    for (std::size_t i = count; i-- > 1;) {
        load += mass[i]*g*dt;
        pairImpulse[i-1] = load;
    }
    for (std::size_t i = 0; i+1 < count; ++i) {
        b = (pairImpulse[i]/slope-mass[i+1]*b)/mass[i];
        a *= -mass[i+1]/mass[i];
        if (a < 0) upper = std::min(upper, -b/a);
        else lower = std::max(lower, -b/a);
    }

    FluidWorld world(settings);
    const float bottom = settings.particleRadius;
    for (std::size_t i = 0; i < count; ++i)
        world.AddParticle(origin+up*(bottom+float(i)*spacing), glm::vec3(0), float(mass[i]));
    const auto original = world.Particles();
    const double totalMass = std::accumulate(mass.begin(), mass.end(), 0.);
    FluidBoxCollider floor{};
    floor.owner = {1,1};
    floor.previousPose.position = floor.currentPose.position = origin-up*.125f;
    floor.previousPose.rotation = floor.currentPose.rotation = turn;
    floor.halfExtents = {1,.125f,1};
    const std::vector<FluidBoxCollider> boxes = supported
        ? std::vector<FluidBoxCollider>{floor} : std::vector<FluidBoxCollider>{};
    const ConstantGravity gravity(-g*up);
    std::vector<FluidContactImpulse> reactions;
    const int horizon = supported ? 600 : 120;
    std::cout << std::setprecision(17) << "layers=" << count << " support=" << supported
              << " rotated=" << rotated << " total_mass=" << totalMass
              << " support_target=" << totalMass*g*dt << " lower=" << lower << " upper=" << upper
              << " hydrostatic_impulse_feasible=" << (lower <= upper) << '\n';
    std::cout << "step,top_height,top_speed,position_error,velocity_error,kinetic_energy,support_impulse\n";
    for (int step = 1; step <= horizon; ++step) {
        world.Step(dt, gravity, boxes, {}, {}, &reactions);
        const auto& p = world.Particles();
        const double expectedSpeed = supported ? 0 : -double(g)*dt*step;
        const double expectedTravel = supported ? 0 : -double(g)*dt*dt*step*(step+1)/2;
        double maxPositionError = 0, maxVelocityError = 0;
        for (std::size_t i = 0; i < count; ++i) {
            maxPositionError = std::max(maxPositionError, glm::length(glm::dvec3(p[i].position)-
                glm::dvec3(original[i].position)-expectedTravel*glm::dvec3(up)));
            maxVelocityError = std::max(maxVelocityError, glm::length(glm::dvec3(p[i].velocity)-
                expectedSpeed*glm::dvec3(up)));
        }
        const double support = std::accumulate(reactions.begin(), reactions.end(), 0.,
            [&](double impulse, const FluidContactImpulse& reaction) {
                return impulse-glm::dot(glm::dvec3(up),glm::dvec3(reaction.impulse));
            });
        std::cout << step << ',' << glm::dot(p.back().position-origin,up) << ','
                  << glm::dot(p.back().velocity,up) << ',' << maxPositionError << ','
                  << maxVelocityError << ',' << world.GetDiagnostics().kineticEnergy << ',' << support << '\n';
    }
}
