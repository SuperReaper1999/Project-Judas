#pragma once

#include <glm/glm.hpp>
#include <cmath>

// Optional conservative equilibrium geometry. Coordinate is metres, potential
// is m^2/s^2. Unsupported fields explicitly return false; never infer world-up.
struct GravityEquilibrium {
 enum class Kind { Plane, Radius }; Kind kind=Kind::Plane;
 glm::dvec3 up{0,1,0},center{0};double magnitude=0;
 double Coordinate(glm::dvec3 p)const{return kind==Kind::Plane?glm::dot(up,p):glm::length(p-center);}
 double Potential(glm::dvec3 p)const{return magnitude*Coordinate(p);}
 glm::dvec3 Up(glm::dvec3 p)const{return kind==Kind::Plane?up:(glm::length(p-center)>1e-12?glm::normalize(p-center):glm::dvec3(0));}
 bool Equivalent(const GravityEquilibrium& b) const {
  if(kind!=b.kind||!(std::abs(magnitude-b.magnitude)<1e-6))return false;
  // A plane has no centre and a radial field has no preferred axis. Compare
  // only parameters used by Coordinate/Up; inactive representation fields
  // must not introduce a physical mismatch after changing the local chart.
  return kind==Kind::Plane?glm::length(up-b.up)<1e-7:glm::length(center-b.center)<1e-7;
 }
};

// Judas's own gravity contract. Every gravity implementation — the uniform
// FaithfulGravity (see FaithfulGravity.h), and future radial/planetary or
// composite/multi-source implementations — shares this one interface.
//
// Everything outside the gravity subsystem (PhysicsWorld, Application, and
// anything else that needs an acceleration at a position) consumes gravity
// only through this interface and stays completely agnostic about which
// implementation is active. The physics middleware's built-in global
// gravity is explicitly disabled (see PhysicsWorld::Init) so that gravity
// is always something a GravityField implementation computes and Judas
// hands to physics — never an assumption baked into the physics engine, and
// never tied to any one implementation of this interface. See
// docs/ARCHITECTURE.md, "Ownership boundary."
class GravityField {
public:
    virtual ~GravityField() = default;

    virtual bool Equilibrium(const glm::vec3&,GravityEquilibrium&) const { return false; }

    virtual glm::vec3 Sample(const glm::vec3& worldPosition) const = 0;
};
