#include "PhysicsWorld.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "Broadphase.h"
#include "CollisionShapes.h"
#include "ContactSolver.h"
#include "Contacts.h"
#include "Narrowphase.h"
#include "RadialTerrain.h"
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

using Clock = std::chrono::steady_clock;
double MillisecondsBetween(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Milestone 32: how far a proxy's fat bound extends past its tight bound.
// A body that moves less than this within its fat box costs the tree
// nothing; one that escapes is reinserted. Static bodies use the same
// margin (only kinematically-driven static bodies such as doors move).
constexpr float kBroadphaseMargin = 0.1f;
// Tolerance added to the player sweep's query box.
constexpr float kSweepQueryEpsilon = 1.0e-3f;
// Warm starting: a new contact inherits last step's converged impulses from
// the same body pair/primitive pair when its anchor (in body A's frame) is
// within this distance of a cached one and the normals agree.
constexpr float kWarmStartAnchorDistance = 0.05f;
constexpr float kWarmStartNormalDot = 0.9f;

// Milestone 32: forward floating-point error bound on a contact's computed
// separation, used only to decide whether a tiny positive gap is real.
//
// The narrowphase evaluates s = n . (x_A - x_B) - (radii), where each
// surface point is x = p + R l (p the primitive's world position, l its
// local offset to the contact, rotated by the body's R). In IEEE-754 single
// precision with unit roundoff u = 2^-24 and gamma_k = k u / (1 - k u)
// (Higham, "Accuracy and Stability of Numerical Algorithms", 3.1):
//   rotating l: one 3-term dot product per component   gamma_3 |l|
//   adding p:                                          gamma_1 (|p| + |l|)
//   subtracting the two surface points:                gamma_1 |x_A - x_B|
//   projecting onto n: a 3-term dot product            gamma_3 |x_A - x_B|
// which accumulates, to first order, to
//   |fl(s) - s| <= gamma_8 (|p_A| + |l_A| + |p_B| + |l_B|),
// with l = contact point - p for each side (a sphere's radius is its |l|).
// A computed gap at or below this bound is indistinguishable from zero in
// the arithmetic that produced it.
float NumericalGapBound(const glm::vec3& positionA, const glm::vec3& positionB, const glm::vec3& contactPoint) {
    constexpr float u = 1.0f / 16777216.0f;  // 2^-24
    constexpr float gamma8 = 8.0f * u / (1.0f - 8.0f * u);
    return gamma8 * (glm::length(positionA) + glm::length(contactPoint - positionA) + glm::length(positionB) +
                     glm::length(contactPoint - positionB));
}

struct ContactKey {
    unsigned int slotA = 0, slotB = 0;
    unsigned int generationA = 0, generationB = 0;
    int partA = 0, partB = 0;
    bool operator<(const ContactKey& o) const {
        return std::tie(slotA, slotB, partA, partB) < std::tie(o.slotA, o.slotB, o.partA, o.partB);
    }
    bool SameBodies(const ContactKey& o) const {
        return slotA == o.slotA && slotB == o.slotB && partA == o.partA && partB == o.partB &&
               generationA == o.generationA && generationB == o.generationB;
    }
};
struct CachedContact {
    ContactKey key;
    glm::vec3 localAnchorA{0.0f};
    glm::vec3 normal{0.0f};
    float normalImpulse = 0.0f;
    glm::vec3 tangentImpulse{0.0f};
};

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
        // Milestone 29: a slot is reused after DestroyBody; the generation
        // in the handle's upper bits makes a handle from a previous
        // occupant of the same slot invalid instead of aliasing the new one.
        unsigned int generation = 0;
        // Milestone 32: this body's leaf in the broadphase tree.
        int proxy = DynamicAabbTree::kNull;
    };

    static constexpr unsigned int kSlotBits = 20;
    static constexpr unsigned int kSlotMask = (1u << kSlotBits) - 1u;

    std::vector<Body> bodies;
    // Slots of destroyed bodies awaiting reuse, and the sorted slots of
    // every live body — the loops below iterate this so a world whose
    // entities were unloaded pays for the bodies that exist, not for the
    // slots they once occupied.
    std::vector<unsigned int> freeSlots;
    std::vector<unsigned int> aliveSlots;
    std::vector<PhysicsWorld::DebugContact> lastStepContacts;

    // Milestone 32: broadphase, the per-step candidate list, the solver
    // (reused so its storage is not reallocated every step) and statistics.
    DynamicAabbTree tree;
    std::vector<std::pair<unsigned int, unsigned int>> candidatePairs;
    mutable std::vector<unsigned int> queryScratch;
    ContactSolver solver;
    // Milestone 32 warm-start state: last step's converged impulses, sorted
    // by key, and the keys/anchors of this step's constraints in solver order.
    std::vector<CachedContact> contactCache;
    std::vector<CachedContact> pendingCache;
    std::vector<char> cacheUsed;
    PhysicsWorld::StepStats stats;
    std::size_t reinsertionsSinceStep = 0;

    // The tight broadphase bound a body must stay inside. For a dynamic
    // body it covers both the previous and the current fixed-step pose —
    // player sweeps interpolate between the two — and, if the body turned
    // during the step, the orientation-independent bounding sphere at both
    // positions (an interpolated orientation can poke outside both
    // endpoint boxes; it can never leave the swept sphere).
    Aabb TightBound(const Body& body) const {
        const Aabb current = ShapeAabb(body.shape, body.rigidBody.position, body.rigidBody.orientation);
        if (!body.isDynamic) return current;
        Aabb bound = current.Union(ShapeAabb(body.shape, body.previousPosition, body.previousOrientation));
        const float turn = std::abs(glm::dot(body.previousOrientation, body.rigidBody.orientation));
        if (turn < 1.0f - 1.0e-7f) {
            const float r = ShapeBoundingRadius(body.shape);
            bound = bound.Union(Aabb{body.previousPosition - glm::vec3(r), body.previousPosition + glm::vec3(r)});
            bound = bound.Union(Aabb{body.rigidBody.position - glm::vec3(r), body.rigidBody.position + glm::vec3(r)});
        }
        return bound;
    }

    // Milestone 32: how far this body's surface can move within the current
    // step at its post-force velocities — the most any of its contacts can
    // close before the next detection.
    float StepReach(const Body& body, float fixedDeltaTime) const {
        return (glm::length(body.rigidBody.linearVelocity) +
                glm::length(body.rigidBody.angularVelocity) * ShapeBoundingRadius(body.shape)) *
               fixedDeltaTime;
    }

    // Before candidate generation: a dynamic proxy's fat bound covers the
    // body grown by its reach, so every pair that can come into contact
    // within this step is a candidate (speculative contacts).
    void CoverStepReach(Body& body, float fixedDeltaTime) {
        const Aabb reach = TightBound(body).Expanded(StepReach(body, fixedDeltaTime));
        if (tree.FatAabb(body.proxy).Contains(reach)) return;
        tree.MoveProxy(body.proxy, reach.Expanded(kBroadphaseMargin));
        ++reinsertionsSinceStep;
    }

    void RefreshProxy(Body& body) {
        const Aabb tight = TightBound(body);
        if (tree.FatAabb(body.proxy).Contains(tight)) return;
        tree.MoveProxy(body.proxy, tight.Expanded(kBroadphaseMargin));
        ++reinsertionsSinceStep;
    }

    // Live slots whose fat bounds overlap `box`, ascending.
    const std::vector<unsigned int>& QuerySlots(const Aabb& box) const {
        queryScratch.clear();
        tree.Query(box, [&](int proxy) { queryScratch.push_back(tree.UserData(proxy)); });
        std::sort(queryScratch.begin(), queryScratch.end());
        return queryScratch;
    }

    // Candidate pairs (lo, hi) for every pair with at least one movable
    // body, in ascending lexicographic slot order — the same order the
    // pre-M32 all-pairs loop visited them in, restricted to candidates.
    void GenerateCandidatePairs() {
        candidatePairs.clear();
        for (const unsigned int slot : aliveSlots) {
            const Body& a = bodies[slot];
            if (a.rigidBody.IsStatic()) continue;
            tree.Query(tree.FatAabb(a.proxy), [&](int proxy) {
                const unsigned int other = tree.UserData(proxy);
                if (other == slot) return;
                const Body& b = bodies[other];
                // A pair of two movable bodies is found by both queries;
                // keep it from the lower slot's query only.
                if (!b.rigidBody.IsStatic() && other < slot) return;
                candidatePairs.emplace_back(std::min(slot, other), std::max(slot, other));
            });
        }
        std::sort(candidatePairs.begin(), candidatePairs.end());
    }

    BodyHandle MakeHandle(unsigned int slot) const {
        BodyHandle handle;
        handle.id = slot | (bodies[slot].generation << kSlotBits);
        return handle;
    }

    bool hasPlayerShape = false;
    Shape playerShape;

    // A member of Impl (rather than a free function taking a body list)
    // specifically so it can name `Body` without exposing this private
    // nested type outside PhysicsWorld.cpp.
    // Milestone 32: evaluates only `candidates` (the broadphase's answer for
    // the whole sweep). Any body not among them has a positive distance, so
    // it could never be the reported closest body of a hit.
    ClosestBodyResult ClosestBodyToCapsule(const std::vector<unsigned int>& candidates,
                                            const glm::vec3& segA, const glm::vec3& segB,
                                            float capsuleRadius,
                                            float bodyMotionAlpha,
                                            bool interpolateDynamicBodyMotion) const {
        ClosestBodyResult result;
        for (const unsigned int i : candidates) {
            const Body& body = bodies[i];
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
            if (body.shape.type == ShapeType::Terrain) {
                if (!body.shape.terrain) continue;
                const float segmentLength = glm::length(segB - segA);
                const float minimumEndRadius = std::min(glm::length(segA - bodyPosition),
                                                        glm::length(segB - bodyPosition));
                if (minimumEndRadius - segmentLength >
                    body.shape.terrain->BoundRadius() + capsuleRadius) continue;
                // Sampling the whole capsule core, rather than just its
                // lower endpoint, also handles arbitrary capsule attitude
                // and terrain slopes without a universal up axis.
                constexpr int kSegmentSamples = 9;
                for (int sampleIndex = 0; sampleIndex < kSegmentSamples; ++sampleIndex) {
                    const float t = static_cast<float>(sampleIndex) /
                                    static_cast<float>(kSegmentSamples - 1);
                    const TerrainSample sample = SampleTerrainAtWorld(
                        *body.shape.terrain, sampledBody, glm::mix(segA, segB, t));
                    const float distance = sample.signedDistance - capsuleRadius;
                    if (distance < result.distance) {
                        result.distance = distance;
                        result.normal = sample.outwardNormal;
                        result.bodyIndex = static_cast<int>(i);
                    }
                }
                continue;
            }
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
        if (!handle.IsValid()) return nullptr;
        const unsigned int slot = handle.id & kSlotMask;
        if (slot >= bodies.size() || !bodies[slot].alive ||
            bodies[slot].generation != (handle.id >> kSlotBits)) {
            return nullptr;
        }
        return &bodies[slot];
    }
    const Body* Get(BodyHandle handle) const {
        if (!handle.IsValid()) return nullptr;
        const unsigned int slot = handle.id & kSlotMask;
        if (slot >= bodies.size() || !bodies[slot].alive ||
            bodies[slot].generation != (handle.id >> kSlotBits)) {
            return nullptr;
        }
        return &bodies[slot];
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
        bodies[slot].proxy = tree.CreateProxy(TightBound(bodies[slot]).Expanded(kBroadphaseMargin), slot);
        return MakeHandle(slot);
    }

    void Remove(BodyHandle handle) {
        Body* body = Get(handle);
        if (!body) return;
        const unsigned int slot = handle.id & kSlotMask;
        body->alive = false;
        tree.DestroyProxy(body->proxy);
        body->proxy = DynamicAabbTree::kNull;
        freeSlots.push_back(slot);
        const auto it = std::lower_bound(aliveSlots.begin(), aliveSlots.end(), slot);
        if (it != aliveSlots.end() && *it == slot) aliveSlots.erase(it);
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

BodyHandle PhysicsWorld::CreateStaticTerrain(const glm::vec3& position,
                                              const glm::quat& rotation,
                                              std::shared_ptr<const RadialTerrain> terrain,
                                              float friction, float restitution) {
    if (!terrain) return BodyHandle{};
    return m_impl->AddBody(Shape::Terrain(std::move(terrain)), position, rotation,
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
    m_impl->Remove(handle);
}

std::size_t PhysicsWorld::AliveBodyCount() const {
    return m_impl->aliveSlots.size();
}

const std::vector<PhysicsWorld::DebugContact>& PhysicsWorld::LastStepContacts() const {
    return m_impl->lastStepContacts;
}

std::size_t PhysicsWorld::DynamicBodyCount() const {
    std::size_t count = 0;
    for (const unsigned int slot : m_impl->aliveSlots) {
        if (m_impl->bodies[slot].isDynamic) ++count;
    }
    return count;
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
        } else if (primitive.shape.type == ShapeType::Terrain && primitive.shape.terrain) {
            // Exact directional support of an arbitrary radial height
            // function would require global optimization. Its immutable
            // radius bound is conservative for this separation query.
            extent = primitive.shape.terrain->BoundRadius();
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
    Impl& w = *m_impl;
    const Clock::time_point stepStart = Clock::now();
    w.stats = StepStats{};

    // 1) Velocity from this step's force/torque accumulators (M12). Gravity
    // is already in linearVelocity via the caller's ApplyLinearAcceleration.
    // The start-of-step pose is kept for player sweeps and presentation.
    std::size_t movable = 0;
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body& body = w.bodies[slot];
        if (!body.rigidBody.IsStatic()) ++movable;
        if (!body.isDynamic) continue;
        body.previousPosition = body.rigidBody.position;
        body.previousOrientation = body.rigidBody.orientation;
        IntegrateRigidBodyVelocity(body.rigidBody, fixedDeltaTime);
        w.CoverStepReach(body, fixedDeltaTime);
    }
    w.stats.bodies = w.aliveSlots.size();
    w.stats.dynamicBodies = movable;
    w.stats.possiblePairs = movable * (movable - (movable > 0 ? 1 : 0)) / 2 +
                            movable * (w.aliveSlots.size() - movable);

    // 2) Broadphase: every fat bound contains its body's current pose
    // (refreshed at the end of the previous Step and by ResetBody), so every
    // touching pair is among the candidates. Static-static pairs never are.
    const Clock::time_point broadphaseStart = Clock::now();
    w.GenerateCandidatePairs();
    w.stats.candidatePairs = w.candidatePairs.size();
    const Clock::time_point narrowphaseStart = Clock::now();

    // 3) Narrowphase at the current poses — the sole authority on contact.
    w.solver.Clear();
    w.lastStepContacts.clear();
    w.pendingCache.clear();
    w.cacheUsed.assign(w.contactCache.size(), 0);
    for (const auto& [slotA, slotB] : w.candidatePairs) {
        Impl::Body& a = w.bodies[slotA];
        Impl::Body& b = w.bodies[slotB];
        const float friction = std::sqrt(std::max(a.friction, 0.0f) * std::max(b.friction, 0.0f));
        const float restitution = std::max(a.restitution, b.restitution);
        // Speculative margin: the distance this pair's surfaces can close
        // within the step at their post-force velocities (relative linear
        // motion plus each body's rotation at its bounding radius). Any
        // contact within it is generated now, before it can penetrate; at
        // rest on a support that is g*dt^2 (2.7 mm at 60 Hz, 9.81 m/s^2).
        const float margin =
            (glm::length(a.rigidBody.linearVelocity - b.rigidBody.linearVelocity) +
             glm::length(a.rigidBody.angularVelocity) * ShapeBoundingRadius(a.shape) +
             glm::length(b.rigidBody.angularVelocity) * ShapeBoundingRadius(b.shape)) *
            fixedDeltaTime;
        int pairPoints = 0;
        const glm::quat inverseA = glm::conjugate(a.rigidBody.orientation);
        for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
            const PrimitivePose childA = PrimitiveAt(a.shape, a.rigidBody, partA);
            for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                const PrimitivePose childB = PrimitiveAt(b.shape, b.rigidBody, partB);
                const ContactManifold manifold =
                    ComputeContacts(childA.shape, childA.body, childB.shape, childB.body, margin);
                const ContactKey key{slotA, slotB, a.generation, b.generation, partA, partB};
                const auto range = std::equal_range(
                    w.contactCache.begin(), w.contactCache.end(), CachedContact{key, {}, {}, 0.0f, {}},
                    [](const CachedContact& x, const CachedContact& y) { return x.key < y.key; });
                for (int p = 0; p < manifold.count; ++p) {
                    Contact contact = manifold.points[p];
                    if (!contact.hit) continue;
                    // A speculative gap no larger than the separation's own
                    // rounding error is not a gap: the pair is touching.
                    // Genuine gaps above the bound are left as they are.
                    if (contact.penetration < 0.0f &&
                        -contact.penetration <= NumericalGapBound(childA.body.position, childB.body.position,
                                                                  contact.point)) {
                        contact.penetration = 0.0f;
                    }
                    // Warm start from the nearest unused cached point of the
                    // same bodies/primitives (same generations: a reused slot
                    // never inherits a previous occupant's impulses).
                    const glm::vec3 anchor = inverseA * (contact.point - a.rigidBody.position);
                    float warmNormal = 0.0f;
                    glm::vec3 warmTangent(0.0f);
                    std::ptrdiff_t best = -1;
                    float bestDistance = kWarmStartAnchorDistance;
                    for (auto it = range.first; it != range.second; ++it) {
                        const std::ptrdiff_t index = it - w.contactCache.begin();
                        if (w.cacheUsed[static_cast<std::size_t>(index)] || !it->key.SameBodies(key) ||
                            glm::dot(it->normal, contact.normal) < kWarmStartNormalDot) continue;
                        const float distance = glm::distance(it->localAnchorA, anchor);
                        if (distance < bestDistance) {
                            bestDistance = distance;
                            best = index;
                        }
                    }
                    if (best >= 0) {
                        w.cacheUsed[static_cast<std::size_t>(best)] = 1;
                        warmNormal = w.contactCache[static_cast<std::size_t>(best)].normalImpulse;
                        warmTangent = w.contactCache[static_cast<std::size_t>(best)].tangentImpulse;
                    }
                    // Impulses act on the parent bodies (compound children
                    // share one).
                    const std::size_t before = w.solver.Constraints().size();
                    w.solver.AddContact(a.rigidBody, b.rigidBody, contact, friction, restitution,
                                        warmNormal, warmTangent);
                    if (w.solver.Constraints().size() > before) {
                        w.pendingCache.push_back(CachedContact{key, anchor, contact.normal, 0.0f, glm::vec3(0.0f)});
                    }
                    w.lastStepContacts.push_back({contact.point, contact.normal, contact.penetration});
                    ++pairPoints;
                }
            }
        }
        if (pairPoints > 0) ++w.stats.collidingPairs;
    }
    w.stats.contactPoints = w.lastStepContacts.size();
    const Clock::time_point solverStart = Clock::now();

    // 4) Accumulated-impulse velocity solve (src/ContactSolver.h), then
    // positions from the solved velocities, then direct penetration removal.
    w.solver.Prepare(fixedDeltaTime);
    w.solver.SolveVelocities();
    // Remember this step's converged impulses for the next step's warm start.
    const std::vector<ContactConstraint>& solved = w.solver.Constraints();
    for (std::size_t i = 0; i < solved.size() && i < w.pendingCache.size(); ++i) {
        w.pendingCache[i].normalImpulse = solved[i].normalImpulse;
        w.pendingCache[i].tangentImpulse = solved[i].tangentImpulse;
    }
    std::stable_sort(w.pendingCache.begin(), w.pendingCache.end(),
                     [](const CachedContact& x, const CachedContact& y) { return x.key < y.key; });
    std::swap(w.contactCache, w.pendingCache);
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body& body = w.bodies[slot];
        if (body.isDynamic) IntegrateRigidBodyPosition(body.rigidBody, fixedDeltaTime);
    }
    w.solver.SolvePositions();
    const Clock::time_point solverEnd = Clock::now();

    // 5) Keep every dynamic proxy's fat bound around its previous-to-current
    // motion for the player's sweeps and the next step's candidates.
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body& body = w.bodies[slot];
        if (body.isDynamic) w.RefreshProxy(body);
    }
    w.stats.proxyReinsertions = w.reinsertionsSinceStep;
    w.reinsertionsSinceStep = 0;
    w.stats.treeHeight = w.tree.Height();
    const Clock::time_point stepEnd = Clock::now();
    w.stats.broadphaseMilliseconds = MillisecondsBetween(broadphaseStart, narrowphaseStart) +
                                     MillisecondsBetween(solverEnd, stepEnd);
    w.stats.narrowphaseMilliseconds = MillisecondsBetween(narrowphaseStart, solverStart);
    w.stats.solverMilliseconds = MillisecondsBetween(solverStart, solverEnd);
    w.stats.totalMilliseconds = MillisecondsBetween(stepStart, stepEnd);
}

const PhysicsWorld::StepStats& PhysicsWorld::LastStepStats() const { return m_impl->stats; }

std::vector<BodyHandle> PhysicsWorld::QueryBodiesInAabb(const glm::vec3& min, const glm::vec3& max) const {
    std::vector<BodyHandle> result;
    for (const unsigned int slot : m_impl->QuerySlots(Aabb{glm::min(min, max), glm::max(min, max)})) {
        result.push_back(m_impl->MakeHandle(slot));
    }
    return result;
}

bool PhysicsWorld::GetBodyBroadphaseBounds(BodyHandle handle, glm::vec3& outMin, glm::vec3& outMax) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return false;
    const Aabb& fat = m_impl->tree.FatAabb(body->proxy);
    outMin = fat.min;
    outMax = fat.max;
    return true;
}

std::vector<PhysicsWorld::CollidingPair> PhysicsWorld::FindCollidingPairs() const {
    Impl& w = *m_impl;
    // Same candidate generation and narrowphase as Step, without solving.
    w.GenerateCandidatePairs();
    std::vector<CollidingPair> result;
    for (const auto& [slotA, slotB] : w.candidatePairs) {
        const Impl::Body& a = w.bodies[slotA];
        const Impl::Body& b = w.bodies[slotB];
        int points = 0;
        for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
            const PrimitivePose childA = PrimitiveAt(a.shape, a.rigidBody, partA);
            for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                const PrimitivePose childB = PrimitiveAt(b.shape, b.rigidBody, partB);
                const ContactManifold manifold =
                    ComputeContacts(childA.shape, childA.body, childB.shape, childB.body);
                for (int p = 0; p < manifold.count; ++p) {
                    if (manifold.points[p].hit) ++points;
                }
            }
        }
        if (points > 0) result.push_back({w.MakeHandle(slotA), w.MakeHandle(slotB), points});
    }
    return result;
}

std::vector<BodyHandle> PhysicsWorld::AliveBodies() const {
    std::vector<BodyHandle> result;
    result.reserve(m_impl->aliveSlots.size());
    for (const unsigned int slot : m_impl->aliveSlots) result.push_back(m_impl->MakeHandle(slot));
    return result;
}

bool PhysicsWorld::GetBodyShape(BodyHandle handle, Shape& outShape, BodyTransform& outPose) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return false;
    outShape = body->shape;
    outPose.position = body->rigidBody.position;
    outPose.rotation = body->rigidBody.orientation;
    return true;
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
    m_impl->RefreshProxy(*body);
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
    const std::vector<unsigned int> candidates = m_impl->QuerySlots(sweptBound);
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
        result.normal = startResult.normal;
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
            result.hitBody = m_impl->MakeHandle(static_cast<unsigned int>(refined.bodyIndex));
            return result;
        }
        previousT = t;
    }

    return result;  // no hit across the entire displacement
}
