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

inline auto PhysicsWorld::Impl::CCDQuery::ImpactPoseAt(unsigned slot, double time)
    -> const ImpactPoseSample & {
    // A parallel batch borrows a frozen world until every query lane joins.
    // Reuse an owner-prepared anchor by const reference; trial/bisection poses
    // and all tight-bound/contact observations remain private to this lane.
    if (!serialWorld && time == impactQueryAnchorTime && slot < world.impactAnchorSamples.size()) {
        const auto& anchor = world.impactAnchorSamples[slot];
        if (anchor.time == time && anchor.generation == bodies[slot].generation &&
            anchor.orientation && anchor.orientation->Matches(anchor.pose.orientation)) {
            ++impactPoseReuses;
            ++ownerAnchorBorrows;
            return anchor;
        }
    }
    EnsureImpactPoseStorage();
    // Independent pair queries share the current event's start pose.
    // Bisection samples must not evict that immutable geometric sample.
    auto &sample =
        time == impactQueryAnchorTime ? impactAnchorSamples[slot] : impactPoseSamples[slot];
    if (sample.time == time && sample.generation == bodies[slot].generation) {
        ++impactPoseReuses;
        return sample;
    }
    sample.pose = GeometryPoseAt(slot, time);
    sample.time = time;
    sample.generation = bodies[slot].generation;
    ++impactPoseEvaluations;
    if (!sample.orientation || !sample.orientation->Matches(sample.pose.orientation)) {
        sample.orientation.emplace(sample.pose.orientation);
        ++impactOrientationBuilds;
    }
    return sample;
}

inline auto PhysicsWorld::Impl::CCDQuery::ActualImpactBound(unsigned slot, const ImpactPoseSample &pose)
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

inline auto PhysicsWorld::Impl::CCDQuery::SimpleImpactBoundsSeparated(unsigned a, unsigned b,
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

inline auto PhysicsWorld::Impl::CCDQuery::PairAt(unsigned a, unsigned b, int pa, int pb, double time,
                                       float margin) -> ContactManifold {
    PhysicsWorkTimer timer(work.geometry, work.active);
    const auto &aa = ImpactPoseAt(a, time);
    const auto &bb = ImpactPoseAt(b, time);
    if (margin == 0 && SimpleImpactBoundsSeparated(a, b, aa, bb))
        return {};
    // Single boxes/spheres already use identity-child primitive arithmetic.
    // Borrow their immutable shapes instead of constructing two owning Shape
    // and RigidBody copies for every trial time in a continuous query.
    const auto simple = [](const Shape& shape) {
        return !shape.asset && (shape.type == ShapeType::Box || shape.type == ShapeType::Sphere);
    };
    if (simple(bodies[a].shape) && simple(bodies[b].shape))
        return PrimitiveContacts(
            bodies[a].shape, {aa.pose.position, aa.pose.orientation, -bodies[a].shape.pivotOffset},
            bodies[b].shape, {bb.pose.position, bb.pose.orientation, -bodies[b].shape.pivotOffset},
            margin, &*aa.orientation, &*bb.orientation, true);
    // An MK2 step's continuous pass judges the same manifolds as its rows.
    return ComputeContacts(PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation),
                           PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation), margin,
                           &*aa.orientation, &*bb.orientation, true);
}

inline auto PhysicsWorld::Impl::CCDQuery::PairPenetration(unsigned a, unsigned b, int pa, int pb, double time)
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

inline auto PhysicsWorld::Impl::CCDQuery::PairPenetrationExceeds(unsigned a, unsigned b, int pa, int pb,
                                                       double time, double target) -> bool {
    PhysicsWorkTimer timer(work.geometry, work.active);
    const auto &aa = ImpactPoseAt(a, time);
    const auto &bb = ImpactPoseAt(b, time);
    if (!bodies[a].shape.asset && !bodies[b].shape.asset &&
        (bodies[a].shape.type == ShapeType::Box || bodies[a].shape.type == ShapeType::Sphere) &&
        (bodies[b].shape.type == ShapeType::Box || bodies[b].shape.type == ShapeType::Sphere))
        return PairPenetration(a, b, pa, pb, time) > target;
    const auto posa = PrimitiveAt(bodies[a].shape, aa.pose, pa, &*aa.orientation);
    const auto posb = PrimitiveAt(bodies[b].shape, bb.pose, pb, &*bb.orientation);
    if (posa.shape.asset || posb.shape.asset)
        return CookedHasPenetrationBeyond(posa, posb, target);
    return PairPenetration(a, b, pa, pb, time) > target;
}

inline auto PhysicsWorld::Impl::CCDQuery::PairTouches(unsigned a, unsigned b, int pa, int pb, double time)
    -> bool {
    PhysicsWorkTimer timer(work.geometry, work.active);
    const auto &aa = ImpactPoseAt(a, time);
    const auto &bb = ImpactPoseAt(b, time);
    if (SimpleImpactBoundsSeparated(a, b, aa, bb))
        return false;
    if (!bodies[a].shape.asset && !bodies[b].shape.asset &&
        (bodies[a].shape.type == ShapeType::Box || bodies[a].shape.type == ShapeType::Sphere) &&
        (bodies[b].shape.type == ShapeType::Box || bodies[b].shape.type == ShapeType::Sphere))
        return PrimitiveHasContacts(
            bodies[a].shape, {aa.pose.position, aa.pose.orientation, -bodies[a].shape.pivotOffset},
            bodies[b].shape, {bb.pose.position, bb.pose.orientation, -bodies[b].shape.pivotOffset}, 0,
            &*aa.orientation, &*bb.orientation);
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

inline auto PhysicsWorld::Impl::CCDQuery::EnsureImpactPoseStorage() -> void {
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

inline auto PhysicsWorld::Impl::ImpactPoseAt(unsigned slot, double time) -> const PhysicsWorld::Impl::ImpactPoseSample & {
    CCDQuery query(*this);
    return query.ImpactPoseAt(slot, time);
}

inline auto PhysicsWorld::Impl::ActualImpactBound(unsigned slot, const ImpactPoseSample &pose) -> const Aabb & {
    CCDQuery query(*this);
    return query.ActualImpactBound(slot, pose);
}

inline auto PhysicsWorld::Impl::SimpleImpactBoundsSeparated(unsigned a, unsigned b, const ImpactPoseSample &aa, const ImpactPoseSample &bb) -> bool {
    CCDQuery query(*this);
    return query.SimpleImpactBoundsSeparated(a, b, aa, bb);
}

inline auto PhysicsWorld::Impl::PairAt(unsigned a, unsigned b, int pa, int pb, double time, float margin) -> ContactManifold {
    CCDQuery query(*this);
    return query.PairAt(a, b, pa, pb, time, margin);
}

inline auto PhysicsWorld::Impl::PairPenetration(unsigned a, unsigned b, int pa, int pb, double time) -> double {
    CCDQuery query(*this);
    return query.PairPenetration(a, b, pa, pb, time);
}

inline auto PhysicsWorld::Impl::PairPenetrationExceeds(unsigned a, unsigned b, int pa, int pb, double time, double target) -> bool {
    CCDQuery query(*this);
    return query.PairPenetrationExceeds(a, b, pa, pb, time, target);
}

inline auto PhysicsWorld::Impl::PairTouches(unsigned a, unsigned b, int pa, int pb, double time) -> bool {
    CCDQuery query(*this);
    return query.PairTouches(a, b, pa, pb, time);
}

inline auto PhysicsWorld::Impl::EnsureImpactPoseStorage() -> void {
    CCDQuery query(*this);
    query.EnsureImpactPoseStorage();
}
