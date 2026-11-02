#pragma once

#include "FluidBoundaryGeometry.h"

namespace FluidFiniteBoundaryTest {

// Direct finite-height column controls. These do not claim that a single
// column is an exact volume quadrature for an entire three-dimensional body.
constexpr float spacing = .0625f;
const BodyHandle owner{37, 901}, foreign{38, 901};

struct Frame {
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 translation{0};

    glm::vec3 Point(glm::vec3 point) const { return translation + rotation * point; }
    glm::vec3 Vector(glm::vec3 vector) const { return rotation * vector; }
};

void AppendParticle(std::vector<FluidParticle>& particles, glm::vec3 point, const Frame& frame) {
    point = frame.Point(point);
    particles.push_back({point, point, {}, 1000 * spacing * spacing * spacing, {}});
}

struct Fixture {
    std::vector<FluidParticle> source, material, dryDomain;
};

Fixture MakeFixture(bool bounded, const Frame& frame) {
    Fixture fixture;
    // Identical distant actual water keeps both material fields nonempty. It
    // cannot supply a local donor or manufacture a contact at the queried box.
    for (int z = 0; z < 8; ++z) for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        const glm::vec3 point(3 + (x + .5f) * spacing,
                              -.25f + (y + .5f) * spacing,
                              -.25f + (z + .5f) * spacing);
        AppendParticle(fixture.source, point, frame);
        AppendParticle(fixture.material, point, frame);
        AppendParticle(fixture.dryDomain, point, frame);
    }
    if (bounded) {
        // Exact flat-sided wet pocket [-.25,.25]^3, fully removed by the
        // captured solid [-.5,.5]^3. Its dry twin has identical actual matter.
        for (int z = 0; z < 8; ++z) for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x)
            AppendParticle(fixture.source,
                glm::vec3(-.25f) + glm::vec3(x + .5f, y + .5f, z + .5f) * spacing, frame);
    } else {
        // Broad reservoir [-1,1]^3, with an independently declared dry-cavity
        // twin. The positive case must stay continuous through the owner edge;
        // its dry twin must not start filling the old void at that same edge.
        for (int z = 0; z < 32; ++z) for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
            const glm::vec3 point(-1 + (x + .5f) * spacing,
                                  -1 + (y + .5f) * spacing,
                                  -1 + (z + .5f) * spacing);
            AppendParticle(fixture.source, point, frame);
            if (glm::any(glm::greaterThanEqual(glm::abs(point), glm::vec3(.5f)))) {
                AppendParticle(fixture.material, point, frame);
                AppendParticle(fixture.dryDomain, point, frame);
            }
        }
    }
    return fixture;
}

void FrozenWetAndDry() {
    const Frame frames[]{Frame{}, Frame{
        glm::angleAxis(.63f, glm::normalize(glm::vec3(1, -2, 3))), {1, -2, 3}}};
    const auto volume = MakeBoxFluidVolume(glm::vec3(.5f), 1);
    for (bool bounded : {false, true}) for (const auto& frame : frames) {
        const auto fixture = MakeFixture(bounded, frame);
        FluidHydrostaticField wet, dry;
        wet.Build(fixture.material, 1000);
        dry.Build(fixture.material, 1000);
        const BodyTransform captured{frame.Point({}), frame.rotation};
        wet.RetainDisplacedBoundary(owner, volume, captured, fixture.source);
        dry.RetainDisplacedBoundary(owner, volume, captured, fixture.dryDomain);
        const auto up = frame.Vector({0, 1, 0});
        const auto tangent = wet.ReconstructionTangent(up, frame.Vector({1, 0, 0}));
        for (float epsilon : {.001f, .0001f, .00001f, .000001f}) for (float side : {-1.f, 1.f}) {
            const float y = .5f + side * epsilon;
            const BodyTransform pose{frame.Point({0, y, 0}), frame.rotation};
            const FluidSolidQuery own{volume, pose, owner}, other{volume, pose, foreign};
            const auto query = [&](const FluidHydrostaticField& field, const FluidSolidQuery& solid) {
                return field.Query(pose.position, up, .5f, volume.boundRadius,
                                   tangent, &solid, true).fraction;
            };
            const double bottom = bounded ? -.25 : -1, top = bounded ? .25 : 1;
            const double lo = std::max(bottom, double(y) - .5);
            const double hi = std::min(top, double(y) + .5);
            const double wetLength = std::max(0., hi - lo);
            const double cavityLength = bounded ? wetLength :
                std::max(0., std::min(hi, .5) - std::max(lo, -.5));
            Near(query(wet, own), wetLength, 3e-6,
                 "finite retained wet support survives captured-centre crossing");
            Near(query(dry, own), wetLength - cavityLength, 3e-6,
                 "finite retained dry support survives captured-centre crossing");
            if (bounded)
                Near(query(wet, other), 0, 0,
                     "foreign owner cannot borrow finite pre-cut water pocket");
        }
        if (bounded) {
            const BodyTransform distant{frame.Point({0, 3, 0}), frame.rotation};
            const FluidSolidQuery moved{volume, distant, owner};
            Near(wet.Query(frame.Point({0, .125f, 0}), up, .0625f, volume.boundRadius,
                           tangent, &moved, true).fraction, 0, 0,
                 "retained support cannot escape current solid after teleport");
        }
    }
}

void NativeCellClipping() {
    for (const auto& frame : {Frame{}, Frame{
        glm::angleAxis(.63f, glm::normalize(glm::vec3(1, -2, 3))), {1, -2, 3}}}) {
        const auto volume = MakeBoxFluidVolume(glm::vec3(.5f), 1);
        const BodyTransform captured{frame.Point({}), frame.rotation};
        const auto up = frame.Vector({0, 1, 0});
        const auto tangent = frame.Vector({1, 0, 0});
        const auto materialPoint = frame.Point({0, .53125f, 0});
        FluidParticle particle{materialPoint, materialPoint, {}, 15.625f, {}};
        FluidHydrostaticField field;
        field.Build({particle}, 1000);
        FluidDisplacementMarker cell;
        cell.owner = owner;
        cell.position = frame.Point({0, .4375f, 0});
        cell.rotation = frame.rotation;
        cell.halfWidth = .125f;
        cell.materialPoint = materialPoint;
        cell.contactPoint = frame.Point({0, .5f, 0});
        field.RetainDisplacementCells(owner, volume, captured, {cell});
        const BodyTransform current{frame.Point({0, .625f, 0}), frame.rotation};
        const FluidSolidQuery solid{volume, current, owner};
        // Sole actual material column is occluded by the current solid, so it
        // cannot supply ordinary displaced volume. Proven birth interval
        // [.3125,.5625] clipped to captured/current support [.25,.5] retains
        // length .1875 in this .75-high query. No fitted surface is used.
        Near(field.Query(current.position, up, .375f, volume.boundRadius,
                         tangent, &solid, true).fraction, .1875 / .75, 3e-6,
             "native birth-cell interval retains partial finite captured support");
        const FluidSolidQuery other{volume, current, foreign};
        Near(field.Query(current.position, up, .375f, volume.boundRadius,
                         tangent, &other, true).fraction, 0, 0,
             "occluded material cannot replace another owner's native birth geometry");
        const BodyTransform distant{frame.Point({0, 3, 0}), frame.rotation};
        const FluidSolidQuery absent{volume, distant, owner};
        Near(field.Query(frame.Point({0, .34375f, 0}), up, .015625f, volume.boundRadius,
                         tangent, &absent, true).fraction, 0, 0,
             "native birth-cell water cannot extend outside current solid");
        // This independently valid witness cell extends above the captured
        // solid. Removing only the centre guard would admit its unowned tail.
        cell.position = frame.Point({0, .6f, 0});
        field.RetainDisplacementCells(owner, volume, captured, {cell});
        Near(field.Query(frame.Point({0, .703125f, 0}), up, .015625f, volume.boundRadius,
                         tangent, &solid, true).fraction, 0, 0,
             "native birth-cell water cannot extend outside captured solid");
    }
}

void RayGeometry() {
    using FluidBoundaryDetail::SolidRayIntervals;
    const double infinity = std::numeric_limits<double>::infinity();
    const BodyTransform identity;
    const std::vector<CompoundBox> unit{{{}, glm::vec3(1)}};
    const auto span = [](const std::vector<std::pair<double, double>>& actual,
                         std::vector<std::pair<double, double>> expected,
                         double tolerance, const char* name) {
        bool match = actual.size() == expected.size();
        for (std::size_t i = 0; i < actual.size() && match; ++i)
            match = std::abs(actual[i].first - expected[i].first) <= tolerance &&
                    std::abs(actual[i].second - expected[i].second) <= tolerance;
        Check(match, name);
    };
    const auto box = [&](glm::vec3 point, glm::dvec3 direction, double lo = -10, double hi = 10) {
        return SolidRayIntervals(point, direction, identity, unit, 0, lo, hi);
    };
    span(box({}, {0, 1, 0}), {{-1, 1}}, 0, "box ray diameter");
    span(box({0, 2, 0}, {0, 1, 0}), {{-3, -1}}, 0, "outside box backward ray");
    span(box({2, 0, 0}, {0, 1, 0}), {}, 0, "parallel exterior box ray");
    span(box({1, 0, 0}, {0, 1, 0}), {{-1, 1}}, 0, "parallel box boundary ray");
    span(box({}, {0, 2, 0}), {{-.5, .5}}, 0, "nonunit box ray parameter");
    span(box({}, {0, 1, 0}, -.25, .5), {{-.25, .5}}, 0, "box ray query clipping");
    span(box({}, {0, 1, 0}, 0, 0), {{0, 0}}, 0, "zero-height ray inside box");
    span(box({0, 2, 0}, {0, 1, 0}, 0, 0), {}, 0, "zero-height ray outside box");
    span(box({}, {}), {{-10, 10}}, 0, "constant ray inside box");
    span(box({2, 0, 0}, {}), {}, 0, "constant ray outside box");
    std::vector<CompoundBox> pieces{{{0, -2, 0}, glm::vec3(.5f)}, {{0, 2, 0}, glm::vec3(.5f)}};
    const auto compound = [&] {
        return SolidRayIntervals({}, {0, 1, 0}, identity, pieces, 0, -infinity, infinity);
    };
    span(compound(), {{-2.5, -1.5}, {1.5, 2.5}}, 0, "disconnected compound ray preserves gap");
    std::reverse(pieces.begin(), pieces.end());
    span(compound(), {{-2.5, -1.5}, {1.5, 2.5}}, 0, "compound ray order independence");
    pieces = {{{0, -.5f, 0}, glm::vec3(.5f)}, {{0, .5f, 0}, glm::vec3(.5f)}};
    span(compound(), {{-1, 1}}, 0, "touching compound ray union");
    pieces = {{{0, -.25f, 0}, glm::vec3(.5f)}, {{0, .25f, 0}, glm::vec3(.5f)}};
    span(compound(), {{-.75, .75}}, 0, "overlapping compound ray union");
    const auto sphere = [&](glm::vec3 point, glm::dvec3 direction, double lo = -10, double hi = 10) {
        return SolidRayIntervals(point, direction, identity, {}, 1, lo, hi);
    };
    span(sphere({}, {0, 1, 0}), {{-1, 1}}, 0, "sphere ray diameter");
    const double edge = std::sqrt(.75);
    span(sphere({.5, 0, 0}, {0, 1, 0}), {{-edge, edge}}, 1e-15, "sphere ray chord");
    span(sphere({1, 0, 0}, {0, 1, 0}), {{0, 0}}, 0, "sphere ray tangent");
    span(sphere({2, 0, 0}, {0, 1, 0}), {}, 0, "sphere ray misses");
    span(sphere({0, 2, 0}, {0, 1, 0}), {{-3, -1}}, 0, "outside sphere backward ray");
    span(sphere({0, 2, 0}, {0, -2, 0}), {{.5, 1.5}}, 0, "nonunit sphere inward ray");
    span(sphere({}, {0, 1, 0}, .25, .5), {{.25, .5}}, 0, "sphere ray query clipping");
    span(sphere({}, {}), {{-10, 10}}, 0, "constant ray inside sphere");
    span(sphere({2, 0, 0}, {}), {}, 0, "constant ray outside sphere");
    for (float angle : {.2f, .63f, 1.4f}) {
        const auto rotation = glm::angleAxis(angle, glm::normalize(glm::vec3(1, -2, 3)));
        const BodyTransform frame{{1, -2, 3}, rotation};
        const auto point = frame.position + rotation * glm::vec3(.25, 0, 0);
        const auto direction = glm::dvec3(rotation * glm::vec3(0, 1, 0));
        span(SolidRayIntervals(point, direction, frame, unit, 0, -infinity, infinity),
             {{-1, 1}}, 3e-6, "global frame covariance box ray");
        span(SolidRayIntervals(frame.position, direction, frame, {}, 1, -infinity, infinity),
             {{-1, 1}}, 3e-6, "global frame covariance sphere ray");
        CompoundBox child;
        child.halfExtents = glm::vec3(1);
        child.rotation = rotation;
        span(SolidRayIntervals(rotation * glm::vec3(.25, 0, 0), direction, identity,
                               {child}, 0, -infinity, infinity),
             {{-1, 1}}, 3e-6, "child frame covariance box ray");
    }
}

} // namespace FluidFiniteBoundaryTest

void FluidFiniteBoundaryChecks() {
    FluidFiniteBoundaryTest::RayGeometry();
    FluidFiniteBoundaryTest::FrozenWetAndDry();
    FluidFiniteBoundaryTest::NativeCellClipping();
}
