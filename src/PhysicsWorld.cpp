#include "PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

#include "CollisionShapes.h"
#include "ContactSolver.h"
#include "Contacts.h"
#include "RigidBody.h"

// Judas's own rigid-body physics — no middleware. Collision detection,
// contact generation, contact resolution, and integration are all owned
// here, built from RigidBody (state/integration), Contacts (narrowphase),
// and ContactSolver (impulse resolution). See docs/ARCHITECTURE.md,
// "Physics ownership" for the migration this replaces (Jolt Physics,
// used through the first Milestone 7-Final attempt) and why: no subsystem here — broadphase,
// narrowphase, contact resolution, the player's sweep query — assumes a
// world-space up axis; every one of them is expressed purely in terms of
// the shapes' and bodies' own positions/orientations. Judas already owned
// gravity, reference frames, and world coordinates before this milestone;
// this closes the remaining gap (collision/contact/rigid-body solving)
// that used to belong to Jolt.
namespace {

// A handful of dynamic bodies plus two static planets and a static plank —
// Project Judas's own bodies, not a general scene. Brute-force all-pairs
// broadphase is exactly right at this scale (see PhysicsWorld::Step): a
// spatial broadphase structure would be unused machinery here, not a
// correctness requirement.
constexpr int kSolverIterations = 4;

// Compound geometry is a set of ordinary boxes attached to one rigid body.
// Each narrowphase call still sees exactly the primitive shape it already
// understands; contact impulses are always applied to the shared parent.
int PrimitiveCount(const Shape& shape) {
    return shape.type == ShapeType::CompoundBoxes ? static_cast<int>(shape.boxes.size()) : 1;
}

struct PrimitivePose {
    Shape shape;
    RigidBody body;
};

PrimitivePose PrimitiveAt(const Shape& shape, const RigidBody& parent, int index) {
    if (shape.type != ShapeType::CompoundBoxes) return {shape, parent};
    const CompoundBox& child = shape.boxes[static_cast<std::size_t>(index)];
    RigidBody childPose = parent;
    childPose.position += parent.orientation * child.localCenter;
    return {Shape::Box(child.halfExtents), childPose};
}

std::vector<BodyBox> BoxesAt(const Shape& shape, const glm::vec3& position,
                              const glm::quat& orientation) {
    std::vector<BodyBox> boxes;
    if (shape.type == ShapeType::Box) {
        boxes.push_back({position, orientation, shape.halfExtents});
    } else if (shape.type == ShapeType::CompoundBoxes) {
        boxes.reserve(shape.boxes.size());
        for (const CompoundBox& child : shape.boxes) {
            boxes.push_back({position + orientation * child.localCenter,
                             orientation, child.halfExtents});
        }
    }
    return boxes;
}

glm::mat3 CompoundInverseInertia(float mass, const std::vector<CompoundBox>& boxes) {
    float totalVolume = 0.0f;
    for (const CompoundBox& box : boxes) {
        totalVolume += 8.0f * box.halfExtents.x * box.halfExtents.y * box.halfExtents.z;
    }
    if (mass <= 0.0f || totalVolume <= 0.0f) return glm::mat3(0.0f);

    // Uniform density, with the parallel-axis theorem for each child. The
    // resulting full tensor includes products of inertia when the geometry
    // is asymmetric; RigidBody rotates it to world space as usual.
    glm::mat3 inertia(0.0f);
    for (const CompoundBox& box : boxes) {
        const float childMass = mass *
            (8.0f * box.halfExtents.x * box.halfExtents.y * box.halfExtents.z / totalVolume);
        const glm::vec3 h = box.halfExtents;
        inertia[0][0] += childMass * (h.y * h.y + h.z * h.z) / 3.0f;
        inertia[1][1] += childMass * (h.x * h.x + h.z * h.z) / 3.0f;
        inertia[2][2] += childMass * (h.x * h.x + h.y * h.y) / 3.0f;
        const glm::vec3& r = box.localCenter;
        inertia += childMass * (glm::dot(r, r) * glm::mat3(1.0f) - glm::outerProduct(r, r));
    }
    return glm::determinant(inertia) > 1.0e-12f ? glm::inverse(inertia) : glm::mat3(0.0f);
}

// Uniform manifold dispatcher: sphere-involving pairs always produce at
// most one contact point (wrapped in a 1-point manifold); box-vs-box uses
// the real multi-point manifold (see Contacts.h for why that one
// specifically needs more than one point).
ContactManifold ComputeContacts(const Shape& shapeA, const RigidBody& bodyA, const Shape& shapeB,
                                 const RigidBody& bodyB) {
    ContactManifold manifold;
    if (shapeA.type == ShapeType::Sphere && shapeB.type == ShapeType::Sphere) {
        manifold.Add(SphereVsSphere(bodyA.position, shapeA.radius, bodyB.position, shapeB.radius));
        return manifold;
    }
    if (shapeA.type == ShapeType::Sphere && shapeB.type == ShapeType::Box) {
        manifold.Add(SphereVsBox(bodyA.position, shapeA.radius, bodyB.position, bodyB.orientation,
                                  shapeB.halfExtents));
        return manifold;
    }
    if (shapeA.type == ShapeType::Box && shapeB.type == ShapeType::Sphere) {
        Contact contact = SphereVsBox(bodyB.position, shapeB.radius, bodyA.position,
                                       bodyA.orientation, shapeA.halfExtents);
        if (contact.hit) contact.normal = -contact.normal;  // keep the "points toward A" convention
        manifold.Add(contact);
        return manifold;
    }
    if (shapeA.type == ShapeType::Box && shapeB.type == ShapeType::Box) {
        return BoxVsBoxManifold(bodyA.position, bodyA.orientation, shapeA.halfExtents, bodyB.position,
                                 bodyB.orientation, shapeB.halfExtents);
    }
    return manifold;  // capsules never appear as world bodies -- only as the player's query shape
}

}  // namespace

// Distance (and separating normal/owning body index) from the player's
// capsule, placed at a given segment, to the closest world body — used by
// SweepPlayerShape's substep march. `bodyIndex == -1` means no body was
// found closer than `distance`'s initial +infinity (never happens once any
// world geometry exists, but keeps the result total).
struct ClosestBodyResult {
    float distance = std::numeric_limits<float>::max();
    glm::vec3 normal{0.0f};
    int bodyIndex = -1;
};

struct PhysicsWorld::Impl {
    struct Body {
        RigidBody rigidBody;
        // Previous fixed-step pose for player movement sweeps against
        // dynamic targets; initialized together with the current pose and
        // refreshed immediately before every physics integration.
        glm::vec3 previousPosition{0.0f};
        glm::quat previousOrientation{1.0f, 0.0f, 0.0f, 0.0f};
        Shape shape;
        float friction = 0.5f;
        float restitution = 0.0f;
        bool isDynamic = false;
        bool alive = false;
    };

    std::vector<Body> bodies;

    bool hasPlayerShape = false;
    Shape playerShape;

    // A member of Impl (rather than a free function taking a body list)
    // specifically so it can name `Body` without exposing this private
    // nested type outside PhysicsWorld.cpp.
    ClosestBodyResult ClosestBodyToCapsule(const glm::vec3& segA, const glm::vec3& segB,
                                            float capsuleRadius,
                                            float bodyMotionAlpha,
                                            bool interpolateDynamicBodyMotion) const {
        ClosestBodyResult result;
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const Body& body = bodies[i];
            if (!body.alive) continue;
            const bool interpolateBody = interpolateDynamicBodyMotion && body.isDynamic;
            const glm::vec3 bodyPosition = interpolateBody
                ? glm::mix(body.previousPosition, body.rigidBody.position, bodyMotionAlpha)
                : body.rigidBody.position;
            const glm::quat bodyOrientation = interpolateBody
                ? glm::normalize(glm::slerp(body.previousOrientation, body.rigidBody.orientation,
                                            bodyMotionAlpha))
                : body.rigidBody.orientation;
            RigidBody sampledBody = body.rigidBody;
            sampledBody.position = bodyPosition;
            sampledBody.orientation = bodyOrientation;
            for (int part = 0; part < PrimitiveCount(body.shape); ++part) {
                const PrimitivePose primitive = PrimitiveAt(body.shape, sampledBody, part);
                CapsuleDistance capsuleDistance;
                if (primitive.shape.type == ShapeType::Sphere) {
                    capsuleDistance = CapsuleDistanceToSphere(segA, segB, capsuleRadius,
                                                               primitive.body.position,
                                                               primitive.shape.radius);
                } else if (primitive.shape.type == ShapeType::Box) {
                    capsuleDistance = CapsuleDistanceToBox(segA, segB, capsuleRadius,
                                                            primitive.body.position,
                                                            primitive.body.orientation,
                                                            primitive.shape.halfExtents);
                } else {
                    continue;
                }
                if (capsuleDistance.distance < result.distance) {
                    result.distance = capsuleDistance.distance;
                    result.normal = capsuleDistance.normal;
                    result.bodyIndex = static_cast<int>(i);
                }
            }
        }
        return result;
    }

    Body* Get(BodyHandle handle) {
        if (!handle.IsValid() || handle.id >= bodies.size() || !bodies[handle.id].alive) {
            return nullptr;
        }
        return &bodies[handle.id];
    }
    const Body* Get(BodyHandle handle) const {
        if (!handle.IsValid() || handle.id >= bodies.size() || !bodies[handle.id].alive) {
            return nullptr;
        }
        return &bodies[handle.id];
    }

    BodyHandle AddBody(const Shape& shape, const glm::vec3& position, const glm::quat& rotation,
                        bool isDynamic, float mass, float friction, float restitution) {
        Body body;
        body.shape = shape;
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
        bodies.push_back(body);
        BodyHandle handle;
        handle.id = static_cast<unsigned int>(bodies.size() - 1);
        return handle;
    }
};

bool PhysicsWorld::Init() {
    m_impl = new Impl();
    return true;
}

PhysicsWorld::~PhysicsWorld() { Shutdown(); }

void PhysicsWorld::Shutdown() {
    delete m_impl;
    m_impl = nullptr;
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                          float friction, float restitution) {
    return CreateStaticBox(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), halfExtents, friction,
                            restitution);
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3& position, const glm::quat& rotation,
                                          const glm::vec3& halfExtents, float friction,
                                          float restitution) {
    return m_impl->AddBody(Shape::Box(halfExtents), position, rotation, false, 0.0f, friction,
                            restitution);
}

BodyHandle PhysicsWorld::CreateStaticSphere(const glm::vec3& position, float radius,
                                             float friction, float restitution) {
    return m_impl->AddBody(Shape::Sphere(radius), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                            false, 0.0f, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                           float mass, float friction, float restitution) {
    return m_impl->AddBody(Shape::Box(halfExtents), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                            true, mass, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicSphere(const glm::vec3& position, float radius, float mass,
                                              float friction, float restitution) {
    return m_impl->AddBody(Shape::Sphere(radius), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), true,
                            mass, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicCompoundBoxes(const glm::vec3& position,
                                                     const std::vector<CompoundBox>& boxes,
                                                     float mass, float friction, float restitution) {
    if (boxes.empty() || mass <= 0.0f) return BodyHandle{};
    for (const CompoundBox& box : boxes) {
        if (box.halfExtents.x <= 0.0f || box.halfExtents.y <= 0.0f || box.halfExtents.z <= 0.0f) {
            return BodyHandle{};
        }
    }
    return m_impl->AddBody(Shape::Compound(boxes), position,
                            glm::quat(1.0f, 0.0f, 0.0f, 0.0f), true,
                            mass, friction, restitution);
}

void PhysicsWorld::DestroyBody(BodyHandle handle) {
    Impl::Body* body = m_impl->Get(handle);
    if (body) body->alive = false;
}

void PhysicsWorld::ApplyLinearAcceleration(BodyHandle handle, const glm::vec3& acceleration,
                                            float fixedDeltaTime) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || body->rigidBody.IsStatic()) return;
    body->rigidBody.linearVelocity += acceleration * fixedDeltaTime;
}

void PhysicsWorld::ApplyForce(BodyHandle handle, const glm::vec3& force) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || body->rigidBody.IsStatic()) return;
    body->rigidBody.ApplyForce(force);
}

void PhysicsWorld::ApplyTorque(BodyHandle handle, const glm::vec3& torque) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || body->rigidBody.IsStatic()) return;
    body->rigidBody.ApplyTorque(torque);
}

bool PhysicsWorld::IsDynamicBody(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body && body->isDynamic;
}

float PhysicsWorld::GetMass(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body && body->isDynamic && body->rigidBody.inverseMass > 0.0f
               ? 1.0f / body->rigidBody.inverseMass
               : 0.0f;
}

void PhysicsWorld::ApplyLinearImpulse(BodyHandle handle, const glm::vec3& impulse) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->isDynamic) return;
    body->rigidBody.ApplyLinearImpulse(impulse);
}

void PhysicsWorld::ApplyImpulseAtPoint(BodyHandle handle, const glm::vec3& impulse,
                                       const glm::vec3& worldPoint) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->isDynamic) return;
    body->rigidBody.ApplyImpulseAtPoint(impulse, worldPoint);
}

glm::vec3 PhysicsWorld::GetLinearVelocity(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body ? body->rigidBody.linearVelocity : glm::vec3(0.0f);
}

void PhysicsWorld::SetLinearVelocity(BodyHandle handle, const glm::vec3& velocity) {
    Impl::Body* body = m_impl->Get(handle);
    if (body) body->rigidBody.linearVelocity = velocity;
}

glm::vec3 PhysicsWorld::GetAngularVelocity(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body ? body->rigidBody.angularVelocity : glm::vec3(0.0f);
}

glm::mat3 PhysicsWorld::GetInertiaWorld(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->isDynamic) return glm::mat3(0.0f);
    return glm::inverse(body->rigidBody.InverseInertiaWorld());
}

float PhysicsWorld::GetBodySupportDistance(BodyHandle handle, const glm::vec3& worldDirection) const {
    const Impl::Body* body = m_impl->Get(handle);
    const float directionLength = glm::length(worldDirection);
    if (!body || directionLength < 1.0e-6f) return 0.0f;

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
        }
        maximum = std::max(maximum,
                            glm::dot(primitive.body.position - body->rigidBody.position,
                                     direction) + extent);
    }
    return maximum;
}

float PhysicsWorld::GetPlayerShapeMaxSupportDistance() const {
    if (!m_impl->hasPlayerShape) return 0.0f;
    return m_impl->playerShape.radius + m_impl->playerShape.halfHeight;
}

void PhysicsWorld::SetAngularVelocity(BodyHandle handle, const glm::vec3& angularVelocity) {
    Impl::Body* body = m_impl->Get(handle);
    if (body) body->rigidBody.angularVelocity = angularVelocity;
}

void PhysicsWorld::Step(float fixedDeltaTime) {
    // 1) Integrate every dynamic body's velocity/angular velocity from its
    // accumulated force/torque (Milestone 12 — see ApplyForce/ApplyTorque
    // above), then its position/orientation from the resulting velocity.
    // Gravity has already been folded directly into velocity by the
    // caller's own ApplyLinearAcceleration call this step (the same
    // "Judas samples gravity, hands it to physics" ordering every consumer
    // already uses) -- IntegrateRigidBody's own force-driven acceleration
    // composes with that additively, not instead of it: both are already
    // sitting in linearVelocity/forceAccumulator respectively by the time
    // this runs.
    //
    // This is the SAME free function (src/RigidBody.h/.cpp) the standalone
    // physics/collision test suites have exercised directly against a bare
    // RigidBody since Milestone 7-Final; through Milestone 11 the live
    // simulation never actually called it, reimplementing just the
    // position/orientation half of it inline instead, because nothing yet
    // used the force/torque accumulator on a live body (every consumer
    // either called ApplyLinearAcceleration directly, or, for the M8-M11
    // flying primitive specifically, overwrote velocity/angular velocity
    // outright via Set*Velocity). With a real force/torque-driven control
    // path now existing, calling the real integrator here closes that gap
    // instead of adding a second, competing one — see docs/ARCHITECTURE.md,
    // "Milestone 12." Behaviorally unchanged for every body that never has
    // ApplyForce/ApplyTorque called on it (accumulator stays exactly
    // zero, contributing zero to velocity, identical to before this
    // milestone) — the position/orientation math itself is byte-identical
    // to what was inlined here previously.
    for (Impl::Body& body : m_impl->bodies) {
        if (!body.alive || !body.isDynamic) continue;
        // PlayerController runs after Step. Keep the starting pose so an
        // airborne player sweep can compare both trajectories at matching
        // fractions through this same fixed interval.
        body.previousPosition = body.rigidBody.position;
        body.previousOrientation = body.rigidBody.orientation;
        IntegrateRigidBody(body.rigidBody, fixedDeltaTime);
    }

    // 2) Broadphase (brute-force all pairs -- see the note above) +
    // narrowphase + contact resolution, run for a few solver iterations so
    // resting/stacked contacts converge within one fixed step rather than
    // visibly settling over several. Static-static pairs (e.g. a planet
    // against the plank) are skipped outright: neither side can move, so
    // there is nothing to resolve.
    const std::size_t bodyCount = m_impl->bodies.size();
    for (int iteration = 0; iteration < kSolverIterations; ++iteration) {
        for (std::size_t i = 0; i < bodyCount; ++i) {
            Impl::Body& a = m_impl->bodies[i];
            if (!a.alive) continue;
            for (std::size_t j = i + 1; j < bodyCount; ++j) {
                Impl::Body& b = m_impl->bodies[j];
                if (!b.alive) continue;
                if (a.rigidBody.IsStatic() && b.rigidBody.IsStatic()) continue;

                const float friction = std::sqrt(std::max(a.friction, 0.0f) * std::max(b.friction, 0.0f));
                const float restitution = std::max(a.restitution, b.restitution);
                for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
                    for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                        // Contact correction may move a parent, so recalculate
                        // each child's world pose before testing the next pair.
                        const PrimitivePose childA = PrimitiveAt(a.shape, a.rigidBody, partA);
                        const PrimitivePose childB = PrimitiveAt(b.shape, b.rigidBody, partB);
                        const ContactManifold manifold = ComputeContacts(
                            childA.shape, childA.body, childB.shape, childB.body);
                        for (int p = 0; p < manifold.count; ++p) {
                            if (!manifold.points[p].hit) continue;
                            ResolveContact(a.rigidBody, b.rigidBody, manifold.points[p],
                                           friction, restitution);
                        }
                    }
                }
            }
        }
    }
}

BodyTransform PhysicsWorld::GetTransform(BodyHandle handle) const {
    BodyTransform result;
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return result;
    result.position = body->rigidBody.position;
    result.rotation = body->rigidBody.orientation;
    return result;
}

BodyTransform PhysicsWorld::GetPreviousTransform(BodyHandle handle) const {
    BodyTransform result;
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return result;
    if (body->isDynamic) {
        result.position = body->previousPosition;
        result.rotation = body->previousOrientation;
    } else {
        result.position = body->rigidBody.position;
        result.rotation = body->rigidBody.orientation;
    }
    return result;
}

std::vector<BodyBox> PhysicsWorld::GetBodyBoxes(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return {};
    return BoxesAt(body->shape, body->rigidBody.position, body->rigidBody.orientation);
}

std::vector<BodyBox> PhysicsWorld::GetPreviousBodyBoxes(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return {};
    return BoxesAt(body->shape,
                   body->isDynamic ? body->previousPosition : body->rigidBody.position,
                   body->isDynamic ? body->previousOrientation : body->rigidBody.orientation);
}

void PhysicsWorld::ResetBody(BodyHandle handle, const glm::vec3& position,
                              const glm::quat& rotation) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body) return;
    body->rigidBody.position = position;
    body->rigidBody.orientation = rotation;
    body->previousPosition = position;
    body->previousOrientation = rotation;
    body->rigidBody.linearVelocity = glm::vec3(0.0f);
    body->rigidBody.angularVelocity = glm::vec3(0.0f);
    body->rigidBody.ClearAccumulators();
}

bool PhysicsWorld::CreatePlayerShape(float radius, float halfHeight) {
    m_impl->playerShape = Shape::Capsule(radius, halfHeight);
    m_impl->hasPlayerShape = true;
    return true;
}

void PhysicsWorld::DestroyPlayerShape() { m_impl->hasPlayerShape = false; }

ShapeSweepHit PhysicsWorld::SweepPlayerShape(const glm::vec3& fromCenter, const glm::quat& rotation,
                                              const glm::vec3& displacement,
                                              bool interpolateDynamicBodyMotion,
                                              float bodyMotionStart,
                                              float bodyMotionEnd) const {
    ShapeSweepHit result;
    if (!m_impl->hasPlayerShape) return result;

    const float displacementLength = glm::length(displacement);
    if (displacementLength < 1.0e-6f) return result;

    const glm::vec3 localSegA(0.0f, -m_impl->playerShape.halfHeight, 0.0f);
    const glm::vec3 localSegB(0.0f, m_impl->playerShape.halfHeight, 0.0f);
    const float capsuleRadius = m_impl->playerShape.radius;

    auto worldSegmentAt = [&](const glm::vec3& center) {
        return std::make_pair(center + rotation * localSegA, center + rotation * localSegB);
    };
    auto evaluateAt = [&](float t) {
        const glm::vec3 center = fromCenter + displacement * t;
        const auto [segA, segB] = worldSegmentAt(center);
        const float bodyMotionAlpha = glm::mix(bodyMotionStart, bodyMotionEnd, t);
        return m_impl->ClosestBodyToCapsule(segA, segB, capsuleRadius, bodyMotionAlpha,
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
        result.normal = startResult.normal;
        result.hitBody.id = static_cast<unsigned int>(startResult.bodyIndex);
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
    float previousT = 0.0f;
    for (int step = 1; step <= kSubsteps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(kSubsteps);
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
            result.hitBody.id = static_cast<unsigned int>(refined.bodyIndex);
            return result;
        }
        previousT = t;
    }

    return result;  // no hit across the entire displacement
}
