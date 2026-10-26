#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

namespace {
// Contexts retain their existing binary format. Exact typed comparison admits
// IEEE signed zeros as the same physical value; no epsilon or state repair is
// involved. Strings, counts and all nonzero numeric fields still compare exactly.
class ContinuationFields {
public:
    explicit ContinuationFields(SaveArchive *comparison) : m_comparison(comparison) {}

    template <class... T> void operator()(T &...values) { (Field(values), ...); }
    void Require(bool condition, const std::string &error) { output.Require(condition, error); }
    SaveArchive output;

private:
    template <class T> void Field(T value) {
        output(value);
        if (m_comparison) {
            T saved = value;
            (*m_comparison)(saved);
            m_comparison->Require(saved == value, "continuation physical context differs");
        }
    }
    SaveArchive *m_comparison;
};
} // namespace

auto PhysicsWorld::Impl::InvalidateRestoredContinuation(unsigned slot) -> void {
    // Live-world cache policy remains unchanged. Newly restored seeds may
    // only continue the exact reconstructed physical context.
    bool physicalContext = !bodies[slot].queryOnly;
    for (auto &joint : joints)
        if ((joint.state.settings.bodyA.id & kSlotMask) == slot ||
            (joint.state.settings.bodyB.IsValid() &&
             (joint.state.settings.bodyB.id & kSlotMask) == slot)) {
            physicalContext = true;
            if (joint.restoredContinuationDt > 0) {
                joint.warm.fill(0);
                joint.state.angularDrive.impulses = {0, 0, 0};
                joint.restoredContinuationDt = 0;
            }
        }
    // Independent query proxies cannot own completed physical contact rows.
    // Their ordinary pre-step refresh must not erase another articulation's
    // restored publication. A mechanically connected proxy still invalidates.
    if (physicalContext)
        restoredCompletedObservations = false;
}

auto PhysicsWorld::Impl::ContinuationBodyContext(const Body &body, SaveArchive *comparison) const -> std::string {
    ContinuationFields a(comparison);
    auto field = [&](auto value) { a(value); };
    field(body.rigidBody.position);
    field(body.rigidBody.orientation);
    field(body.rigidBody.linearVelocity);
    field(body.rigidBody.angularVelocity);
    field(body.rigidBody.inverseMass);
    field(body.rigidBody.inverseInertiaLocal);
    field(body.isDynamic);
    field(body.isKinematic);
    field(body.enabled);
    field(body.sensor);
    field(body.queryOnly);
    field(body.collisionLayer);
    field(body.collisionMask);
    field(body.friction);
    field(body.restitution);
    auto material = body.physicalMaterial;
    a(material);
    const auto &shape = body.shape;
    field(shape.type);
    field(shape.radius);
    field(shape.halfHeight);
    field(shape.halfExtents);
    field(shape.pivotOffset);
    auto assetId = shape.assetId;
    a(assetId);
    auto fingerprint = [&](const auto &asset) {
        std::string source = asset ? asset->sourceFingerprint : std::string{},
                    settings = asset ? asset->settingsFingerprint : std::string{};
        a(source, settings);
    };
    fingerprint(shape.asset);
    float terrainBound = shape.terrain ? shape.terrain->BoundRadius() : 0;
    a(terrainBound);
    uint32_t count = uint32_t(shape.boxes.size());
    a(count);
    a.Require(count <= 65536, "continuation compound-shape limit");
    for (const auto &child : shape.boxes) {
        field(child.type);
        field(child.localCenter);
        field(child.halfExtents);
        field(child.rotation);
        field(child.radius);
        field(child.key);
        auto id = child.assetId;
        a(id);
        fingerprint(child.asset);
    }
    return a.output.bytes;
}

auto PhysicsWorld::Impl::ContinuationJointContext(const JointRecord &joint, SaveArchive *comparison) const -> std::string {
    ContinuationFields a(comparison);
    auto s = joint.state.settings;
    // Handles are validated/remapped by the world owner; never archive slots.
    a(s.type, s.anchorA, s.anchorB, s.frameA, s.frameB, s.enabled, s.limits, s.motor, s.spring,
      s.lower, s.upper, s.speed, s.maxForce, s.rest, s.stiffness, s.damping,
      s.rotationalResistance);
    const auto *bodyA = Get(s.bodyA);
    const auto *bodyB = Get(s.bodyB);
    a.Require(bodyA && (!s.bodyB.IsValid() || bodyB), "stale joint continuation body");
    auto first = ContinuationBodyContext(*bodyA),
         second = bodyB ? ContinuationBodyContext(*bodyB) : std::string{};
    a.output(first, second);
    if (comparison) {
        std::string savedFirst, savedSecond;
        (*comparison)(savedFirst, savedSecond);
        SaveArchive firstFields(savedFirst);
        ContinuationBodyContext(*bodyA, &firstFields);
        firstFields.Finish();
        if (bodyB) {
            SaveArchive secondFields(savedSecond);
            ContinuationBodyContext(*bodyB, &secondFields);
            secondFields.Finish();
        } else {
            comparison->Require(savedSecond.empty(), "joint continuation world anchor differs");
        }
    }
    return a.output.bytes;
}

void PhysicsWorld::PersistTouches(SaveArchive &a,
                                  const std::function<uint64_t(BodyHandle)> &identity,
                                  const std::function<BodyHandle(uint64_t)> &resolve,
                                  const std::function<bool(BodyHandle)> &include) {
    std::vector<TouchEvent> live;
    if (!a.reading)
        for (const auto &[_, event] : m_impl->previousTouches)
            if (m_impl->Get(event.a) && m_impl->Get(event.b) &&
                (!include || (include(event.a) && include(event.b))))
                live.push_back(event);
    uint32_t count = uint32_t(live.size());
    a(count);
    a.Require(count <= 8192, "saved contact-pair limit");
    if (a.reading)
        ClearTouchHistory();
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t first = 0, second = 0;
        TouchEvent event;
        if (!a.reading) {
            event = live.at(i);
            first = identity(event.a);
            second = identity(event.b);
        }
        a.Require(a.reading || (first && second), "saved contact has unowned required body");
        a(first, second, event.sensor, event.point, event.normal, event.relativeVelocity,
          event.impulseAvailable, event.normalImpulse);
        if (a.reading) {
            event.a = resolve(first);
            event.b = resolve(second);
            a.Require(event.a.IsValid() && event.b.IsValid() && event.a.id != event.b.id,
                      "saved contact body unavailable");
            if (event.a.id > event.b.id) {
                std::swap(event.a, event.b);
                event.normal = -event.normal;
                event.relativeVelocity = -event.relativeVelocity;
            }
            event.phase = TouchPhase::Stay;
            a.Require(m_impl->previousTouches.emplace(std::make_pair(event.a.id, event.b.id), event)
                          .second,
                      "duplicate saved contact pair");
        }
    }
}

void PhysicsWorld::PersistBodyForces(SaveArchive &archive, BodyHandle handle) {
    auto *body = m_impl->Get(handle);
    archive.Require(body != nullptr, "saved force body unavailable");
    archive(body->rigidBody.forceAccumulator, body->rigidBody.torqueAccumulator);
}

void PhysicsWorld::PersistBodyMass(SaveArchive &archive, BodyHandle handle) {
    auto *body = m_impl->Get(handle);
    archive.Require(body != nullptr, "saved mass body unavailable");
    auto inverseMass = body->rigidBody.inverseMass;
    auto inverseInertia = body->rigidBody.inverseInertiaLocal;
    archive(inverseMass, inverseInertia);
    bool valid = false;
    if (body->isDynamic) {
        const glm::dmat3 tensor(inverseInertia);
        valid = inverseMass > 0 && tensor[0][0] > 0 &&
                tensor[0][0] * tensor[1][1] - tensor[0][1] * tensor[1][0] > 0 &&
                glm::determinant(tensor) > 0;
        for (int column = 0; column < 3; ++column)
            for (int row = 0; row < 3; ++row)
                valid &= std::abs(tensor[column][row] - tensor[row][column]) <=
                         1e-5 * std::max(1.0, std::abs(tensor[column][row]));
    } else {
        valid = inverseMass == 0 && inverseInertia == glm::mat3(0);
    }
    archive.Require(valid, "invalid saved mass/inertia for body authority");
    if (archive.reading) {
        m_impl->InvalidateRestoredContinuation(handle.id & Impl::kSlotMask);
        body->rigidBody.inverseMass = inverseMass;
        body->rigidBody.inverseInertiaLocal = inverseInertia;
    }
}

void PhysicsWorld::PersistKinematic(SaveArchive &archive, BodyHandle handle,
                                    glm::dvec3 capturedOriginDelta) {
    auto *body = m_impl->Get(handle);
    archive.Require(body && body->isKinematic, "saved kinematic body unavailable");
    auto state = body->kinematic;
    auto linear = body->rigidBody.linearVelocity, angular = body->rigidBody.angularVelocity;
    archive(state.control, state.target.position, state.target.rotation, state.linearVelocity,
            state.angularVelocity, state.remainingSeconds, state.targetNextStep, linear, angular);
    if (archive.reading)
        state.target.position = glm::vec3(glm::dvec3(state.target.position) - capturedOriginDelta);
    archive.Require(state.control == KinematicControl::Stopped ||
                        state.control == KinematicControl::Target ||
                        state.control == KinematicControl::Velocity,
                    "invalid kinematic control");
    archive.Require(UsableKinematicPose(state.target) && state.remainingSeconds >= 0 &&
                        state.remainingSeconds <= 60,
                    "invalid kinematic target state");
    archive.Require(state.control == KinematicControl::Target ||
                        (!state.targetNextStep && state.remainingSeconds == 0),
                    "inconsistent kinematic interval");
    archive.Require(UsableVelocity(state.linearVelocity) && UsableVelocity(state.angularVelocity) &&
                        UsableVelocity(linear) && UsableVelocity(angular),
                    "unusable saved kinematic velocity");
    if (state.control == KinematicControl::Target) {
        const auto delta = state.target.position + state.target.rotation * body->shape.pivotOffset -
                           body->rigidBody.position;
        archive.Require(UsableVelocity(delta) &&
                            (state.targetNextStep || state.remainingSeconds == 0 ||
                             (UsableVelocity(delta / state.remainingSeconds) &&
                              std::isfinite(glm::pi<float>() / state.remainingSeconds))),
                        "unusable saved kinematic target trajectory");
    }
    if (archive.reading) {
        body->kinematic = state;
        body->rigidBody.linearVelocity = linear;
        body->rigidBody.angularVelocity = angular;
        body->kinematicActiveDuration = 0;
        body->motion.Clear();
        body->previousPosition = body->rigidBody.position;
        body->previousOrientation = body->rigidBody.orientation;
        m_impl->InvalidateImpactPose(handle.id & Impl::kSlotMask);
        m_impl->RefreshProxy(*body);
    }
}

void PhysicsWorld::PersistJointContinuation(SaveArchive &a, JointHandle handle) {
    auto found = std::find_if(m_impl->joints.begin(), m_impl->joints.end(),
                              [&](const auto &joint) { return joint.handle.id == handle.id; });
    a.Require(found != m_impl->joints.end(), "stale joint continuation handle");
    auto &joint = *found;
    auto context = m_impl->ContinuationJointContext(joint);
    const auto expectedContext = context;
    uint32_t version = 2;
    float dt = m_impl->completedObservationsReady ? m_impl->completedSolverDt : 0;
    auto warm = joint.warm;
    auto drive = joint.state.angularDrive;
    a(version, context, dt);
    a.Require(version == 1 || version == 2, "newer joint continuation format");
    if (context != expectedContext) {
        SaveArchive comparison(context);
        m_impl->ContinuationJointContext(joint, &comparison);
        comparison.Finish();
    }
    a.Require(dt >= 0 && dt <= .1f, "invalid joint continuation interval");
    // Warm impulses are per solve interval: version 2 records MK2's substep
    // length; version 1 (MK1) solved joints once per step.
    float warmH = dt > 0 && m_impl->mk2JointWarmH > 0 ? std::min(m_impl->mk2JointWarmH, dt) : dt;
    if (version >= 2)
        a(warmH);
    else
        warmH = dt;
    a.Require(warmH >= 0 && warmH <= dt, "invalid joint continuation warm interval");
    for (auto &value : warm)
        a(value);
    a(drive.enabled, drive.target, drive.relativeAngularVelocity, drive.stiffness, drive.damping,
      drive.maxTorque, drive.impulses);
    a.Require(std::abs(glm::dot(drive.target, drive.target) - 1.f) < 1e-4f,
              "nonunit completed angular-drive target");
    if (dt == 0)
        a.Require(std::all_of(warm.begin(), warm.end(), [](float x) { return x == 0; }) &&
                      drive.impulses == glm::vec3(0),
                  "joint history has no completed interval");
    if (a.reading) {
        a.Require(SetJointAngularDrive(handle, drive), "invalid restored angular drive");
        // Preserve the actual completed producer command, including its target
        // deadband, rather than recomputing a slightly different quaternion.
        joint.state.angularDrive = drive;
        joint.warm = warm;
        joint.restoredContinuationDt = dt;
        m_impl->completedSolverDt = dt;
        if (warmH > 0)
            m_impl->mk2JointWarmH = warmH;
    }
}

bool PhysicsWorld::HasRestoredCompletedContactObservations(float dt) const {
    return m_impl && std::isfinite(dt) && dt > 0 && m_impl->restoredCompletedObservations &&
           m_impl->completedObservationsReady && m_impl->completedObservationEpoch != 0 &&
           m_impl->completedObservationDt == dt;
}

void PhysicsWorld::PersistCompletedContactObservations(
    SaveArchive &a, const std::function<uint64_t(BodyHandle)> &identity,
    const std::function<BodyHandle(uint64_t)> &resolve,
    const std::function<bool(BodyHandle)> &include) {
    auto &w = *m_impl;
    uint32_t version = 1;
    auto epoch = w.completedObservationEpoch;
    auto dt = w.completedObservationDt;
    bool ready = w.completedObservationsReady;
    auto eligible = [&](const CachedContact &c) {
        auto *first = w.Get(w.MakeHandle(c.key.slotA));
        auto *second = w.Get(w.MakeHandle(c.key.slotB));
        return first && second && first->generation == c.key.generationA &&
               second->generation == c.key.generationB && first->enabled && second->enabled &&
               !first->sensor && !second->sensor && !first->queryOnly && !second->queryOnly &&
               w.CanCollide(c.key.slotA, c.key.slotB) && c.normalImpulse > 0 &&
               c.observationRevisionA == first->contactObservationRevision &&
               c.observationRevisionB == second->contactObservationRevision;
    };
    // Omitting a readable transient contact cannot become a fictional complete
    // (possibly empty) publication. The normal unavailable-sample policy then
    // remains authoritative for this intentionally incomplete world snapshot.
    if (!a.reading && ready && include)
        for (const auto &c : w.completedContactObservations)
            if (eligible(c) &&
                (!include(w.MakeHandle(c.key.slotA)) || !include(w.MakeHandle(c.key.slotB)))) {
                ready = false;
                epoch = 0;
                dt = 0;
                break;
            }
    a(version, ready, epoch, dt);
    a.Require(version == 1, "newer completed-observation format");
    a.Require((!ready && epoch == 0 && dt == 0) || (ready && epoch != 0 && dt > 0 && dt <= .1f),
              "invalid completed-observation publication");
    std::vector<const CachedContact *> live;
    if (!a.reading && ready)
        for (const auto &c : w.completedContactObservations)
            if (eligible(c))
                live.push_back(&c);
    uint32_t count = uint32_t(live.size());
    a(count);
    a.Require(count <= 65536, "saved completed-observation limit");
    a.Require(ready || count == 0, "unpublished completed-observation rows");
    std::vector<CachedContact> restored;
    restored.reserve(count);
    using Signature =
        std::tuple<unsigned, unsigned, int, int, double, double, double, float, float, float>;
    std::set<Signature> seen;
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t first = 0, second = 0;
        int partA = 0, partB = 0;
        glm::dvec3 anchorA{0}, anchorB{0};
        glm::vec3 normal{0};
        float impulse = 0, separation = 0;
        std::string contextA, contextB;
        if (!a.reading) {
            const auto &c = *live[i];
            const auto &bodyA = w.bodies[c.key.slotA];
            const auto &bodyB = w.bodies[c.key.slotB];
            first = identity(w.MakeHandle(c.key.slotA));
            second = identity(w.MakeHandle(c.key.slotB));
            partA = c.key.partA;
            partB = c.key.partB;
            anchorA = c.localAnchorA;
            normal = c.normal;
            impulse = c.normalImpulse;
            separation = c.separation;
            const auto point = glm::dvec3(bodyA.rigidBody.position) +
                               glm::mat3_cast(glm::dquat(bodyA.rigidBody.orientation)) * anchorA;
            // The existing observation has only an A anchor. Its corresponding
            // B anchor is derived from that SAME point for slot-order remapping.
            anchorB = glm::transpose(glm::mat3_cast(glm::dquat(bodyB.rigidBody.orientation))) *
                      (point - glm::dvec3(bodyB.rigidBody.position));
            contextA = w.ContinuationBodyContext(bodyA);
            contextB = w.ContinuationBodyContext(bodyB);
        }
        a(first, second, partA, partB, anchorA, anchorB, normal, impulse, separation, contextA,
          contextB);
        a.Require(first && second && first != second && impulse > 0 &&
                      glm::dot(normal, normal) > .99f && glm::dot(normal, normal) < 1.01f,
                  "invalid completed-observation row");
        if (a.reading) {
            auto ha = resolve(first), hb = resolve(second);
            auto *bodyA = w.Get(ha);
            auto *bodyB = w.Get(hb);
            a.Require(bodyA && bodyB && bodyA->enabled && bodyB->enabled && !bodyA->sensor &&
                          !bodyB->sensor && !bodyA->queryOnly && !bodyB->queryOnly,
                      "completed-observation body unavailable");
            auto sa = ha.id & Impl::kSlotMask, sb = hb.id & Impl::kSlotMask;
            if (contextA != w.ContinuationBodyContext(*bodyA)) {
                SaveArchive comparison(contextA);
                w.ContinuationBodyContext(*bodyA, &comparison);
                comparison.Finish();
            }
            if (contextB != w.ContinuationBodyContext(*bodyB)) {
                SaveArchive comparison(contextB);
                w.ContinuationBodyContext(*bodyB, &comparison);
                comparison.Finish();
            }
            a.Require(w.CanCollide(sa, sb) && partA >= 0 && partA < PrimitiveCount(bodyA->shape) &&
                          partB >= 0 && partB < PrimitiveCount(bodyB->shape),
                      "completed-observation filter/primitive differs");
            if (sa > sb) {
                std::swap(sa, sb);
                std::swap(bodyA, bodyB);
                std::swap(partA, partB);
                std::swap(anchorA, anchorB);
                normal = -normal;
            }
            a.Require(seen.insert({sa, sb, partA, partB, anchorA.x, anchorA.y, anchorA.z, normal.x,
                                   normal.y, normal.z})
                          .second,
                      "duplicate completed-observation row");
            restored.push_back(
                {ContactKey{sa, sb, bodyA->generation, bodyB->generation, partA, partB}, anchorA,
                 normal, impulse, glm::vec3(0), separation, bodyA->contactObservationRevision,
                 bodyB->contactObservationRevision});
        }
    }
    if (a.reading) {
        std::stable_sort(restored.begin(), restored.end(),
                         [](const auto &x, const auto &y) { return x.key < y.key; });
        w.completedContactObservations = std::move(restored);
        w.completedObservationEpoch = epoch;
        w.completedObservationDt = dt;
        w.completedObservationsReady = ready;
        w.restoredCompletedObservations = ready;
    }
}

void PhysicsWorld::PersistJointMaterial(SaveArchive &a, JointHandle h) {
    a.Require(m_impl != nullptr, "material history physics unavailable");
    for (auto &j : m_impl->joints)
        if (j.handle.id == h.id) {
            auto &c = j.state.constitutive;
            a.Require(c.material.enabled, "material history requires a material connection");
            a(c.translation, c.rotation, c.translationTravel, c.rotationTravel, c.plasticWork,
              c.failed);
            for (auto v : {c.translation, c.rotation, c.translationTravel, c.rotationTravel})
                for (int k = 0; k < 3; ++k)
                    a.Require(std::isfinite(v[k]), "nonfinite material connection history");
            a.Require(glm::all(glm::greaterThanEqual(c.translationTravel, glm::vec3(0))) &&
                          glm::all(glm::greaterThanEqual(c.rotationTravel, glm::vec3(0))) &&
                          std::isfinite(c.plasticWork) && c.plasticWork >= 0,
                      "invalid material connection history");
            return;
        }
    throw std::runtime_error("stale material joint history");
}

void PhysicsWorld::PersistJointSolverState(SaveArchive &a, JointHandle h) {
    for (auto &j : m_impl->joints)
        if (j.handle.id == h.id) {
            for (auto &x : j.warm) {
                a(x);
                a.Require(std::isfinite(x), "nonfinite fracture joint history");
            }
            a(j.state.reactionImpulse, j.state.reactionAngularImpulse);
            for (auto v : {j.state.reactionImpulse, j.state.reactionAngularImpulse})
                for (int k = 0; k < 3; ++k)
                    a.Require(std::isfinite(v[k]), "nonfinite fracture reaction history");
            return;
        }
    throw std::runtime_error("stale fracture solver history");
}
