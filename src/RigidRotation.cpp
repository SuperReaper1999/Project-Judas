#include "RigidMotion.h"
#include <cmath>

namespace {
// Local orientation truncation error, independent of kinetic-energy checking.
// Step doubling controls geometry accuracy; midpoint leaves conserve invariants.
constexpr double kAngleError = 1e-6;
constexpr unsigned kMaximumLeaves = 512;

bool Principal(const RigidBody& body) {
    if(body.IsPlanar())return true;
    const glm::dmat3 inverse(body.inverseInertiaLocal);
    if (!(glm::determinant(inverse) > 0)) return true;
    if (inverse[0][0] == inverse[1][1] && inverse[1][1] == inverse[2][2] &&
        inverse[0][1] == 0 && inverse[0][2] == 0 && inverse[1][0] == 0 &&
        inverse[1][2] == 0 && inverse[2][0] == 0 && inverse[2][1] == 0) return true;
    const auto rotation = glm::mat3_cast(glm::normalize(glm::dquat(body.orientation)));
    const auto omega = glm::transpose(rotation) * glm::dvec3(body.angularVelocity);
    const auto momentum = glm::inverse(inverse) * omega;
    return glm::cross(momentum, omega) == glm::dvec3(0);
}

bool MakeLeaf(RigidMotionSegment& segment, unsigned owner, const RigidBody& body,
              double begin, double end, bool principal) {
    segment.owner = owner;
    segment.begin = begin;
    segment.end = end;
    segment.planarInverseIz=body.planarInverseIz;segment.planeNormal=body.planeNormal;
    segment.inverseMass = body.inverseMass;
    segment.inverseInertiaLocal = body.inverseInertiaLocal;
    segment.position = body.position;
    segment.orientation = body.orientation;
    segment.linearVelocity = body.linearVelocity;
    segment.angularVelocity = body.angularVelocity;
    segment.exactRotation = principal;
    if (principal) segment.driftAngularVelocity = body.angularVelocity;
    else if (!TryAngularDriftGenerator(body.inverseInertiaLocal, body.orientation,
                                       body.angularVelocity, float(end-begin),
                                       segment.driftAngularVelocity)) return false;
    const auto pose = segment.EvaluateGeometry(end);
    segment.endPosition = pose.position;
    segment.endOrientation = pose.orientation;
    return true;
}

void Append(std::vector<RigidMotionSegment>& result, unsigned owner, const RigidBody& body,
            double begin, double end, unsigned& leaves, unsigned depth) {
    RigidMotionSegment whole, first, second;
    const bool principal = Principal(body);
    if ((principal || begin == end) && MakeLeaf(whole, owner, body, begin, end, principal)) {
        if (++leaves > kMaximumLeaves) throw std::runtime_error("rotation trajectory leaf budget exceeded");
        result.push_back(whole);
        return;
    }
    const double middle = begin + .5*(end-begin);
    if (depth >= 24 || !(begin < middle && middle < end))
        throw std::runtime_error("rotation trajectory interval cannot be refined");
    const bool full = MakeLeaf(whole, owner, body, begin, end, false);
    const bool halves = MakeLeaf(first, owner, body, begin, middle, false) &&
        MakeLeaf(second, owner, first.Evaluate(middle), middle, end, false);
    if (full && halves) {
        const glm::dquat a(whole.endOrientation), b(second.endOrientation);
        const double error = 2*std::min(glm::length(a-b), glm::length(a+b));
        if (error <= kAngleError) {
            if (++leaves > kMaximumLeaves) throw std::runtime_error("rotation trajectory leaf budget exceeded");
            result.push_back(whole);
            return;
        }
    }
    Append(result, owner, body, begin, middle, leaves, depth+1);
    Append(result, owner, result.back().Evaluate(middle), middle, end, leaves, depth+1);
}
} // namespace

void AppendDynamicRigidMotion(std::vector<RigidMotionSegment>& result, unsigned owner,
                              const RigidBody& body, double begin, double end) {
    unsigned leaves = 0;
    const auto first = result.size();
    Append(result, owner, body, begin, end, leaves, 0);
    for (auto i = first; i < result.size(); ++i) {
        auto& leaf = result[i];
        leaf.anchoredTranslation = true;
        leaf.translationOrigin = body.position;
        leaf.translationTime = begin;
        leaf.position = body.position + body.linearVelocity * float(leaf.begin - begin);
        leaf.endPosition = body.position + body.linearVelocity * float(leaf.end - begin);
    }
}
