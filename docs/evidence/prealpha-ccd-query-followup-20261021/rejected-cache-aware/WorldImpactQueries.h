#pragma once

inline auto PhysicsWorld::Impl::PoseAt(unsigned slot, double time) const -> RigidBody {
    const Body &b = bodies[slot];
    RigidBody pose = b.rigidBody;
    if (b.HasMotion() && !b.motion.Segments().empty()) {
        const auto sampled = b.motion.EvaluateAfterEvents(time);
        pose.position = sampled.position;
        pose.orientation = sampled.orientation;
        pose.linearVelocity = sampled.linearVelocity;
        pose.angularVelocity = sampled.angularVelocity;
    }
    return pose;
}

inline auto PhysicsWorld::Impl::GeometryPoseAt(unsigned slot, double time) const -> RigidBody {
    const auto &body = bodies[slot];
    RigidBody pose = body.rigidBody;
    if (body.HasMotion() && !body.motion.Segments().empty()) {
        const auto geometry = body.motion.EvaluateGeometryAfterEvents(time);
        pose.position = geometry.position;
        pose.orientation = geometry.orientation;
        pose.linearVelocity = geometry.linearVelocity;
    }
    return pose;
}

inline auto PhysicsWorld::Impl::ImpactPoseAt(unsigned slot, double time, bool prepareOrientation)
    -> const ImpactPoseSample & {
    EnsureImpactPoseStorage();
    // Independent pair queries share the current event's start pose.
    // Bisection samples must not evict that immutable geometric sample.
    auto &sample =
        time == impactQueryAnchorTime ? impactAnchorSamples[slot] : impactPoseSamples[slot];
    if (sample.time == time && sample.generation == bodies[slot].generation) {
        ++impactPoseReuses;
    } else {
        sample.pose = GeometryPoseAt(slot, time);
        sample.time = time;
        sample.generation = bodies[slot].generation;
        ++impactPoseEvaluations;
    }
    // Exact represented-pose comparisons do not consume collision orientation
    // data. A later geometry caller may hit this pose after a pose-only read;
    // validate preparation even on that cache hit.
    if (prepareOrientation &&
        (!sample.orientation || !sample.orientation->Matches(sample.pose.orientation))) {
        sample.orientation.emplace(sample.pose.orientation);
        ++impactOrientationBuilds;
    }
    return sample;
}

inline auto PhysicsWorld::Impl::ActualImpactBound(unsigned slot, const ImpactPoseSample &pose)
    -> const Aabb & {
    auto &sample = impactPoseSamples[slot];
    if (!sample.tightValid || !SamePosition(sample.tightPosition, pose.pose.position) ||
        !SameRotation(sample.tightOrientation, pose.pose.orientation)) {
        const auto &shape = bodies[slot].shape;
        if (shape.type == ShapeType::Box || shape.type == ShapeType::Sphere) {
            PreparedShapeBounds prepared;
            PrepareShapeBounds(shape, *pose.orientation, prepared);
            sample.tightBound = ShapeAabb(prepared, pose.pose.position);
        } else
            sample.tightBound = ShapeAabb(shape, pose.pose.position, pose.pose.orientation);
        sample.tightPosition = pose.pose.position;
        sample.tightOrientation = pose.pose.orientation;
        sample.tightValid = true;
    }
    return sample.tightBound;
}

inline auto PhysicsWorld::Impl::SimpleImpactBoundsSeparated(unsigned a, unsigned b,
                                                            const ImpactPoseSample &aa,
                                                            const ImpactPoseSample &bb) -> bool {
    const auto simple = [](const Shape &shape) {
        return (shape.type == ShapeType::Box || shape.type == ShapeType::Sphere) &&
               shape.pivotOffset == glm::vec3(0);
    };
    if (!simple(bodies[a].shape) || !simple(bodies[b].shape))
        return false;
    const auto &first = ActualImpactBound(a, aa);
    const auto &second = ActualImpactBound(b, bb);
    if (!rigid_motion_bounds::FiniteBound(first) || !rigid_motion_bounds::FiniteBound(second) ||
        first.Overlaps(second))
        return false;
    ++impactActualBoundsRejected;
    return true;
}

inline auto PhysicsWorld::Impl::PairAt(unsigned a, unsigned b, int pa, int pb, double time,
                                       float margin) -> ContactManifold {
    PhysicsWorkTimer timer(work.geometry, work.active);
    const auto &aa = ImpactPoseAt(a, time);
    const auto &bb = ImpactPoseAt(b, time);
    if (margin == 0 && SimpleImpactBoundsSeparated(a, b, aa, bb))
        return {};
    return ComputeContacts(PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation),
                           PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation), margin,
                           &*aa.orientation, &*bb.orientation);
}

inline auto PhysicsWorld::Impl::PairPenetration(unsigned a, unsigned b, int pa, int pb, double time)
    -> double {
    const auto manifold = PairAt(a, b, pa, pb, time, 0);
    double deepest = 0;
    for (int k = 0; k < manifold.count; ++k) {
        const auto &c = manifold.points[k];
        deepest =
            std::max(deepest, c.hasLocalAnchors ? -c.signedSeparation : double(c.penetration));
    }
    return deepest;
}

inline auto PhysicsWorld::Impl::PairPenetrationExceeds(unsigned a, unsigned b, int pa, int pb,
                                                       double time, double target) -> bool {
    PhysicsWorkTimer timer(work.geometry, work.active);
    const auto &aa = ImpactPoseAt(a, time);
    const auto &bb = ImpactPoseAt(b, time);
    const auto posa = PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation);
    const auto posb = PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation);
    if (posa.shape.asset || posb.shape.asset)
        return CookedHasPenetrationBeyond(posa, posb, target);
    return PairPenetration(a, b, pa, pb, time) > target;
}

inline auto PhysicsWorld::Impl::PairTouches(unsigned a, unsigned b, int pa, int pb, double time)
    -> bool {
    PhysicsWorkTimer timer(work.geometry, work.active);
    const auto &aa = ImpactPoseAt(a, time);
    const auto &bb = ImpactPoseAt(b, time);
    if (SimpleImpactBoundsSeparated(a, b, aa, bb))
        return false;
    const auto posa = PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation);
    const auto posb = PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation);
    if (posa.shape.asset || posb.shape.asset)
        return CookedHasContacts(posa, posb, 0);
    if (posa.childRotation == glm::quat(1, 0, 0, 0) && posb.childRotation == glm::quat(1, 0, 0, 0))
        return PrimitiveHasContacts(
            posa.shape, {posa.parentPosition, posa.parentOrientation, posa.parentLocalCenter},
            posb.shape, {posb.parentPosition, posb.parentOrientation, posb.parentLocalCenter}, 0,
            &*aa.orientation, &*bb.orientation);
    return ComputeContacts(posa, posb, 0, &*aa.orientation, &*bb.orientation).count > 0;
}

inline auto PhysicsWorld::Impl::EnsureImpactPoseStorage() -> void {
    if (impactPoseSamples.size() >= bodies.size())
        return;
    const auto capacity = impactPoseSamples.capacity();
    impactPoseSamples.resize(bodies.size());
    const auto anchorCapacity = impactAnchorSamples.capacity();
    impactAnchorSamples.resize(bodies.size());
    if (impactAnchorSamples.capacity() != anchorCapacity) {
        ++cacheCounters.allocations;
        cacheCounters.allocatedBytes += impactAnchorSamples.capacity() * sizeof(ImpactPoseSample);
    }
    if (impactPoseSamples.capacity() != capacity) {
        ++cacheCounters.allocations;
        cacheCounters.allocatedBytes += impactPoseSamples.capacity() * sizeof(ImpactPoseSample);
    }
}
