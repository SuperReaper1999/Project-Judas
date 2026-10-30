#pragma once

inline auto PhysicsWorld::Impl::JointActive(const JointRecord &joint) const -> bool {
    const auto *a = Get(joint.state.settings.bodyA);
    const auto *b = Get(joint.state.settings.bodyB);
    return joint.state.settings.enabled && a && a->enabled &&
           (!joint.state.settings.bodyB.IsValid() || (b && b->enabled));
}

inline auto PhysicsWorld::Impl::InvalidateImpactPose(unsigned slot) -> void {
    if (slot < impactPoseSamples.size()) {
        impactPoseSamples[slot].time = std::numeric_limits<double>::quiet_NaN();
        impactAnchorSamples[slot].time = std::numeric_limits<double>::quiet_NaN();
        impactPoseSamples[slot].boundStart = std::numeric_limits<double>::quiet_NaN();
    }
}

inline auto PhysicsWorld::Impl::SameFloat(float a, float b) -> bool {
    return std::memcmp(&a, &b, sizeof(float)) == 0;
}

inline auto PhysicsWorld::Impl::SamePosition(const glm::vec3 &a, const glm::vec3 &b) -> bool {
    return SameFloat(a.x, b.x) && SameFloat(a.y, b.y) && SameFloat(a.z, b.z);
}

inline auto PhysicsWorld::Impl::SameRotation(const glm::quat &a, const glm::quat &b) -> bool {
    return SameFloat(a.w, b.w) && SameFloat(a.x, b.x) && SameFloat(a.y, b.y) && SameFloat(a.z, b.z);
}

inline auto PhysicsWorld::Impl::Matches(const PoseBound &bound, const glm::vec3 &p,
                                        const glm::quat &q) -> bool {
    return bound.valid && SamePosition(bound.position, p) && SameRotation(bound.orientation, q);
}

inline auto PhysicsWorld::Impl::CanCollide(unsigned a, unsigned b) const -> bool {
    const bool planarA=bodies[a].shape.type==ShapeType::Planar,planarB=bodies[b].shape.type==ShapeType::Planar;
    if(planarA!=planarB)return false; // Runtime preflight diagnoses shared 2D/3D collision masks.
    auto ha = MakeHandle(a).id, hb = MakeHandle(b).id;
    if (!suppressedPairs.empty() && suppressedPairs.count(std::minmax(ha, hb)))
        return false;
    return bodies[a].enabled && bodies[b].enabled &&
           CollisionPermitted(bodies[a].collisionLayer, bodies[a].collisionMask,
                              bodies[b].collisionLayer, bodies[b].collisionMask);
}

inline auto PhysicsWorld::Impl::OneWayAllows(unsigned a,unsigned b) const -> bool {
    const auto pose=[&](const Body& body){
        const bool start=stepping&&(body.isDynamic||body.isKinematic);
        return ContactPose{start?body.previousPosition:body.rigidBody.position,
                           start?body.previousOrientation:body.rigidBody.orientation,-body.shape.pivotOffset};
    };
    const auto& aa=bodies[a];const auto& bb=bodies[b];
    // Default shapes avoid all pose/support work. This policy is intentionally
    // separate from broadphase pair admission, whose cache outlives a step.
    const bool wa=aa.shape.planar&&aa.shape.planar->oneWay,wb=bb.shape.planar&&bb.shape.planar->oneWay;
    return (!wa||PlanarOneWayAllows(aa.shape,pose(aa),bb.shape,pose(bb),kMk2MinimumMargin))&&
           (!wb||PlanarOneWayAllows(bb.shape,pose(bb),aa.shape,pose(aa),kMk2MinimumMargin));
}

inline auto PhysicsWorld::Impl::CanRespond(unsigned a, unsigned b) const -> bool {
    return CanCollide(a, b) && OneWayAllows(a,b) && !bodies[a].sensor && !bodies[b].sensor && !bodies[a].queryOnly &&
           !bodies[b].queryOnly &&
           !((bodies[a].sleeping || bodies[a].rigidBody.IsStatic()) &&
             (bodies[b].sleeping || bodies[b].rigidBody.IsStatic()));
}

inline auto PhysicsWorld::Impl::MatchesQuery(unsigned slot, const PhysicsQueryFilter &filter) const
    -> bool {
    const auto &b = bodies[slot];
    if((b.shape.type==ShapeType::Planar)!=filter.planar)return false;
    if (!b.enabled || !b.queriesEnabled || (b.sensor && !filter.includeSensors))
        return false;
    const auto bit = CategoryBit(b.collisionLayer);
    if (!(filter.includeLayers & bit) || (filter.excludeLayers & bit) ||
        (b.tags & filter.requiredTags) != filter.requiredTags || (b.tags & filter.excludedTags))
        return false;
    for (const auto handle : filter.ignoredBodies)
        if (Get(handle) && handle.id == MakeHandle(slot).id)
            return false;
    return true;
}

inline auto PhysicsWorld::Impl::MakeHandle(unsigned int slot) const -> BodyHandle {
    BodyHandle handle;
    handle.id = slot | (bodies[slot].generation << kSlotBits);
    handle.world = worldToken;
    return handle;
}

inline auto PhysicsWorld::Impl::Get(BodyHandle handle) -> Body * {
    if (!handle.IsValid() || (handle.world && handle.world != worldToken))
        return nullptr;
    const unsigned int slot = handle.id & kSlotMask;
    if (slot >= bodies.size() || !bodies[slot].alive ||
        bodies[slot].generation != (handle.id >> kSlotBits)) {
        return nullptr;
    }
    return &bodies[slot];
}

inline auto PhysicsWorld::Impl::Get(BodyHandle handle) const -> const Body * {
    if (!handle.IsValid() || (handle.world && handle.world != worldToken))
        return nullptr;
    const unsigned int slot = handle.id & kSlotMask;
    if (slot >= bodies.size() || !bodies[slot].alive ||
        bodies[slot].generation != (handle.id >> kSlotBits)) {
        return nullptr;
    }
    return &bodies[slot];
}

inline auto PhysicsWorld::Impl::ConcaveShape(const Body &body) -> bool {
    return body.shape.type == ShapeType::TriangleMesh || body.shape.type == ShapeType::Terrain;
}

