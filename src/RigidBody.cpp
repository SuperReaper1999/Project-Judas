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
 const double tolerance=1e-12*std::max(glm::length(m0),1e-12);
 // One Newton solve of the midpoint equation for half-interval h from `guess`.
 const auto solve=[&](double h,glm::dvec3& m,int iterations){
  for(int iteration=0;iteration<iterations;++iteration){
   const auto omega=inverseBody*m;
   const auto residual=m-m0-h*glm::cross(m,omega);
   if(glm::length(residual)<=tolerance)return true;
   glm::dmat3 J(1);
   for(int axis=0;axis<3;++axis){glm::dvec3 e(0);e[axis]=1;
    J[axis]=e-h*(glm::cross(e,omega)+glm::cross(m,inverseBody*e));}
   m-=glm::inverse(J)*residual;
  }
  return false;
 };
 if(solve(half,middle,12))return glm::vec3(R*(inverseBody*middle));
 // A fast spin (|omega| dt of several radians, e.g. a small light body spun
 // up by a hard off-centre impact) can leave the cold-started solve outside
 // Newton's basin. Solve the same equation by continuation in the interval:
 // each stage starts from the converged midpoint of a slightly shorter one,
 // which follows the root that grows continuously from m0 at h=0. The answer
 // meets the same residual tolerance, so the same invariants hold.
 for(int stages=8;stages<=4096;stages*=8){
  glm::dvec3 m=m0;bool solved=true;
  for(int stage=1;stage<=stages&&solved;++stage)solved=solve(half*double(stage)/double(stages),m,24);
  if(solved)return glm::vec3(R*(inverseBody*m));
 }
 throw std::runtime_error("angular midpoint did not converge");
}

bool TryAngularDriftGenerator(const glm::mat3& inverseInertiaLocal,
    const glm::quat& orientation, const glm::vec3& velocity, float duration,
    glm::vec3& generator) {
    const glm::dmat3 inverseBody(inverseInertiaLocal);
    if (duration == 0 || velocity == glm::vec3(0) || !(glm::determinant(inverseBody) > 0)) {
        generator = velocity;
        return true;
    }
    const auto rotation = glm::mat3_cast(glm::normalize(glm::dquat(orientation)));
    const auto initial = glm::inverse(inverseBody) * glm::transpose(rotation) * glm::dvec3(velocity);
    auto middle = initial;
    const double half = .5 * duration;
    const double tolerance = 1e-12 * std::max(glm::length(initial), 1e-12);
    for (int iteration = 0; iteration < 12; ++iteration) {
        const auto omega = inverseBody * middle;
        const auto residual = middle - initial - half * glm::cross(middle, omega);
        if (glm::length(residual) <= tolerance) {
            generator = glm::vec3(rotation * omega);
            return true;
        }
        glm::dmat3 jacobian(1);
        for (int axis = 0; axis < 3; ++axis) {
            glm::dvec3 unit(0);
            unit[axis] = 1;
            jacobian[axis] = unit - half * (glm::cross(unit, omega) + glm::cross(middle, inverseBody * unit));
        }
        if (!std::isfinite(glm::determinant(jacobian)) || glm::determinant(jacobian) == 0)
            return false;
        middle -= glm::inverse(jacobian) * residual;
    }
    return false;
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
    const glm::vec3 linearAcceleration = body.LinearResponse(body.forceAccumulator);
    body.linearVelocity += linearAcceleration * fixedDeltaTime;
    // The world inertia uses the start-of-step orientation, as it always has.
    const glm::mat3 inverseInertiaWorld = body.ResponseInertiaWorld();
    const glm::vec3 angularAcceleration = inverseInertiaWorld * body.torqueAccumulator;
    body.angularVelocity += angularAcceleration * fixedDeltaTime;
    body.ClearAccumulators();
}

void IntegrateRigidBodyPosition(RigidBody& body, float fixedDeltaTime) {
    if (body.IsStatic()) return;
    body.position += body.linearVelocity * fixedDeltaTime;
    const auto generator=body.IsPlanar()?body.angularVelocity:AngularDriftGenerator(body.inverseInertiaLocal,body.orientation,body.angularVelocity,fixedDeltaTime);
    const glm::quat angularVelocityQuat(0.0f,generator.x,generator.y,generator.z);
    const glm::quat orientationDelta = angularVelocityQuat * body.orientation;
    const auto previous=body.orientation;
    const auto advanced=body.orientation + orientationDelta * (0.5f * fixedDeltaTime);
    if(advanced!=previous)body.orientation=glm::normalize(advanced);
    if(!body.IsPlanar())body.angularVelocity=TransportAngularVelocity(body.inverseInertiaLocal,previous,body.orientation,body.angularVelocity);
}
