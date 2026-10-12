#include "RigidBody.h"

#include <algorithm>
#include <stdexcept>

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


glm::vec3 TransportAngularVelocity(const glm::mat3& inverseInertiaLocal,
    const glm::quat& from,const glm::quat& to,const glm::vec3& velocity) {
    if(from==to||velocity==glm::vec3(0))return velocity;
    const glm::dmat3 inverseBody(inverseInertiaLocal);
    // Locked axes cannot be reconstructed by inverting a singular tensor.
    // Normal authored dynamic shapes have a positive definite full tensor.
    if(!(glm::determinant(inverseBody)>0))return velocity;
    const auto oldRotation=glm::mat3_cast(glm::normalize(glm::dquat(from)));
    const auto newRotation=glm::mat3_cast(glm::normalize(glm::dquat(to)));
    const auto momentum=oldRotation*glm::inverse(inverseBody)*glm::transpose(oldRotation)*glm::dvec3(velocity);
    return glm::vec3(newRotation*inverseBody*glm::transpose(newRotation)*momentum);
}

glm::vec3 AngularDriftGenerator(const glm::mat3& inverseInertiaLocal,
 const glm::quat& orientation,const glm::vec3& velocity,float duration) {
 const glm::dmat3 inverseBody(inverseInertiaLocal);
 if(duration==0||velocity==glm::vec3(0)||!(glm::determinant(inverseBody)>0))return velocity;
 const auto R=glm::mat3_cast(glm::normalize(glm::dquat(orientation)));
 const auto m0=glm::inverse(inverseBody)*glm::transpose(R)*glm::dvec3(velocity);
 auto middle=m0;const double half=.5*double(duration);
 // Body Euler equation m'=m cross I^-1m. The implicit midpoint Cayley
 // rotation preserves both the momentum norm and quadratic kinetic energy.
 // Newton solves the three-component midpoint, within a fixed iteration cap.
 // Failure is explicit; there is no unconserved predictor fallback.
 for(int iteration=0;iteration<12;++iteration){
  const auto omega=inverseBody*middle;
  const auto residual=middle-m0-half*glm::cross(middle,omega);
  if(glm::length(residual)<=1e-12*std::max(glm::length(m0),1e-12))
   return glm::vec3(R*omega);
  glm::dmat3 J(1);
  for(int axis=0;axis<3;++axis){glm::dvec3 e(0);e[axis]=1;
   J[axis]=e-half*(glm::cross(e,omega)+glm::cross(middle,inverseBody*e));}
  middle-=glm::inverse(J)*residual;
 }
 throw std::runtime_error("angular midpoint did not converge");
}

void IntegrateRigidBody(RigidBody& body, float fixedDeltaTime) {
    IntegrateRigidBodyVelocity(body, fixedDeltaTime);
    IntegrateRigidBodyPosition(body, fixedDeltaTime);
}

void IntegrateRigidBodyVelocity(RigidBody& body, float fixedDeltaTime) {
    if (body.IsStatic()) {
        body.ClearAccumulators();
        return;
    }
    const glm::vec3 linearAcceleration = body.forceAccumulator * body.inverseMass;
    body.linearVelocity += linearAcceleration * fixedDeltaTime;
    // The world inertia uses the start-of-step orientation, as it always has.
    const glm::mat3 inverseInertiaWorld = body.InverseInertiaWorld();
    const glm::vec3 angularAcceleration = inverseInertiaWorld * body.torqueAccumulator;
    body.angularVelocity += angularAcceleration * fixedDeltaTime;
    body.ClearAccumulators();
}

void IntegrateRigidBodyPosition(RigidBody& body, float fixedDeltaTime) {
    if (body.IsStatic()) return;
    body.position += body.linearVelocity * fixedDeltaTime;
    const auto generator=AngularDriftGenerator(body.inverseInertiaLocal,body.orientation,body.angularVelocity,fixedDeltaTime);
    const glm::quat angularVelocityQuat(0.0f,generator.x,generator.y,generator.z);
    const glm::quat orientationDelta = angularVelocityQuat * body.orientation;
    const auto previous=body.orientation;
    const auto advanced=body.orientation + orientationDelta * (0.5f * fixedDeltaTime);
    if(advanced!=previous)body.orientation=glm::normalize(advanced);
    body.angularVelocity=TransportAngularVelocity(body.inverseInertiaLocal,previous,body.orientation,body.angularVelocity);
}
