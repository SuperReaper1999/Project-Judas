#pragma once
#include "Narrowphase.h"
#include <limits>

// Shared double-precision geometric evaluator. Distances are nearest-surface
// for points; convex containment is explicit. Meshes have no invented inside.
struct GeometryDistance {
    bool valid=false,contains=false;
    double gap=std::numeric_limits<double>::infinity();
    glm::dvec3 point{0},queryPoint{0},normal{0};
    uint32_t feature=UINT32_MAX;
    bool normalUnique=true;
};
struct CollisionGeometryStats {uint64_t nodes=0,triangles=0,convexTests=0;};
CollisionGeometryStats GetCollisionGeometryStats();
void ResetCollisionGeometryStats();
void CountCollisionQuery(uint64_t nodes,uint64_t triangles);
glm::dmat3 PrimitiveRotation(const PrimitivePose&);
glm::dvec3 PrimitiveCenter(const PrimitivePose&);
GeometryDistance SegmentGeometry(glm::dvec3 a,glm::dvec3 b,double radius,const PrimitivePose& target,double range=std::numeric_limits<double>::infinity(),bool nearestSurface=false,uint32_t feature=UINT32_MAX);
GeometryDistance PointGeometry(glm::dvec3 point,const PrimitivePose& target,double range=std::numeric_limits<double>::infinity());
ContactManifold CookedContacts(const PrimitivePose&,const PrimitivePose&,double margin);
GeometryDistance PolyGeometry(const PrimitivePose& query,const PrimitivePose& target,double range=std::numeric_limits<double>::infinity(),uint32_t feature=UINT32_MAX);
struct CollisionMassProperties {double volume=0;glm::dvec3 center{0};glm::dmat3 inertia{0};};
CollisionMassProperties ShapeMassProperties(const Shape&);
