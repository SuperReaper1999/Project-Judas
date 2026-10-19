#pragma once

#include "Contacts.h"
#include "ContactGeometryInternal.h"
#include <cfenv>

// A first adaptive filter for the existing homogeneous SAT polynomial. This
// class supplies only certified signs / nonzero axes; it supplies no response
// gap, normal, witness, contact tolerance or alternative contact law.
//
// Domain and range proof:
// Every source scalar is binary32. Restrict nonzero magnitudes to [2^-64,2^64].
// Such inputs are integer multiples of 2^-87. A quaternion numerator/norm has
// degree 2, a pair denominator degree 4, and a positioned relative numerator
// degree 5. Face/edge axes have degree 2/4. Sat therefore has degree at most 9;
// every expanded monomial is a multiple of 2^-783. Scalar sums/products retain
// that lattice or a coarser one, with magnitude below 2^620: their nonzero
// intermediate results remain normal and finite.
//
// Forward error proof:
// Abs is 1-Lipschitz. Expand the existing Sat expression with every source
// magnitude positive. The fully expanded scalar expression has at most 1,133
// rounded operations (rotation <= 7, relative numerator <= 87, edge axis <= 31).
// The factored interval workspace has <= 749. Round-to-nearest plus outward
// nextafter costs at most 3*u per interval operation. Thus the distance between
// the scalar result and a legacy interval endpoint is bounded by
// [gamma(1133) + gamma(2247)] times the positive expression majorant.
// An Iv zero endpoint can acquire subnormal padding. At a padding-producing
// node, a nonzero positive expanded monomial is >= 2^-576, so eta/majorant is
// <= 2^-498. Positive propagation through fewer than 1,134 nodes contributes
// < 2^-487 times the majorant. Identically zero subtrees stay exact zero via
// Iv's zero cases. Including this padding still fits within gamma(4096).
//
// Let DA/DB enclose the exact quaternion squared norms. Every rotation
// numerator's absolute expanded sum is at most DA/DB. An axis component's
// absolute expanded sum is at most DA (A face), DB (B face), or 2*DA*DB (edge).
// If P is the summed absolute parent positions, C the summed absolute local
// centres, and H the summed half extents, the whole SAT polynomial's expanded
// magnitude is bounded by DA*DB*(P+3*C+3*H)*axisMagnitude. Doubled computed norms
// enclose the exact positive four-term norms, and the final factor 2 covers all
// rounding while evaluating that positive majorant. Thus gamma(4096)*M is an
// absolute arithmetic error bound, not a distance tolerance. It preserves the
// legacy interval's resolved sign, including its exact-gap branch selection.
// Ambiguity retains the original interval/exact path; no exact-zero decision
// or numerical response is supplied by this filter.
class ContactSatFilter {
  public:
    static constexpr int Unresolved = 2;

    ContactSatFilter(const ContactPose &a, const ContactPose &b,
                     const contact_geometry::Pair<double> &pair,
                     const glm::vec3 &ha, const glm::vec3 &hb) {
        // The established native arithmetic contract requires round-to-nearest.
        // A caller changing the environment retains the old interval/exact path.
        if (std::fegetround() != FE_TONEAREST)
            return;
        if (!Bounded(a) || !Bounded(b) || !Bounded(ha) || !Bounded(hb) ||
            glm::any(glm::lessThanEqual(ha, glm::vec3(0))) ||
            glm::any(glm::lessThanEqual(hb, glm::vec3(0))))
            return;
        firstNorm = 2 * pair.a.d;
        secondNorm = 2 * pair.b.d;
        if (!(firstNorm > 0) || !(secondNorm > 0))
            return;
        const double positions = L1(a.position) + L1(b.position);
        const double centres = L1(a.localCenter) + L1(b.localCenter);
        const double extents = L1(ha) + L1(hb);
        magnitude = 2 * firstNorm * secondNorm *
                    (positions + 3 * centres + 3 * extents);
        valid = std::isfinite(magnitude) && magnitude > 0;
    }

    int Sign(int axis, double value) const {
        if (!valid || !std::isfinite(value))
            return Unresolved;
        const double error = Gamma(4096) * magnitude * AxisMagnitude(axis);
        if (value > error)
            return 1;
        if (value < -error)
            return -1;
        return Unresolved;
    }

    bool NonzeroAxis(int axis, const contact_geometry::V<double> &value) const {
        if (!valid)
            return false;
        const double error = 2 * Gamma(64) * AxisMagnitude(axis);
        return std::max({std::abs(value[0]), std::abs(value[1]), std::abs(value[2])}) > error;
    }

  private:
    static constexpr double Gamma(unsigned operations) {
        constexpr double unit = std::numeric_limits<double>::epsilon() * .5;
        return (operations * unit) / (1 - operations * unit);
    }
    static bool Bounded(double x) {
        return x == 0 || (std::isfinite(x) && std::abs(x) >= 0x1p-64 &&
                          std::abs(x) <= 0x1p64);
    }
    static bool Bounded(const glm::vec3 &v) {
        return Bounded(double(v.x)) && Bounded(double(v.y)) && Bounded(double(v.z));
    }
    static bool Bounded(const ContactPose &pose) {
        const auto &q = pose.orientation;
        return Bounded(pose.position) && Bounded(pose.localCenter) && Bounded(double(q.w)) &&
               Bounded(double(q.x)) && Bounded(double(q.y)) && Bounded(double(q.z));
    }
    static double L1(const glm::vec3 &value) {
        return std::abs(double(value.x)) + std::abs(double(value.y)) + std::abs(double(value.z));
    }
    double AxisMagnitude(int axis) const {
        return axis < 3 ? firstNorm : axis < 6 ? secondNorm : 2 * firstNorm * secondNorm;
    }
    double firstNorm = 0, secondNorm = 0, magnitude = 0;
    bool valid = false;
};
