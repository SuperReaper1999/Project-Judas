#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::ClosestBodyToCapsule(const std::vector<unsigned int> &candidates,
                                              const glm::vec3 &segA, const glm::vec3 &segB,
                                              float capsuleRadius, float bodyMotionAlpha,
                                              bool interpolateDynamicBodyMotion) const
    -> ClosestBodyResult {
    ClosestBodyResult result;
    for (const unsigned int i : candidates) {
        const Body &body = bodies[i];
        if (!body.enabled || body.sensor || body.queryOnly)
            continue;
        const bool interpolateBody = interpolateDynamicBodyMotion && body.HasMotion();
        const bool ledger =
            interpolateBody && bodyMotionAlpha < 1.0f && !body.motion.Segments().empty();
        const auto observed =
            ledger
                ? body.motion.Evaluate(std::clamp(double(bodyMotionAlpha), 0.0, 1.0) * stepDuration)
                : body.rigidBody;
        const glm::vec3 bodyPosition =
            ledger ? observed.position
            : interpolateBody
                ? glm::mix(body.previousPosition, body.rigidBody.position, bodyMotionAlpha)
                : body.rigidBody.position;
        const glm::quat bodyOrientation =
            ledger ? observed.orientation
            : interpolateBody
                ? glm::normalize(glm::slerp(body.previousOrientation, body.rigidBody.orientation,
                                            bodyMotionAlpha))
                : body.rigidBody.orientation;
        RigidBody sampledBody = body.rigidBody;
        sampledBody.position = bodyPosition;
        sampledBody.orientation = bodyOrientation;
        if (body.shape.type == ShapeType::Terrain) {
            if (!body.shape.terrain)
                continue;
            const float segmentLength = glm::length(segB - segA);
            const float minimumEndRadius =
                std::min(glm::length(segA - bodyPosition), glm::length(segB - bodyPosition));
            if (minimumEndRadius - segmentLength >
                body.shape.terrain->BoundRadius() + capsuleRadius)
                continue;
            // Sampling the whole capsule core, rather than just its
            // lower endpoint, also handles arbitrary capsule attitude
            // and terrain slopes without a universal up axis.
            constexpr int kSegmentSamples = 9;
            for (int sampleIndex = 0; sampleIndex < kSegmentSamples; ++sampleIndex) {
                const float t =
                    static_cast<float>(sampleIndex) / static_cast<float>(kSegmentSamples - 1);
                const TerrainSample sample =
                    SampleTerrainAtWorld(*body.shape.terrain, sampledBody, glm::mix(segA, segB, t));
                const float distance = sample.signedDistance - capsuleRadius;
                if (distance < result.distance) {
                    result.distance = distance;
                    result.normal = sample.outwardNormal;
                    result.point =
                        glm::mix(segA, segB, t) - sample.outwardNormal * sample.signedDistance;
                    result.bodyIndex = static_cast<int>(i);
                }
            }
            continue;
        }
        for (int part = 0; part < PrimitiveCount(body.shape); ++part) {
            const PrimitivePose primitive = PrimitiveAt(body.shape, sampledBody, part);
            CapsuleDistance capsuleDistance;
            if (primitive.shape.type == ShapeType::Sphere) {
                capsuleDistance = CapsuleDistanceToSphere(
                    segA, segB, capsuleRadius, primitive.body.position, primitive.shape.radius);
            } else if (primitive.shape.type == ShapeType::Box) {
                capsuleDistance =
                    CapsuleDistanceToBox(segA, segB, capsuleRadius, primitive.body.position,
                                         primitive.body.orientation, primitive.shape.halfExtents);
            } else if (primitive.shape.asset) {
                // This sampler only uses distance <= 0 to bracket overlap. A
                // triangle outside the capsule AABB cannot overlap it; use
                // the cooked BVH instead of scanning the entire mesh at
                // every one of the 24 sweep/20 refinement samples. Convex
                // containment still takes the evaluator's full inside path.
                const auto d = SegmentGeometry(glm::dvec3(segA), glm::dvec3(segB), capsuleRadius,
                                               primitive, 0);
                if (!d.valid)
                    continue;
                capsuleDistance = {float(d.gap), glm::vec3(d.normal), glm::vec3(d.point)};
            } else {
                continue;
            }
            if (capsuleDistance.distance < result.distance) {
                result.distance = capsuleDistance.distance;
                result.normal = capsuleDistance.normal;
                result.point = capsuleDistance.otherPoint;
                result.bodyIndex = static_cast<int>(i);
            }
        }
    }
    return result;
}

void PhysicsWorld::QueryBodiesInAabbInto(const glm::vec3 &min, const glm::vec3 &max,
                                         std::vector<BodyHandle> &output,
                                         const PhysicsQueryFilter &filter) const {
    output.clear();
    for (auto slot : m_impl->QuerySlots(Aabb{glm::min(min, max), glm::max(min, max)}))
        if (m_impl->MatchesQuery(slot, filter))
            output.push_back(m_impl->MakeHandle(slot));
}

std::vector<BodyHandle> PhysicsWorld::QueryBodiesInAabb(const glm::vec3 &min, const glm::vec3 &max,
                                                        const PhysicsQueryFilter &filter) const {
    std::vector<BodyHandle> result;
    for (const unsigned int slot :
         m_impl->QuerySlots(Aabb{glm::min(min, max), glm::max(min, max)})) {
        if (m_impl->MatchesQuery(slot, filter))
            result.push_back(m_impl->MakeHandle(slot));
    }
    return result;
}

bool PhysicsWorld::GetBodyBroadphaseBounds(BodyHandle handle, glm::vec3 &outMin,
                                           glm::vec3 &outMax) const {
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return false;
    const Aabb &fat = m_impl->tree.FatAabb(body->proxy);
    outMin = fat.min;
    outMax = fat.max;
    return true;
}

bool PhysicsWorld::GetBodyGeometryCacheState(BodyHandle handle, GeometryCacheState &state) const {
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return false;
    const auto &current = body->currentBound;
    const auto &previous = body->previousBound;
    state.currentPose = {current.position, current.orientation};
    state.previousPose = {previous.position, previous.orientation};
    state.currentMin = current.bound.min;
    state.currentMax = current.bound.max;
    state.previousMin = previous.bound.min;
    state.previousMax = previous.bound.max;
    state.currentValid = current.valid;
    state.previousValid = previous.valid;
    state.boundingRadius = body->boundingRadius;
    return true;
}

std::vector<PhysicsWorld::CollidingPair> PhysicsWorld::FindCollidingPairs() const {
    Impl &w = *m_impl;
    // Same candidate generation and narrowphase as Step, without solving.
    w.GenerateCandidatePairs();
    std::vector<CollidingPair> result;
    for (const auto &[slotA, slotB] : w.candidatePairs) {
        Impl::Body &a = w.bodies[slotA];
        Impl::Body &b = w.bodies[slotB];
        const auto &orientationA = w.Orientation(a);
        const auto &orientationB = w.Orientation(b);
        int points = 0;
        for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
            const PrimitivePose childA = PrimitiveAt(a.shape, a.rigidBody, partA, &orientationA);
            for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                const PrimitivePose childB =
                    PrimitiveAt(b.shape, b.rigidBody, partB, &orientationB);
                const ContactManifold manifold =
                    ComputeContacts(childA, childB, 0.0f, &orientationA, &orientationB);
                for (int p = 0; p < manifold.count; ++p) {
                    if (manifold.points[p].hit)
                        ++points;
                }
            }
        }
        if (points > 0)
            result.push_back({w.MakeHandle(slotA), w.MakeHandle(slotB), points});
    }
    return result;
}

bool PhysicsWorld::CreatePlayerShape(float radius, float halfHeight) {
    m_impl->playerShape = Shape::Capsule(radius, halfHeight);
    m_impl->hasPlayerShape = true;
    return true;
}

void PhysicsWorld::DestroyPlayerShape() {
    m_impl->hasPlayerShape = false;
}

ShapeSweepHit PhysicsWorld::SweepPlayerShape(const glm::vec3 &fromCenter, const glm::quat &rotation,
                                             const glm::vec3 &displacement,
                                             bool interpolateDynamicBodyMotion,
                                             float bodyMotionStart, float bodyMotionEnd,
                                             const PhysicsQueryFilter *filter) const {
    ShapeSweepHit result;
    if (!m_impl->hasPlayerShape)
        return result;

    return SweepCapsuleMotion(m_impl->playerShape.radius, m_impl->playerShape.halfHeight,
                              fromCenter, rotation, displacement, interpolateDynamicBodyMotion,
                              bodyMotionStart, bodyMotionEnd, filter, m_impl->playerCollisionLayer,
                              m_impl->playerCollisionMask, filter == nullptr);
}

ShapeSweepHit PhysicsWorld::SweepCapsuleMotion(
    float radius, float halfHeight, const glm::vec3 &fromCenter, const glm::quat &rotation,
    const glm::vec3 &displacement, bool interpolateDynamicBodyMotion, float bodyMotionStart,
    float bodyMotionEnd, const PhysicsQueryFilter *filter, unsigned collisionLayer,
    CategoryMask collisionMask, bool bilateralFilter) const {
    ShapeSweepHit result;
    if (!(radius > 0) || halfHeight < 0)
        return result;
    const float displacementLength = glm::length(displacement);
    if (displacementLength < 1.0e-6f && !interpolateDynamicBodyMotion)
        return result;

    const glm::vec3 localSegA(0.0f, -halfHeight, 0.0f);
    const glm::vec3 localSegB(0.0f, halfHeight, 0.0f);
    const float capsuleRadius = radius;

    auto worldSegmentAt = [&](const glm::vec3 &center) {
        return std::make_pair(center + rotation * localSegA, center + rotation * localSegB);
    };
    // Milestone 32: one broadphase query for the whole swept capsule; every
    // evaluation below then tests only those bodies. Dynamic bodies' fat
    // bounds already cover their previous-to-current motion.
    Aabb sweptBound;
    {
        const auto [a0, b0] = worldSegmentAt(fromCenter);
        const auto [a1, b1] = worldSegmentAt(fromCenter + displacement);
        sweptBound.min = glm::min(glm::min(a0, b0), glm::min(a1, b1));
        sweptBound.max = glm::max(glm::max(a0, b0), glm::max(a1, b1));
        sweptBound = sweptBound.Expanded(capsuleRadius + kSweepQueryEpsilon);
    }
    std::vector<unsigned int> candidates = m_impl->QuerySlots(sweptBound);
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&](unsigned slot) {
                           if (filter && !m_impl->MatchesQuery(slot, *filter))
                               return true;
                           if (!m_impl->bodies[slot].enabled ||
                               (m_impl->bodies[slot].sensor && (!filter || bilateralFilter)))
                               return true;
                           const auto &b = m_impl->bodies[slot];
                           return bilateralFilter &&
                                  !CollisionPermitted(collisionLayer, collisionMask,
                                                      b.collisionLayer, b.collisionMask);
                       }),
        candidates.end());
    auto evaluateAt = [&](float t) {
        const glm::vec3 center = fromCenter + displacement * t;
        const auto [segA, segB] = worldSegmentAt(center);
        const float bodyMotionAlpha = glm::mix(bodyMotionStart, bodyMotionEnd, t);
        return m_impl->ClosestBodyToCapsule(candidates, segA, segB, capsuleRadius, bodyMotionAlpha,
                                            interpolateDynamicBodyMotion);
    };

    // Already touching/overlapping at the very start of the sweep — report
    // an immediate zero-distance hit rather than marching forward, mirroring
    // the "already touching" case a grounded move-and-slide step produces
    // routinely (see docs/ARCHITECTURE.md, "Contact normal correctness and
    // the 'already touching' case," law #13 — this normal is likewise
    // unconditional, never derived from travel direction).
    const ClosestBodyResult startResult = evaluateAt(0.0f);
    if (startResult.bodyIndex >= 0 && startResult.distance <= 0.0f) {
        result.hit = true;
        result.distance = 0.0f;
        result.penetration = -startResult.distance;
        result.normal = startResult.normal;
        result.point = startResult.point;
        result.hitBody = m_impl->MakeHandle(static_cast<unsigned int>(startResult.bodyIndex));
        return result;
    }

    // March forward in substeps looking for the first t where the capsule
    // starts overlapping something, then refine that bracket by bisection.
    // Per-step displacements in this engine are always small (a fraction
    // of a meter — see docs/ARCHITECTURE.md's move-and-slide/ground-probe
    // distances), so a modest fixed substep count plus a short bisection
    // pass gives ample precision without needing closed-form continuous
    // collision detection for every shape pair.
    constexpr int kSubsteps = 24;
    constexpr int kBisectionIterations = 20;
    std::vector<float> divisions{0, 1};
    if (interpolateDynamicBodyMotion && bodyMotionEnd > bodyMotionStart && m_impl->stepDuration > 0)
        for (auto slot : candidates)
            for (const auto &segment : m_impl->bodies[slot].motion.Segments()) {
                const float t = (float(segment.end / m_impl->stepDuration) - bodyMotionStart) /
                                (bodyMotionEnd - bodyMotionStart);
                if (t > 0 && t < 1)
                    divisions.push_back(t);
            }
    std::sort(divisions.begin(), divisions.end());
    divisions.erase(std::unique(divisions.begin(), divisions.end()), divisions.end());
    float previousT = 0.0f;
    for (std::size_t interval = 1; interval < divisions.size(); ++interval)
        for (int step = 1; step <= kSubsteps; ++step) {
            const float t =
                glm::mix(divisions[interval - 1], divisions[interval], float(step) / kSubsteps);
            const ClosestBodyResult stepResult = evaluateAt(t);
            if (stepResult.bodyIndex >= 0 && stepResult.distance <= 0.0f) {
                float lo = previousT;
                float hi = t;
                ClosestBodyResult refined = stepResult;
                for (int iteration = 0; iteration < kBisectionIterations; ++iteration) {
                    const float mid = (lo + hi) * 0.5f;
                    const ClosestBodyResult midResult = evaluateAt(mid);
                    if (midResult.bodyIndex >= 0 && midResult.distance <= 0.0f) {
                        hi = mid;
                        refined = midResult;
                    } else {
                        lo = mid;
                    }
                }
                result.hit = true;
                result.distance = lo * displacementLength;
                result.normal = refined.normal;
                result.point = refined.point;
                result.hitBody = m_impl->MakeHandle(static_cast<unsigned int>(refined.bodyIndex));
                return result;
            }
            previousT = t;
        }

    return result; // no hit across the entire displacement
}

bool PhysicsWorld::ReadBodyContactSample(BodyHandle handle, std::vector<BodyContactSample> &output,
                                         unsigned maximumContacts,
                                         unsigned maximumCandidates) const {
    output.clear();
    auto *body = m_impl ? m_impl->Get(handle) : nullptr;
    if (!body || !body->enabled || body->sensor || body->queryOnly || maximumContacts == 0 ||
        maximumContacts > 64 || maximumCandidates == 0 || maximumCandidates > 128)
        return false;
    const auto slot = handle.id & Impl::kSlotMask;
    unsigned count = 0;
    bool overflow = false;
    // Existing broadphase only selects the cache ranges to inspect. A nearby
    // candidate without an authoritative cached solver row is NOT a contact.
    m_impl->tree.Query(m_impl->tree.FatAabb(body->proxy), [&](int proxy) {
        const auto other = m_impl->tree.UserData(proxy);
        if (other == slot)
            return;
        if (++count > maximumCandidates) {
            overflow = true;
            return;
        }
        auto &support = m_impl->bodies[other];
        if (!support.alive || !m_impl->CanCollide(slot, other) || support.sensor ||
            support.queryOnly)
            return;
        const auto first = std::min(slot, other), second = std::max(slot, other);
        ContactKey key{
            first, second, m_impl->bodies[first].generation, m_impl->bodies[second].generation,
            -1,    -1};
        const auto begin = std::lower_bound(
            m_impl->completedContactObservations.begin(),
            m_impl->completedContactObservations.end(), key,
            [](const auto &cached, const auto &target) { return cached.key < target; });
        for (auto it = begin; it != m_impl->completedContactObservations.end() &&
                              it->key.slotA == first && it->key.slotB == second;
             ++it) {
            const auto &cached = *it;
            if (!cached.key.SameBodies(ContactKey{first, second, key.generationA, key.generationB,
                                                  cached.key.partA, cached.key.partB}) ||
                cached.normalImpulse <= 0 ||
                cached.observationRevisionA != m_impl->bodies[first].contactObservationRevision ||
                cached.observationRevisionB != m_impl->bodies[second].contactObservationRevision)
                continue;
            if (output.size() >= maximumContacts) {
                overflow = true;
                break;
            }
            const auto &a = m_impl->bodies[first];
            auto point = glm::vec3(glm::dvec3(a.rigidBody.position) +
                                   glm::mat3_cast(glm::dquat(a.rigidBody.orientation)) *
                                       cached.localAnchorA);
            BodyContactSample sample;
            sample.other = m_impl->MakeHandle(other);
            sample.point = point;
            sample.normal = slot == first ? cached.normal : -cached.normal;
            sample.normalImpulse = cached.normalImpulse;
            sample.separation = cached.separation;
            sample.supportVelocity =
                support.rigidBody.linearVelocity +
                glm::cross(support.rigidBody.angularVelocity, point - support.rigidBody.position);
            sample.sleeping = body->sleeping && (support.sleeping || support.rigidBody.IsStatic());
            output.push_back(sample);
        }
    });
    if (overflow) {
        output.clear();
        return false;
    }
    std::stable_sort(output.begin(), output.end(), [](const auto &a, const auto &b) {
        return std::tie(a.other.id, a.point.x, a.point.y, a.point.z) <
               std::tie(b.other.id, b.point.x, b.point.y, b.point.z);
    });
    return true;
}

PhysicsCastHit PhysicsWorld::CastPlanar(const Shape& shape,const BodyTransform& pose,const glm::vec3& direction,float maximum,const PhysicsQueryFilter& filter,PhysicsCastStats* stats) const {
    std::string error;
    if(shape.type!=ShapeType::Planar||!shape.planar||shape.planar->kind==PlanarKind::Chain||!ValidatePlanar(*shape.planar,error,true))throw std::invalid_argument("invalid convex 2D query: "+error);
    const auto n=glm::vec3(ContactRotation(pose.rotation)[2]);
    if(!FiniteVector(direction)||std::abs(glm::dot(n,direction))>1e-5f)throw std::invalid_argument("2D query direction must lie in its motion plane");
    auto domain=filter;domain.planar=true;
    return Cast(shape,pose,direction,maximum,domain,stats);
}

PhysicsCastHit PhysicsWorld::Cast(const Shape &shape, const BodyTransform &pose,
                                  const glm::vec3 &direction, float maximum,
                                  const PhysicsQueryFilter &filter, PhysicsCastStats *stats) const {
    JUDAS_PROFILE_SCOPE("Physics shape query");
    if (stats)
        *stats = {};
    auto finite = [](glm::vec3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    double q2 =
        glm::dot(glm::dvec4(pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w),
                 glm::dvec4(pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w));
    double d2 = glm::dot(glm::dvec3(direction), glm::dvec3(direction));
    if (!finite(pose.position) || !finite(direction) || !std::isfinite(maximum) || maximum < 0 ||
        d2 <= 0 || !std::isfinite(q2) || q2 <= 0)
        throw std::invalid_argument(
            "cast requires finite pose, nonzero orientation/direction and nonnegative distance");
    if (!finite(shape.halfExtents) || !std::isfinite(shape.radius) ||
        !std::isfinite(shape.halfHeight) || shape.radius < 0 || shape.halfHeight < 0 ||
        (shape.type == ShapeType::Box &&
         (shape.halfExtents.x <= 0 || shape.halfExtents.y <= 0 || shape.halfExtents.z <= 0)))
        throw std::invalid_argument("invalid cast dimensions");
    PhysicsCastHit result;
    if (!m_impl)
        return result;
    const glm::vec3 unit = glm::vec3(glm::dvec3(direction) / std::sqrt(d2));
    auto bound = ShapeAabb(shape, pose.position, pose.rotation);
    auto end = ShapeAabb(shape, pose.position + unit * maximum, pose.rotation);
    Aabb swept{glm::min(bound.min, end.min), glm::max(bound.max, end.max)};
    auto candidates = m_impl->QuerySlots(swept.Expanded(kSweepQueryEpsilon));
    if (stats)
        stats->broadphaseCandidates = static_cast<unsigned>(candidates.size());
    double nearest = maximum;
    for (auto slot : candidates) {
        if (!m_impl->MatchesQuery(slot, filter))
            continue;
        if (stats)
            ++stats->filteredCandidates;
        const auto &body = m_impl->bodies[slot];
        for (int part = 0; part < PrimitiveCount(body.shape); ++part) {
            if (stats)
                ++stats->primitivesTested;
            const auto primitive = PrimitiveAt(body.shape, body.rigidBody, part);
            const auto hit = shape.type==ShapeType::Planar?CastPlanarPrimitive(shape,pose,unit,nearest,primitive):CastAgainstPrimitive(shape, pose, unit, nearest, primitive);
            const auto handle = m_impl->MakeHandle(slot);
            if (hit.hit && (!result.hit || hit.distance < nearest ||
                            (hit.distance == nearest && handle.id < result.body.id))) {
                nearest = hit.distance;
                result.hit = true;
                result.initialOverlap = hit.initialOverlap;
                result.body = handle;
                result.point = hit.point;
                result.normal = hit.normal;
                result.distance = static_cast<float>(hit.distance);
                result.fraction = maximum > 0 ? result.distance / maximum : 0;
                result.primitiveIndex = part;
                result.shape = primitive.shape.type;
                result.childKey = primitive.childKey;
                result.feature = hit.feature;
            }
        }
    }
    return result;
}

PhysicsClosestPoint PhysicsWorld::ClosestPoint(const glm::vec3 &point, float maximum,
                                               const PhysicsQueryFilter &filter) const {
    for (int k = 0; k < 3; ++k)
        if (!std::isfinite(point[k]))
            throw std::invalid_argument("closestPoint requires finite point");
    if (!std::isfinite(maximum) || maximum < 0)
        throw std::invalid_argument("closestPoint requires finite nonnegative maximum radius");
    PhysicsClosestPoint out;
    if (!m_impl)
        return out;
    double nearest = maximum;
    for (auto slot : m_impl->QuerySlots({point - glm::vec3(maximum), point + glm::vec3(maximum)})) {
        if (!m_impl->MatchesQuery(slot, filter))
            continue;
        const auto &body = m_impl->bodies[slot];
        for (int part = 0; part < PrimitiveCount(body.shape); ++part) {
            auto primitive = PrimitiveAt(body.shape, body.rigidBody, part);
            auto result = PointGeometry(glm::dvec3(point), primitive, nearest);
            if (!result.valid)
                continue;
            auto handle = m_impl->MakeHandle(slot);
            if (!out.hit || result.gap < nearest ||
                (result.gap == nearest &&
                 std::tie(handle.id, part) < std::tie(out.body.id, out.primitiveIndex))) {
                nearest = result.gap;
                out.hit = true;
                out.body = handle;
                out.point = glm::vec3(result.point);
                out.normal = glm::vec3(result.normal);
                out.distance = nearest;
                out.contains = result.contains;
                out.normalUnique = result.normalUnique;
                out.containmentKnown = primitive.shape.type != ShapeType::TriangleMesh;
                out.primitiveIndex = part;
                out.childKey = primitive.childKey;
                out.feature = result.feature;
                out.shape = primitive.shape.type;
            }
        }
    }
    return out;
}

PhysicsCastHit PhysicsWorld::Raycast(const glm::vec3 &o, const glm::vec3 &d, float m,
                                     const PhysicsQueryFilter &f, PhysicsCastStats *s) const {
    return Cast(Shape::Sphere(0), {o, {1, 0, 0, 0}}, d, m, f, s);
}

PhysicsCastHit PhysicsWorld::SphereCast(const glm::vec3 &o, float r, const glm::vec3 &d, float m,
                                        const PhysicsQueryFilter &f, PhysicsCastStats *s) const {
    return Cast(Shape::Sphere(r), {o, {1, 0, 0, 0}}, d, m, f, s);
}

PhysicsCastHit PhysicsWorld::CapsuleCast(const BodyTransform &p, float r, float h,
                                         const glm::vec3 &d, float m, const PhysicsQueryFilter &f,
                                         PhysicsCastStats *s) const {
    return Cast(Shape::Capsule(r, h), p, d, m, f, s);
}

PhysicsCastHit PhysicsWorld::BoxCast(const BodyTransform &p, const glm::vec3 &h, const glm::vec3 &d,
                                     float m, const PhysicsQueryFilter &f,
                                     PhysicsCastStats *s) const {
    return Cast(Shape::Box(h), p, d, m, f, s);
}

BodyHandle PhysicsWorld::CreateQueryCapsule(float radius, float halfHeight,
                                            const BodyTransform &pose) {
    auto h = m_impl->AddBody(Shape::Capsule(radius, halfHeight), pose.position, pose.rotation,
                             false, 0, 0, 0);
    m_impl->Get(h)->queryOnly = true;
    return h;
}
