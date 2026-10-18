#pragma once
#include "LiquidGeometry.h"
#include <memory>
#include <optional>
#include <iosfwd>
#include <cstdint>

struct LiquidSurfaceSettings {
 bool enabled=false;unsigned columns=12,rows=12;
 double friction=.2,tolerance=1e-9;unsigned iterations=128;
 double splashSpeed=2,splashFraction=.02;unsigned parcelBudget=128;
};
struct LiquidSurfaceCellData {
 LiquidBasinData storage;std::vector<LiquidTetQuery> loadingGeometry;unsigned column=0,row=0;
 glm::dvec3 centre{0};glm::dvec2 chart{0};
};
struct LiquidSurfaceFaceData {
 unsigned a=0,b=0;glm::dvec3 normal{0};double offset=0,distance=0;
 std::vector<LiquidPolygon> polygons;
};
struct LiquidSurfaceData {
 LiquidSurfaceSettings settings;GravityEquilibrium equilibrium;
 glm::dvec3 axis{0,1,0},x{1,0,0},y{0,0,1};glm::dvec2 minimum{0},maximum{0};
 std::vector<LiquidSurfaceCellData> cells;std::vector<LiquidSurfaceFaceData> faces;
 std::vector<int> grid;
 glm::dvec2 Chart(glm::dvec3)const;
 int Cell(glm::dvec3)const;
 glm::dvec3 Point(glm::dvec2,double q)const;
};
bool BakeLiquidSurface(const LiquidBasinData&,const LiquidSurfaceSettings&,std::shared_ptr<const LiquidSurfaceData>&,std::string&,const LiquidGeometry* topology=nullptr);
bool SaveLiquidSurface(std::ostream&,const LiquidSurfaceData&);
bool ReadLiquidSurface(std::istream&,const GravityEquilibrium&,std::shared_ptr<const LiquidSurfaceData>&,std::string&);
// No competing owner total: this object is the spatial allocation of one M54
// reservoir. LiquidSystem is the only owner mutation/transaction path.
struct LiquidSurfaceStats {unsigned iterations=0,retries=0,limitedFaces=0;double residual=0,partitionError=0,seconds=0;};
struct LiquidSurfaceCell {std::uint64_t geometryRevision=1;double volume=0,previousQ=0,q=0;LiquidBasinData storage;glm::dvec3 velocity{0};};
struct LiquidSurfaceAllocation {std::vector<std::pair<unsigned,double>> changes;double amount=0;};
class LiquidSurface {
public:
 void Persist(class SaveArchive&);
 explicit LiquidSurface(std::shared_ptr<const LiquidSurfaceData>,double volume);
 std::shared_ptr<const LiquidSurfaceData> data;
 std::vector<LiquidSurfaceCell> cells;std::vector<double> discharge;
 LiquidSurfaceStats stats;bool enabled=true;
 double Volume()const;
 double Capacity()const;
 double Coordinate(unsigned,float alpha=1)const;
 int Cell(glm::dvec3)const;
 LiquidSurfaceAllocation Plan(double signedAmount,std::optional<glm::dvec3> point={})const;
 void Apply(const LiquidSurfaceAllocation&,bool add);
 bool Step(double dt,std::string&);
 void BeginStep();
 void Impulse(glm::dvec3 point,glm::dvec3 impulse,double density);
 glm::dvec3 Normal(unsigned,float alpha=1)const;
 MeshData Mesh(float alpha=1)const;
 double Energy()const;
 double KineticEnergy(unsigned cell)const;
 bool TakeKineticEnergy(unsigned cell,double perDensityEnergy);
 // Geometry changes remove space, never water. Excess remains allocated until
 // LiquidSystem explicitly emits it through the same conserved parcel path.
 double SetSolids(const std::vector<std::vector<glm::dvec4>>&,std::string&,unsigned staticCount=0);
private:
 bool Once(double,std::string&);
 bool Advance(double,unsigned,std::string&);
 void Resolve();
 double FaceArea(size_t,double)const;
 std::vector<unsigned> Component(unsigned)const;
 std::vector<std::vector<size_t>> m_adjacency;
 struct CapTet {std::array<double,4> coordinates;glm::dvec3 normal;unsigned mask=16,count=0;std::array<std::pair<unsigned,unsigned>,4> edges{};};
 mutable std::vector<std::pair<std::uint64_t,std::vector<CapTet>>> m_caps;
 std::vector<std::vector<glm::dvec4>> m_solids;
 std::vector<std::vector<std::vector<glm::dvec4>>> m_cellSolids;
 std::vector<std::pair<glm::dvec3,glm::dvec3>> m_bounds;
 std::vector<LiquidGeometry> m_staticGeometry;
 std::vector<std::vector<std::vector<glm::dvec4>>> m_staticSolids;
 bool m_geometryCacheValid=false;
 std::vector<std::vector<LiquidPolygon>> m_apertures;
 mutable std::vector<std::pair<double,double>> m_faceAreas;
};
