#include "RigidBodyGravity.h"

#include "GravityField.h"
#include "RigidBody.h"

void ApplyGravity(RigidBody& body, const GravityField& gravity) {
    if (body.IsStatic()) return;

    const glm::vec3 acceleration = gravity.Sample(body.position);
    // Force, not acceleration directly: F = m*a. Dividing by inverseMass
    // (i.e. multiplying by mass) here means IntegrateRigidBody's own
    // `forceAccumulator * inverseMass` cancels back to exactly
    // `acceleration` regardless of the body's mass — gravity accelerates
    // every body identically, the same physical fact every existing
    // gravity consumer already relies on.
    body.ApplyForce(acceleration / body.inverseMass);
}
