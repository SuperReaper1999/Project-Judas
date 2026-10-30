#pragma once
#include "MeshData.h"
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Absolute positions and sampling remain binary64. Only an already-subtracted
// tile-local displacement is narrowed into MeshVertex or PhysicsWorld.
struct PlanetTileKey {
 unsigned face=0,level=0,x=0,y=0;
 bool operator<(const PlanetTileKey& b)const;
 bool operator==(const PlanetTileKey& b)const;
 PlanetTileKey Parent()const;
};
struct PlanetHeightBand {double amplitude=0;glm::dvec3 wave{0};}; // wave: radians/metre
struct PlanetHeightPatch {
 glm::dvec3 direction{0,1,0};double extent=100;
 unsigned resolution=2;std::vector<double> heights{0,0,0,0};
};
struct PlanetSettings {
 double radius=6371000;glm::dvec3 center{0};glm::dquat rotation{1,0,0,0};
 unsigned segments=16,maxLevel=24,maxTiles=384,maxResident=768,buildsPerFrame=4;
 double pixelError=2,morphSeconds=.35,collisionSpacing=.25,collisionRadius=20;
 unsigned maxCollisionTiles=128;
 std::vector<PlanetHeightBand> bands;
 std::vector<PlanetHeightPatch> patches;
};
bool ValidatePlanetSettings(const PlanetSettings&,std::string&);
std::string EncodePlanetSettings(const PlanetSettings&);
bool DecodePlanetSettings(const std::string&,PlanetSettings&,std::string&);
struct PlanetSample {glm::dvec3 point{0},normal{0,1,0};double height=0,distance=0;};
class PlanetSurface {
public:
 explicit PlanetSurface(PlanetSettings);
 // Immutable source seam. Custom providers must be thread-safe and supply
 // conservative height/error bounds; disk/network IO belongs in preparation.
 using Height=std::function<double(glm::dvec3)>;
 PlanetSurface(PlanetSettings,Height,double maximumHeight,double curvatureBound);
 const PlanetSettings settings;
 double HeightAt(glm::dvec3 modelDirection)const;
 glm::dvec3 Point(glm::dvec3 modelDirection)const;
 PlanetSample Sample(glm::dvec3 absolutePoint,double normalStep=0)const;
 double MaximumHeight()const{return m_heightBound;}
 double Error(PlanetTileKey)const;
 static glm::dvec3 Direction(unsigned face,double u,double v);
 static PlanetTileKey Locate(glm::dvec3 direction,unsigned level);
 static glm::dvec2 FaceCoordinates(unsigned face,glm::dvec3 direction);
 glm::dvec3 GridPoint(PlanetTileKey,unsigned x,unsigned y,unsigned segments)const;
 glm::dvec3 TileCenter(PlanetTileKey)const;
 double TileRadius(PlanetTileKey)const;
private:
 Height m_source;double m_heightBound=0,m_curvature=0;
};
struct PlanetTileData {
 PlanetTileKey key;glm::dvec3 origin{0};
 std::vector<glm::dvec3> points,parentPoints,normals;
 MeshData Mesh(double morph=1,double skirt=0)const;
};
PlanetTileData BuildPlanetTile(const PlanetSurface&,PlanetTileKey,unsigned segments,bool presentation=true);
// Greedy screen-space-error refinement: six roots always cover the sphere.
// A hard leaf budget bounds work; unsatisfied error is observable, not hidden.
struct PlanetSelection {std::vector<PlanetTileKey> leaves;double maximumPixelError=0;bool budgetLimited=false;};
struct PlanetView {glm::dvec3 eye;double fov;unsigned pixels;double orthographicHeight=0;};
PlanetSelection SelectPlanetTiles(const PlanetSurface&,const std::vector<PlanetView>&);
PlanetSelection SelectPlanetTiles(const PlanetSurface&,glm::dvec3 eye,double verticalFov,unsigned pixels);
// Barycentric parent surface used by geomorphs, not a radial-height blend.
glm::dvec3 PlanetTrianglePoint(const PlanetSurface&,PlanetTileKey,glm::dvec3 direction,unsigned segments);
