#include "PhysicsWorld.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <thread>

// The Jolt headers don't include Jolt.h themselves — it must come first.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

// Every Jolt-specific detail (layers, filters, the job system, the factory)
// lives in this file only. Nothing here is visible through PhysicsWorld.h.
namespace {

using namespace JPH;

void JoltTraceImpl(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    std::fprintf(stderr, "[Jolt] %s\n", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
bool JoltAssertFailedImpl(const char* expression, const char* message, const char* file,
                           JPH::uint line) {
    std::fprintf(stderr, "%s:%u: (%s) %s\n", file, line, expression, message ? message : "");
    return true;  // trigger a breakpoint
}
#endif

// Two object layers is all this milestone needs: one static sphere/boxes,
// and (for any future dynamic body) a "moving" layer. The player is not a
// body at all, so it doesn't occupy either — see SweepPlayerShape.
namespace Layers {
constexpr ObjectLayer kNonMoving = 0;
constexpr ObjectLayer kMoving = 1;
constexpr unsigned int kNumLayers = 2;
}  // namespace Layers

namespace BroadPhaseLayers {
constexpr BroadPhaseLayer kNonMoving(0);
constexpr BroadPhaseLayer kMoving(1);
constexpr unsigned int kNumLayers = 2;
}  // namespace BroadPhaseLayers

class BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::kNonMoving] = BroadPhaseLayers::kNonMoving;
        mObjectToBroadPhase[Layers::kMoving] = BroadPhaseLayers::kMoving;
    }

    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::kNumLayers; }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        return mObjectToBroadPhase[layer];
    }

    const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
        switch ((BroadPhaseLayer::Type)layer) {
            case (BroadPhaseLayer::Type)BroadPhaseLayers::kNonMoving:
                return "NON_MOVING";
            case (BroadPhaseLayer::Type)BroadPhaseLayers::kMoving:
                return "MOVING";
            default:
                return "INVALID";
        }
    }

private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::kNumLayers];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer layer1, BroadPhaseLayer layer2) const override {
        switch (layer1) {
            case Layers::kNonMoving:
                return layer2 == BroadPhaseLayers::kMoving;
            case Layers::kMoving:
                return true;
            default:
                return false;
        }
    }
};

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer object1, ObjectLayer object2) const override {
        switch (object1) {
            case Layers::kNonMoving:
                return object2 == Layers::kMoving;
            case Layers::kMoving:
                return true;
            default:
                return false;
        }
    }
};

// Small values, deliberately: this milestone's active demo has exactly one
// static body (the sphere). A real scene would raise these, not architect
// around them being large.
constexpr unsigned int kMaxBodies = 128;
constexpr unsigned int kNumBodyMutexes = 0;  // 0 = Jolt picks a sensible default
constexpr unsigned int kMaxBodyPairs = 128;
constexpr unsigned int kMaxContactConstraints = 128;
constexpr int kCollisionStepsPerUpdate = 1;

JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }

JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }

glm::vec3 ToGlm(JPH::Vec3Arg v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }

glm::quat ToGlm(JPH::QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

BodyID ToJoltId(BodyHandle handle) { return BodyID(handle.id); }

BodyHandle ToHandle(BodyID id) { return BodyHandle{id.GetIndexAndSequenceNumber()}; }

// Keeps only the closest hit from a NarrowPhaseQuery::CastShape call. This
// is the entire "collision collector" this engine needs — not a generic
// collector framework, just the one policy (closest wins) the player
// controller's move-and-slide and ground-probe queries both want.
class ClosestHitCastShapeCollector final
    : public JPH::CollisionCollector<JPH::ShapeCastResult, JPH::CollisionCollectorTraitsCastShape> {
public:
    void AddHit(const JPH::ShapeCastResult& result) override {
        if (mHadHit && result.mFraction >= mHit.mFraction) return;
        mHit = result;
        mHadHit = true;
        UpdateEarlyOutFraction(std::max(result.mFraction, 0.0f));
    }

    bool mHadHit = false;
    JPH::ShapeCastResult mHit{};
};

}  // namespace

struct PhysicsWorld::Impl {
    BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;

    JPH::TempAllocatorImpl tempAllocator{10 * 1024 * 1024};
    JPH::JobSystemThreadPool jobSystem{
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        static_cast<int>(std::max(1u, std::thread::hardware_concurrency() - 1))};

    JPH::PhysicsSystem physicsSystem;

    // The player's collision shape. Never added to physicsSystem as a body
    // — it exists purely so SweepPlayerShape has something to cast. See
    // PhysicsWorld.h.
    JPH::RefConst<JPH::Shape> playerShape;
};

bool PhysicsWorld::Init() {
    JPH::RegisterDefaultAllocator();

    JPH::Trace = JoltTraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl;)

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_impl = new Impl();
    m_impl->physicsSystem.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
                                m_impl->broadPhaseLayerInterface,
                                m_impl->objectVsBroadPhaseLayerFilter,
                                m_impl->objectLayerPairFilter);

    // Judas owns gravity (see PhysicsWorld.h and docs/ARCHITECTURE.md,
    // "Ownership boundary"). Jolt's own global gravity — which defaults to
    // (0, -9.81, 0) applied automatically to every dynamic body — must
    // never be relied upon, so it is explicitly zeroed here. Gravity only
    // ever reaches a body through ApplyLinearAcceleration, and the player
    // (not a body at all) never touches Jolt gravity in any form.
    m_impl->physicsSystem.SetGravity(JPH::Vec3::sZero());

    return true;
}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
}

void PhysicsWorld::Shutdown() {
    delete m_impl;
    m_impl = nullptr;

    if (JPH::Factory::sInstance) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                          float friction, float restitution) {
    JPH::BodyCreationSettings settings(new JPH::BoxShape(ToJolt(halfExtents)), ToJolt(position),
                                        JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                        Layers::kNonMoving);
    settings.mFriction = friction;
    settings.mRestitution = restitution;

    JPH::BodyID id = m_impl->physicsSystem.GetBodyInterface().CreateAndAddBody(
        settings, JPH::EActivation::DontActivate);
    return ToHandle(id);
}

BodyHandle PhysicsWorld::CreateStaticSphere(const glm::vec3& position, float radius,
                                             float friction, float restitution) {
    JPH::BodyCreationSettings settings(new JPH::SphereShape(radius), ToJolt(position),
                                        JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                        Layers::kNonMoving);
    settings.mFriction = friction;
    settings.mRestitution = restitution;

    JPH::BodyID id = m_impl->physicsSystem.GetBodyInterface().CreateAndAddBody(
        settings, JPH::EActivation::DontActivate);
    return ToHandle(id);
}

BodyHandle PhysicsWorld::CreateDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                           float mass, float friction, float restitution) {
    JPH::BodyCreationSettings settings(new JPH::BoxShape(ToJolt(halfExtents)), ToJolt(position),
                                        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                        Layers::kMoving);
    settings.mFriction = friction;
    settings.mRestitution = restitution;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = mass;

    JPH::BodyID id = m_impl->physicsSystem.GetBodyInterface().CreateAndAddBody(
        settings, JPH::EActivation::Activate);
    return ToHandle(id);
}

void PhysicsWorld::DestroyBody(BodyHandle handle) {
    if (!handle.IsValid()) return;
    JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
    JPH::BodyID id = ToJoltId(handle);
    bodyInterface.RemoveBody(id);
    bodyInterface.DestroyBody(id);
}

void PhysicsWorld::ApplyLinearAcceleration(BodyHandle handle, const glm::vec3& acceleration,
                                            float fixedDeltaTime) {
    if (!handle.IsValid()) return;
    m_impl->physicsSystem.GetBodyInterface().AddLinearVelocity(
        ToJoltId(handle), ToJolt(acceleration) * fixedDeltaTime);
}

void PhysicsWorld::Step(float fixedDeltaTime) {
    m_impl->physicsSystem.Update(fixedDeltaTime, kCollisionStepsPerUpdate, &m_impl->tempAllocator,
                                  &m_impl->jobSystem);
}

BodyTransform PhysicsWorld::GetTransform(BodyHandle handle) const {
    BodyTransform result;
    if (!handle.IsValid()) return result;

    JPH::RVec3 position;
    JPH::Quat rotation;
    m_impl->physicsSystem.GetBodyInterface().GetPositionAndRotation(ToJoltId(handle), position,
                                                                      rotation);
    result.position = ToGlm(JPH::Vec3(position));
    result.rotation = ToGlm(rotation);
    return result;
}

void PhysicsWorld::ResetBody(BodyHandle handle, const glm::vec3& position,
                              const glm::quat& rotation) {
    if (!handle.IsValid()) return;
    JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
    JPH::BodyID id = ToJoltId(handle);

    bodyInterface.SetPositionAndRotation(id, ToJolt(position), ToJolt(rotation),
                                          JPH::EActivation::Activate);
    bodyInterface.SetLinearVelocity(id, JPH::Vec3::sZero());
    bodyInterface.SetAngularVelocity(id, JPH::Vec3::sZero());
}

bool PhysicsWorld::CreatePlayerShape(float radius, float halfHeight) {
    m_impl->playerShape = new JPH::CapsuleShape(halfHeight, radius);
    return m_impl->playerShape != nullptr;
}

void PhysicsWorld::DestroyPlayerShape() {
    m_impl->playerShape = nullptr;
}

ShapeSweepHit PhysicsWorld::SweepPlayerShape(const glm::vec3& fromCenter, const glm::quat& rotation,
                                              const glm::vec3& displacement) const {
    ShapeSweepHit result;
    if (!m_impl->playerShape) return result;

    const float length = glm::length(displacement);
    if (length < 1.0e-6f) return result;

    const JPH::RMat44 startTransform =
        JPH::RMat44::sRotationTranslation(ToJolt(rotation), ToJolt(fromCenter));
    const JPH::RShapeCast shapeCast = JPH::RShapeCast::sFromWorldTransform(
        m_impl->playerShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), startTransform,
        ToJolt(displacement));

    JPH::ShapeCastSettings settings;

    ClosestHitCastShapeCollector collector;
    m_impl->physicsSystem.GetNarrowPhaseQuery().CastShape(
        shapeCast, settings, ToJolt(fromCenter), collector,
        m_impl->physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::kMoving),
        m_impl->physicsSystem.GetDefaultLayerFilter(Layers::kMoving));

    if (collector.mHadHit) {
        result.hit = true;
        result.distance = collector.mHit.mFraction * length;

        glm::vec3 normal = ToGlm(collector.mHit.mPenetrationAxis);
        if (glm::length(normal) > 1.0e-6f) {
            normal = glm::normalize(normal);
            // Jolt's convention for this field isn't guaranteed to point
            // any particular way relative to the cast direction — enforce
            // "points back toward the caster" ourselves so callers get a
            // consistent, sane contact normal regardless.
            if (glm::dot(normal, displacement) > 0.0f) {
                normal = -normal;
            }
        } else {
            normal = -glm::normalize(displacement);
        }
        result.normal = normal;
    }
    return result;
}
