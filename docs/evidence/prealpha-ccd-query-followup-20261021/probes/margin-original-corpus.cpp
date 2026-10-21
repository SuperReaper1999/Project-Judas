#include "Contacts.h"
#include "ContactGeometryInternal.h"
#include "/home/conner/Documents/GitHub/Project-Judas/.cache/ccd-query-followup-20261021/ContactSatFilter.h"

#include <cmath>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

using namespace contact_geometry;

namespace {
std::uint64_t checks = 0, failures = 0;
std::uint64_t certified = 0, ambiguous = 0, exactChecks = 0, unsafeScalarSigns = 0;
std::uint32_t randomState = 0x7842bed1;

void Check(bool condition, const char *message, unsigned row = 0, unsigned axis = 0) {
    ++checks;
    if (!condition) {
        if (failures < 20)
            std::printf("FAIL %s row=%u axis=%u\n", message, row, axis);
        ++failures;
    }
}

std::uint32_t Random() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

float Between(float low, float high) {
    return low + (high - low) * float(Random() >> 8) * (1.f / 16777216.f);
}

float Scaled() {
    return std::ldexp(Between(-1, 1), int(Random() % 125) - 62);
}

glm::vec3 Vector() {
    return {Scaled(), Scaled(), Scaled()};
}

// This traverses the legacy interval workspace and the unfactored exact
// polynomial independently of the new filter. Testing the legacy interval sign
// also detects a change to the exact-gap branch, rather than checking overlap
// truth alone. The deliberately unsafe mode proves why fallback is necessary.
void PredicateCorpus(bool omitBound) {
    for (unsigned row = 0; row < 12000; ++row) {
        ContactPose a, b;
        a.position = Vector();
        b.position = Vector();
        a.localCenter = Vector();
        b.localCenter = Vector();
        a.orientation = {Scaled(), Scaled(), Scaled(), Scaled()};
        b.orientation = {Scaled(), Scaled(), Scaled(), Scaled()};
        glm::vec3 ha = glm::abs(Vector()), hb = glm::abs(Vector());

        if (row % 2 == 0) {
            a.position = {Between(-2, 2), Between(-2, 2), Between(-2, 2)};
            b.position = a.position +
                         glm::vec3(Between(-2, 2), Between(-2, 2), Between(-2, 2));
            a.localCenter = b.localCenter = {0, 0, 0};
            a.orientation = {Between(-1, 1), Between(-1, 1), Between(-1, 1), Between(-1, 1)};
            b.orientation = {Between(-1, 1), Between(-1, 1), Between(-1, 1), Between(-1, 1)};
            ha = hb = {.5f, .5f, .5f};
        }
        if (row % 3 == 0) {
            b.orientation = a.orientation;
            b.position = a.position;
            ha = hb;
        }
        if (row % 5 == 0) {
            a.position = {65536, -32768, 131072};
            b.position = a.position + glm::vec3(1, 0, 0);
        }
        if (row % 7 == 0) {
            b.localCenter = a.localCenter + glm::vec3(1, 0, 0);
            b.position = a.position - glm::vec3(1, 0, 0);
        }
        if (row % 11 == 0) {
            a.orientation = b.orientation = {1, 0, 0, 0};
            ha = hb = {.5f, .5f, .5f};
            b.position = a.position + glm::vec3(1, 0, 0);
            a.localCenter = b.localCenter = {0, 0, 0};
        }
        if (row % 13 == 0) {
            a.orientation = {1, 0x1p-64f, 0, 0};
            b.orientation = {1, 0, 0x1p-64f, 0};
        }
        if (row % 17 == 0) {
            a.orientation = {1, 0x1p-65f, 0, 0};
            b.orientation = {1, 0, 0x1p-65f, 0};
        }

        const Pair<double> number(a, b);
        const Pair<Iv> interval(a, b);
        const SatWorkspace<Iv> workspace(interval);
        const ContactSatFilter filter(a, b, number, ha, hb);
        for (unsigned axis = 0; axis < 15; ++axis) {
            const auto numberAxis = Axis(number, int(axis));
            const auto intervalAxis = Axis(interval, int(axis));
            const auto squaredLength = Dot(intervalAxis, intervalAxis);
            const auto old = workspace.Evaluate(ha, hb, int(axis), intervalAxis);
            const double scalar = Sat(number, ha, hb, int(axis));
            int actual = filter.Sign(int(axis), scalar);
            if (omitBound)
                actual = scalar > 0 ? 1 : scalar < 0 ? -1 : ContactSatFilter::Unresolved;
            if (actual != ContactSatFilter::Unresolved) {
                ++certified;
                Check(actual > 0 ? old.lo > 0 : old.hi < 0,
                      "certified sign preserves legacy interval resolution", row, axis);
            } else {
                ++ambiguous;
            }
            // Compare independent legacy interval/exact margin expressions.
            for (float margin : {0x1p-64f, .001f, .1f, 1.f, 0x1p64f}) {
                if (!(old.lo > 0) || !(squaredLength.lo > 0)) continue;
                const Iv m{double(margin)};
                const Iv legacy = old * old - m * m * interval.denominator *
                                    interval.denominator * squaredLength;
                const int answer = filter.BeyondMargin(int(axis), scalar, margin,
                                                         interval.denominator, squaredLength);
                if (answer != ContactSatFilter::Unresolved) {
                    Check(answer > 0 ? legacy.lo > 0 : legacy.hi < 0,
                          "margin certificate preserves legacy interval branch", row, axis);
                }
            }
            if (filter.NonzeroAxis(int(axis), numberAxis))
                Check(squaredLength.lo > 0,
                      "certified nonzero axis preserves legacy interval resolution", row, axis);

            if (row < 300 || row % 101 == 0) {
                const Pair<Exact> exact(a, b);
                const int expected = Sat(exact, ha, hb, int(axis)).Sign();
                ++exactChecks;
                if (actual != ContactSatFilter::Unresolved)
                    Check(actual == expected, "certified sign matches exact polynomial", row, axis);
                if (scalar != 0 && (scalar > 0 ? 1 : -1) != expected)
                    ++unsafeScalarSigns;
            }
        }
    }
    Check(certified > 10000, "substantial positive and negative filter population");
    Check(ambiguous > 1000, "real ambiguity and out-of-domain fallback exercised");
    Check(exactChecks > 1000, "independent exact arithmetic exercised");
    Check(unsafeScalarSigns > 0, "unfiltered scalar arithmetic really fails this corpus");
}

void DomainFallbacks() {
    const ContactPose ordinary;
    const glm::vec3 half(.5f);
    for (unsigned row = 0; row < 6; ++row) {
        ContactPose other;
        glm::vec3 extent = half;
        if (row == 0)
            other.position.x = 0x1p65f;
        if (row == 1)
            other.localCenter.y = 0x1p-65f;
        if (row == 2)
            other.orientation.x = std::numeric_limits<float>::denorm_min();
        if (row == 3)
            extent.z = 0;
        if (row == 4)
            other.position.y = std::numeric_limits<float>::infinity();
        if (row == 5)
            other.orientation = {0, 0, 0, 0};
        const Pair<double> number(ordinary, other);
        const ContactSatFilter filter(ordinary, other, number, half, extent);
        for (int axis = 0; axis < 15; ++axis) {
            Check(filter.Sign(axis, 1) == ContactSatFilter::Unresolved,
                  "invalid or out-of-domain inputs retain fallback", row, unsigned(axis));
            Check(!filter.NonzeroAxis(axis, {1, 0, 0}),
                  "invalid or out-of-domain axes retain fallback", row, unsigned(axis));
        }
    }
}

void RoundingFallbacks() {
    const int original = std::fegetround();
    for (int mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) != 0)
            continue; // This environment cannot expose the requested mode.
        ContactPose a, b;
        b.position = {.25f, 0, 0};
        const glm::vec3 half(.5f);
        const Pair<double> number(a, b);
        const ContactSatFilter filter(a, b, number, half, half);
        for (int axis = 0; axis < 15; ++axis) {
            Check(filter.Sign(axis, -1) == ContactSatFilter::Unresolved,
                  "directed rounding retains legacy predicate fallback");
            Check(!filter.NonzeroAxis(axis, {1, 0, 0}),
                  "directed rounding retains legacy axis fallback");
        }
        // A simple exactly represented positive/negative physical control is
        // still useful. This does not certify legacy Exact under directed modes.
        const auto box = Shape::Box(half);
        Check(PrimitiveContacts(box, a, box, b).count > 0,
              "directed fallback retains known overlapping boxes");
        b.position = {2, 0, 0};
        Check(PrimitiveContacts(box, a, box, b).count == 0,
              "directed fallback retains known separated boxes");
        std::fesetround(original);
    }
    Check(std::fegetround() == original, "test restores the caller's rounding environment");
}

void PublicGeometry() {
    const auto box = Shape::Box({.5f, .5f, .5f});
    for (float x : {0.f, .5f, 1.f, std::nextafter(1.f, 0.f), std::nextafter(1.f, 2.f), 2.f}) {
        ContactPose a, b;
        b.position = {x, 0, 0};
        const auto manifold = PrimitiveContacts(box, a, box, b);
        const bool expected = double(x) <= 1;
        Check(!manifold.uncertain, "ordinary axis-aligned contact is resolved");
        Check((manifold.count > 0) == expected, "independent represented-input overlap oracle");
        Check(PrimitiveHasContacts(box, a, box, b) == expected,
              "public Boolean query respects represented-input overlap oracle");
        for (int i = 0; i < manifold.count; ++i) {
            const auto &contact = manifold.points[i];
            Check(contact.signedSeparation == double(x) - 1,
                  "actual response depth matches known box face separation");
            Check(contact.hasLocalAnchors, "physical contacts retain precise local anchors");
            if (x > 0)
                Check(contact.preciseNormal == glm::dvec3(-1, 0, 0),
                      "actual response normal points from B toward A");
        }
    }
    ContactPose a, b;
    b.orientation = {0, 0, 0, 0};
    auto invalid = PrimitiveContacts(box, a, box, b);
    Check(invalid.uncertain && invalid.count == 0, "invalid orientation preserves uncertainty");
    Check(!PrimitiveHasContacts(box, a, box, b), "invalid orientation produces no Boolean hit");
    b = {};
    b.position.x = std::numeric_limits<float>::quiet_NaN();
    invalid = PrimitiveContacts(box, a, box, b);
    Check(invalid.uncertain && invalid.count == 0, "nonfinite position preserves uncertainty");
}
} // namespace

int main(int argc, char **argv) {
    const bool omitBound = argc == 2 && std::string(argv[1]) == "--omit-bound";
    if (argc > 2 || (argc == 2 && !omitBound)) {
        std::fprintf(stderr, "Usage: judas_contact_sat_filter_tests [--omit-bound]\n");
        return 2;
    }
    PredicateCorpus(omitBound);
    DomainFallbacks();
    RoundingFallbacks();
    PublicGeometry();
    std::printf("Contact SAT filter: checks=%llu failures=%llu certified=%llu ambiguous=%llu "
                "exact_checks=%llu unsafe_scalar_signs=%llu\n",
                static_cast<unsigned long long>(checks), static_cast<unsigned long long>(failures),
                static_cast<unsigned long long>(certified), static_cast<unsigned long long>(ambiguous),
                static_cast<unsigned long long>(exactChecks),
                static_cast<unsigned long long>(unsafeScalarSigns));
    return failures ? 1 : 0;
}
