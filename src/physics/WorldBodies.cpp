#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

glm::mat3 JudasPhysicsDetail::CompoundInverseInertia(float mass,
                                                     const std::vector<CompoundBox> &boxes) {
    float totalVolume = 0.0f;
    for (const CompoundBox &box : boxes) {
        totalVolume += 8.0f * box.halfExtents.x * box.halfExtents.y * box.halfExtents.z;
    }
    if (mass <= 0.0f || totalVolume <= 0.0f)
        return glm::mat3(0.0f);

    // Uniform density, with the parallel-axis theorem for each child. The
    // resulting full tensor includes products of inertia when the geometry
    // is asymmetric; RigidBody rotates it to world space as usual.
    glm::mat3 inertia(0.0f);
    for (const CompoundBox &box : boxes) {
        const float childMass =
            mass * (8.0f * box.halfExtents.x * box.halfExtents.y * box.halfExtents.z / totalVolume);
        const glm::vec3 h = box.halfExtents;
        glm::mat3 tensor(0);
        tensor[0][0] = childMass * (h.y * h.y + h.z * h.z) / 3;
        tensor[1][1] = childMass * (h.x * h.x + h.z * h.z) / 3;
        tensor[2][2] = childMass * (h.x * h.x + h.y * h.y) / 3;
        const glm::mat3 rotation(ContactRotation(box.rotation));
        inertia += rotation * tensor * glm::transpose(rotation);
        const glm::vec3 &r = box.localCenter;
        inertia += childMass * (glm::dot(r, r) * glm::mat3(1.0f) - glm::outerProduct(r, r));
    }
    return glm::determinant(inertia) > 1.0e-12f ? glm::inverse(inertia) : glm::mat3(0.0f);
}

std::vector<BodyBox> JudasPhysicsDetail::BoxesAt(const Shape &shape, const glm::vec3 &position,
                                                 const glm::quat &orientation) {
    std::vector<BodyBox> boxes;
    if (shape.type == ShapeType::Box) {
        boxes.push_back({position, orientation, shape.halfExtents});
    } else if (shape.type == ShapeType::CompoundBoxes) {
        boxes.reserve(shape.boxes.size());
        for (const CompoundBox &child : shape.boxes) {
            if (child.type != ShapeType::Box)
                continue;
            boxes.push_back({position + orientation * (child.localCenter - shape.pivotOffset),
                             glm::normalize(orientation * child.rotation), child.halfExtents});
        }
    }
    return boxes;
}

auto PhysicsWorld::Impl::KinematicVelocity(const Body &body, float dt, glm::vec3 &linear,
                                           glm::vec3 &angular, double &duration) -> void {
    linear = angular = glm::vec3(0);
    duration = 0;
    const auto &rigid = body.rigidBody;
    const auto &command = body.kinematic;
    if (!body.enabled || !(dt > 0))
        return;
    if (command.control == KinematicControl::Velocity) {
        linear = command.linearVelocity;
        angular = command.angularVelocity;
        duration = dt;
    } else if (command.control == KinematicControl::Target) {
        const float remaining = command.targetNextStep ? dt : command.remainingSeconds;
        if (remaining > 0) {
            const auto targetCenter =
                command.target.position + command.target.rotation * body.shape.pivotOffset;
            linear = (targetCenter - rigid.position) / remaining;
            glm::quat delta = command.target.rotation * glm::conjugate(rigid.orientation);
            if (delta.w < 0)
                delta = -delta;
            const double sine = glm::length(glm::dvec3(delta.x, delta.y, delta.z));
            if (sine > 0 && !SameRotation(command.target.rotation, rigid.orientation)) {
                const double angle = 2 * std::atan2(sine, double(delta.w));
                angular =
                    glm::vec3(glm::dvec3(delta.x, delta.y, delta.z) * (angle / (sine * remaining)));
            }
            duration = std::min(double(dt), double(remaining));
        }
    }
}

auto PhysicsWorld::Impl::PublishKinematics(float dt) -> void {
    for (auto slot : aliveSlots) {
        auto &body = bodies[slot];
        if (!body.isKinematic || body.simulationSuspended)
            continue;
        auto &rigid = body.rigidBody;
        auto &command = body.kinematic;
        body.previousPosition = rigid.position;
        body.previousOrientation = rigid.orientation;
        CurrentBound(body);
        body.previousBound = body.currentBound;
        KinematicVelocity(body, dt, rigid.linearVelocity, rigid.angularVelocity,
                          body.kinematicActiveDuration);
        if (body.enabled && command.control == KinematicControl::Target && command.targetNextStep) {
            command.remainingSeconds = dt;
            command.targetNextStep = false;
        }
        rigid.ClearAccumulators();
        body.deferredAcceleration = false;
        body.motion.BeginPrescribed(MakeHandle(slot).id, rigid, dt, body.kinematicActiveDuration);
        InvalidateImpactPose(slot);
    }
}

auto PhysicsWorld::Impl::AddBody(const Shape &shape, const glm::vec3 &position,
                                 const glm::quat &rotation, bool isDynamic, float mass,
                                 float friction, float restitution) -> BodyHandle {
    Body body;
    body.shape = shape;
    body.boundingRadius = ShapeBoundingRadius(shape);
    ++cacheCounters.shapeRebuilds;
    body.friction = friction;
    body.restitution = restitution;
    body.isDynamic = isDynamic;
    body.alive = true;
    body.rigidBody.position = position;
    body.rigidBody.orientation = rotation;
    body.previousPosition = position;
    body.previousOrientation = rotation;
    if (isDynamic) {
        body.rigidBody.inverseMass = mass > 0.0f ? 1.0f / mass : 0.0f;
        if (shape.type == ShapeType::Sphere) {
            body.rigidBody.inverseInertiaLocal = SolidSphereInverseInertia(mass, shape.radius);
        } else if (shape.type == ShapeType::Box) {
            body.rigidBody.inverseInertiaLocal = SolidBoxInverseInertia(mass, shape.halfExtents);
        } else if (shape.type == ShapeType::CompoundBoxes) {
            body.rigidBody.inverseInertiaLocal = CompoundInverseInertia(mass, shape.boxes);
        }
    }
    // Static bodies keep inverseMass=0 / zero inverse inertia (RigidBody's own defaults),
    // which is exactly what "never moved by force or impulse" means throughout this engine.
    unsigned int slot;
    if (!freeSlots.empty()) {
        slot = freeSlots.back();
        freeSlots.pop_back();
        body.generation = (bodies[slot].generation + 1u) & ((1u << (32u - kSlotBits)) - 1u);
        bodies[slot] = body;
    } else {
        slot = static_cast<unsigned int>(bodies.size());
        bodies.push_back(body);
    }
    aliveSlots.insert(std::lower_bound(aliveSlots.begin(), aliveSlots.end(), slot), slot);
    bodies[slot].proxy =
        tree.CreateProxy(TightBound(bodies[slot]).Expanded(kBroadphaseMargin), slot);
    return MakeHandle(slot);
}

auto PhysicsWorld::Impl::Remove(BodyHandle handle) -> void {
    Body *body = Get(handle);
    if (!body)
        return;
    const unsigned int slot = handle.id & kSlotMask;
    body->alive = false;
    overlapPairs.Remove(slot);
    tree.DestroyProxy(body->proxy);
    body->proxy = DynamicAabbTree::kNull;
    freeSlots.push_back(slot);
    const auto it = std::lower_bound(aliveSlots.begin(), aliveSlots.end(), slot);
    if (it != aliveSlots.end() && *it == slot)
        aliveSlots.erase(it);
}

void PhysicsWorld::SetJobSystem(JobSystem *jobs, unsigned maxLanes) {
    if (!m_impl)
        throw std::logic_error("physics job service requires initialized world");
    if (m_impl->stepping || m_impl->coupledStep)
        throw std::logic_error("physics job service requires completed step boundary");
    m_impl->contactJobs = jobs;
    m_impl->contactJobLanes = std::min(maxLanes, JobSystem::kMaxWorkers + 1);
    m_impl->initialContactGeometry.clear();
}

bool PhysicsWorld::SetBodySimulationSuspended(BodyHandle handle, bool suspended) {
    auto *body = m_impl->Get(handle);
    if (!body || (suspended && body->enabled))
        return false;
    body->simulationSuspended = suspended;
    body->motion.Clear();
    body->previousPosition = body->rigidBody.position;
    body->previousOrientation = body->rigidBody.orientation;
    return true;
}

bool PhysicsWorld::ReplaceStaticCollision(BodyHandle handle,
                                          std::shared_ptr<const CollisionAsset> asset) {
    auto *body = m_impl->Get(handle);
    if (!body || body->isDynamic || body->isKinematic || !asset || asset->convex ||
        asset->faces.empty())
        return false;
    const auto old = m_impl->CurrentBound(*body);
    for (auto slot : m_impl->QuerySlots(old))
        if (m_impl->bodies[slot].sleeping)
            m_impl->Wake(slot);
    body->shape = Shape::Cooked(std::move(asset), "");
    body->boundingRadius = ShapeBoundingRadius(body->shape);
    body->currentBound.valid = body->previousBound.valid = false;
    body->boundPreparationValid = false;
    const auto slot = handle.id & Impl::kSlotMask;
    m_impl->InvalidateRestoredContinuation(slot);
    auto uses = [&](const auto &entry) {
        return entry.key.slotA == slot || entry.key.slotB == slot;
    };
    auto &cache = m_impl->contactCache;
    cache.erase(std::remove_if(cache.begin(), cache.end(), uses), cache.end());
    auto &observations = m_impl->completedContactObservations;
    observations.erase(std::remove_if(observations.begin(), observations.end(), uses),
                       observations.end());
    m_impl->InvalidateImpactPose(slot);
    m_impl->RefreshProxy(*body);
    for (auto other : m_impl->QuerySlots(m_impl->CurrentBound(*body)))
        if (m_impl->bodies[other].sleeping)
            m_impl->Wake(other);
    return true;
}

BodyHandle PhysicsWorld::CreateShape(const Shape &input, const BodyTransform &pose, bool dynamic,
                                     float mass, float friction, float restitution) {
    Shape shape = input;
    auto positive = [](glm::vec3 h) {
        return std::isfinite(h.x) && std::isfinite(h.y) && std::isfinite(h.z) && h.x > 0 &&
               h.y > 0 && h.z > 0;
    };
    auto valid = [&](const Shape &s) {
        if (s.type == ShapeType::Box && !positive(s.halfExtents))
            throw std::invalid_argument("box dimensions must be finite positive");
        if (s.type == ShapeType::Sphere && (!std::isfinite(s.radius) || s.radius <= 0))
            throw std::invalid_argument("sphere radius must be finite positive");
        if ((s.type == ShapeType::ConvexHull || s.type == ShapeType::TriangleMesh) &&
            (!s.asset || s.asset->convex != (s.type == ShapeType::ConvexHull)))
            throw std::invalid_argument("cooked shape kind/asset mismatch");
    };
    valid(shape);
    if (!std::isfinite(pose.position.x) || !std::isfinite(pose.position.y) ||
        !std::isfinite(pose.position.z) || !std::isfinite(friction) || friction < 0 ||
        !std::isfinite(restitution) || restitution < 0 || restitution > 1)
        throw std::invalid_argument("invalid body pose/material");
    if (shape.type == ShapeType::TriangleMesh && dynamic)
        throw std::invalid_argument(
            "concave triangle collision is static-only; use convex/compound");
    if (shape.type == ShapeType::TriangleMesh && !shape.asset)
        throw std::invalid_argument("missing cooked mesh");
    if (shape.type == ShapeType::CompoundBoxes) {
        if (shape.boxes.empty() || shape.boxes.size() > 64)
            throw std::invalid_argument("compound limit: 1..64 children");
        std::set<uint32_t> keys;
        for (auto &c : shape.boxes) {
            if (!c.key)
                c.key = uint32_t(&c - shape.boxes.data() + 1);
            if (!keys.insert(c.key).second)
                throw std::invalid_argument("duplicate compound child key");
            if (c.type != ShapeType::Box && c.type != ShapeType::Sphere &&
                c.type != ShapeType::ConvexHull)
                throw std::invalid_argument("unsupported compound child shape");
            ContactRotation(c.rotation);
            if (!std::isfinite(c.localCenter.x) || !std::isfinite(c.localCenter.y) ||
                !std::isfinite(c.localCenter.z))
                throw std::invalid_argument("nonfinite child position");
            valid(c.type == ShapeType::Box      ? Shape::Box(c.halfExtents)
                  : c.type == ShapeType::Sphere ? Shape::Sphere(c.radius)
                                                : Shape::Cooked(c.asset, c.assetId));
        }
    }
    ContactRotation(pose.rotation);
    CollisionMassProperties properties;
    if (dynamic) {
        if (!(mass > 0) || !std::isfinite(mass))
            throw std::invalid_argument("dynamic mass must be positive");
        properties = ShapeMassProperties(shape);
        shape.pivotOffset = glm::vec3(properties.center);
    }
    const auto center = pose.position + pose.rotation * shape.pivotOffset;
    auto handle =
        m_impl->AddBody(shape, center, pose.rotation, dynamic, mass, friction, restitution);
    if (dynamic)
        m_impl->Get(handle)->rigidBody.inverseInertiaLocal =
            glm::mat3(glm::inverse(properties.inertia * (double(mass) / properties.volume)));
    return handle;
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3 &position, const glm::vec3 &halfExtents,
                                         float friction, float restitution) {
    return CreateStaticBox(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), halfExtents, friction,
                           restitution);
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3 &position, const glm::quat &rotation,
                                         const glm::vec3 &halfExtents, float friction,
                                         float restitution) {
    return m_impl->AddBody(Shape::Box(halfExtents), position, rotation, false, 0.0f, friction,
                           restitution);
}

BodyHandle PhysicsWorld::CreateStaticSphere(const glm::vec3 &position, float radius, float friction,
                                            float restitution) {
    return m_impl->AddBody(Shape::Sphere(radius), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                           false, 0.0f, friction, restitution);
}

BodyHandle PhysicsWorld::CreateStaticTerrain(const glm::vec3 &position, const glm::quat &rotation,
                                             std::shared_ptr<const RadialTerrain> terrain,
                                             float friction, float restitution) {
    if (!terrain)
        return BodyHandle{};
    return m_impl->AddBody(Shape::Terrain(std::move(terrain)), position, rotation, false, 0.0f,
                           friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicBox(const glm::vec3 &position, const glm::vec3 &halfExtents,
                                          float mass, float friction, float restitution) {
    return m_impl->AddBody(Shape::Box(halfExtents), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                           true, mass, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicSphere(const glm::vec3 &position, float radius, float mass,
                                             float friction, float restitution) {
    return m_impl->AddBody(Shape::Sphere(radius), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), true,
                           mass, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicCompoundBoxes(const glm::vec3 &position,
                                                    const std::vector<CompoundBox> &boxes,
                                                    float mass, float friction, float restitution) {
    if (boxes.empty() || mass <= 0.0f)
        return BodyHandle{};
    for (const CompoundBox &box : boxes) {
        if (box.halfExtents.x <= 0.0f || box.halfExtents.y <= 0.0f || box.halfExtents.z <= 0.0f) {
            return BodyHandle{};
        }
    }
    return m_impl->AddBody(Shape::Compound(boxes), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                           true, mass, friction, restitution);
}

void PhysicsWorld::DestroyBody(BodyHandle handle) {
    if (m_impl && m_impl->Get(handle))
        m_impl->InvalidateRestoredContinuation(handle.id & Impl::kSlotMask);
    Wake(handle);
    if (m_impl) {
        for (auto it = m_impl->suppressedPairs.begin(); it != m_impl->suppressedPairs.end();) {
            if (it->first == handle.id || it->second == handle.id)
                it = m_impl->suppressedPairs.erase(it);
            else
                ++it;
        }
    }
    if (m_impl) {
        auto &joints = m_impl->joints;
        joints.erase(std::remove_if(joints.begin(), joints.end(),
                                    [&](const auto &j) {
                                        return j.state.settings.bodyA.id == handle.id ||
                                               j.state.settings.bodyB.id == handle.id;
                                    }),
                     joints.end());
    }
    m_impl->Remove(handle);
}

std::size_t PhysicsWorld::AliveBodyCount() const {
    return m_impl->aliveSlots.size();
}

const std::vector<PhysicsWorld::DebugContact> &PhysicsWorld::LastStepContacts() const {
    return m_impl->lastStepContacts;
}

std::size_t PhysicsWorld::DynamicBodyCount() const {
    std::size_t count = 0;
    for (const unsigned int slot : m_impl->aliveSlots) {
        if (m_impl->bodies[slot].isDynamic)
            ++count;
    }
    return count;
}

void PhysicsWorld::ApplyLinearAcceleration(BodyHandle handle, const glm::vec3 &acceleration,
                                           float fixedDeltaTime) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body || body->simulationSuspended || body->rigidBody.IsStatic())
        return;
    if (body->sleeping && glm::length(acceleration - body->lastAcceleration) > 1e-5f)
        m_impl->Wake(handle.id & Impl::kSlotMask);
    body->lastAcceleration = acceleration;
    if (body->sleeping) {
        body->deferredAcceleration = true;
        return;
    }
    body->deferredAcceleration = false;
    body->rigidBody.linearVelocity += acceleration * fixedDeltaTime;
    body->stepVelocityDelta += acceleration * fixedDeltaTime;
}

void PhysicsWorld::ApplyForce(BodyHandle handle, const glm::vec3 &force) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body || body->simulationSuspended || body->rigidBody.IsStatic())
        return;
    if (glm::length(force) > 1e-8f)
        m_impl->Wake(handle.id & Impl::kSlotMask);
    body->rigidBody.ApplyForce(force);
}

void PhysicsWorld::ApplyTorque(BodyHandle handle, const glm::vec3 &torque, bool wake) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body || body->simulationSuspended || body->rigidBody.IsStatic())
        return;
    // Unchanged pose drives retain the normal quiet-time clock at equilibrium.
    // Existing callers keep their normal explicit wake behaviour.
    if (wake && glm::length(torque) > 1e-8f)
        m_impl->Wake(handle.id & Impl::kSlotMask);
    body->rigidBody.ApplyTorque(torque);
}

bool PhysicsWorld::IsDynamicBody(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    return body && body->isDynamic;
}

bool PhysicsWorld::IsKinematicBody(BodyHandle handle) const {
    const auto *body = m_impl->Get(handle);
    return body && body->isKinematic;
}

BodyMotionType PhysicsWorld::GetMotionType(BodyHandle handle) const {
    const auto *body = m_impl->Get(handle);
    return body && body->isKinematic ? BodyMotionType::Kinematic
           : body && body->isDynamic ? BodyMotionType::Dynamic
                                     : BodyMotionType::Static;
}

bool PhysicsWorld::SetMotionType(BodyHandle handle, BodyMotionType mode, float mass,
                                 bool preserveVelocity) {
    auto *body = m_impl->Get(handle);
    if (!body || body->queryOnly ||
        (mode != BodyMotionType::Static && mode != BodyMotionType::Dynamic &&
         mode != BodyMotionType::Kinematic))
        return false;
    if (GetMotionType(handle) == mode)
        return true;
    if (mode != BodyMotionType::Static &&
        (!KinematicShape(body->shape) || !std::isfinite(mass) || mass <= 0))
        return false;
    // A joint's anchors are COM-local. Changing mass authority/COM while it
    // participates would silently reinterpret the connection; reject atomically.
    for (const auto &j : m_impl->joints)
        if (j.state.settings.bodyA.id == handle.id || j.state.settings.bodyB.id == handle.id)
            return false;
    Shape shape = body->shape;
    glm::mat3 inverseInertia(0);
    float inverseMass = 0;
    try {
        if (mode != BodyMotionType::Static) {
            const auto properties = ShapeMassProperties(shape);
            shape.pivotOffset = glm::vec3(properties.center);
            if (mode == BodyMotionType::Dynamic) {
                inverseMass = 1 / mass;
                inverseInertia = glm::mat3(
                    glm::inverse(properties.inertia * (double(mass) / properties.volume)));
            }
        }
    } catch (const std::exception &) {
        return false;
    }
    if (!std::isfinite(inverseMass) || !FiniteVector(shape.pivotOffset))
        return false;
    for (int column = 0; column < 3; ++column)
        if (!FiniteVector(inverseInertia[column]))
            return false;
    const auto pivot = GetTransform(handle);
    const auto oldLinear = body->rigidBody.linearVelocity;
    const auto oldAngular = body->rigidBody.angularVelocity;
    if (!FiniteVector(pivot.position + pivot.rotation * shape.pivotOffset) ||
        (preserveVelocity && (!UsableVelocity(oldLinear) || !UsableVelocity(oldAngular))))
        return false;
    m_impl->Wake(handle.id & Impl::kSlotMask);
    for (auto i : m_impl->QuerySlots(m_impl->CurrentBound(*body)))
        if (m_impl->bodies[i].sleeping)
            m_impl->Wake(i);
    body->shape = std::move(shape);
    body->boundingRadius = ShapeBoundingRadius(body->shape);
    body->isDynamic = mode == BodyMotionType::Dynamic;
    body->isKinematic = mode == BodyMotionType::Kinematic;
    body->rigidBody.inverseMass = inverseMass;
    body->rigidBody.inverseInertiaLocal = inverseInertia;
    body->rigidBody.position = pivot.position + pivot.rotation * body->shape.pivotOffset;
    body->rigidBody.linearVelocity =
        preserveVelocity && mode != BodyMotionType::Static ? oldLinear : glm::vec3(0);
    body->rigidBody.angularVelocity =
        preserveVelocity && mode != BodyMotionType::Static ? oldAngular : glm::vec3(0);
    body->rigidBody.ClearAccumulators();
    body->lastAcceleration = glm::vec3(0);
    body->deferredAcceleration = false;
    body->kinematic = {};
    body->kinematicActiveDuration = 0;
    if (body->isKinematic && preserveVelocity) {
        body->kinematic.control = KinematicControl::Velocity;
        body->kinematic.linearVelocity = oldLinear;
        body->kinematic.angularVelocity = oldAngular;
    }
    body->motion.Clear();
    body->previousPosition = body->rigidBody.position;
    body->previousOrientation = body->rigidBody.orientation;
    body->currentBound.valid = body->previousBound.valid = false;
    body->boundPreparationValid = false;
    m_impl->InvalidateRestoredContinuation(handle.id & Impl::kSlotMask);
    // Disposable solver history belongs to the old authority, never the handle
    // itself. Contact-event identity remains continuous across a safe handoff.
    const auto slot = handle.id & Impl::kSlotMask;
    auto uses = [&](const auto &entry) {
        return entry.key.slotA == slot || entry.key.slotB == slot;
    };
    auto &cache = m_impl->contactCache;
    cache.erase(std::remove_if(cache.begin(), cache.end(), uses), cache.end());
    auto &observations = m_impl->completedContactObservations;
    observations.erase(std::remove_if(observations.begin(), observations.end(), uses),
                       observations.end());
    m_impl->InvalidateImpactPose(slot);
    m_impl->RefreshProxy(*body);
    return true;
}

bool PhysicsWorld::MoveKinematic(BodyHandle handle, const BodyTransform &target, float seconds) {
    auto *body = m_impl->Get(handle);
    if (!body || !body->enabled || !body->isKinematic || !KinematicShape(body->shape) ||
        !UsableKinematicPose(target) || !std::isfinite(seconds) || seconds < 0 || seconds > 60)
        return false;
    auto normalized = target;
    const auto anchor = body->rigidBody.orientation;
    normalized.rotation =
        Impl::SameRotation(target.rotation, anchor) || Impl::SameRotation(-target.rotation, anchor)
            ? anchor
            : glm::normalize(target.rotation);
    const auto center = normalized.position + normalized.rotation * body->shape.pivotOffset;
    if (!FiniteVector(center) || !FiniteVector(center - body->rigidBody.position) ||
        !std::isfinite(glm::length(center - body->rigidBody.position)) ||
        (seconds > 0 && (!UsableVelocity((center - body->rigidBody.position) / seconds) ||
                         !std::isfinite(glm::pi<float>() / seconds))))
        return false;
    KinematicMotionState command;
    command.control = KinematicControl::Target;
    command.target = normalized;
    command.remainingSeconds = seconds;
    command.targetNextStep = seconds == 0;
    body->kinematic = command;
    return true;
}

bool PhysicsWorld::SetKinematicVelocity(BodyHandle handle, const glm::vec3 &linear,
                                        const glm::vec3 &angular) {
    auto *body = m_impl->Get(handle);
    if (!body || !body->enabled || !body->isKinematic || !KinematicShape(body->shape) ||
        !UsableVelocity(linear) || !UsableVelocity(angular))
        return false;
    KinematicMotionState command;
    command.control = KinematicControl::Velocity;
    command.linearVelocity = linear;
    command.angularVelocity = angular;
    body->kinematic = command;
    return true;
}

bool PhysicsWorld::StopKinematic(BodyHandle handle) {
    auto *body = m_impl->Get(handle);
    if (!body || !body->isKinematic)
        return false;
    body->kinematic = {};
    return true;
}

bool PhysicsWorld::GetKinematicMotion(BodyHandle handle, KinematicMotionState &state) const {
    const auto *body = m_impl->Get(handle);
    if (!body || !body->isKinematic)
        return false;
    state = body->kinematic;
    return true;
}

glm::vec3 PhysicsWorld::GetPointVelocity(BodyHandle handle, const glm::vec3 &point) const {
    const auto *body = m_impl->Get(handle);
    if (!body || !body->enabled || !FiniteVector(point))
        return glm::vec3(0);
    return body->rigidBody.linearVelocity +
           glm::cross(body->rigidBody.angularVelocity, point - body->rigidBody.position);
}

glm::vec3 PhysicsWorld::GetStepPointVelocity(BodyHandle handle, const glm::vec3 &point,
                                             float dt) const {
    const auto *body = m_impl->Get(handle);
    if (!body || !body->enabled || !FiniteVector(point) || !std::isfinite(dt) || dt <= 0)
        return glm::vec3(0);
    if (!body->isKinematic)
        return GetPointVelocity(handle, point);
    glm::vec3 linear, angular;
    double duration = 0;
    Impl::KinematicVelocity(*body, dt, linear, angular, duration);
    return linear + glm::cross(angular, point - body->rigidBody.position);
}

glm::vec3 PhysicsWorld::GetPreviousPointVelocity(BodyHandle handle, const glm::vec3 &point) const {
    const auto *body = m_impl->Get(handle);
    if (!body || !body->enabled || !FiniteVector(point))
        return glm::vec3(0);
    return body->rigidBody.linearVelocity +
           glm::cross(body->rigidBody.angularVelocity,
                      point -
                          (body->HasMotion() ? body->previousPosition : body->rigidBody.position));
}

float PhysicsWorld::GetMass(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    return body && body->isDynamic && body->rigidBody.inverseMass > 0.0f
               ? 1.0f / body->rigidBody.inverseMass
               : 0.0f;
}

glm::vec3 PhysicsWorld::GetCenterOfMass(BodyHandle handle) const {
    if (!m_impl)
        return glm::vec3(0);
    const auto *body = m_impl->Get(handle);
    return body ? body->rigidBody.position : glm::vec3(0);
}

void PhysicsWorld::ApplyLinearImpulse(BodyHandle handle, const glm::vec3 &impulse, bool wake) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body || body->simulationSuspended || !body->isDynamic)
        return;
    if (wake && glm::length(impulse) > 1e-8f)
        m_impl->Wake(handle.id & Impl::kSlotMask);
    body->rigidBody.ApplyLinearImpulse(impulse);
}

void PhysicsWorld::ApplyAngularImpulse(BodyHandle handle, const glm::vec3 &impulse, bool wake) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body || body->simulationSuspended || !body->isDynamic || !FiniteVector(impulse))
        return;
    if (wake && glm::length(impulse) > 1e-8f)
        m_impl->Wake(handle.id & Impl::kSlotMask);
    body->rigidBody.ApplyAngularImpulse(impulse);
}

void PhysicsWorld::ApplyImpulseAtPoint(BodyHandle handle, const glm::vec3 &impulse,
                                       const glm::vec3 &worldPoint) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body || body->simulationSuspended || !body->isDynamic)
        return;
    if (glm::length(impulse) > 1e-8f)
        m_impl->Wake(handle.id & Impl::kSlotMask);
    body->rigidBody.ApplyImpulseAtPoint(impulse, worldPoint);
}

glm::vec3 PhysicsWorld::GetLinearVelocity(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    return body ? body->rigidBody.linearVelocity : glm::vec3(0.0f);
}

void PhysicsWorld::SetLinearVelocity(BodyHandle handle, const glm::vec3 &velocity) {
    Impl::Body *body = m_impl->Get(handle);
    if (body) {
        if (velocity != body->rigidBody.linearVelocity)
            m_impl->Wake(handle.id & Impl::kSlotMask);
        body->rigidBody.linearVelocity = velocity;
        body->stepVelocityDelta = glm::vec3(0);
    }
}

glm::vec3 PhysicsWorld::GetAngularVelocity(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    return body ? body->rigidBody.angularVelocity : glm::vec3(0.0f);
}

glm::mat3 PhysicsWorld::GetInertiaWorld(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    if (!body || !body->isDynamic)
        return glm::mat3(0.0f);
    return glm::inverse(body->rigidBody.InverseInertiaWorld());
}

float PhysicsWorld::GetBodySupportDistance(BodyHandle handle,
                                           const glm::vec3 &worldDirection) const {
    const Impl::Body *body = m_impl->Get(handle);
    const float directionLength = glm::length(worldDirection);
    if (!body || directionLength < 1.0e-6f)
        return 0.0f;

    const glm::vec3 direction = worldDirection / directionLength;
    float maximum = 0.0f;
    for (int part = 0; part < PrimitiveCount(body->shape); ++part) {
        const PrimitivePose primitive = PrimitiveAt(body->shape, body->rigidBody, part);
        float extent = 0.0f;
        if (primitive.shape.type == ShapeType::Sphere) {
            extent = primitive.shape.radius;
        } else if (primitive.shape.type == ShapeType::Box) {
            const glm::vec3 localDirection =
                glm::conjugate(glm::normalize(primitive.body.orientation)) * direction;
            extent = glm::dot(glm::abs(localDirection), primitive.shape.halfExtents);
        } else if (primitive.shape.asset) {
            // Directional support uses the physical vertices about the body's COM.
            // Child offsets and rotations have already been composed in PrimitiveAt.
            const auto r = PrimitiveRotation(primitive);
            const auto c = PrimitiveCenter(primitive);
            for (const auto &vertex : primitive.shape.asset->vertices)
                maximum =
                    std::max(maximum, static_cast<float>(glm::dot(
                                          c + r * vertex - glm::dvec3(body->rigidBody.position),
                                          glm::dvec3(direction))));
            continue;
        } else if (primitive.shape.type == ShapeType::Terrain && primitive.shape.terrain) {
            // Exact directional support of an arbitrary radial height
            // function would require global optimization. Its immutable
            // radius bound is conservative for this separation query.
            extent = primitive.shape.terrain->BoundRadius();
        }
        maximum = std::max(maximum,
                           glm::dot(primitive.body.position - body->rigidBody.position, direction) +
                               extent);
    }
    return maximum;
}

float PhysicsWorld::GetPlayerShapeMaxSupportDistance() const {
    if (!m_impl->hasPlayerShape)
        return 0.0f;
    return m_impl->playerShape.radius + m_impl->playerShape.halfHeight;
}

void PhysicsWorld::SetAngularVelocity(BodyHandle handle, const glm::vec3 &angularVelocity) {
    Impl::Body *body = m_impl->Get(handle);
    if (body) {
        if (angularVelocity != body->rigidBody.angularVelocity)
            m_impl->Wake(handle.id & Impl::kSlotMask);
        body->rigidBody.angularVelocity = angularVelocity;
    }
}

std::vector<BodyHandle> PhysicsWorld::AliveBodies() const {
    std::vector<BodyHandle> result;
    result.reserve(m_impl->aliveSlots.size());
    for (const unsigned int slot : m_impl->aliveSlots)
        result.push_back(m_impl->MakeHandle(slot));
    return result;
}

bool PhysicsWorld::GetBodyShape(BodyHandle handle, Shape &outShape, BodyTransform &outPose) const {
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return false;
    outShape = body->shape;
    outPose.position = body->rigidBody.position;
    outPose.rotation = body->rigidBody.orientation;
    return true;
}

BodyTransform PhysicsWorld::GetTransform(BodyHandle handle) const {
    BodyTransform result;
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return result;
    result.position =
        body->rigidBody.position - body->rigidBody.orientation * body->shape.pivotOffset;
    result.rotation = body->rigidBody.orientation;
    return result;
}

BodyTransform PhysicsWorld::GetPreviousTransform(BodyHandle handle) const {
    BodyTransform result;
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return result;
    if (body->HasMotion()) {
        result.position =
            body->previousPosition - body->previousOrientation * body->shape.pivotOffset;
        result.rotation = body->previousOrientation;
    } else {
        result.position =
            body->rigidBody.position - body->rigidBody.orientation * body->shape.pivotOffset;
        result.rotation = body->rigidBody.orientation;
    }
    return result;
}

std::vector<PhysicsWorld::BodyMotionSegment>
PhysicsWorld::GetBodyMotionSegments(BodyHandle handle) const {
    std::vector<BodyMotionSegment> result;
    const Impl::Body *body = m_impl->Get(handle);
    if (!body || !body->HasMotion())
        return result;
    result.reserve(body->motion.Segments().size());
    for (const auto &source : body->motion.Segments()) {
        if (source.owner != handle.id)
            return {};
        result.push_back(
            {source.begin,
             source.end,
             {source.position - source.orientation * body->shape.pivotOffset, source.orientation},
             {source.endPosition - source.endOrientation * body->shape.pivotOffset,
              source.endOrientation},
             source.linearVelocity,
             source.driftAngularVelocity,
             source.inverseMass > 0 || source.prescribed,
             body->shape.pivotOffset,
             source.prescribed,
             source.exactRotation,
             source.anchoredTranslation,
             source.translationOrigin,
             source.translationTime});
    }
    if (!result.empty())
        result.back().endPose = {body->rigidBody.position -
                                     body->rigidBody.orientation * body->shape.pivotOffset,
                                 body->rigidBody.orientation};
    return result;
}

BodyTransform PhysicsWorld::EvaluateBodyMotionSegment(const BodyMotionSegment &segment,
                                                      double time) {
    if (!std::isfinite(time) || time < segment.begin || time > segment.end)
        throw std::out_of_range("body motion segment time");
    RigidMotionSegment source;
    source.begin = segment.begin;
    source.end = segment.end;
    source.position = segment.start.position + segment.start.rotation * segment.pivotOffset;
    source.orientation = segment.start.rotation;
    source.linearVelocity = segment.linearVelocity;
    source.angularVelocity = segment.angularVelocity;
    source.driftAngularVelocity = segment.angularVelocity;
    source.inverseMass = segment.movable && !segment.prescribed ? 1.f : 0.f;
    source.prescribed = segment.prescribed;
    source.exactRotation = segment.exactRotation;
    source.anchoredTranslation = segment.anchoredTranslation;
    source.translationOrigin = segment.translationOrigin;
    source.translationTime = segment.translationTime;
    const auto body = source.Evaluate(time);
    return {body.position - body.orientation * segment.pivotOffset, body.orientation};
}

std::vector<BodyBox> PhysicsWorld::GetBodyBoxes(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return {};
    return BoxesAt(body->shape, body->rigidBody.position, body->rigidBody.orientation);
}

std::vector<BodyBox> PhysicsWorld::GetPreviousBodyBoxes(BodyHandle handle) const {
    const Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return {};
    return BoxesAt(body->shape,
                   body->HasMotion() ? body->previousPosition : body->rigidBody.position,
                   body->HasMotion() ? body->previousOrientation : body->rigidBody.orientation);
}

void PhysicsWorld::ResetBody(BodyHandle handle, const glm::vec3 &position,
                             const glm::quat &rotation) {
    Impl::Body *body = m_impl->Get(handle);
    if (!body)
        return;
    if (body->rigidBody.position != position + rotation * body->shape.pivotOffset ||
        body->rigidBody.orientation != rotation) {
        m_impl->Wake(handle.id & Impl::kSlotMask);
        const auto bounds = m_impl->CurrentBound(*body);
        auto slots = m_impl->QuerySlots(bounds);
        for (auto i : slots)
            if (m_impl->bodies[i].sleeping)
                m_impl->Wake(i);
    }
    for (auto &j : m_impl->joints)
        if (j.state.settings.bodyA.id == handle.id || j.state.settings.bodyB.id == handle.id)
            j.warm.fill(0);
    m_impl->InvalidateRestoredContinuation(handle.id & Impl::kSlotMask);
    ++body->contactObservationRevision;
    body->rigidBody.position = position + rotation * body->shape.pivotOffset;
    body->rigidBody.orientation = rotation;
    body->previousPosition = body->rigidBody.position;
    body->motion.Clear();
    body->kinematic = {};
    body->kinematicActiveDuration = 0;
    body->previousOrientation = rotation;
    body->stepVelocityDelta = glm::vec3(0.0f);
    body->rigidBody.linearVelocity = glm::vec3(0.0f);
    body->rigidBody.angularVelocity = glm::vec3(0.0f);
    body->rigidBody.ClearAccumulators();
    m_impl->RefreshProxy(*body);
    if (!body->isDynamic && !body->queryOnly) {
        auto slots = m_impl->QuerySlots(m_impl->CurrentBound(*body));
        for (auto i : slots)
            if (m_impl->bodies[i].sleeping)
                m_impl->Wake(i);
    }
}

bool PhysicsWorld::SetCollisionFilter(BodyHandle handle, unsigned layer, CategoryMask mask) {
    auto *b = m_impl->Get(handle);
    if (!b || layer >= 64)
        return false;
    if (b->collisionLayer == layer && b->collisionMask == mask)
        return true;
    m_impl->InvalidateRestoredContinuation(handle.id & Impl::kSlotMask);
    m_impl->Wake(handle.id & Impl::kSlotMask);
    b->collisionLayer = layer;
    b->collisionMask = mask;
    const unsigned slot = handle.id & Impl::kSlotMask;
    auto remove = [&](auto &cache) {
        cache.erase(std::remove_if(
                        cache.begin(), cache.end(),
                        [&](const auto &c) { return c.key.slotA == slot || c.key.slotB == slot; }),
                    cache.end());
    };
    remove(m_impl->contactCache);
    remove(m_impl->pendingCache);
    remove(m_impl->completedContactObservations);
    return true;
}

bool PhysicsWorld::GetCollisionFilter(BodyHandle handle, unsigned &layer,
                                      CategoryMask &mask) const {
    const auto *b = m_impl->Get(handle);
    if (!b)
        return false;
    layer = b->collisionLayer;
    mask = b->collisionMask;
    return true;
}

bool PhysicsWorld::SetBodyTags(BodyHandle handle, CategoryMask tags) {
    auto *b = m_impl->Get(handle);
    if (!b)
        return false;
    b->tags = tags;
    return true;
}

void PhysicsWorld::SetPlayerCollisionFilter(unsigned layer, CategoryMask mask) {
    if (layer < 64) {
        m_impl->playerCollisionLayer = layer;
        m_impl->playerCollisionMask = mask;
    }
}

bool PhysicsWorld::SetBodySensor(BodyHandle h, bool value) {
    auto *b = m_impl->Get(h);
    if (!b || (value && b->shape.type == ShapeType::TriangleMesh))
        return false;
    if (b->sensor != value)
        m_impl->Wake(h.id & Impl::kSlotMask);
    if (b->sensor != value)
        m_impl->InvalidateRestoredContinuation(h.id & Impl::kSlotMask);
    if (b->sensor != value)
        ++b->contactObservationRevision;
    b->sensor = value;
    return true;
}

bool PhysicsWorld::IsBodySensor(BodyHandle h) const {
    auto *b = m_impl->Get(h);
    return b && b->sensor;
}

bool PhysicsWorld::SetBodyQueriesEnabled(BodyHandle h, bool enabled) {
    auto *body = m_impl->Get(h);
    if (!body)
        return false;
    body->queriesEnabled = enabled;
    return true;
}

bool PhysicsWorld::SetBodyEnabled(BodyHandle h, bool value) {
    auto *b = m_impl->Get(h);
    if (!b)
        return false;
    if (b->enabled == value)
        return true;
    m_impl->Wake(h.id & Impl::kSlotMask);
    m_impl->InvalidateRestoredContinuation(h.id & Impl::kSlotMask);
    ++b->contactObservationRevision;
    b->enabled = value;
    if (!value && b->isKinematic) {
        b->kinematic = {};
        b->kinematicActiveDuration = 0;
        b->motion.Clear();
        b->rigidBody.linearVelocity = b->rigidBody.angularVelocity = glm::vec3(0);
        b->previousPosition = b->rigidBody.position;
        b->previousOrientation = b->rigidBody.orientation;
    }
    for (auto &j : m_impl->joints)
        if (j.state.settings.bodyA.id == h.id || j.state.settings.bodyB.id == h.id)
            j.warm.fill(0);
    return true;
}

bool PhysicsWorld::IsBodyEnabled(BodyHandle h) const {
    auto *b = m_impl->Get(h);
    return b && b->enabled;
}

bool PhysicsWorld::SetMassDistribution(BodyHandle h, float mass, const glm::mat3 &inertia) {
    auto *b = m_impl->Get(h);
    if (!b || !b->isDynamic || !std::isfinite(mass) || mass <= 0)
        return false;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!std::isfinite(inertia[i][j]) || std::abs(inertia[i][j] - inertia[j][i]) > 1e-5f)
                return false;
    if (inertia[0][0] <= 0 || inertia[0][0] * inertia[1][1] - inertia[0][1] * inertia[0][1] <= 0 ||
        glm::determinant(inertia) <= 0)
        return false;
    m_impl->InvalidateRestoredContinuation(h.id & Impl::kSlotMask);
    Wake(h);
    b->rigidBody.inverseMass = 1 / mass;
    b->rigidBody.inverseInertiaLocal = glm::inverse(inertia);
    return true;
}

bool PhysicsWorld::GetPhysicalMaterial(BodyHandle h, std::string &id, float &friction,
                                       float &restitution) const {
    auto *b = m_impl ? m_impl->Get(h) : nullptr;
    if (!b)
        return false;
    id = b->physicalMaterial;
    friction = b->friction;
    restitution = b->restitution;
    return true;
}

bool PhysicsWorld::SetPhysicalMaterial(BodyHandle h, const std::string &id, float friction,
                                       float restitution) {
    auto *b = m_impl ? m_impl->Get(h) : nullptr;
    if (!b || !std::isfinite(friction) || friction < 0 || !std::isfinite(restitution) ||
        restitution < 0 || restitution > 1)
        return false;
    if (b->physicalMaterial == id && b->friction == friction && b->restitution == restitution)
        return true;
    m_impl->InvalidateRestoredContinuation(h.id & Impl::kSlotMask);
    Wake(h);
    b->physicalMaterial = id;
    b->friction = friction;
    b->restitution = restitution;
    const auto slot = h.id & Impl::kSlotMask;
    auto remove = [&](auto &cache) {
        cache.erase(std::remove_if(
                        cache.begin(), cache.end(),
                        [&](const auto &c) { return c.key.slotA == slot || c.key.slotB == slot; }),
                    cache.end());
    };
    remove(m_impl->contactCache);
    remove(m_impl->pendingCache);
    remove(m_impl->completedContactObservations);
    return true;
}
