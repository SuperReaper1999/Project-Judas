#pragma once

#include "Contacts.h"
#include "ContactGeometryInternal.h"

#include <cfenv>

// Adaptive signs for the existing sphere/sphere and sphere/box homogeneous
// predicates. A certificate preserves the OLD INTERVAL'S strictly resolved
// sign, including whether response-gap construction needs exact arithmetic.
// It supplies no zero decision, witness, normal, gap or collision tolerance.
//
// Restrict nonzero binary32 sources to [2^-48, 2^48]. Their lattice is 2^-71.
// The relative centre numerator has degree 5; sphere predicates have degree
// 10 and sphere/box predicates degree 14. Nonzero expanded intermediates are
// therefore at least 2^-994 and remain normal binary64 values; their largest
// magnitude, including coefficient slack, is below 2^720. Unsupported ranges,
// invalid geometry and directed rounding retain the existing interval path.
//
// Let DA/DB enclose the quaternion squared norms, P be the absolute summed
// parent positions, C the summed local centres, H the summed box half extents
// and R the summed radii/margin. Each rotation numerator component's expanded
// absolute sum is <= its quaternion norm, hence:
//   L = DA*DB*(P+3*C)
//   sphere majorant = L^2 + (R*DA*DB)^2
//   sphere/box majorant = [DA*DB^2*(3*(P+3*C)+H)]^2 + [R*DA*DB^2]^2.
// Abs and Positive are 1-Lipschitz; no box-clamp branch is assumed.
//
// Unfolding the existing expressions takes fewer than 600 rounded operations
// for sphere/sphere and at most 1932 for sphere/box. Scalar rounding plus the
// old interval's outward rounding is bounded by gamma(1932)+gamma(5796).
// gamma(16384) also covers interval zero-endpoint padding: a padding unit is
// <=2^-80 of a nonzero node majorant, propagated through fewer than1933 nodes.
// Doubling computed quaternion norms encloses their exact positive sums; the
// final factor 2 covers rounding of these short positive majorants. This is
// an arithmetic error bound, not a distance or penetration allowance.
class SpherePredicateFilter {
  public:
    static constexpr int Unresolved = 2;

    SpherePredicateFilter(const ContactPose &a, const ContactPose &b,
                          const contact_geometry::Pair<double> &number) {
        if (std::fegetround() != FE_TONEAREST)
            return;
        if (!Bounded(a) || !Bounded(b))
            return;
        const double da = 2 * number.a.d, db = 2 * number.b.d;
        if (!(da > 0) || !(db > 0) || !std::isfinite(da) || !std::isfinite(db))
            return;
        denominator = da * db;
        boxDenominator = denominator * db;
        positioned = L1(a.position) + L1(b.position) +
                     3 * (L1(a.localCenter) + L1(b.localCenter));
        valid = std::isfinite(denominator) && std::isfinite(boxDenominator) &&
                std::isfinite(positioned);
    }

    int Sphere(double value, float firstRadius, float secondRadius, float margin = 0) const {
        if (!valid || !PositiveInput(firstRadius) || !PositiveInput(secondRadius) ||
            !Margin(margin))
            return Unresolved;
        const double radius = double(firstRadius) + double(secondRadius) + double(margin);
        const double a = denominator * positioned, b = denominator * radius;
        return Sign(value, 2 * (a * a + b * b));
    }

    int SphereBox(double value, float radius, const glm::vec3 &half, float margin = 0) const {
        if (!valid || !PositiveInput(radius) || !Bounded(half) ||
            glm::any(glm::lessThanEqual(half, glm::vec3(0))) || !Margin(margin))
            return Unresolved;
        const double a = boxDenominator * (3 * positioned + L1(half));
        const double b = boxDenominator * (double(radius) + double(margin));
        return Sign(value, 2 * (a * a + b * b));
    }

  private:
    static constexpr double Gamma(unsigned n) {
        constexpr double u = std::numeric_limits<double>::epsilon() * .5;
        return (n * u) / (1 - n * u);
    }

    static int Sign(double value, double magnitude) {
        if (!std::isfinite(value) || !std::isfinite(magnitude) || !(magnitude > 0))
            return Unresolved;
        const double error = Gamma(16384) * magnitude;
        if (value > error)
            return 1;
        if (value < -error)
            return -1;
        return Unresolved;
    }

    static bool Bounded(double value) {
        return value == 0 || (std::isfinite(value) && std::abs(value) >= 0x1p-48 &&
                              std::abs(value) <= 0x1p48);
    }

    static bool Bounded(glm::vec3 v) {
        return Bounded(double(v.x)) && Bounded(double(v.y)) && Bounded(double(v.z));
    }

    static bool Bounded(const ContactPose &p) {
        return Bounded(p.position) && Bounded(p.localCenter) &&
               Bounded(double(p.orientation.w)) && Bounded(double(p.orientation.x)) &&
               Bounded(double(p.orientation.y)) && Bounded(double(p.orientation.z));
    }

    static bool PositiveInput(float value) {
        return value > 0 && Bounded(double(value));
    }

    static bool Margin(float value) {
        return value >= 0 && Bounded(double(value));
    }

    static double L1(glm::vec3 value) {
        return std::abs(double(value.x)) + std::abs(double(value.y)) +
               std::abs(double(value.z));
    }

    double denominator = 0, boxDenominator = 0, positioned = 0;
    bool valid = false;
};
