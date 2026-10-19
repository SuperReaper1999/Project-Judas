#include "SpherePredicateFilter.h"
#include "ContactPreparedGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>

using namespace contact_geometry;
namespace {
std::uint64_t checks = 0, failures = 0, certified = 0, ambiguous = 0, legacyExactBranches = 0;
std::uint64_t exactZeros = 0, legacyZeroBranches = 0, exactCompared = 0;
std::uint64_t rawChangesLegacyBranch = 0;
std::uint32_t state = 0x41e9a771;

std::uint32_t Random() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float Between(float lo, float hi) {
    return lo + (hi - lo) * float(Random() >> 8) / float(1u << 24);
}

float Scaled() {
    return std::ldexp(Between(-1, 1), int(Random() % 87) - 43);
}

void Check(bool value, const char *text, unsigned row) {
    ++checks;
    if (!value) {
        ++failures;
        if (failures < 20)
            std::printf("FAIL %s row=%u\n", text, row);
    }
}

int IntervalSign(Iv value) {
    if (std::isfinite(value.lo) && std::isfinite(value.hi)) {
        if (value.lo > 0)
            return 1;
        if (value.hi < 0)
            return -1;
        if (value.lo == 0 && value.hi == 0)
            return 0;
    }
    return SpherePredicateFilter::Unresolved;
}
void Corpus(bool omitBound) {
    for (unsigned row = 0; row < 200000; ++row) {
        ContactPose a, b;
        auto quaternion = []() {
            glm::quat q{Between(-2, 2), Between(-2, 2), Between(-2, 2), Between(-2, 2)};
            if (glm::dot(q, q) == 0)
                q.w = 1;
            return q;
        };
        a.position = {Between(-100, 100), Between(-100, 100), Between(-100, 100)};
        b.position = a.position + glm::vec3(Between(-3, 3), Between(-3, 3), Between(-3, 3));
        a.localCenter = {Between(-1, 1), Between(-1, 1), Between(-1, 1)};
        b.localCenter = {Between(-1, 1), Between(-1, 1), Between(-1, 1)};
        a.orientation = quaternion();
        b.orientation = quaternion();
        glm::vec3 half{Between(.05f, 2), Between(.05f, 2), Between(.05f, 2)};
        float ra = Between(.05f, 2), rb = Between(.05f, 2);
        float margin = row % 4 == 0 ? Between(0, .1f) : 0;
        const bool sphere = row % 2 == 0;
        if (row % 10 == 0) {
            a.position = {Scaled(), Scaled(), Scaled()};
            b.position = {Scaled(), Scaled(), Scaled()};
            a.localCenter = {Scaled(), Scaled(), Scaled()};
            b.localCenter = {Scaled(), Scaled(), Scaled()};
            a.orientation = {Scaled(), Scaled(), Scaled(), Scaled()};
            b.orientation = {Scaled(), Scaled(), Scaled(), Scaled()};
            half = glm::abs(glm::vec3(Scaled(), Scaled(), Scaled()));
            ra = std::abs(Scaled());
            rb = std::abs(Scaled());
        }
        if (row % 11 == 0) {
            // Exact3-4-5 tangency with arbitrary homogeneous denominators.
            // This exposes a raw-double sign which changes the original
            // interval/exact branch even when no geometry tolerance is used.
            a.position = {0, 0, 0};
            b.position = {3, 4, 0};
            a.localCenter = b.localCenter = {0, 0, 0};
            ra = 2;
            rb = 3;
            margin = 0;
            if (!sphere) {
                a.position = {4, 5, 1};
                b.position = {0, 0, 0};
                half = {1, 1, 1};
                ra = 5;
                b.orientation = {Between(.01f, 3), 0, 0, 0};
            }
        }
        if (row % 13 == 0) {
            a.position = {65536, -32768, 131072};
            b.position = a.position + glm::vec3(1, 0, 0);
            a.localCenter = b.localCenter = {0, 0, 0};
            a.orientation = b.orientation = {1, 0, 0, 0};
            half = {.5f, .5f, .5f};
            ra = rb = .5f;
            margin = 0;
        }
        if (row % 17 == 0) {
            a.position = {std::nextafter(1.f, row % 34 ? 0.f : 2.f), 0, 0};
            b.position = {0, 0, 0};
            a.localCenter = b.localCenter = {0, 0, 0};
            a.orientation = b.orientation = {1, 0, 0, 0};
            half = {.5f, .5f, .5f};
            ra = rb = .5f;
            margin = 0;
        }
        if (row % 19 == 0) {
            // The positive clamp is exercised exactly on faces/edges and one
            // represented float on either side. No guessed branch is certified.
            b.orientation = {1, 0, 0, 0};
            a.position = {.5f, .5f, .5f};
            b.position = {0, 0, 0};
            a.localCenter = b.localCenter = {0, 0, 0};
            half = {.5f, .5f, .5f};
            a.position[row % 3] = std::nextafter(.5f, row % 38 ? 0.f : 1.f);
        }
        const Pair<double> number(a, b);
        const Pair<Iv> interval(a, b);
        const SpherePredicateFilter filter(a, b, number);
        const double r = sphere ? double(ra) + double(rb) + double(margin)
                                : double(ra) + double(margin);
        const Iv ri = sphere ? Iv(double(ra)) + Iv(double(rb)) + Iv(double(margin))
                             : Iv(double(ra)) + Iv(double(margin));
        const double scalar = sphere ? SpherePredicate(number, r)
                                     : SphereBoxPredicate(number, half, r);
        const Iv legacy = sphere ? SpherePredicate(interval, ri)
                                 : SphereBoxPredicate(interval, half, ri);
        const int expectedInterval = IntervalSign(legacy);
        int actual = sphere ? filter.Sphere(scalar, ra, rb, margin)
                            : filter.SphereBox(scalar, ra, half, margin);
        legacyExactBranches += expectedInterval == SpherePredicateFilter::Unresolved;
        legacyZeroBranches += expectedInterval == 0;
        const int raw = scalar > 0 ? 1 : scalar < 0 ? -1 : SpherePredicateFilter::Unresolved;
        if (raw != SpherePredicateFilter::Unresolved && raw != expectedInterval)
            ++rawChangesLegacyBranch;
        if (omitBound)
            actual = raw;
        if (actual == SpherePredicateFilter::Unresolved)
            ++ambiguous;
        else {
            ++certified;
            Check(actual == expectedInterval,
                  "certificate preserves legacy interval-resolved sign and exact-gap branch", row);
        }
        Check(actual != 0, "filter never substitutes an exact-zero decision", row);
        if (row < 512 || (row % 997 == 0 && row % 10 != 0) || row % 1100 == 11) {
            try {
                const Pair<Exact> exact(a, b);
                const Exact re = sphere ? Exact(double(ra)) + Exact(double(rb)) + Exact(double(margin))
                                        : Exact(double(ra)) + Exact(double(margin));
                const auto value = sphere ? SpherePredicate(exact, re)
                                          : SphereBoxPredicate(exact, half, re);
                const int truth = value.Sign();
                ++exactCompared;
                exactZeros += truth == 0;
                if (actual != SpherePredicateFilter::Unresolved)
                    Check(actual == truth, "certificate agrees with independent expansion", row);
                if (expectedInterval != SpherePredicateFilter::Unresolved)
                    Check(expectedInterval == truth,
                          "legacy interval agrees with independent expansion", row);
            } catch (const ArithmeticFailure &) {
                Check(actual == SpherePredicateFilter::Unresolved || expectedInterval == actual,
                      "legacy exact range limitation is not silently bypassed", row);
            }
        }
    }
    Check(certified > 10000, "substantial certified positive and negative population", 200000);
    Check(ambiguous > 1000, "substantial real ambiguity/fallback population", 200000);
    Check(exactCompared > 500, "independent exact arithmetic was exercised", 200000);
    Check(exactZeros > 10, "genuine exact zeros retain legacy branch", 200000);
    Check(rawChangesLegacyBranch > 100,
          "negative scalar-only control genuinely changes legacy branch", 200000);
}

void DomainFallbacks() {
    for (unsigned row = 0; row < 12; ++row) {
        ContactPose a, b;
        glm::vec3 half(.5f);
        float radius = .5f, margin = 0;
        if (row == 0) a.position.x = 0x1p49f;
        if (row == 1) b.localCenter.y = 0x1p-49f;
        if (row == 2) a.orientation.x = std::numeric_limits<float>::denorm_min();
        if (row == 3) half.z = 0;
        if (row == 4) radius = 0;
        if (row == 5) a.orientation = {0, 0, 0, 0};
        if (row == 6) margin = -.01f;
        if (row == 7) a.position.x = std::numeric_limits<float>::infinity();
        if (row == 8) b.orientation.z = std::numeric_limits<float>::quiet_NaN();
        if (row == 9) half.x = -.5f;
        if (row == 10) radius = std::numeric_limits<float>::infinity();
        if (row == 11) margin = std::numeric_limits<float>::quiet_NaN();
        const Pair<double> pair(a, b);
        const SpherePredicateFilter filter(a, b, pair);
        Check(filter.SphereBox(1, radius, half, margin) == SpherePredicateFilter::Unresolved,
              "invalid/out-of-domain sphere-box retains legacy fallback", row);
        if (row != 3 && row != 9)
            Check(filter.Sphere(1, radius, .5f, margin) == SpherePredicateFilter::Unresolved,
                  "invalid/out-of-domain sphere pair retains legacy fallback", row);
    }
    const int original = std::fegetround();
    for (const int mode : {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) != 0)
            continue;
        ContactPose a, b;
        const Pair<double> pair(a, b);
        const SpherePredicateFilter filter(a, b, pair);
        Check(filter.Sphere(1, .5f, .5f) == SpherePredicateFilter::Unresolved,
              "directed rounding retains original sphere path", unsigned(mode));
        Check(filter.SphereBox(-1, .5f, {.5f, .5f, .5f}) == SpherePredicateFilter::Unresolved,
              "directed rounding retains original sphere-box path", unsigned(mode));
    }
    std::fesetround(original);
}

template<class T> bool SameScalar(T first, T second) {
    return std::memcmp(&first, &second, sizeof(T)) == 0;
}

template<class T> bool SameVector(const T &first, const T &second) {
    for (int axis = 0; axis < 3; ++axis)
        if (!SameScalar(first[axis], second[axis]))
            return false;
    return true;
}

bool SameContact(const Contact &first, const Contact &second) {
    return first.hit == second.hit && SameVector(first.point, second.point) &&
           SameVector(first.normal, second.normal) &&
           SameScalar(first.penetration, second.penetration) &&
           SameScalar(first.signedSeparation, second.signedSeparation) &&
           SameVector(first.preciseNormal, second.preciseNormal) &&
           SameVector(first.localAnchorA, second.localAnchorA) &&
           SameVector(first.localAnchorB, second.localAnchorB) &&
           SameVector(first.localWitnessA, second.localWitnessA) &&
           SameVector(first.localWitnessB, second.localWitnessB) &&
           first.hasLocalAnchors == second.hasLocalAnchors &&
           first.anchorAInWorldFrame == second.anchorAInWorldFrame &&
           first.anchorBInWorldFrame == second.anchorBInWorldFrame &&
           first.separationState == second.separationState;
}

void CheckPublic(const Shape &a, const ContactPose &pa, const Shape &b,
                 const ContactPose &pb, float margin, bool expectedHit,
                 double expectedGap, bool sphereBox, unsigned row) {
    const auto actual = PrimitiveContacts(a, pa, b, pb, margin);
    Check(!actual.uncertain, "public ordinary sphere geometry is resolved", row);
    Check(actual.count == (expectedHit ? 1 : 0),
          "public sphere hit follows independent distance oracle", row);
    Check(PrimitiveHasContacts(a, pa, b, pb, margin) == expectedHit,
          "public Boolean sphere query follows distance oracle", row);

    ContactPreparedOrientation preparedA(pa.orientation), preparedB(pb.orientation);
    const auto prepared = PrimitiveContacts(a, pa, b, pb, margin, &preparedA, &preparedB);
    Check(prepared.count == actual.count && prepared.uncertain == actual.uncertain,
          "prepared sphere classification matches direct query", row);
    for (int point = 0; point < std::min(actual.count, prepared.count); ++point)
        Check(SameContact(actual.points[point], prepared.points[point]),
              "prepared sphere response preserves every represented field", row);

    if (!expectedHit || actual.count != 1)
        return;
    const auto &contact = actual.points[0];
    Check(std::abs(contact.signedSeparation - expectedGap) < 1e-12,
          "public sphere response gap follows independent distance oracle", row);
    Check(contact.hasLocalAnchors,
          "public sphere response retains precise physical anchors", row);
    const auto expectedState = expectedGap > 0 ? SeparationState::Separated
                               : expectedGap < 0 ? SeparationState::Penetrating
                                                 : SeparationState::Touching;
    Check(contact.separationState == expectedState,
          "public sphere touching/penetration/separation state is truthful", row);

    if (!sphereBox)
        return;
    // The reverse dispatcher must preserve the same sphere-first geometry,
    // then convert ownership and normals. This also detects missing anchors.
    const auto reverse = PrimitiveContacts(b, pb, a, pa, margin);
    Check(reverse.count == 1 && !reverse.uncertain,
          "reverse box/sphere query remains an actual contact", row);
    if (reverse.count != 1)
        return;
    const auto &other = reverse.points[0];
    Check(SameScalar(other.signedSeparation, contact.signedSeparation) &&
              SameScalar(other.penetration, contact.penetration) &&
              SameVector(other.point, contact.point),
          "reverse sphere dispatch preserves actual gap and world point", row);
    Check(SameVector(other.normal, -contact.normal) &&
              SameVector(other.preciseNormal, -contact.preciseNormal),
          "reverse sphere dispatch preserves normal direction", row);
    Check(SameVector(other.localAnchorA, contact.localAnchorB) &&
              SameVector(other.localAnchorB, contact.localAnchorA) &&
              SameVector(other.localWitnessA, contact.localWitnessB) &&
              SameVector(other.localWitnessB, contact.localWitnessA),
          "reverse sphere dispatch preserves physical witness ownership", row);
}

void PublicGeometry() {
    unsigned row = 0;
    const auto sphere = Shape::Sphere(.5f);
    for (float margin : {0.f, .125f}) {
        const float limit = 1.f + margin;
        for (float distance : {0.f, .5f, 1.f, std::nextafter(limit, 0.f),
                               limit, std::nextafter(limit, 2.f), 2.f}) {
            ContactPose a, b;
            b.position.x = distance;
            CheckPublic(sphere, a, sphere, b, margin, double(distance) <= 1 + double(margin),
                        double(distance) - 1, false, row++);
        }
    }
    const auto small = Shape::Sphere(.25f), box = Shape::Box({.5f, .5f, .5f});
    for (float margin : {0.f, .125f}) {
        const float limit = .75f + margin;
        for (float distance : {0.f, .25f, .5f, .75f, std::nextafter(limit, 0.f),
                               limit, std::nextafter(limit, 2.f), 1.f}) {
            ContactPose a, b;
            a.position.x = distance;
            CheckPublic(small, a, box, b, margin, double(distance) <= .75 + double(margin),
                        double(distance) - .75, true, row++);
        }
    }

    // True face/edge/corner boundaries have known represented-input distances.
    ContactPose a, b;
    b.position = {3, 4, 0};
    CheckPublic(Shape::Sphere(2), a, Shape::Sphere(3), b, 0, true, 0, false, row++);
    a.position = {4, 5, 1};
    b.position = {0, 0, 0};
    CheckPublic(Shape::Sphere(5), a, Shape::Box({1, 1, 1}), b, 0, true, 0, true, row++);
    a.position.x = std::nextafter(4.f, 5.f);
    const auto cornerMiss = PrimitiveContacts(Shape::Sphere(5), a, Shape::Box({1, 1, 1}), b);
    Check(cornerMiss.count == 0 && !cornerMiss.uncertain,
          "one represented step beyond a true box corner is separated", row++);

    // Exact nonunit quaternion rotates the axes cyclically; the configuration
    // is independent of a preferred gravity/up axis and translation.
    a = {}; b = {};
    a.position = {8, -4, 2}; b.position = {8, -3, 2};
    a.orientation = b.orientation = {1, 1, 1, 1};
    CheckPublic(sphere, a, box, b, 0, true, 0, true, row++);

    // Authored local centres cancel parent positions without changing the
    // physical gap. This detects filters which forget those source terms.
    a = {}; b = {};
    a.position = {8, -4, 2}; a.localCenter = {-7, 4, -2};
    b.position = {-3, 6, -1}; b.localCenter = {3, -6, 1};
    CheckPublic(sphere, a, box, b, 0, true, 0, true, row++);

    a = {}; b = {};
    b.orientation = {0, 0, 0, 0};
    const auto invalid = PrimitiveContacts(sphere, a, box, b);
    Check(invalid.count == 0 && invalid.uncertain,
          "invalid sphere query orientation retains public uncertainty", row++);
    Check(!PrimitiveHasContacts(sphere, a, box, b),
          "invalid sphere query creates no Boolean hit", row++);
}
} // namespace
int main(int argc, char **argv) {
    const bool omitBound = argc == 2 && std::string(argv[1]) == "--omit-bound";
    if (argc > 2 || (argc == 2 && !omitBound))
        return 2;
    Corpus(omitBound);
    DomainFallbacks();
    PublicGeometry();
    std::printf("sphere_filter rows=200000 checks=%llu failures=%llu certified=%llu ambiguous=%llu legacy_exact_branches=%llu legacy_zero_branches=%llu independent_exact=%llu exact_zeros=%llu scalar_changes_legacy_branch=%llu\n",
                (unsigned long long)checks, (unsigned long long)failures,
                (unsigned long long)certified, (unsigned long long)ambiguous,
                (unsigned long long)legacyExactBranches, (unsigned long long)legacyZeroBranches,
                (unsigned long long)exactCompared, (unsigned long long)exactZeros,
                (unsigned long long)rawChangesLegacyBranch);
    return failures ? 1 : 0;
}
