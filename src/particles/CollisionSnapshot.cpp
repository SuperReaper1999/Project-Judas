#include "CollisionSnapshot.h"

#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "Narrowphase.h"
#include "PhysicsCastGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace JudasParticles {
namespace {
bool Finite(glm::dvec3 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool ValidDomain(const Aabb& b) {
    return Finite(b.min) && Finite(b.max) && glm::all(glm::lessThanEqual(b.min, b.max));
}
glm::dvec3 CenterOfMass(const BodyTransform& pivot, glm::vec3 offset) {
    return glm::dvec3(pivot.position) + glm::dvec3(pivot.rotation * offset);
}
BodyTransform SegmentPose(const PhysicsWorld::BodyMotionSegment& segment, double t) {
    const auto pivot = PhysicsWorld::EvaluateBodyMotionSegment(segment, t);
    return {glm::vec3(CenterOfMass(pivot, segment.pivotOffset)), pivot.rotation};
}
BodyTransform ResolvedEnd(const PhysicsWorld::BodyMotionSegment& segment) {
    return {glm::vec3(CenterOfMass(segment.endPose, segment.pivotOffset)), segment.endPose.rotation};
}
glm::dvec3 SurfaceVelocity(const CollisionBody& body,
                           const PhysicsWorld::BodyMotionSegment* segment,
                           const BodyTransform& pose, glm::dvec3 point, double time) {
    glm::dvec3 linear = body.linearVelocity, angular = body.angularVelocity;
    if (segment) {
        linear = segment->linearVelocity;
        angular = segment->angularVelocity;
        // The normalized midpoint quaternion follows the Cayley arc, whose
        // instantaneous angle rate is generator/(1+(|generator| elapsed/2)^2).
        // Prescribed/principal-axis leaves follow the exact exponential arc.
        if (!segment->prescribed && !segment->exactRotation) {
            const double halfAngle = glm::length(angular) * (time - segment->begin) * .5;
            angular /= 1 + halfAngle * halfAngle;
        }
    }
    return linear + glm::cross(angular, point - glm::dvec3(pose.position));
}
Shape ColliderShape(const CollisionCollider& c) {
    Shape shape;
    shape.type = c.kind;
    shape.radius = c.radius;
    shape.halfHeight = c.halfHeight;
    shape.halfExtents = c.halfExtents;
    shape.asset = c.asset;
    shape.terrain = c.terrain;
    return shape;
}
PrimitivePose Primitive(const CollisionCollider& c, const BodyTransform& pose) {
    RigidBody body;
    body.position = glm::vec3(glm::dvec3(pose.position) +
                              ContactRotation(pose.rotation) * glm::dvec3(c.localCenter));
    body.orientation = glm::normalize(pose.rotation * c.localRotation);
    return {ColliderShape(c), body, pose.position, pose.rotation, c.localCenter,
            c.localRotation, c.childKey};
}
bool SameGeometry(const CollisionCollider& a, const CollisionCollider& b) {
    return a.childKey == b.childKey && a.kind == b.kind && a.radius == b.radius &&
           a.halfHeight == b.halfHeight && a.halfExtents == b.halfExtents &&
           a.localCenter == b.localCenter && a.localRotation == b.localRotation &&
           a.asset == b.asset && a.terrain == b.terrain;
}
bool Supported(const CollisionCollider& c) {
    return c.kind == ShapeType::Sphere || c.kind == ShapeType::Box ||
           c.kind == ShapeType::Capsule || c.kind == ShapeType::ConvexHull ||
           c.kind == ShapeType::TriangleMesh || c.kind == ShapeType::Terrain;
}
bool Moving(const PhysicsWorld::BodyMotionSegment& s) {
    return s.movable && (s.linearVelocity != glm::vec3(0) || s.angularVelocity != glm::vec3(0));
}
bool Stationary(const CollisionBody& body) {
    const auto same = [&](const BodyTransform& pose) {
        return pose.position == body.pose.position && pose.rotation == body.pose.rotation;
    };
    for (const auto& segment : body.motion)
        if (Moving(segment) || !same(SegmentPose(segment, segment.begin)) ||
            !same(SegmentPose(segment, segment.end)) || !same(ResolvedEnd(segment)))
            return false;
    return true;
}
Aabb SweptBodyBounds(const CollisionBody& body) {
    glm::dvec3 lo(body.pose.position), hi(lo);
    for (const auto& s : body.motion) {
        for (const auto p : {SegmentPose(s, s.begin), SegmentPose(s, s.end), ResolvedEnd(s)}) {
            lo = glm::min(lo, glm::dvec3(p.position));
            hi = glm::max(hi, glm::dvec3(p.position));
        }
    }
    lo -= body.boundingRadius;
    hi += body.boundingRadius;
    Aabb bounds;
    for (unsigned axis = 0; axis < 3; ++axis) {
        bounds.min[axis] = std::nextafter(float(lo[axis]), -std::numeric_limits<float>::infinity());
        bounds.max[axis] = std::nextafter(float(hi[axis]), std::numeric_limits<float>::infinity());
    }
    return bounds;
}
struct Distance {
    bool valid = false;
    double gap = 0;
    glm::dvec3 point{0}, normal{0};
    std::uint32_t feature = UINT32_MAX;
};
Distance SphereDistance(const CollisionCollider& collider, const BodyTransform& pose,
                        glm::dvec3 center, double radius) {
    const auto primitive = Primitive(collider, pose);
    if (collider.asset) {
        const auto d = SegmentGeometry(center, center, radius, primitive);
        return {d.valid, d.gap, d.point, d.normal, d.feature};
    }
    const auto d = PointGeometry(center, primitive);
    return {d.valid, (d.contains ? -d.gap : d.gap) - radius, d.point, d.normal, d.feature};
}
} // namespace

bool CollisionSnapshot::Capture(const PhysicsWorld& physics, const Aabb& domain, double dt,
                                 std::string& error, const PhysicsQueryFilter& filter,
                                 const EntityMapper& mapper, const CollisionCaptureLimits& limits) {
    if (!ValidDomain(domain) || !std::isfinite(dt) || dt < 0 || !limits.maxBodies ||
        !limits.maxColliders || !limits.maxTriangles || !limits.maxMotionSegments) {
        error = "particle collision capture requires finite bounds, nonnegative time and positive budgets";
        return false;
    }
    try {
        std::vector<CollisionBody> bodies;
        std::vector<CollisionCollider> colliders;
        CollisionCaptureStats stats;
        std::set<const CollisionAsset*> assets;
        std::uint64_t next = m_nextGeometryRevision;
        std::unordered_map<std::uint64_t, const CollisionCollider*> previousGeometry;
        for (const auto& old : m_colliders)
            previousGeometry.emplace((std::uint64_t(m_bodies[old.bodyIndex].handle.id) << 32) |
                                         old.childKey, &old);
        // A single filtered enumeration is intentionally O(live bodies), not
        // O(particles). A current-pose-only query would miss a fast object that
        // crossed this domain earlier in the recorded interval and departed.
        const float extent = std::numeric_limits<float>::max();
        const auto handles = physics.QueryBodiesInAabb(glm::vec3(-extent), glm::vec3(extent), filter);
        stats.examinedBodies = handles.size();
        for (const auto handle : handles) {
            Shape shape;
            CollisionBody body;
            body.handle = handle;
            if (!physics.GetBodyShape(handle, shape, body.pose))
                continue;
            body.boundingRadius = ShapeBoundingRadius(shape);
            body.linearVelocity = physics.GetLinearVelocity(handle);
            body.angularVelocity = physics.GetAngularVelocity(handle);
            body.motion = physics.GetBodyMotionSegments(handle);
            if (!SweptBodyBounds(body).Overlaps(domain))
                continue;
            if (!body.motion.empty() &&
                (body.motion.front().begin != 0 ||
                 std::abs(body.motion.back().end - dt) > 1e-6 * std::max(1., dt))) {
                error = "particle collision interval does not match recorded body motion";
                return false;
            }
            if (bodies.size() == limits.maxBodies ||
                stats.motionSegments + body.motion.size() > limits.maxMotionSegments) {
                error = "particle collision body/motion budget exceeded; previous snapshot retained";
                return false;
            }
            body.entity = mapper ? mapper(handle) : 0;
            const auto bodyIndex = bodies.size();
            stats.motionSegments += body.motion.size();
            bodies.push_back(std::move(body));
            RigidBody rigid;
            rigid.position = bodies.back().pose.position;
            rigid.orientation = bodies.back().pose.rotation;
            bool unsupported = false;
            for (int part = 0; part < PrimitiveCount(shape); ++part) {
                if (colliders.size() == limits.maxColliders) {
                    error = "particle collision primitive budget exceeded; previous snapshot retained";
                    return false;
                }
                const auto primitive = PrimitiveAt(shape, rigid, part);
                CollisionCollider c;
                c.bodyIndex = bodyIndex;
                c.childKey = primitive.childKey;
                c.kind = primitive.shape.type;
                c.radius = primitive.shape.radius;
                c.halfHeight = primitive.shape.halfHeight;
                c.halfExtents = primitive.shape.halfExtents;
                c.localCenter = primitive.parentLocalCenter;
                c.localRotation = primitive.childRotation;
                c.asset = primitive.shape.asset;
                c.terrain = primitive.shape.terrain;
                if (c.asset && assets.insert(c.asset.get()).second) {
                    stats.uniqueTriangles += c.asset->faces.size();
                    if (stats.uniqueTriangles > limits.maxTriangles) {
                        error = "particle collision triangle budget exceeded; previous snapshot retained";
                        return false;
                    }
                }
                unsupported |= !Supported(c);
                const auto previous = previousGeometry.find((std::uint64_t(handle.id) << 32) | c.childKey);
                c.geometryRevision = previous != previousGeometry.end() && SameGeometry(*previous->second, c)
                                         ? previous->second->geometryRevision : next++;
                if (!c.geometryRevision || !next) {
                    error = "particle geometry identity exhausted";
                    return false;
                }
                colliders.push_back(std::move(c));
            }
            stats.unsupportedBodies += unsupported;
        }
        stats.retainedBodies = bodies.size();
        stats.colliders = colliders.size();
        auto token=std::make_shared<const unsigned char>(0);
        m_bodies.swap(bodies);
        m_colliders.swap(colliders);
        m_domain = domain;
        m_dt = dt;
        m_stats = stats;
        m_nextGeometryRevision = next;
        m_captureToken = std::move(token);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string("particle collision capture failed: ") + exception.what();
        return false;
    }
}

BodyTransform CollisionSnapshot::PoseAt(std::size_t index, double fraction) const {
    if (index >= m_bodies.size() || !std::isfinite(fraction) || fraction < 0 || fraction > 1)
        throw std::out_of_range("particle collision pose sample");
    const auto& body = m_bodies[index];
    if (body.motion.empty() || fraction == 1)
        return body.pose;
    const double time = fraction * m_dt;
    auto it = std::upper_bound(body.motion.begin(), body.motion.end(), time,
        [](double t, const auto& segment) { return t < segment.begin; });
    if (it == body.motion.begin())
        throw std::out_of_range("particle collision motion starts after sample");
    --it;
    if (time < it->begin || time > it->end)
        throw std::out_of_range("particle collision motion has a gap");
    return SegmentPose(*it, time);
}

bool CollisionSnapshot::IsStationary(std::size_t body) const {
    return Stationary(m_bodies.at(body));
}

ParticleCollisionHit CollisionSnapshot::SweepSphere(glm::dvec3 from, glm::dvec3 to, double radius,
    double beginFraction, double endFraction, unsigned maxIterations, double tolerance) const {
    if (!Finite(from) || !Finite(to) || !std::isfinite(radius) || radius < 0 ||
        !std::isfinite(beginFraction) || !std::isfinite(endFraction) || beginFraction < 0 ||
        beginFraction > endFraction || endFraction > 1 || !std::isfinite(tolerance) || tolerance <= 0)
        throw std::invalid_argument("invalid particle collision sweep");
    if ((m_dt == 0 || beginFraction == endFraction) && from != to)
        throw std::invalid_argument("particle collision displacement requires a positive interval");
    ParticleCollisionHit best;
    if (glm::any(glm::lessThan(glm::min(from, to) - radius, glm::dvec3(m_domain.min))) ||
        glm::any(glm::greaterThan(glm::max(from, to) + radius, glm::dvec3(m_domain.max)))) {
        best.status = CollisionQueryStatus::OutsideDomain;
        return best;
    }
    if (!maxIterations) {
        best.status = CollisionQueryStatus::Exhausted;
        return best;
    }
    const double begin = beginFraction * m_dt, finish = endFraction * m_dt;
    const double seconds = finish - begin;
    const auto particleAt = [&](double t) {
        return seconds > 0 ? glm::mix(from, to, (t - begin) / seconds) : from;
    };
    const auto record = [&](std::size_t index, double time, const BodyTransform& pose,
                            const PhysicsWorld::BodyMotionSegment* segment, const Distance& d,
                            bool initial, bool repaired) {
        const auto& c = m_colliders[index];
        const auto& body = m_bodies[c.bodyIndex];
        const double fraction = m_dt > 0 ? time / m_dt : beginFraction;
        if (best.status == CollisionQueryStatus::Hit &&
            (fraction > best.fraction ||
             (fraction == best.fraction &&
              std::make_pair(body.handle.id, c.childKey) >= std::make_pair(best.body.id, best.childKey))))
            return;
        best = {};
        best.status = CollisionQueryStatus::Hit;
        best.fraction = fraction;
        best.initialOverlap = initial;
        best.repairedEndpoint = repaired;
        best.penetration = std::max(0., -d.gap);
        best.center = particleAt(time);
        best.point = d.point;
        best.normal = d.normal;
        best.surfaceVelocity = SurfaceVelocity(body, segment, pose, d.point, time);
        const auto inverse = glm::transpose(ContactRotation(pose.rotation));
        best.localPoint = inverse * (d.point - glm::dvec3(pose.position));
        best.localNormal = inverse * d.normal;
        best.body = body.handle;
        best.entity = body.entity;
        best.geometryRevision = c.geometryRevision;
        best.childKey = c.childKey;
        best.feature = d.feature;
        best.colliderIndex = index;
    };
    for (std::size_t index = 0; index < m_colliders.size(); ++index) {
        const auto& c = m_colliders[index];
        const auto& body = m_bodies[c.bodyIndex];
        if (!Supported(c)) {
            best.status = CollisionQueryStatus::Unsupported;
            return best;
        }
        // Concave finite-feature casts are already supported by the native
        // static kernel. A moving concave union / arbitrary terrain callback
        // needs its own certified motion query, never a frozen-pose substitute.
        const bool stationary = Stationary(body);
        if ((c.kind == ShapeType::TriangleMesh || c.kind == ShapeType::Terrain) && !stationary) {
            best.status = CollisionQueryStatus::Unsupported;
            return best;
        }
        if (stationary) {
            const auto displacement = to - from;
            const double length = glm::length(displacement);
            try {
                const auto hit = CastAgainstPrimitive(Shape::Sphere(float(radius)),
                    {glm::vec3(from), {1, 0, 0, 0}},
                    length > 0 ? glm::vec3(displacement / length) : glm::vec3(1, 0, 0),
                    length, Primitive(c, body.pose), length == 0);
                if (hit.hit) {
                    const double t = length > 0 ? begin + seconds * hit.distance / length : begin;
                    const auto d = SphereDistance(c, body.pose, particleAt(t), radius);
                    record(index, t, body.pose, nullptr,
                        {true, d.valid ? d.gap : 0, hit.point, hit.normal, hit.feature},
                        hit.initialOverlap, false);
                }
            } catch (const std::exception&) {
                best.status = CollisionQueryStatus::Exhausted;
                return best;
            }
            continue;
        }
        const double particleSpeed = seconds > 0 ? glm::length(to - from) / seconds : 0;
        for (const auto& segment : body.motion) {
            const double lo = std::max(begin, segment.begin), hi = std::min(finish, segment.end);
            if (lo > hi)
                continue;
            double time = lo;
            const double speed = particleSpeed + glm::length(glm::dvec3(segment.linearVelocity)) +
                glm::length(glm::dvec3(segment.angularVelocity)) * body.boundingRadius;
            bool completed = false;
            for (unsigned iteration = 0; iteration < maxIterations; ++iteration) {
                const auto pose = SegmentPose(segment, time);
                const auto d = SphereDistance(c, pose, particleAt(time), radius);
                if (!d.valid || !std::isfinite(d.gap)) {
                    best.status = CollisionQueryStatus::Unsupported;
                    return best;
                }
                if (d.gap <= tolerance) {
                    record(index, time, pose, &segment, d, time == begin && d.gap <= 0, false);
                    completed = true;
                    break;
                }
                if (time == hi || !(speed > 0) || d.gap - tolerance > speed * (hi - time)) {
                    completed = true;
                    break;
                }
                // Approach half the declared distance tolerance, avoiding a
                // stall just outside it due to represented body-pose rounding.
                // This step remains shorter than the gap / closing bound.
                const double next = std::min(hi, time + (d.gap - tolerance * .5) / speed);
                if (!(next > time))
                    break;
                time = next;
            }
            if (!completed) {
                best.status = CollisionQueryStatus::Exhausted;
                return best;
            }
            // MK2 may correct an endpoint position after resolving constraints.
            // Sample that final geometry as overlap; don't sweep the correction.
            if (hi == segment.end) {
                const auto endpoint = ResolvedEnd(segment);
                const auto d = SphereDistance(c, endpoint, particleAt(hi), radius);
                if (d.valid && d.gap <= tolerance)
                    record(index, hi, endpoint, nullptr, d, hi == begin && d.gap <= 0, true);
            }
        }
    }
    return best;
}

ParticleCollisionHit CollisionSnapshot::OverlapSphere(glm::dvec3 center, double radius,
                                                       double fraction, double tolerance) const {
    return SweepSphere(center, center, radius, fraction, fraction, 128, tolerance);
}

CollisionAnchor CollisionSnapshot::MakeAnchor(const ParticleCollisionHit& hit) const {
    if (hit.status != CollisionQueryStatus::Hit || hit.colliderIndex >= m_colliders.size())
        throw std::invalid_argument("particle collision anchor requires a captured hit");
    const auto& c = m_colliders[hit.colliderIndex];
    const auto& body = m_bodies[c.bodyIndex];
    if (body.handle.id != hit.body.id || c.geometryRevision != hit.geometryRevision)
        throw std::invalid_argument("particle collision hit belongs to another snapshot");
    return {body.handle, c.geometryRevision, c.childKey,
            hit.localPoint, hit.localNormal};
}

bool CollisionSnapshot::ResolveAnchor(const CollisionAnchor& anchor, double fraction,
    glm::dvec3& point, glm::dvec3& normal, glm::dvec3* velocity) const {
    for (const auto& c : m_colliders) {
        const auto& body = m_bodies[c.bodyIndex];
        if (body.handle.id != anchor.body.id || c.childKey != anchor.childKey ||
            c.geometryRevision != anchor.geometryRevision)
            continue;
        const auto pose = PoseAt(c.bodyIndex, fraction);
        const auto rotation = ContactRotation(pose.rotation);
        point = glm::dvec3(pose.position) + rotation * anchor.localPoint;
        normal = rotation * anchor.localNormal;
        if (velocity) {
            const double time = fraction * m_dt;
            const PhysicsWorld::BodyMotionSegment* active = nullptr;
            if (fraction != 1)
                for (const auto& segment : body.motion)
                    if (segment.begin <= time && segment.end >= time)
                        active = &segment;
            *velocity = SurfaceVelocity(body, active, pose, point, time);
        }
        return true;
    }
    return false;
}

glm::dvec3 CollisionResponse(glm::dvec3 velocity, const ParticleCollisionHit& hit,
                              double restitution, double friction, bool slide) {
    if (hit.status != CollisionQueryStatus::Hit || !Finite(velocity) ||
        !Finite(hit.normal) || !Finite(hit.surfaceVelocity) ||
        std::abs(glm::dot(hit.normal, hit.normal) - 1) > 1e-3 ||
        !std::isfinite(restitution) || restitution < 0 || restitution > 1 ||
        !std::isfinite(friction) || friction < 0)
        throw std::invalid_argument("invalid visual particle collision response");
    const auto normal = glm::normalize(hit.normal);
    auto relative = velocity - hit.surfaceVelocity;
    const double closing = glm::dot(relative, normal);
    if (closing >= 0)
        return velocity;
    const double impulse = -(1 + (slide ? 0 : restitution)) * closing;
    relative += normal * impulse;
    const auto tangent = relative - normal * glm::dot(relative, normal);
    const double speed = glm::length(tangent);
    if (speed > 0)
        relative -= tangent * std::min(1., friction * impulse / speed);
    return relative + hit.surfaceVelocity;
}

} // namespace JudasParticles
