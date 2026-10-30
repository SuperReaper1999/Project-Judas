#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

// Immutable native planar geometry, expressed in a body's local XY frame.
// Rounded convex cores cover circles/capsules without polygonal approximations.
// Chains are static segment unions (two-sided by default); they do not invent a filled area.
enum class PlanarKind { Circle, Box, Capsule, Convex, Chain };
struct PlanarCollider {
    PlanarKind kind=PlanarKind::Box;
    std::vector<glm::dvec2> vertices;
    double radius=0;
    bool lockRotation=false;
    bool oneWay=false; // Static straight chain; solid on the left of vertex 0 -> 1.
};
struct PlanarMass {
    double area=0;
    glm::dvec2 center{0};
    double product=0; // integral (x-Cx)(y-Cy), used by the physical tensor
    glm::dvec3 inertia{0}; // unit surface-density moments about COM: Ix, Iy, Iz
};
bool ValidatePlanar(const PlanarCollider&,std::string& error,bool query=false);
PlanarMass PlanarMassProperties(const PlanarCollider&);
std::shared_ptr<const PlanarCollider> PlanarCircle(double radius,bool lock=false);
std::shared_ptr<const PlanarCollider> PlanarBox(glm::dvec2 half,bool lock=false);
std::shared_ptr<const PlanarCollider> PlanarCapsule(double radius,double halfHeight,bool lock=false);
std::shared_ptr<const PlanarCollider> PlanarConvex(std::vector<glm::dvec2>,bool lock=false);
std::shared_ptr<const PlanarCollider> PlanarChain(std::vector<glm::dvec2>,bool oneWay=false);
std::string EncodePlanar(const PlanarCollider&);
bool DecodePlanar(const std::string&,std::shared_ptr<const PlanarCollider>&,std::string& error);
struct Shape;
struct ContactPose;
struct ContactManifold;
ContactManifold PlanarContacts(const Shape&,const ContactPose&,const Shape&,const ContactPose&,double margin,glm::dvec3 translationA=glm::dvec3(0));
std::vector<glm::dvec2> PlanarCore(const Shape&);

struct PrimitiveCastHit;struct BodyTransform;struct PrimitivePose;
PrimitiveCastHit CastPlanarPrimitive(const Shape&,const BodyTransform&,glm::vec3 direction,double maximum,const PrimitivePose&);

// Start-side policy only: keep separation/contact geometry continuous during CCD.
bool PlanarOneWayAllows(const Shape& platform,const ContactPose&,const Shape& other,const ContactPose&,double tolerance);
