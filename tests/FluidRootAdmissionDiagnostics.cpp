// Known-failing diagnostics for unresolved particle-column admission.
// New controls; no previously accepted gate has been removed or relaxed.
#include "FluidHydrostatics.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::cout << "FAIL " << name << '\n'; }
}
void Near(double value, double expected, double tolerance, const char* name) {
    Check(std::abs(value - expected) <= tolerance, name);
    if (std::abs(value - expected) > tolerance)
        std::cout << " observed=" << value << " expected=" << expected << " tolerance=" << tolerance << '\n';
}
void VectorNear(glm::vec3 value, glm::vec3 expected, float tolerance, const char* name) {
    Check(glm::length(value - expected) <= tolerance, name);
}
struct Uniform : GravityField {
    glm::vec3 value;
    explicit Uniform(glm::vec3 gravity) : value(gravity) {}
    glm::vec3 Sample(const glm::vec3&) const override { return value; }
};
constexpr float spacing = .0625f;
const BodyHandle owner{37, 901};
struct Frame {
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 translation{0};
    glm::vec3 Point(glm::vec3 point) const { return translation + rotation * point; }
    glm::vec3 Vector(glm::vec3 vector) const { return rotation * vector; }
};
void FineFullyImmersedSourceTwins() {
    const auto volume = MakeBoxFluidVolume(glm::vec3(.3f));
    const glm::vec3 localCenter(0, -.75f, 0);
    const Frame frames[]{Frame{}, Frame{
        glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3))), {2, -1, 3}}};
    for (const auto& frame : frames) {
        std::vector<FluidParticle> source, material;
        source.reserve(32 * 32 * 32);
        material.reserve(32 * 32 * 32);
        for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x) for (int y = 0; y < 32; ++y) {
            const glm::vec3 local(-1 + (x + .5f) * spacing,
                                  -(y + .5f) * spacing,
                                  -1 + (z + .5f) * spacing);
            const auto point = frame.Point(local);
            const FluidParticle particle{point, point, {}, 1000 * spacing * spacing * spacing, {}};
            source.push_back(particle);
            if (!glm::all(glm::lessThan(glm::abs(local - localCenter), glm::vec3(.3f))))
                material.push_back(particle);
        }
        // Match the original fine fully immersed known-geometry material.
        // Centre removal leaves a .0125 m represented contact gap. The full
        // precut source and explicit dry source share these exact particles.
        Check(source.size() == 32768 && material.size() == 31768,
              "fine fully immersed source twins preserve original material fixture");
        FluidHydrostaticField wet, dry;
        wet.Build(material, 1000);
        dry.Build(material, 1000);
        const BodyTransform pose{frame.Point(localCenter), frame.rotation};
        const Uniform gravity(frame.Vector({0, -9.81f, 0}));
        const auto measure = [&](const FluidHydrostaticField& field) {
            return EvaluateFluidHydrostatics(field, volume, pose, gravity,
                                             FluidVolumeGeometry::DisplacedSolid, owner);
        };
        Near(measure(wet).fraction, 0, 0,
             "unowned fine contact gap cannot prove fully immersed source water");
        // The first exterior column is one grid step from this root column,
        // strictly inside the reconstruction halo. Its water cannot erase
        // the root's independently resolved vertical cavity.
        Near(wet.Query(frame.Point({.2625f, -.75f, 0}), frame.Vector({0, 1, 0}),
                       .0375f, volume.boundRadius, frame.Vector({1, 0, 0}), nullptr, true).fraction,
             0, 0, "near-wall resolved air cannot borrow exterior neighbor rows");
        wet.RetainDisplacedBoundary(owner, volume, pose, source);
        dry.RetainDisplacedBoundary(owner, volume, pose, material);
        const auto wetResult = measure(wet), dryResult = measure(dry);
        Near(wetResult.fraction, 1, 2e-6,
             "owned fine precut full source retains genuine full immersion");
        VectorNear(wetResult.buoyancyForce,
                   frame.Vector({0, static_cast<float>(1000 * 9.81 * volume.totalVolume), 0}), .02f,
                   "owned fine full immersion retains Archimedes force");
        Near(dryResult.fraction, 0, 0,
             "owned fine explicit dry source stays dry with identical material");
        VectorNear(dryResult.buoyancyForce, {}, 0,
                   "owned fine explicit dry source has no hydrostatic force");
    }
}

void ConnectedRootAirControls() {
    const Frame frames[]{Frame{}, Frame{
        glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3))), {2, -1, 3}}};
    struct Case {
        float rowHeight;
        bool localWater, connected;
        double expected;
        const char* label;
    };
    const Case cases[]{
        {.125f, false, true, 0, "connected root retains resolved .1875 m air gap"},
        {.125f, true, true, 1, "actual local water retains connected root admission"},
        {.046875f, false, true, 1, "unchanged spacing resolution closes .03125 m disorder"},
        {.125f, false, false, 1, "legacy unconnected mean retains neighbor reconstruction"},
    };
    for (const auto& frame : frames) for (const auto& scenario : cases) {
        std::vector<FluidParticle> particles;
        const auto append = [&](glm::vec3 p) {
            p = frame.Point(p);
            particles.push_back({p, p, {}, 1000 * spacing * spacing * spacing, {}});
        };
        append({0, -scenario.rowHeight, 0});
        append({0, scenario.rowHeight, 0});
        append({spacing, 0, 0});
        if (scenario.localWater) append({0, 0, 0});
        FluidHydrostaticField field;
        field.Build(particles, 1000);
        Near(field.Query(frame.Point({0, 0, 0}), frame.Vector({0, 1, 0}),
                         0, .2f, frame.Vector({1, 0, 0}), nullptr, scenario.connected).fraction,
             scenario.expected, 0, scenario.label);
    }
}

void MissingNominalRoot() {
    const Frame frames[]{Frame{}, Frame{
        glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3))), {2, -1, 3}}};
    for (const auto& frame : frames) {
        std::vector<FluidParticle> particles;
        for (const auto local : {glm::vec3(-2, -.5f, 0), glm::vec3(.1f, -.5f, 0)}) {
            const auto point = frame.Point(local);
            particles.push_back({point, point, {}, 15.625f, {}});
        }
        FluidHydrostaticField field;
        field.Build(particles, 1000);
        // The actual square covers x=[-.025,.225]. Nominal bin0 exists,
        // whereas bin.25 is absent: bin occupancy must not substitute for matter.
        for (float x : {.18f, .3f, -.1f}) for (float half : {0.f, .025f}) {
            Near(field.Query(frame.Point({x, -.5f, 0}), frame.Vector({0, 1, 0}),
                             half, .2f, frame.Vector({1, 0, 0}), nullptr, true).fraction,
                 x == .18f ? 1 : 0, 0,
                 "actual footprint admission is independent of nominal bin presence");
        }
    }
}
} // namespace
int main() {
    FineFullyImmersedSourceTwins();
    ConnectedRootAirControls();
    MissingNominalRoot();
    std::cout << "Root-admission diagnostic: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
