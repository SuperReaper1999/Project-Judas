#include "FluidHydrostatics.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Near(double observed, double expected, double tolerance, const std::string& label) {
    ++checks;
    if (std::abs(observed - expected) <= tolerance) return;
    ++failures;
    std::cout << "FAIL " << label << " observed=" << observed << " expected=" << expected << '\n';
}
struct Uniform final : GravityField {
    glm::vec3 value;
    explicit Uniform(glm::vec3 g) : value(g) {}
    glm::vec3 Sample(const glm::vec3&) const override { return value; }
};
struct Frame {
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 offset{0};
    glm::vec3 Point(glm::vec3 p) const { return offset + rotation * p; }
    glm::vec3 Vector(glm::vec3 p) const { return rotation * p; }
};
std::vector<FluidParticle> Pool(int hole, const Frame& frame) {
    std::vector<FluidParticle> particles;
    for (int x = -8; x <= 8; ++x) for (int z = -8; z <= 8; ++z) {
        if (std::abs(x) < hole && std::abs(z) < hole) continue;
        for (int y = -4; y <= -1; ++y) {
            const auto p = frame.Point({.25f * x, .25f * y, .25f * z});
            particles.push_back({p, p, glm::vec3(0), 15.625f, glm::vec3(0)});
        }
    }
    return particles;
}
BodyTransform Pose(const Frame& frame) {
    BodyTransform pose;
    pose.position = frame.Point({0, -.5f, 0});
    pose.rotation = frame.rotation;
    return pose;
}
void Load(const FluidHydrostaticField& field, const FluidVolumeQuadrature& volume,
          const Frame& frame, double expected, const std::string& label) {
    const auto result = EvaluateFluidHydrostatics(field, volume, Pose(frame),
                                                 Uniform(frame.Vector({0, -9.81f, 0})));
    Near(result.fraction, expected, 2e-5, label + " fraction");
    const glm::dvec3 force = frame.Vector({0, 1, 0}) * float(1000 * 9.81 * volume.totalVolume * expected);
    Near(glm::length(glm::dvec3(result.buoyancyForce) - force), 0, .005, label + " force");
}
void RingAndShapeCases(const Frame& frame) {
    for (int hole = 0; hole <= 5; ++hole) {
        FluidHydrostaticField field;
        field.Build(Pool(hole, frame), 1000);
        // For hole=1 the nearest represented water starts .125 m from the
        // centre; the .1 m half-width body still has a genuine .025 m dry gap.
        Load(field, MakeBoxFluidVolume(glm::vec3(.1f)), frame, hole ? 0 : 1,
             "box in connected pool/hole " + std::to_string(hole));
    }
    for (bool hole : {false, true}) {
        FluidHydrostaticField field;
        field.Build(Pool(hole ? 3 : 0, frame), 1000);
        Load(field, MakeSphereFluidVolume(.1f), frame, hole ? 0 : 1, "sphere");
        Load(field, MakeCompoundFluidVolume({{{-.3f, 0, 0}, {.1f, .1f, .1f}},
                                             {{.3f, 0, 0}, {.1f, .1f, .1f}}}),
             frame, hole ? 0 : 1, "separated compound");
        CompoundBox child{{0, 0, 0}, {.2f, .1f, .1f}};
        child.rotation = glm::angleAxis(.4f, glm::vec3(0, 1, 0));
        Load(field, MakeCompoundFluidVolume({child}), frame, hole ? 0 : 1, "oriented child");
    }
    {
        // Particle footprints, not their nearest grid-bin centres, define
        // contact. This outward phase shift leaves a .045 m real dry gap.
        std::vector<FluidParticle> particles;
        for (int x = -8; x <= 8; ++x) for (int z = -8; z <= 8; ++z) {
            if (!x && !z) continue;
            const float px = .25f * x + (x > 0 ? .05f : x < 0 ? -.05f : 0);
            const float pz = .25f * z + (z > 0 ? .05f : z < 0 ? -.05f : 0);
            for (int y = -4; y <= -1; ++y) {
                const auto p = frame.Point({px, .25f * y, pz});
                particles.push_back({p, p, glm::vec3(0), 15.625f, glm::vec3(0)});
            }
        }
        FluidHydrostaticField field;
        field.Build(particles, 1000);
        Load(field, MakeBoxFluidVolume(glm::vec3(.13f)), frame, 0,
             "quantized bin cannot replace actual water footprint");
    }
}
void CompoundOwnership(const Frame& frame) {
    FluidHydrostaticField field;
    field.Build(Pool(2, frame), 1000);
    for (bool reversed : {false, true}) {
        std::vector<CompoundBox> children{{{0, 0, 0}, {.1f, .1f, .1f}},
                                          {{.4f, 0, 0}, {.1f, .1f, .1f}}};
        if (reversed) std::swap(children[0], children[1]);
        const auto volume = MakeCompoundFluidVolume(children);
        const auto pose = Pose(frame);
        const FluidSolidQuery solid{volume, pose};
        const auto dry = field.Query(pose.position, frame.Vector({0, 1, 0}), .025f,
                                     volume.boundRadius, frame.Vector({1, 0, 0}), &solid, true);
        Near(dry.fraction, 0, 0, "remote wet child cannot support separated dry child");
        const auto air = field.Query(frame.Point({.2f, -.5f, 0}), frame.Vector({0, 1, 0}),
                                     .025f, volume.boundRadius, frame.Vector({1, 0, 0}), &solid, true);
        Near(air.fraction, 0, 0, "air between compound children is not displaced solid");
    }
    {
        const auto volume = MakeCompoundFluidVolume({{{0, 0, 0}, {.1f, .1f, .1f}},
                                                     {{.3f, 0, 0}, {.2f, .1f, .1f}}});
        const auto pose = Pose(frame);
        const FluidSolidQuery solid{volume, pose};
        const auto dry = field.Query(pose.position, frame.Vector({0, 1, 0}), .025f,
                                     volume.boundRadius, frame.Vector({1, 0, 0}), &solid, true);
        Near(dry.fraction, 0, 0, "dry end of connected body cannot borrow distant wet end");
    }
    // Nine touching pieces exactly occupy the dry square in the particle
    // lattice. The central piece has no direct wet neighbour; legitimate
    // displacement support must traverse the physically connected outer pieces.
    std::vector<CompoundBox> connected;
    for (int x = -1; x <= 1; ++x) for (int z = -1; z <= 1; ++z)
        connected.push_back({{.25f * x, 0, .25f * z}, {.125f, .1f, .125f}});
    Load(field, MakeCompoundFluidVolume(connected), frame, 1, "connected partition displaces water");
    // Without a displaced solid, neither scripts nor swimming queries may
    // bridge the same dry hole merely because it lies inside the outer hull.
    const auto air = field.Query(frame.Point({0, -.5f, 0}), frame.Vector({0, 1, 0}),
                                 .025f, .6f, frame.Vector({1, 0, 0}), nullptr, true);
    Near(air.fraction, 0, 0, "non-displacing physical field query preserves dry hole");
}
void VerticalMean() {
    // All columns exist. Some have a lower surface and therefore already
    // contribute zero occupied height to the established area-weighted mean.
    // A lateral-hole admission rule must not count that same dry height twice.
    std::vector<FluidParticle> particles;
    for (int x = -8; x <= 8; ++x) for (int z = -8; z <= 8; ++z) {
        const bool high = x < 0 || x >= 3;
        for (int y = -4; y <= (high ? 2 : -1); ++y) {
            const glm::vec3 p(.25f * x, .25f * y, .25f * z);
            particles.push_back({p, p, glm::vec3(0), 15.625f, glm::vec3(0)});
        }
    }
    const auto volume = MakeBoxFluidVolume(glm::vec3(.3f));
    BodyTransform pose;
    pose.position = {0, .25f, 0};
    const FluidSolidQuery solid{volume, pose};
    FluidHydrostaticField field;
    field.Build(particles, 1000);
    const glm::vec3 point(.2f, .25f, 0);
    // Independent equal-area square-column calculation for this axis-aligned
    // fixture: columns overlapping the .3 m solid footprint are excluded.
    const double reach = volume.boundRadius + .5;
    double wet = 0, total = 0;
    for (int x = -8; x <= 8; ++x) for (int z = -8; z <= 8; ++z) {
        if (std::abs(.25 * x) < double(.3f) + .125 &&
            std::abs(.25 * z) < double(.3f) + .125) continue;
        const double dx = .25 * x - double(point.x), dz = .25 * z;
        const double distance2 = dx * dx + dz * dz;
        if (distance2 >= reach * reach) continue;
        const double kernel = 1 - distance2 / (reach * reach);
        const double weight = .25 * .25 * kernel * kernel;
        total += weight;
        if (x < 0 || x >= 3) wet += weight;
    }
    const auto result = field.Query(point, {0, 1, 0}, .05f, volume.boundRadius,
                                     {1, 0, 0}, &solid, true);
    Near(result.fraction, wet / total, 2e-6, "existing lower columns vote dry exactly once");
}
void AnchoredFootprint(const Frame& frame) {
    // The remote first particle fixes the bin lattice. The local particle's
    // actual footprint reaches beyond its bin footprint and fully contains
    // the small body. A rejection hull must conservatively contain that water.
    std::vector<FluidParticle> particles;
    for (const glm::vec3 local : {glm::vec3(-2, -.5f, 0), glm::vec3(.1f, -.5f, 0)}) {
        const auto p = frame.Point(local);
        particles.push_back({p, p, glm::vec3(0), 15.625f, glm::vec3(0)});
    }
    FluidHydrostaticField field;
    field.Build(particles, 1000);
    auto pose = Pose(frame);
    pose.position = frame.Point({.18f, -.5f, 0});
    const auto volume = MakeBoxFluidVolume(glm::vec3(.025f));
    const auto result = EvaluateFluidHydrostatics(field, volume, pose, Uniform(frame.Vector({0, -9.81f, 0})));
    Near(result.fraction, 1, 2e-5, "outer bound contains actual particle footprint");
    Near(glm::length(result.buoyancyForce), 1000 * 9.81 * volume.totalVolume, .0001,
         "positive coarse footprint retains Archimedes load");
}
}
int main() {
    const auto rotation = glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3)));
    const Frame frames[] = {{}, {rotation, {3, -2, 4}}, {rotation, {4096, 0, 0}}};
    for (const auto& frame : frames) {
        RingAndShapeCases(frame);
        CompoundOwnership(frame);
        AnchoredFootprint(frame);
    }
    VerticalMean();
    std::cout << "Fluid wet support: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
