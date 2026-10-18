#pragma once
#include "GravityField.h"
#include "MeshData.h"
#include <array>
#include <string>
#include <vector>
#include <memory>
struct LiquidSurfaceData;
// Authored physical cavity: a disjoint union of positive-volume tetrahedra.
// This is independent of the visual mesh. Coordinates and integration use doubles.
using LiquidTet = std::array<glm::dvec3,4>;
struct LiquidGeometry {std::vector<LiquidTet> cells;};
struct LiquidMoment {double volume=0;glm::dvec3 first{0};};
struct LiquidCurvePoint {double q=0,volume=0;};
struct LiquidBasinData {
 std::shared_ptr<const LiquidSurfaceData> dynamicSurface;
 LiquidGeometry geometry;GravityEquilibrium equilibrium;
 std::vector<LiquidCurvePoint> curve;std::string fingerprint;
 // Optional exact piecewise-cubic storage; empty preserves M54 linear tables.
 std::vector<std::array<double,4>> capacityPolynomials;
 double volumeTolerance=1e-5,heightTolerance=.002,coordinateError=0,capacity=0;
};
bool LoadLiquidGeometry(const std::string&,LiquidGeometry&,std::string&);
bool ValidateLiquidGeometry(const LiquidGeometry&,std::string&);
bool DecodeLiquidBasin(const std::vector<unsigned char>&,LiquidBasinData&,std::string&);
bool SaveLiquidBasin(const std::string&,const LiquidBasinData&,std::string&);
bool LoadLiquidBasin(const std::string&,LiquidBasinData&,std::string&);
LiquidGeometry LiquidBox(glm::dvec3 minimum,glm::dvec3 maximum);
LiquidMoment LiquidClip(const LiquidTet&,const std::array<double,4>& coordinates,double q,MeshData* cap=nullptr);
double LiquidCapacity(const LiquidGeometry&,const GravityEquilibrium&,double q);
double LiquidInverse(const LiquidBasinData&,double volume);
bool BakeLiquidBasin(const LiquidGeometry&,const GravityEquilibrium&,double volumeTolerance,double heightTolerance,const std::string& fingerprint,LiquidBasinData&,std::string&);
double LiquidCoordinate(const LiquidTet&,const GravityEquilibrium&,glm::dvec3 point,bool& inside);
struct LiquidTetQuery {
 glm::dvec3 minimum{0},maximum{0};std::array<glm::dvec4,5> planes{};bool valid=false;
};
LiquidTetQuery CompileLiquidTetQuery(const LiquidTet&,const GravityEquilibrium&);
LiquidMoment LiquidIntersection(const LiquidTet& body,const LiquidTetQuery&,double q);
LiquidMoment LiquidIntersection(const LiquidTet& body,const LiquidTet& basin,const GravityEquilibrium&,double q);

// M55 static geometric operations: planes use dot(n,p)<=d. Result tetrahedra
// remain a disjoint physical volume; no bounding-box storage approximation.
using LiquidPolygon=std::vector<glm::dvec3>;
LiquidGeometry LiquidCutGeometry(const LiquidGeometry&,const std::vector<glm::dvec4>&);
LiquidGeometry LiquidSubtractConvex(const LiquidGeometry&,const std::vector<glm::dvec4>&);
std::vector<LiquidPolygon> LiquidBoundary(const LiquidGeometry&,glm::dvec3 normal,double offset);
double LiquidFaceArea(const std::vector<LiquidPolygon>&,const GravityEquilibrium&,double coordinate);
bool LiquidConvexCell(const LiquidGeometry&);

std::vector<glm::dvec4> LiquidTetPlanes(const LiquidTet&);
LiquidGeometry LiquidOccupiedGeometry(const LiquidGeometry&,const GravityEquilibrium&,double q);
std::vector<glm::dvec4> LiquidOccupiedPlanes(const LiquidTet&,const GravityEquilibrium&,double q);

// Same bounded radial quadrature refinement used by M54 basin baking.
bool RefineLiquidGeometry(const LiquidGeometry&,const GravityEquilibrium&,double heightTolerance,LiquidGeometry&,double& coordinateError,std::string&);

// Subtract the union before re-tetrahedralizing; avoids repeated fan subdivision.
LiquidGeometry LiquidSubtractSolids(const LiquidGeometry&,const std::vector<std::vector<glm::dvec4>>&);

// Extract only the actual affine-coordinate cap; no volume integration.
void LiquidCap(const LiquidTet&,const std::array<double,4>&,double,MeshData&,glm::dvec3 normal={0,0,0});

// Consolidate certified convex physical geometry, retaining the original on
// an unresolved volume comparison. Never applied to nonconvex solid cutouts.
LiquidGeometry LiquidSimplifyConvex(const LiquidGeometry&);
