#include "RigidBody.h"

glm::mat3 SolidSphereInverseInertia(float mass, float radius) {
    // I = 2/5 * m * r^2 (solid sphere, isotropic — same about every axis).
    const float inertia = 0.4f * mass * radius * radius;
    if (inertia <= 0.0f) return glm::mat3(0.0f);
    return glm::mat3(1.0f / inertia);
}

glm::mat3 SolidBoxInverseInertia(float mass, const glm::vec3& halfExtents) {
    // Solid cuboid with full extents (2*halfExtents): standard closed-form
    // diagonal inertia tensor about its own center, in its own body axes.
    const glm::vec3 extents = halfExtents * 2.0f;
    const glm::vec3 sq = extents * extents;
    const float ixx = (mass / 12.0f) * (sq.y + sq.z);
    const float iyy = (mass / 12.0f) * (sq.x + sq.z);
    const float izz = (mass / 12.0f) * (sq.x + sq.y);
    glm::mat3 inverseInertia(0.0f);
    if (ixx > 0.0f) inverseInertia[0][0] = 1.0f / ixx;
    if (iyy > 0.0f) inverseInertia[1][1] = 1.0f / iyy;
    if (izz > 0.0f) inverseInertia[2][2] = 1.0f / izz;
    return inverseInertia;
}

void IntegrateRigidBody(RigidBody& body, float fixedDeltaTime) {
    if (body.IsStatic()) {
        body.ClearAccumulators();
        return;
    }

    const glm::vec3 linearAcceleration = body.forceAccumulator * body.inverseMass;
    body.linearVelocity += linearAcceleration * fixedDeltaTime;
    body.position += body.linearVelocity * fixedDeltaTime;

    const glm::mat3 inverseInertiaWorld = body.InverseInertiaWorld();
    const glm::vec3 angularAcceleration = inverseInertiaWorld * body.torqueAccumulator;
    body.angularVelocity += angularAcceleration * fixedDeltaTime;

    const glm::quat angularVelocityQuat(0.0f, body.angularVelocity.x, body.angularVelocity.y,
                                         body.angularVelocity.z);
    const glm::quat orientationDelta = angularVelocityQuat * body.orientation;
    body.orientation =
        glm::normalize(body.orientation + orientationDelta * (0.5f * fixedDeltaTime));

    body.ClearAccumulators();
}
