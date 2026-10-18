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
// Exact accepted-contact existence; full manifolds remain authoritative for response.
bool CookedHasContacts(const PrimitivePose&,const PrimitivePose&,double margin);
// Exact deep-contact existence for continuous penetration searches.
bool CookedHasPenetrationBeyond(const PrimitivePose&,const PrimitivePose&,double threshold);
GeometryDistance PolyGeometry(const PrimitivePose& query,const PrimitivePose& target,double range=std::numeric_limits<double>::infinity(),uint32_t feature=UINT32_MAX);
struct MeshContactEpisode;
// Earliest possible overlap of a moving convex polyhedron and a stationary
// triangle union. Each finite patch retains its own SAT bounds; no selected
// local response plane is treated as a plane separating the entire mesh.
// The angular generator's magnitude bounds the anchored rotation rate.
double MeshSeparationAdvance(const PrimitivePose& query,const PrimitivePose& mesh,
                             glm::dvec3 velocity,glm::dvec3 angularGenerator,
                             double rotationRadius,double duration,const std::vector<uint32_t>* excluded=nullptr,const MeshContactEpisode* episode=nullptr);
struct CollisionMassProperties {double volume=0;glm::dvec3 center{0};glm::dmat3 inertia{0};};
CollisionMassProperties ShapeMassProperties(const Shape&);

// Internal physical front-contact episode: stateless queries pass no episode.
struct MeshContactEpisode { std::vector<uint32_t> fronts; };
ContactManifold EpisodeCookedContacts(const PrimitivePose&,const PrimitivePose&,double,const MeshContactEpisode*);
std::vector<uint32_t> MeshContactFeatures(const PrimitivePose&,const PrimitivePose&,const MeshContactEpisode*);
bool MeshEpisodeHasContacts(const PrimitivePose&,const PrimitivePose&,const MeshContactEpisode*,const std::vector<uint32_t>* excluded=nullptr);
bool MeshEpisodeHasPenetrationBeyond(const PrimitivePose&,const PrimitivePose&,double,const MeshContactEpisode*);
