#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::RemainingImpactBound(unsigned slot, double start, double finish)
    -> const Aabb & {
    EnsureImpactPoseStorage();
    auto &sample = impactPoseSamples[slot];
    const auto &body = bodies[slot];
    // An unchanged anchored trajectory's earlier enclosure also contains
    // every later subinterval. Velocity changes invalidate this cache.
    // Reusing the superset is conservative; it never supplies contact truth.
    if (sample.boundStart <= start && sample.boundFinish >= finish &&
        sample.boundGeneration == body.generation)
        return sample.remainingBound;
    if (!body.HasMotion())
        sample.remainingBound = CurrentBound(bodies[slot]);
    else {
        // The remaining motion is one anchored segment. Each represented
        // center component stays between its sampled endpoints. An outward
        // enclosing sphere at both endpoints therefore bounds every shape
        // orientation, including a full turn with equal endpoint rotations.
        // Only centers are needed: do not evict the prepared start pose or
        // prepare an unused endpoint orientation for this sphere enclosure.
        const auto fromPose = GeometryPoseAt(slot, start), toPose = GeometryPoseAt(slot, finish);
        const auto from = fromPose.position, to = toPose.position;
        const auto sphere = Shape::Sphere(body.boundingRadius);
        const glm::quat identity(1, 0, 0, 0);
        sample.remainingBound =
            ShapeAabb(sphere, from, identity).Union(ShapeAabb(sphere, to, identity));
        const auto &segments = body.motion.Segments();
        auto segment =
            std::upper_bound(segments.begin(), segments.end(), start,
                             [](double time, const auto &value) { return time < value.begin; });
        if (segment != segments.begin()) {
            --segment;
            auto tighter = rigid_motion_bounds::TryTranslationSweepBound(
                body.shape, *segment, fromPose, toPose, start, finish);
            if (!tighter)
                tighter = rigid_motion_bounds::TryDynamicBoxSweepBound(
                    body.shape, *segment, fromPose, toPose, start, finish);
            if (tighter)
                sample.remainingBound = rigid_motion_bounds::IntersectConservativeBounds(
                    sample.remainingBound, *tighter);
        }
    }
    sample.boundStart = start;
    sample.boundFinish = finish;
    sample.boundGeneration = body.generation;
    return sample.remainingBound;
}

auto PhysicsWorld::Impl::NormalSpeed(const Contact &c, const RigidBody &a, const RigidBody &b)
    -> double {
    const glm::dvec3 ra =
        c.hasLocalAnchors
            ? (c.anchorAInWorldFrame ? c.localAnchorA
                                     : ContactRotation(a.orientation) * c.localAnchorA)
            : glm::dvec3(c.point) - glm::dvec3(a.position);
    const glm::dvec3 rb =
        c.hasLocalAnchors
            ? (c.anchorBInWorldFrame ? c.localAnchorB
                                     : ContactRotation(b.orientation) * c.localAnchorB)
            : glm::dvec3(c.point) - glm::dvec3(b.position);
    return glm::dot(glm::dvec3(c.normal), glm::dvec3(a.linearVelocity - b.linearVelocity) +
                                              glm::cross(glm::dvec3(a.angularVelocity), ra) -
                                              glm::cross(glm::dvec3(b.angularVelocity), rb));
}

auto PhysicsWorld::Impl::DriftGenerator(unsigned slot, double time) const -> glm::vec3 {
    const auto &body = bodies[slot];
    if (!body.HasMotion())
        return glm::vec3(0);
    const auto &segments = body.motion.Segments();
    auto it = std::upper_bound(segments.begin(), segments.end(), time,
                               [](double t, const auto &segment) { return t < segment.begin; });
    return it == segments.begin() ? body.rigidBody.angularVelocity : (--it)->driftAngularVelocity;
}

auto PhysicsWorld::Impl::PlaneDrift(unsigned a, unsigned b, int pa, int pb, double time,
                                    const glm::dvec3 &n) -> double {
    struct Vertex {
        glm::dvec3 r, velocity;
        double curvature;
    };
    struct Support {
        std::array<Vertex, 128> vertices;
        int count = 0;
        double radius = 0;
    };
    auto support = [&](unsigned slot, int part) {
        Support result;
        const auto &body = bodies[slot];
        const auto &sample = ImpactPoseAt(slot, time);
        const auto &pose = sample.pose;
        const auto child = PrimitiveAt(body.shape, pose, part, &*sample.orientation);
        if (child.shape.type != ShapeType::Box && child.shape.type != ShapeType::Sphere &&
            child.shape.type != ShapeType::ConvexHull &&
            child.shape.type != ShapeType::TriangleMesh)
            return result;
        const auto &rotation = sample.orientation->rotation;
        const glm::dvec3 omega(DriftGenerator(slot, time));
        const double w2 = glm::dot(omega, omega);
        const auto &segments = body.motion.Segments();
        auto segment = std::upper_bound(segments.begin(), segments.end(), time,
                                        [](double t, const auto &s) { return t < s.begin; });
        const double elapsed =
            body.isDynamic && segment != segments.begin() ? time - (--segment)->begin : 0;
        const glm::dvec3 instantaneous =
            body.isKinematic ? omega : omega / (1 + .25 * w2 * elapsed * elapsed);
        auto vertex = [&](glm::dvec3 local) {
            const glm::dvec3 r = rotation * local;
            const glm::dvec3 perpendicular =
                w2 > 0 ? r - omega * (glm::dot(omega, r) / w2) : glm::dvec3(0);
            result.vertices[result.count++] = {
                r,
                glm::dvec3(body.HasMotion() ? pose.linearVelocity : glm::vec3(0)) +
                    glm::cross(instantaneous, r),
                w2 * glm::length(perpendicular)};
        };
        if (child.shape.type == ShapeType::Sphere) {
            vertex(glm::dvec3(child.parentLocalCenter));
            result.radius = child.shape.radius;
        } else if (child.shape.type == ShapeType::ConvexHull) {
            const auto r = ContactRotation(child.childRotation);
            for (auto p : child.shape.asset->vertices)
                vertex(glm::dvec3(child.parentLocalCenter) + r * p);
        } else if (child.shape.type == ShapeType::TriangleMesh) {
            // The asset-space AABB encloses every mesh vertex. Its eight
            // transformed corners therefore certify a separating plane for
            // the entire mesh, including faces outside the current query.
            // This is only an advancement bound, never contact geometry.
            // If this larger enclosure crosses the plane we return zero
            // below and retain the original conservative/sampling search.
            const auto r = ContactRotation(child.childRotation);
            const auto lo = child.shape.asset->minimum, hi = child.shape.asset->maximum;
            for (int k = 0; k < 8; ++k)
                vertex(glm::dvec3(child.parentLocalCenter) + r * glm::dvec3(k & 1 ? hi.x : lo.x,
                                                                            k & 2 ? hi.y : lo.y,
                                                                            k & 4 ? hi.z : lo.z));
        } else {
            const auto childRotation = ContactRotation(child.childRotation);
            for (int k = 0; k < 8; ++k)
                vertex(glm::dvec3(child.parentLocalCenter) +
                       childRotation * glm::dvec3(child.shape.halfExtents) *
                           glm::dvec3(k & 1 ? 1 : -1, k & 2 ? 1 : -1, k & 4 ? 1 : -1));
        }
        return result;
    };
    const auto sa = support(a, pa), sb = support(b, pb);
    if (!sa.count || !sb.count)
        return 0;
    const glm::dvec3 delta = glm::dvec3(ImpactPoseAt(a, time).pose.position) -
                             glm::dvec3(ImpactPoseAt(b, time).pose.position);
    double advance = std::numeric_limits<double>::infinity();
    for (int i = 0; i < sa.count; ++i)
        for (int j = 0; j < sb.count; ++j) {
            const auto &va = sa.vertices[i];
            const auto &vb = sb.vertices[j];
            const double gap = glm::dot(n, delta + va.r - vb.r) - sa.radius - sb.radius;
            if (!(gap > 0))
                return 0; // This sampled normal cannot certify a separating plane.
            const double rate = glm::dot(n, va.velocity - vb.velocity),
                         curvature = va.curvature + vb.curvature;
            double root = std::numeric_limits<double>::infinity();
            if (curvature > 0) {
                const double discriminant = std::sqrt(rate * rate + 2 * curvature * gap);
                root =
                    rate < 0 ? 2 * gap / (discriminant - rate) : (rate + discriminant) / curvature;
            } else if (rate < 0)
                root = -gap / rate;
            advance = std::min(advance, root);
        }
    return advance;
}

auto PhysicsWorld::Impl::StaticMeshPolyPair(unsigned a, unsigned b, int pa, int pb) const -> bool {
    const auto type = [](const Shape& shape, int part) {
        return shape.type == ShapeType::CompoundBoxes ? shape.boxes[std::size_t(part)].type
                                                     : shape.type;
    };
    const auto poly = [](ShapeType value) {
        return value == ShapeType::Box || value == ShapeType::ConvexHull;
    };
    const auto first = type(bodies[a].shape, pa), second = type(bodies[b].shape, pb);
    return (first == ShapeType::TriangleMesh && !bodies[a].HasMotion() && poly(second)) ||
           (second == ShapeType::TriangleMesh && !bodies[b].HasMotion() && poly(first));
}

auto PhysicsWorld::Impl::MeshDepthGrowthSafe(unsigned moving, unsigned meshSlot, int movingPart,
                                             int meshPart, double start, double finish,
                                             const MeshFeatureBudget &budget) -> bool {
    // This is only a sufficient depth-growth rejection. It does not replace
    // contact geometry, create contacts, change limits or sample the interval.
    // Unsupported shapes and uncertain arithmetic retain the original search.
    const auto &body = bodies[moving];
    if (body.shape.type != ShapeType::Box || movingPart != 0 ||
        body.shape.pivotOffset != glm::vec3(0) ||
        !rigid_motion_bounds::Finite3(body.shape.halfExtents) ||
        glm::any(glm::lessThanEqual(body.shape.halfExtents, glm::vec3(0))))
        return false;
    const auto first = ImpactPoseAt(moving, start);
    const auto last = ImpactPoseAt(moving, finish);
    const auto mesh = PrimitiveAt(bodies[meshSlot].shape, ImpactPoseAt(meshSlot, start).pose,
                                  meshPart);
    if (mesh.shape.type != ShapeType::TriangleMesh || !mesh.shape.asset ||
        bodies[meshSlot].HasMotion())
        return false;
    const RigidMotionSegment *segment = nullptr;
    if (body.HasMotion()) {
        const auto &segments = body.motion.Segments();
        auto found = std::upper_bound(segments.begin(), segments.end(), start,
                                      [](double time, const auto &value) {
                                          return time < value.begin;
                                      });
        if (found == segments.begin())
            return false;
        segment = &*--found;
        if (finish > segment->end)
            return false;
    }
    using contact_geometry::Iv;
    try {
        auto rotation = first.orientation->normalizedInterval;
        if (segment) {
            if (segment->prescribed) {
                if (segment->driftAngularVelocity != glm::vec3(0) ||
                    !rigid_motion_bounds::SameQuat(first.pose.orientation, last.pose.orientation))
                    return false;
            } else {
                const auto swept = rigid_motion_bounds::TryDynamicRotationInterval(*segment,
                                                                                   start, finish);
                if (!swept)
                    return false;
                rotation = *swept;
            }
        }
        const auto meshCenter = PrimitiveCenter(mesh);
        const auto meshRotation = PrimitiveRotation(mesh);
        const auto inverse = glm::transpose(meshRotation);
        const auto &sweep = RemainingImpactBound(moving, start, finish);
        if (!rigid_motion_bounds::FiniteBound(sweep))
            return false;
        std::array<Iv, 3> center;
        double queryMagnitude = 1;
        for (unsigned k = 0; k < 3; ++k) {
            center[k] = Iv(double(std::min(first.pose.position[k], last.pose.position[k])),
                           double(std::max(first.pose.position[k], last.pose.position[k]))) -
                        Iv(meshCenter[k]);
            queryMagnitude = std::max({queryMagnitude,
                                       std::abs(double(sweep.min[k]) - meshCenter[k]),
                                       std::abs(double(sweep.max[k]) - meshCenter[k])});
        }
        // The existing outward body enclosure bounds every represented point.
        // Transform its corners outward into the same asset-local query frame
        // used by the native BVH. One extra float ULP also encloses the native
        // visitor's subtract-origin/add-origin round trip at a bound face.
        glm::dvec3 localMin(INFINITY), localMax(-INFINITY);
        for (unsigned corner = 0; corner < 8; ++corner) {
            std::array<Iv, 3> world;
            for (unsigned k = 0; k < 3; ++k) {
                const double value = corner & (1u << k) ? sweep.max[k] : sweep.min[k];
                const double padding = rigid_motion_bounds::FloatRoundingBound(std::abs(value));
                if (!std::isfinite(padding))
                    return false;
                world[k] = Iv(value) + Iv(-padding, padding) - Iv(meshCenter[k]);
            }
            for (unsigned j = 0; j < 3; ++j) {
                Iv projected(0);
                for (unsigned k = 0; k < 3; ++k)
                    projected = projected + Iv(inverse[k][j]) * world[k];
                localMin[j] = std::min(localMin[j], projected.lo);
                localMax[j] = std::max(localMax[j], projected.hi);
            }
        }
        const auto &asset = *mesh.shape.asset;
        std::vector<uint32_t> candidates;
        uint64_t nodes = 0;
        asset.Candidates(localMin, localMax, candidates, &nodes);
        CountCollisionQuery(nodes, candidates.size());
        for (const auto feature : candidates) {
            const auto &face = asset.faces[feature];
            const auto normal = meshRotation * face.normal;
            Iv norm2(0), projectedCenter(0), extent(0);
            for (unsigned k = 0; k < 3; ++k) {
                norm2 = norm2 + Iv(normal[k]) * Iv(normal[k]);
                projectedCenter = projectedCenter + center[k] * Iv(normal[k]);
            }
            const auto norm = contact_geometry::Sqrt(norm2);
            if (!(norm.lo >= .5) || !(norm.hi <= 2))
                return false;
            for (unsigned j = 0; j < 3; ++j) {
                Iv projection(0);
                for (unsigned k = 0; k < 3; ++k)
                    projection = projection + rotation[j][k] * Iv(normal[k]);
                extent = extent + contact_geometry::Abs(projection) *
                                      Iv(double(body.shape.halfExtents[j]));
            }
            // Bound ALL represented triangle planes. Native SAT takes the
            // furthest support point; clipped witnesses use a winding-selected
            // vertex. Taking their hull also avoids assuming exact coplanarity
            // after a represented rotation or for externally supplied assets.
            Iv plane(INFINITY, -INFINITY);
            double magnitude = queryMagnitude;
            for (const auto vertex : face.vertices) {
                const auto point = meshRotation * asset.vertices[vertex];
                Iv projection(0);
                for (unsigned k = 0; k < 3; ++k) {
                    projection = projection + Iv(normal[k]) * Iv(point[k]);
                    magnitude = std::max(magnitude, std::abs(point[k]));
                }
                plane.lo = std::min(plane.lo, projection.lo);
                plane.hi = std::max(plane.hi, projection.hi);
            }
            const auto relative = projectedCenter - plane;
            const auto minimum = relative - extent, maximum = relative + extent;
            const auto found = std::lower_bound(budget.limits.begin(), budget.limits.end(), feature,
                                                [](const auto &entry, uint32_t id) {
                                                    return entry.first < id;
                                                });
            const double limit = found != budget.limits.end() && found->first == feature
                                     ? found->second
                                     : budget.newFeatureLimit;
            if (!std::isfinite(limit) || !(limit >= 0))
                return false;
            // SAT includes the normalized triangle plane. Clipped witnesses
            // multiply the unnormalized plane distance by dot(response,n).
            // Enclose both normal-length factors rather than assuming exact
            // unit normals. The epsilon factor covers axis normalization.
            constexpr double e = std::numeric_limits<double>::epsilon();
            const auto factor = Iv(std::max(contact_geometry::Up(1 / norm.lo), norm.hi)) *
                                Iv(1 + 32 * e);
            // At most three triangle clipping stages, each consisting of
            // bounded convex interpolation, precede two dot products and
            // projection/subtraction. 8*gamma(128)*M overbounds their accumulated
            // binary64 error for normal lengths in [1/2,2]. This padding tightens
            // a rejection only; it is never a contact tolerance or depth repair.
            constexpr double gamma128 = 128 * e / (1 - 128 * e);
            const auto padding = Iv(8 * gamma128) * Iv(magnitude);
            auto fits = [&](double rawDepth) {
                const auto worst = Iv(std::max(0., rawDepth)) * factor + padding;
                return std::isfinite(worst.hi) && worst.hi < limit;
            };
            if (!fits(-minimum.lo))
                return false;
            // Native two-sided selection (and its tiny one-sided eps strip)
            // can reverse the front. Prove BOTH actual orientations whenever
            // the interval cannot establish a strictly front-side mean.
            if ((asset.twoSided || relative.lo <= padding.hi) && !fits(maximum.hi))
                return false;
        }
        return true;
    } catch (const contact_geometry::ArithmeticFailure &) {
        return false;
    }
}

auto PhysicsWorld::Impl::PrimitiveFeature(unsigned slot, int part) const -> double {
    const Shape &shape = bodies[slot].shape;
    auto primitive = PrimitiveAt(shape, bodies[slot].rigidBody, part);
    if (primitive.shape.type == ShapeType::Sphere)
        return double(primitive.shape.radius);
    if (primitive.shape.type == ShapeType::ConvexHull) {
        auto extent = primitive.shape.asset->maximum - primitive.shape.asset->minimum;
        return std::min({extent.x, extent.y, extent.z}) * .5;
    }
    const glm::vec3 half = shape.type == ShapeType::CompoundBoxes
                               ? shape.boxes[std::size_t(part)].halfExtents
                               : shape.halfExtents;
    if (shape.type == ShapeType::Box || shape.type == ShapeType::CompoundBoxes)
        return double(std::min({half.x, half.y, half.z}));
    // Terrain uses the other, finite rigid primitive's feature size.
    return std::numeric_limits<double>::infinity();
}

auto PhysicsWorld::Impl::SpinsThroughSupport(const std::array<unsigned, 4> &key, double remaining,
                                             double time, bool changedVelocity) -> bool {
    const auto convex = [](ShapeType type) {
        return type == ShapeType::Box || type == ShapeType::Sphere || type == ShapeType::ConvexHull;
    };
    const auto typeA =
        PrimitiveAt(bodies[key[0]].shape, bodies[key[0]].rigidBody, int(key[2])).shape.type;
    const auto typeB =
        PrimitiveAt(bodies[key[1]].shape, bodies[key[1]].rigidBody, int(key[3])).shape.type;
    // A mesh/terrain contact normal is only a LOCAL triangle plane, not a
    // separating plane for the whole concave primitive. Keep its old rule.
    if (!convex(typeA) || !convex(typeB)) {
        double oldSweep = 0;
        for (int side = 0; side < 2; ++side) {
            const auto &body = bodies[key[side]];
            if (body.HasMotion() && (side ? typeB : typeA) != ShapeType::Sphere)
                oldSweep += double(glm::length(body.rigidBody.angularVelocity)) *
                            body.boundingRadius * remaining;
        }
        const double feature =
            std::min(PrimitiveFeature(key[0], int(key[2])), PrimitiveFeature(key[1], int(key[3])));
        return std::isfinite(feature) && oldSweep > feature;
    }
    std::array<glm::dvec3, 2> omega{};
    double sweep = 0;
    for (int side = 0; side < 2; ++side) {
        const unsigned slot = key[std::size_t(side)];
        const auto &body = bodies[slot];
        if (!body.HasMotion() ||
            PrimitiveAt(body.shape, body.rigidBody, int(key[std::size_t(2 + side)])).shape.type ==
                ShapeType::Sphere)
            continue;
        // Use the same anchored geometric generator as the trajectory.
        // Immediately after an impulse the replacement segment has not
        // been published yet; predict exactly what Append will retain.
        omega[side] = glm::dvec3(
            changedVelocity
                ? body.isKinematic
                      ? body.rigidBody.angularVelocity
                      : AngularDriftGenerator(body.rigidBody.inverseInertiaLocal,
                                              body.rigidBody.orientation,
                                              body.rigidBody.angularVelocity, float(remaining))
                : DriftGenerator(slot, time));
        sweep +=
            double(glm::length(body.rigidBody.angularVelocity)) * body.boundingRadius * remaining;
    }
    const double feature =
        std::min(PrimitiveFeature(key[0], int(key[2])), PrimitiveFeature(key[1], int(key[3])));
    if (!std::isfinite(feature) || sweep <= feature)
        return false;
    // A fixed support plane also bounds penetration after rotation. For
    // anchored drift about omega, projected vertex speed is bounded by
    // |n x omega|*R. In particular, rotation ABOUT n cannot penetrate that
    // plane. The old all-direction bound sent flat yaw-spinning bodies
    // through thousands of samples despite invariant support geometry.
    // Proximity supplies a separating plane only; no impulse is introduced.
    const auto manifold = PairAt(key[0], key[1], int(key[2]), int(key[3]), time, float(feature));
    if (!manifold.count)
        return true; // retain the conservative old decision
    const auto &contact = manifold.points[0];
    auto normal = contact.hasLocalAnchors ? contact.preciseNormal : glm::dvec3(contact.normal);
    const double magnitude = glm::length(normal);
    if (!(magnitude > 0) || !std::isfinite(magnitude))
        return true;
    normal /= magnitude;
    double projected = 0;
    for (int side = 0; side < 2; ++side)
        projected += glm::length(glm::cross(normal, omega[side])) *
                     bodies[key[side]].boundingRadius * remaining;
    return projected > feature;
}

auto PhysicsWorld::Impl::SupportContinuationSafe(const std::array<unsigned, 4> &key, double start,
                                                 double finish) -> bool {
    PhysicsWorkTimer timer(work.continuationProof, work.active);
    ++work.continuationAttempts;
    const auto &a = bodies[key[0]];
    const auto &b = bodies[key[1]];
    if (!ContinuationPrimitive(a) || !ContinuationPrimitive(b))
        return false;
    const auto &aa = ImpactPoseAt(key[0], start);
    const auto &bb = ImpactPoseAt(key[1], start);
    const float clearance = std::min(
        ContactSolver::kPenetrationSlop,
        float(.25 * std::min(PrimitiveFeature(key[0], key[2]), PrimitiveFeature(key[1], key[3]))));
    const auto m = PairAt(key[0], key[1], key[2], key[3], start, clearance);
    if (!m.count || m.uncertain)
        return false;
    const glm::dvec3 normal = m.points[0].preciseNormal;
    // Preserve the original fallback for penetration already beyond the
    // ordinary position solver's allowance; do not enlarge that allowance.
    for (int i = 0; i < m.count; ++i)
        if (m.points[i].signedSeparation < -clearance) {
            ++work.continuationOverBudget;
            return false;
        }
    std::array<const RigidMotionSegment *, 2> segments{};
    for (unsigned side = 0; side < 2; ++side) {
        const auto &body = bodies[key[side]];
        if (!body.HasMotion())
            continue;
        const auto &path = body.motion.Segments();
        auto it = std::upper_bound(path.begin(), path.end(), start,
                                   [](double t, const auto &s) { return t < s.begin; });
        if (it == path.begin())
            return false;
        segments[side] = &*--it;
        if (finish > segments[side]->end)
            return false;
    }
    using contact_geometry::Iv;
    Iv norm(0);
    for (int k = 0; k < 3; ++k)
        norm = norm + Iv(normal[k]) * Iv(normal[k]);
    const Iv allowance = Iv(double(clearance)) * contact_geometry::Sqrt(norm);
    auto gap = [&](double lo, double hi) -> std::optional<Iv> {
        auto extent = [&](unsigned side, const ImpactPoseSample &sample) {
            auto pose = sample.pose;
            pose.position = glm::vec3(0);
            return rigid_motion_bounds::TryPrimitiveSweepProjection(
                bodies[key[side]].shape, segments[side], pose, pose, lo, hi, normal, glm::dvec3(0),
                *sample.orientation);
        };
        const auto first = extent(0, aa), second = extent(1, bb);
        const auto relative = rigid_motion_bounds::TryRelativeCentreProjection(
            segments[0], segments[1], aa.pose, bb.pose, lo, hi, normal);
        if (!first || !second || !relative)
            return std::nullopt;
        return *relative + Iv(first->lo) - Iv(second->hi) + allowance;
    };
    const auto entire = gap(start, finish);
    if (!entire)
        return false;
    if (entire->lo > 0) {
        ++work.continuationAccepted;
        return true;
    }
    const auto end = gap(finish, finish);
    if (!end || end->hi <= 0)
        return false;
    // Subdivision tightens a conservative rotation enclosure; it is not
    // temporal collision sampling. Every accepted leaf covers ALL times.
    auto certify = [&](auto &&self, double lo, double hi, unsigned depth) -> bool {
        ++work.continuationPartitions;
        const auto bound = gap(lo, hi);
        if (!bound)
            return false;
        if (bound->lo > 0)
            return true;
        // This encloses a lower-clearance proxy using the whole interval's
        // worst rotational extents. Its upper endpoint does not bound actual
        // clearance: smaller intervals can tighten those extents even when
        // the entire current proxy is nonpositive. Exhaustion ends the proof.
        if (!depth)
            return false;
        const double mid = (lo + hi) * .5;
        return self(self, lo, mid, depth - 1) && self(self, mid, hi, depth - 1);
    };
    if (certify(certify, start, finish, 3)) {
        ++work.continuationAccepted;
        return true;
    }
    return false;
}

auto PhysicsWorld::Impl::SpinPenetrationTarget(const std::array<unsigned, 4> &key) const -> double {
    return .25 *
           std::min(PrimitiveFeature(key[0], int(key[2])), PrimitiveFeature(key[1], int(key[3])));
}

auto PhysicsWorld::Impl::ImpactTime(unsigned a, unsigned b, int pa, int pb, double start,
                                    double finish, double target) -> double {
    PhysicsWorkTimer timer(work.toi, work.active);
    // A target can finish inside the step. Never certify a plane across
    // its velocity discontinuity; search the two real ledger intervals.
    for (auto slot : {a, b})
        if (bodies[slot].isKinematic)
            for (const auto &s : bodies[slot].motion.Segments())
                if (s.end > start && s.end < finish) {
                    auto first = ImpactTime(a, b, pa, pb, start, s.end, target);
                    return first <= s.end ? first : ImpactTime(a, b, pa, pb, s.end, finish, target);
                }
    ++stats.impactQueries;
    const auto &ba = bodies[a];
    const auto &bb = bodies[b];
    const auto simpleRadius = [](const Body &body) {
        return (body.shape.type == ShapeType::Box || body.shape.type == ShapeType::Sphere) &&
               body.shape.pivotOffset == glm::vec3(0) && std::isfinite(body.boundingRadius) &&
               body.boundingRadius > 0;
    };
    // Restrict this rejection to the existing outward-proven primitive
    // radius/bounds. Compound, cooked and terrain shapes retain the full
    // old query until their radius enclosure is independently established.
    if (simpleRadius(ba) && simpleRadius(bb)) {
        const auto &boundA = RemainingImpactBound(a, start, finish);
        const auto &boundB = RemainingImpactBound(b, start, finish);
        const auto finite = [](const Aabb &bound) {
            for (int k = 0; k < 3; ++k)
                if (!std::isfinite(bound.min[k]) || !std::isfinite(bound.max[k]))
                    return false;
            return true;
        };
        // An arithmetic-range failure is not a geometric rejection. Keep
        // the established query path whenever the enclosure is uncertain.
        if (finite(boundA) && finite(boundB) && !boundA.Overlaps(boundB)) {
            ++impactBoundsRejected;
            return finish + 1;
        }
    }
    const auto posa = ImpactPoseAt(a, start).pose, posb = ImpactPoseAt(b, start).pose;
    const auto firstPrimitive = PrimitiveAt(ba.shape, posa, pa),
               secondPrimitive = PrimitiveAt(bb.shape, posb, pb);
    const bool firstMesh = firstPrimitive.shape.type == ShapeType::TriangleMesh && !ba.HasMotion();
    const bool secondMesh =
        secondPrimitive.shape.type == ShapeType::TriangleMesh && !bb.HasMotion();
    const auto convexPoly = [](ShapeType type) {
        return type == ShapeType::Box || type == ShapeType::ConvexHull;
    };
    const bool meshAdvance = (firstMesh && convexPoly(secondPrimitive.shape.type)) ||
                             (secondMesh && convexPoly(firstPrimitive.shape.type));
    const glm::dvec3 relative = glm::dvec3(ba.HasMotion() ? posa.linearVelocity : glm::vec3(0)) -
                                glm::dvec3(bb.HasMotion() ? posb.linearVelocity : glm::vec3(0));
    const double angular =
        (ba.HasMotion() ? glm::length(glm::dvec3(DriftGenerator(a, start))) * ba.boundingRadius
                        : 0) +
        (bb.HasMotion() ? glm::length(glm::dvec3(DriftGenerator(b, start))) * bb.boundingRadius
                        : 0);
    const double speed = glm::length(relative) + angular;
    if (!(speed > 0))
        return finish + 1;
    MeshFeatureBudget meshBudget;
    if (meshAdvance) {
        meshBudget = BuildMeshFeatureBudget(firstMesh ? secondPrimitive : firstPrimitive,
                                           firstMesh ? firstPrimitive : secondPrimitive,
                                           SpinPenetrationTarget({a, b, unsigned(pa), unsigned(pb)}));
    }
    const bool meshContinuation = meshAdvance && meshBudget.HasContacts();
    if (meshContinuation &&
        MeshDepthGrowthSafe(firstMesh ? b : a, firstMesh ? a : b, firstMesh ? pb : pa,
                            firstMesh ? pa : pb, start, finish, meshBudget))
        return finish + 1;
    // Native mesh start depths were already collected in the one traversal
    // above. With no accepted start contact their global penetration is zero.
    if (target > 0 && !meshAdvance)
        target += PairPenetration(a, b, pa, pb, start);
    // Bisection still performs all 32 represented-time bracket updates.
    // Binary32 pose integration can map several of those times to exactly
    // the same pair of poses. Contact truth for that identical geometry is
    // reusable; no margin contact, tolerance or earlier stopping is used.
    struct ActualTruth {
        std::array<float, 14> pose{};
        bool valid = false, touching = false;
    };
    std::array<ActualTruth, 3> actualTruth{};
    std::size_t nextTruth = 0;
    auto touchingAt = [&](double at) {
        ++impactTruthRequests;
        const auto &aa = ImpactPoseAt(a, at).pose;
        const auto &bb = ImpactPoseAt(b, at).pose;
        const std::array<float, 14> represented = {
            aa.position.x,    aa.position.y,    aa.position.z,    aa.orientation.w,
            aa.orientation.x, aa.orientation.y, aa.orientation.z, bb.position.x,
            bb.position.y,    bb.position.z,    bb.orientation.w, bb.orientation.x,
            bb.orientation.y, bb.orientation.z};
        for (const auto &old : actualTruth)
            if (old.valid &&
                std::memcmp(old.pose.data(), represented.data(), sizeof(represented)) == 0) {
                ++impactTruthReuses;
                return old.touching;
            }
        bool touching;
        if (meshAdvance && at == start) {
            // A budget cannot be exceeded at its own actual-depth anchor;
            // absent native start contacts also give a proven false query.
            touching = false;
        } else if (meshContinuation) {
            const auto first = PrimitiveAt(bodies[a].shape, aa, pa),
                       second = PrimitiveAt(bodies[b].shape, bb, pb);
            touching = MeshFeatureBudgetExceeded(firstMesh ? second : first,
                                                 firstMesh ? first : second, meshBudget);
        } else {
            touching = target > 0 ? PairPenetrationExceeds(a, b, pa, pb, at, target)
                                 : PairTouches(a, b, pa, pb, at);
        }
        actualTruth[nextTruth] = {represented, true, touching};
        nextTruth = (nextTruth + 1) % actualTruth.size();
        return touching;
    };
    double time = start, previous = start;
    bool uncertifiedAdvance = false, stalled = false;
    // A grazing feature on a concave surface (a limb sliding in a curved
    // mesh ramp) keeps the gap tiny while the surface keeps closing ahead,
    // so each advance covers a sliver of the interval. After several such
    // consecutive advances, stop paying full proximity queries and finish
    // with the bounded sampling below, as the 64-iteration limit already does.
    int stalledAdvances = 0;
    // Conservative advancement needs a separating plane. A pair searched
    // to a penetration target is already in contact, where no single
    // contact normal bounds the approach of every feature; go straight to
    // the bounded sampling below.
    const int advanceIterations = target > 0 || meshContinuation ? 0 : 64;
    for (int iteration = 0; iteration < advanceIterations; ++iteration) {
        ++stats.impactSearchIterations;
        stats.impactPeakIterations =
            std::max(stats.impactPeakIterations, std::size_t(iteration + 1));
        const float reach = std::nextafter(static_cast<float>(speed * (finish - time)),
                                           std::numeric_limits<float>::infinity());
        // Margin manifolds contain proximity features, not necessarily the
        // zero-margin contact set. Contact truth must use the latter.
        if (touchingAt(time)) {
            // A pair touching at the start is left to the contact solver,
            // unless its spin can turn another feature through the support
            // before the interval ends: then search to a penetration target.
            if (time == start)
                return target == 0 &&
                               (meshAdvance || SpinsThroughSupport({a, b, unsigned(pa), unsigned(pb)},
                                                                  finish - start, start))
                           ? ImpactTime(a, b, pa, pb, start, finish,
                                        SpinPenetrationTarget({a, b, unsigned(pa), unsigned(pb)}))
                           : finish + 1;
            double lo = previous, hi = time;
            for (int k = 0; k < 32; ++k) {
                const double mid = (lo + hi) * .5;
                if (touchingAt(mid))
                    hi = mid;
                else
                    lo = mid;
            }
            return hi;
        }
        // The exact endpoint has already been checked. There is no future
        // interval left from which a proximity reach could reveal a hit.
        if (time == finish)
            return finish + 1;
        double increment;
        if (meshAdvance) {
            PhysicsWorkTimer geometryTimer(work.geometry, work.active);
            const auto &aa = ImpactPoseAt(a, time);
            const auto &bb = ImpactPoseAt(b, time);
            const auto query = firstMesh
                                   ? PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation)
                                   : PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation);
            const auto mesh = firstMesh
                                  ? PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation)
                                  : PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation);
            const unsigned moving = firstMesh ? b : a;
            increment = MeshSeparationAdvance(
                query, mesh,
                glm::dvec3(firstMesh ? bb.pose.linearVelocity : aa.pose.linearVelocity),
                glm::dvec3(DriftGenerator(moving, time)), bodies[moving].boundingRadius,
                finish - time);
            if (!std::isfinite(increment))
                return finish + 1;
            if (!(increment > 0)) {
                ++stats.impactUncertifiedAdvances;
                uncertifiedAdvance = true;
                break;
            }
        } else {
            const auto manifold = PairAt(a, b, pa, pb, time, reach);
            if (!manifold.count)
                return finish + 1;
            const Contact *nearest = &manifold.points[0];
            for (int k = 1; k < manifold.count; ++k)
                if (manifold.points[k].signedSeparation < nearest->signedSeparation)
                    nearest = &manifold.points[k];
            const double gap = nearest->hasLocalAnchors ? nearest->signedSeparation
                                                        : -double(nearest->penetration);
            if (gap <= 0) {
                ++stats.impactUncertifiedAdvances;
                uncertifiedAdvance = true;
                break;
            }
            const auto normal =
                nearest->hasLocalAnchors ? nearest->preciseNormal : glm::dvec3(nearest->normal);
            const double closing = -glm::dot(relative, normal) + angular;
            if (!(closing > 0))
                return finish + 1;
            increment = gap / closing;
            if (angular > 0)
                increment = std::max(increment, PlaneDrift(a, b, pa, pb, time, normal));
        }
        // Progress in represented time only. A large coordinate orthogonal
        // to motion must not enlarge the step and skip a thin obstacle.
        increment = std::max(increment, std::nextafter(time, finish) - time);
        previous = time;
        time = std::min(finish, time + increment);
        if (time <= previous)
            return finish + 1;
        stalledAdvances =
            time - previous < kImpactStallFraction * (finish - previous) ? stalledAdvances + 1 : 0;
        if (stalledAdvances >= kImpactStallAdvances) {
            stalled = true;
            break;
        }
    }
    if (stalled)
        ++stats.impactSearchStalls;
    else if (!uncertifiedAdvance && target == 0 && !meshContinuation)
        ++stats.impactSearchLimit;
    ++stats.impactSamplingFallbacks;
    // Rotating grazing features can keep a fixed separating-plane bound
    // arbitrarily short. Finish this PAIR query with a bounded temporal
    // sampling approximation, never by applying an impulse across a gap.
    // Intersections shorter than the reported sampling interval can be
    // missed; this is deliberately not a complete continuous-CCD proof.
    auto firstContact = [&](double lo, double hi) {
        for (int k = 0; k < 32; ++k) {
            const double mid = (lo + hi) * .5;
            if (touchingAt(mid))
                hi = mid;
            else
                lo = mid;
        }
        return hi;
    };
    ++stats.impactSamplingTests;
    if (touchingAt(time))
        return firstContact(previous, time);
    const double remaining = finish - time;
    if (!(remaining > 0))
        return finish + 1;
    const double feature = std::min(PrimitiveFeature(a, pa), PrimitiveFeature(b, pb));
    if (!(feature > 0))
        throw std::runtime_error("invalid primitive feature in impact sampling");
    const double angularSpeed =
        (ba.HasMotion() ? glm::length(glm::dvec3(DriftGenerator(a, start))) : 0) +
        (bb.HasMotion() ? glm::length(glm::dvec3(DriftGenerator(b, start))) : 0);
    const double requested = std::max({16.0, std::ceil(16.0 * speed * remaining / feature),
                                       std::ceil(256.0 * angularSpeed * remaining)});
    constexpr unsigned kMaxSamples = 65536;
    if (requested > double(kMaxSamples))
        ++stats.impactSamplingResolutionCaps;
    const unsigned samples = static_cast<unsigned>(std::min(double(kMaxSamples), requested));
    const double interval = remaining / double(samples);
    stats.impactSamplingMaxInterval = std::max(stats.impactSamplingMaxInterval, interval);
    double lo = time;
    for (unsigned sample = 1; sample <= samples; ++sample) {
        const double hi =
            sample == samples ? finish : time + remaining * (double(sample) / double(samples));
        ++stats.impactSamplingTests;
        if (touchingAt(hi))
            return firstContact(lo, hi);
        lo = hi;
    }
    return finish + 1;
}
