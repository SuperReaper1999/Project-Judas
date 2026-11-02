#include "FluidLocalWetSupport.h"
#include "FluidHydrostatics.h"
#include "GravityField.h"

#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using FluidHydrostaticDetail::LocalWetCell;
using FluidHydrostaticDetail::LocalWetInterval;
using FluidHydrostaticDetail::LocalWetIntervals;
using Cells = std::vector<LocalWetCell>;
using Spans = std::vector<LocalWetInterval>;
int checks = 0, failures = 0;

void Check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::cout << "FAIL " << label << '\n'; }
}
void Near(double actual, double expected, double tolerance, const char* label) {
    Check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, label);
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        std::cout << " actual=" << actual << " expected=" << expected
                  << " tolerance=" << tolerance << '\n';
}

Spans Observe(const Cells& cells, double halfHeight) {
    const auto original = cells;
    const auto result = LocalWetIntervals(cells, halfHeight);
    bool unchanged = cells.size() == original.size();
    for (std::size_t i = 0; unchanged && i < cells.size(); ++i)
        unchanged = cells[i].center == original[i].center &&
                    cells[i].spacing == original[i].spacing &&
                    cells[i].rounding == original[i].rounding;
    Check(unchanged, "query leaves actual source geometry and uncertainty unchanged");
    bool canonical = true;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto& span = result[i];
        canonical = canonical && std::isfinite(span.first) && std::isfinite(span.second) &&
                    span.first <= span.second && span.first >= -halfHeight &&
                    span.second <= halfHeight &&
                    (i == 0 || result[i - 1].second < span.first);
    }
    Check(canonical, "ray intervals are finite clipped ordered and disjoint");
    return result;
}
double Fraction(const Spans& spans, double halfHeight) {
    if (halfHeight == 0)
        return std::any_of(spans.begin(), spans.end(), [](const auto& span) {
            return span.first <= 0 && span.second >= 0;
        }) ? 1 : 0;
    double length = 0;
    for (const auto& span : spans) length += span.second - span.first;
    return length / (2 * halfHeight);
}
Spans Expect(const Cells& cells, double halfHeight, double expected,
             double tolerance, const char* label) {
    const auto spans = Observe(cells, halfHeight);
    Near(Fraction(spans, halfHeight), expected, tolerance, label);
    return spans;
}
bool Equivalent(const Spans& a, const Spans& b, double tolerance = 1e-14) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::abs(a[i].first - b[i].first) > tolerance ||
            std::abs(a[i].second - b[i].second) > tolerance) return false;
    return true;
}
void ExpectBothOrders(Cells cells, double halfHeight, double expected,
                      double tolerance, const char* label) {
    const auto forward = Expect(cells, halfHeight, expected, tolerance, label);
    std::reverse(cells.begin(), cells.end());
    const auto backward = Expect(cells, halfHeight, expected, tolerance, label);
    Check(Equivalent(forward, backward), "swapping pair endpoints preserves the physical ray interval");
}

void RawGeometry() {
    using FluidHydrostaticDetail::LocalWetCellMayTouchQuery;
    Check(LocalWetCellMayTouchQuery({{1.49, 0, 1.49}, 1, 1}, .25),
          "large raw-footprint uncertainty is not lost by a spherical candidate shortcut");
    Check(!LocalWetCellMayTouchQuery({{10, 0, 10}, 1, 0}, .25),
          "resolved remote material supplies no raw or compact interpolation candidate");
    Expect({}, 1, 0, 0, "empty water field is dry");
    Expect({{{0, 0, 0}, 1, 0}}, 2, .25, 0, "unit cube contributes its actual one metre height");
    Expect({{{0, 0, 0}, 1, 0}}, .25, 1, 0, "finite query clips inside a real cube");
    Expect({{{0, .75, 0}, 1, 0}}, 1, .375, 0, "query clipping retains partial raw-cube height");
    Expect({{{.5001, 0, 0}, 1, 0}}, .25, 0, 0, "a resolved lateral raw-cube gap stays dry");
    Expect({{{0, 0, 0}, 1, 0}}, 0, 1, 0, "point inside raw material remains wet");
    Expect({{{0, .5001, 0}, 1, 0}}, 0, 0, 0, "point beyond raw vertical face stays dry");
    Expect({{{0, .5, 0}, 1, 0}}, 0, 1, 0, "point on an actual raw-cube face remains represented");
    Expect({{{.5, 0, 0}, 1, 0}}, 0, 1, 0, "raw lateral face uses actual coordinates");
}

void CompactLensAndCutoff() {
    // Two unit cubes are horizontally separated. Each support ball has radius1.
    // At the central ray their common cross-section has halfheight sqrt(1-a*a),
    // capped only by the raw hull halfheight .5. This is an analytic sphere
    // intersection oracle, independent of the kernel's affine hull constraints.
    for (double separation : {1.5, 1.8, 1.9, 1.99, 1.9999, 1.999999, 2., 2.000001}) {
        const double a = separation * .5;
        const double expected = separation < 2 ?
            std::min(1., 2 * std::sqrt(1 - a * a)) : 0;
        ExpectBothOrders({{{-a, 0, 0}, 1, 0}, {{a, 0, 0}, 1, 0}},
                         .5, expected, 2e-13,
                         "finite lens shrinks continuously at the compact pair boundary");
    }
    Expect({{{-.9, 0, 0}, 1, 0}, {{.9, 0, 0}, 1, 0}}, 0, 1, 0,
           "point inside positive-height pair support remains wet");
    // The spheres touch at the origin but enclose no positive ray interval.
    // A zero-measure lens must not become water merely because H is zero.
    Expect({{{0, -1, 0}, 1, 0}, {{0, 1, 0}, 1, 0}}, 0, 0, 0,
           "vertical tangent lens does not publish an isolated wet point");
    Expect({{{0, -1, 0}, 1, 0}, {{0, 1, 0}, 1, 0}}, .5, 0, 0,
           "vertical tangent lens supplies no finite volume");
    for (double halfHeight : {0., .5})
        Expect({{{-1.1, 0, 0}, 1, 0}, {{1.1, 0, 0}, 1, 0}}, halfHeight, 0, 0,
               "two resolved separated patches do not bridge across their air gap");
}

void VerticalDisorderAndRing() {
    Expect({{{0, -.75, 0}, 1, 0}, {{0, .75, 0}, 1, 0}}, .5, 1, 0,
           "short vertical sampling disorder is filled by actual local pair support");
    Expect({{{0, -.75, 0}, 1, 0}, {{0, .75, 0}, 1, 0}}, 0, 1, 0,
           "point in unresolved vertical disorder remains wet");
    const Cells resolved{{{0, -2, 0}, 1, 0}, {{0, 2, 0}, 1, 0}, {{1, 0, 0}, 1, 0}};
    Expect(resolved, .1, 0, 0, "neighbor water cannot fill a resolved vertical cavity");
    auto actual = resolved;
    actual.push_back({{0, 0, 0}, 1, 0});
    Expect(actual, .1, 1, 0, "actual material at the query remains a positive control");
    Expect({{{0, -2, 0}, 1, 0}, {{0, 2, 0}, 1, 0}}, 1, 0, 0,
           "remote vertical water cannot extend into the query");

    constexpr double pi = 3.1415926535897932384626433832795;
    Cells ring;
    for (int i = 0; i < 12; ++i) {
        const double angle = 2 * pi * i / 12;
        ring.push_back({{1.9 * std::cos(angle), 0, 1.9 * std::sin(angle)}, 1, 0});
    }
    bool connected = true;
    for (std::size_t i = 0; i < ring.size(); ++i)
        connected = connected && glm::length(ring[i].center - ring[(i + 1) % ring.size()].center) < 2;
    Check(connected, "ring is locally connected rather than disconnected by construction");
    Expect(ring, .5, 0, 0, "a connected ring's global hull does not turn its dry centre wet");
    Expect(ring, 0, 0, 0, "connected ring point query preserves the same dry cavity");
}

void MixedResolutionAndLocalUncertainty() {
    Expect({{{-.7, 0, 0}, 1, 0}, {{.7, 0, 0}, .5, 0}}, .5, 0, 0,
           "mixed-size pair uses its smaller local support rather than a global spacing");
    // The smaller particle's ball radius is .5, with horizontal offset .35.
    // It is the limiting sphere; the larger sphere and connecting hull both
    // contain its central-ray segment. Thus the physical height is 2sqrt(.1275).
    ExpectBothOrders({{{-.55, 0, 0}, 1, 0}, {{.35, 0, 0}, .5, 0}}, .5,
                     2 * std::sqrt(.1275), 2e-13,
                     "mixed-size support has the independently known limiting sphere height");
    Cells local{{{-.9, 0, 0}, 1, 0}, {{.9, 0, 0}, 1, 0}};
    const auto before = Expect(local, .5, 2 * std::sqrt(.19), 2e-13,
                               "local support before unrelated coarse material");
    local.push_back({{100, 100, 100}, 16, 0});
    const auto after = Expect(local, .5, 2 * std::sqrt(.19), 2e-13,
                              "distant coarse material cannot enlarge local support");
    Check(Equivalent(before, after), "distant coarse material leaves local intervals unchanged");

    Cells separated{{{0, -1.1, 0}, 1, 0}, {{0, 1.1, 0}, 1, 0}};
    const auto separatedBefore = Expect(separated, 2, .5, 2e-14,
                                        "two local layers preserve their resolved internal air");
    const glm::dvec3 remote(1000000, 1000000, 0);
    const double remoteError = 8 * std::numeric_limits<float>::epsilon() * (1 + glm::length(remote));
    separated.push_back({remote, 16, remoteError});
    const auto separatedAfter = Expect(separated, 2, .5, 2e-14,
                                       "off-ray uncertainty cannot close another layer's real gap");
    Check(Equivalent(separatedBefore, separatedAfter),
          "irrelevant remote rounding leaves local interval topology unchanged");

    // Declared uncertainty erodes BOTH the lens admission radius and the sphere
    // used to calculate its height. Using the original radius for sqrt would
    // merely move the old finite jump inward to separation1.96.
    constexpr double error = .02, interior = 1 - error;
    for (double separation : {1.9, 1.95, 1.9599, 1.959999, 1.96, 1.960001}) {
        const double a = separation * .5;
        const double expected = a < interior ?
            std::min(1., 2 * std::sqrt(interior * interior - a * a)) : 0;
        ExpectBothOrders({{{-a, 0, 0}, 1, error}, {{a, 0, 0}, 1, error}},
                         .5, expected, 2e-13,
                         "uncertainty-eroded lens has no displaced finite-volume jump");
    }
}

struct Frame {
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 Point(glm::vec3 local) const { return translation + rotation * local; }
    glm::vec3 Vector(glm::vec3 local) const { return rotation * local; }
};
Cells QuantizedFrameCells(const Frame& frame, glm::vec3 query,
                          const std::vector<glm::vec3>& points, double spacing) {
    const auto worldPoint = frame.Point(query);
    const auto up = glm::normalize(glm::dvec3(frame.Vector({0, 1, 0})));
    auto tangent = glm::dvec3(frame.Vector({1, 0, 0}));
    tangent = glm::normalize(tangent - up * glm::dot(up, tangent));
    const auto bitangent = glm::cross(up, tangent);
    Cells cells;
    for (const auto point : points) {
        const auto world = frame.Point(point);
        const auto difference = glm::dvec3(world) - glm::dvec3(worldPoint);
        const double error = 8 * std::numeric_limits<float>::epsilon() *
            (1 + glm::length(glm::dvec3(world)) + glm::length(glm::dvec3(worldPoint)));
        cells.push_back({{glm::dot(difference, tangent), glm::dot(difference, up),
                          glm::dot(difference, bitangent)}, spacing, error});
    }
    return cells;
}
void ActualFrameTangency() {
    // This isolates the native-v1 failure: neighboring water's support sphere
    // is tangent to the query ray while the local cube ends below the slab.
    // Float rotation can round that tangent into a tiny positive radicand.
    // The square root magnifies it into false wet height unless the same local
    // input uncertainty governs tangency admission AND the calculated radius.
    const std::array<glm::vec3, 3> translations{{{0, 0, 0}, {2, -1, 3}, {4096, 0, 0}}};
    for (float angle : {.2f, .63f, .83f, 1.4f}) for (const auto translation : translations) {
        const Frame frame{glm::angleAxis(angle, glm::normalize(glm::vec3(1, -2, 3))), translation};
        const glm::vec3 query(0, .101f, 0);
        const auto tangent = QuantizedFrameCells(frame, query, {{.25f, .125f, 0}, {0, -.125f, 0}}, .25);
        Expect(tangent, .1, 0, 0, "quantized tangent neighbor does not create false water above a dry slab");
        const auto positive = QuantizedFrameCells(frame, query, {{.225f, .125f, 0}, {0, -.125f, 0}}, .25);
        const auto spans = Observe(positive, .1);
        Check(Fraction(spans, .1) > 0,
              "same frame retains an independently interior positive-height contact");
    }
}
struct UniformGravity : GravityField {
    glm::vec3 Sample(const glm::vec3&) const override { return {0, -9.81f, 0}; }
};
void PublicQueryAndForce() {
    constexpr float spacing = .25f, density = 1000;
    const float mass = density * spacing * spacing * spacing;
    const auto volume = MakeBoxFluidVolume({.01f, spacing * .5f, .01f}, 1);
    const UniformGravity gravity;
    for (float separation : {1.5f, 1.8f, 1.9f, 1.99f, 1.9999f, 1.99999f,
                             1.999999f, 2.f, 2.000001f, 2.1f}) {
        const float a = separation * spacing * .5f;
        std::vector<FluidParticle> particles;
        for (float x : {-a, a}) {
            const glm::vec3 point(x, 0, 0);
            particles.push_back({point, point, {}, mass, {}});
        }
        FluidHydrostaticField field;
        field.Build(particles, density);
        // The public field stores binary32 particles and computes binary32
        // spacing from mass/density. Use those represented inputs, not the
        // nominal decimal separation, for the independent sphere-section oracle.
        const double representedSpacing = std::cbrt(mass / density);
        const double representedA = std::abs(double(particles[1].position.x));
        const double uncertainty = 8 * std::numeric_limits<float>::epsilon() *
            (1 + glm::length(glm::dvec3(particles[1].position)));
        const double supportRadius = representedSpacing - uncertainty;
        const double expected = representedA < supportRadius ?
            std::min(1., 2 * std::sqrt(supportRadius * supportRadius -
                                      representedA * representedA) / representedSpacing) : 0;
        const double tolerance = expected == 0 ? 0 : 8 * std::numeric_limits<float>::epsilon();
        const auto observed = field.Query({}, {0, 1, 0}, spacing * .5f,
                                           3 * spacing, {1, 0, 0}, nullptr, true);
        Near(observed.fraction, expected, tolerance,
             "public connected query follows analytic compact lens through its cutoff");
        const auto hydro = EvaluateFluidHydrostatics(field, volume, {}, gravity,
                                                    FluidVolumeGeometry::NonDisplacingVolume);
        Near(hydro.fraction, expected, tolerance,
             "finite physical volume preserves the independently measured local wet fraction");
        const double expectedForce = density * double(volume.totalVolume) *
                                     double(-gravity.Sample({}).y) * expected;
        const double forceTolerance = expected == 0 ? 0 :
            8 * std::numeric_limits<float>::epsilon() * std::max(1., expectedForce);
        Near(hydro.buoyancyForce.y, expectedForce, forceTolerance,
             "actual buoyancy force follows represented wet volume across compact support cutoff");
    }
}
} // namespace

int main() {
    RawGeometry();
    CompactLensAndCutoff();
    VerticalDisorderAndRing();
    MixedResolutionAndLocalUncertainty();
    ActualFrameTangency();
    PublicQueryAndForce();
    std::cout << "Local wet support: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
