#pragma once

#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "RigidMotionBounds.h"
#include <array>
#include <vector>

namespace episode_probe {
using V = glm::dvec3;
using M = glm::dmat3;
using Iv = contact_geometry::Iv;

enum class Result { Connected, Separated, Uncertain };
struct Measurement {
    Result result = Result::Uncertain;
    unsigned intervals = 0, pointCertificates = 0, constantCertificates = 0;
    double separationTime = -1;
};
struct Plane { V normal; double offset; };

inline std::vector<Plane> Planes(const Shape &shape) {
    std::vector<Plane> planes;
    if (shape.type == ShapeType::Box) {
        for (unsigned axis = 0; axis < 3; ++axis)
            for (double sign : {-1., 1.}) {
                V normal(0); normal[axis] = sign;
                planes.push_back({normal, shape.halfExtents[axis]});
            }
    } else if (shape.type == ShapeType::ConvexHull && shape.asset) {
        for (const auto &face : shape.asset->polygons)
            planes.push_back({face.normal,
                              glm::dot(face.normal, shape.asset->vertices[face.vertices[0]])});
    }
    return planes;
}

inline PrimitivePose At(const PrimitivePose &reference, const RigidMotionSegment &segment,
                        double time) {
    PrimitivePose result = reference;
    const auto pose = segment.EvaluateGeometry(time);
    result.parentPosition = pose.position;
    result.parentOrientation = pose.orientation;
    return result;
}

// The exact finite triangle/convex intersection in the moving primitive frame.
// No infinite plane, old witness, contact skin or expanded mesh is substituted.
inline std::vector<V> Intersection(const PrimitivePose &query, const PrimitivePose &mesh,
                                   unsigned feature, const std::vector<Plane> &planes) {
    const V origin = PrimitiveCenter(mesh), center = PrimitiveCenter(query) - origin;
    const M rotation = PrimitiveRotation(query), inverse = glm::transpose(rotation);
    const M fixed = PrimitiveRotation(mesh);
    std::vector<V> polygon;
    for (auto vertex : mesh.shape.asset->faces[feature].vertices)
        polygon.push_back(inverse * (fixed * mesh.shape.asset->vertices[vertex] - center));
    for (const auto &plane : planes) {
        std::vector<V> clipped;
        if (polygon.empty()) break;
        V previous = polygon.back();
        double before = glm::dot(plane.normal, previous) - plane.offset;
        for (V point : polygon) {
            const double after = glm::dot(plane.normal, point) - plane.offset;
            if ((before <= 0) != (after <= 0))
                clipped.push_back(previous + (point - previous) * (before / (before - after)));
            if (after <= 0) clipped.push_back(point);
            previous = point; before = after;
        }
        polygon.swap(clipped);
    }
    for (auto &point : polygon) point = center + rotation * point;
    return polygon;
}

inline bool Constant(const RigidMotionSegment &segment, double start, double finish) {
    const auto from = segment.EvaluateGeometry(start), to = segment.EvaluateGeometry(finish);
    if (from.position != to.position) return false;
    if (segment.prescribed) return segment.driftAngularVelocity == glm::vec3(0);
    const glm::quat omega(0, segment.driftAngularVelocity.x,
                         segment.driftAngularVelocity.y, segment.driftAngularVelocity.z);
    const auto raw = [&](double time) {
        return segment.orientation + (omega * segment.orientation) * (.5f * float(time - segment.begin));
    };
    // All raw components and centre components are monotone represented float
    // expressions. Equal endpoints prove every interior represented pose equal.
    return rigid_motion_bounds::SameQuat(raw(start), raw(finish));
}

inline std::optional<std::array<Iv, 3>> TriangleWitness(
    V approximate, const PrimitivePose &mesh, unsigned feature) {
    const auto &face = mesh.shape.asset->faces[feature];
    const auto rotation = PrimitiveRotation(mesh);
    const V first = rotation * mesh.shape.asset->vertices[face.vertices[0]],
            ab = rotation * mesh.shape.asset->vertices[face.vertices[1]] - first,
            ac = rotation * mesh.shape.asset->vertices[face.vertices[2]] - first,
            offset = approximate - first;
    const double aa = glm::dot(ab, ab), bb = glm::dot(ab, ac), cc = glm::dot(ac, ac),
                 ax = glm::dot(ab, offset), cx = glm::dot(ac, offset), denominator = aa*cc-bb*bb;
    if (!(denominator > 0)) return std::nullopt;
    const double u = (cc*ax-bb*cx)/denominator, v = (aa*cx-bb*ax)/denominator;
    if (!std::isfinite(u) || !std::isfinite(v)) return std::nullopt;
    // Exact dyadic barycentric coefficients keep the witness on the finite
    // triangle; transforming clipped vertices back alone could leave its plane
    // by a rounding error. Their sum is exact with this 40-bit denominator.
    constexpr uint64_t scale = uint64_t(1) << 40;
    const auto numeratorU = uint64_t(std::floor(std::clamp(u,0.,1.)*double(scale)));
    const auto numeratorV = std::min(scale-numeratorU,
        uint64_t(std::floor(std::clamp(v,0.,1.)*double(scale))));
    const double weightU = double(numeratorU)/double(scale), weightV = double(numeratorV)/double(scale);
    const V second = rotation * mesh.shape.asset->vertices[face.vertices[1]],
            third = rotation * mesh.shape.asset->vertices[face.vertices[2]];
    std::array<Iv,3> point;
    for(unsigned k=0;k<3;++k)
        point[k] = Iv(first[k]) + Iv(weightU)*(Iv(second[k])-Iv(first[k])) +
                   Iv(weightV)*(Iv(third[k])-Iv(first[k]));
    return point;
}

inline bool ContainsThroughout(const std::array<Iv, 3> &point, const PrimitivePose &reference,
                               const PrimitivePose &mesh, const RigidMotionSegment &segment,
                               const std::vector<Plane> &planes, double start, double finish) {
    const auto from = segment.EvaluateGeometry(start), to = segment.EvaluateGeometry(finish);
    std::optional<contact_geometry::M<Iv>> rotation;
    if (segment.driftAngularVelocity == glm::vec3(0) &&
        rigid_motion_bounds::SameQuat(from.orientation, to.orientation)) {
        const auto fixed = ContactRotation(from.orientation);
        contact_geometry::M<Iv> value;
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 3; ++j) value[i][j] = Iv(fixed[i][j]);
        rotation = value;
    } else rotation = rigid_motion_bounds::TryDynamicRotationInterval(segment, start, finish);
    if (!rotation) return false;
    try {
        const V origin = PrimitiveCenter(mesh);
        std::array<Iv, 3> relative, parentLocal, childLocal;
        for (unsigned k = 0; k < 3; ++k) {
            const auto first = double(from.position[k]) - origin[k];
            const auto last = double(to.position[k]) - origin[k];
            relative[k] = point[k] - Iv(std::min(first, last), std::max(first, last));
        }
        for (unsigned j = 0; j < 3; ++j) {
            parentLocal[j] = Iv(-double(reference.parentLocalCenter[j]));
            for (unsigned k = 0; k < 3; ++k)
                parentLocal[j] = parentLocal[j] + (*rotation)[j][k] * relative[k];
        }
        const auto child = ContactRotation(reference.childRotation);
        for (unsigned j = 0; j < 3; ++j) {
            childLocal[j] = Iv(0);
            for (unsigned k = 0; k < 3; ++k)
                childLocal[j] = childLocal[j] + Iv(child[j][k]) * parentLocal[k];
        }
        for (const auto &plane : planes) {
            Iv distance(-plane.offset);
            for (unsigned k = 0; k < 3; ++k)
                distance = distance + Iv(plane.normal[k]) * childLocal[k];
            if (distance.hi > 0) return false;
        }
        return true;
    } catch (const contact_geometry::ArithmeticFailure &) { return false; }
}

inline Measurement Connected(const PrimitivePose &query, const PrimitivePose &mesh,
                              unsigned feature, const RigidMotionSegment &segment,
                              double start, double finish, unsigned maximumIntervals = 1024) {
    Measurement result;
    const auto planes = Planes(query.shape);
    if (planes.empty() || mesh.shape.type != ShapeType::TriangleMesh ||
        !mesh.shape.asset || feature >= mesh.shape.asset->faces.size() || start > finish)
        return result;
    for (double time : {start, finish})
        if (Intersection(At(query, segment, time), mesh, feature, planes).empty()) {
            result.result = Result::Separated; result.separationTime = time; return result;
        }
    struct Range { double first, last; unsigned depth; };
    std::vector<Range> ranges{{start, finish, 0}};
    while (!ranges.empty()) {
        const auto range = ranges.back(); ranges.pop_back();
        if (++result.intervals > maximumIntervals) return result;
        if (Constant(segment, range.first, range.last)) {
            ++result.constantCertificates; continue;
        }
        const double middle = (range.first + range.last) * .5;
        auto polygon = Intersection(At(query, segment, middle), mesh, feature, planes);
        if (polygon.empty()) {
            result.result = Result::Separated; result.separationTime = middle; return result;
        }
        V witness(0); for (V point : polygon) witness += point;
        witness /= double(polygon.size());
        const auto point = TriangleWitness(witness, mesh, feature);
        if (point && ContainsThroughout(*point, query, mesh, segment, planes, range.first, range.last)) {
            ++result.pointCertificates; continue;
        }
        if (range.depth >= 40 || middle <= range.first || middle >= range.last) return result;
        ranges.push_back({middle, range.last, range.depth + 1});
        ranges.push_back({range.first, middle, range.depth + 1});
    }
    result.result = Result::Connected;
    return result;
}
} // namespace episode_probe
