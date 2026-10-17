#pragma once
#include "LiquidTypes.h"
#include "Scene.h"
#include <cstdint>
#include <map>
class RuntimeWorld;
struct LiquidHandle {std::uint64_t id=0,generation=0;bool operator==(const LiquidHandle& h)const{return id==h.id&&generation==h.generation;}};
struct LiquidState {
 LiquidHandle handle;SceneObjectId entity=0;bool capacityResolved=false;bool container=false,enabled=true,equilibriumValid=true;
 LiquidMaterial material;double volume=0,capacity=0,stableCapacity=0,q=0;
 SceneTransform pose;GravityEquilibrium equilibrium;LiquidGeometry geometry;
 std::shared_ptr<LiquidSurface> dynamicSurface;
 std::shared_ptr<const LiquidBasinData> basin;LiquidContainerSettings containerSettings;
 MeshData surface;glm::dvec3 minimum{0},maximum{0};
};
struct DetachedLiquid {std::uint64_t id=0;LiquidMaterial material;double volume=0;glm::vec3 position{0},velocity{0};bool parked=false;SceneObjectId source=0;};
struct LiquidAccounting {double reservoirs=0,containers=0,detached=0,total=0,expected=0,error=0,tolerance=0;};
struct LiquidSample {LiquidHandle handle;SceneObjectId entity=0;LiquidMaterial material;double depth=0,coordinate=0;glm::vec3 surfacePoint{0},normal{0},up{0},velocity{0};};
struct LiquidSubmersion {double volume=0,displacedMass=0;glm::vec3 center{0},buoyancy{0},velocity{0};};
// All quantity mutations go through paired transactions. No surface or body
// solver owns another copy of liquid. Entity removal releases a retained parcel.
struct LiquidStepTimes {double containers=0,geometry=0,surface=0,loading=0,total=0;};
class LiquidSystem {
public:
 bool PreparePersistence(RuntimeWorld&,std::string&);
 void Persist(class SaveArchive&);
 LiquidSystem();
 // Local geometry, wave allocation, flow and quantity remain unchanged.
 // Only owner poses and detached world-space parcels follow the physics chart.
 bool CanShiftOrigin(glm::dvec3 delta,std::string& error)const;
 void ShiftOrigin(glm::dvec3 delta);
 const LiquidStepTimes& StepTimes()const{return m_stepTimes;}
 LiquidHandle AddBasin(SceneObjectId,const LiquidBasinSettings&,const SceneTransform&,std::shared_ptr<const LiquidBasinData>,std::string&);
 LiquidHandle AddContainer(SceneObjectId,const LiquidContainerSettings&,const SceneTransform&,const LiquidGeometry&,const GravityEquilibrium&,std::string&);
 LiquidState* Get(LiquidHandle);const LiquidState* Get(LiquidHandle)const;
 LiquidHandle Handle(SceneObjectId)const;
 bool Remove(LiquidHandle);bool SetEnabled(LiquidHandle,bool);
 double Transfer(LiquidHandle source,LiquidHandle destination,double cubicMetres,std::optional<glm::vec3> sourcePoint={},std::optional<glm::vec3> destinationPoint={});
 double Emit(LiquidHandle,double,glm::vec3 position,glm::vec3 velocity);
 double Receive(std::uint64_t parcel,LiquidHandle,double maximum,std::optional<glm::vec3> point={});
 LiquidAccounting Accounting(const std::string& material)const;
 std::optional<LiquidSample> Sample(glm::vec3 point,LiquidHandle excluded={},float presentationAlpha=1)const;
 bool SurfaceImpulse(LiquidHandle,glm::vec3 point,glm::vec3 impulse);
 bool SurfaceEnabled(LiquidHandle,bool);
 bool SetContainerPose(LiquidHandle,const SceneTransform&,const GravityEquilibrium&,std::string&);
 void Resolve(LiquidState&);
 void BeginStep();
 void Update(RuntimeWorld&,double dt);
 LiquidSubmersion Submerged(const LiquidGeometry& worldGeometry)const;
 std::vector<glm::vec2> OpticalPaths(const glm::mat4& view,const glm::mat4& projection,unsigned columns,unsigned rows,float alpha)const;
 const std::map<std::uint64_t,LiquidState>& States()const{return m_states;}
 const std::map<std::uint64_t,DetachedLiquid>& Parcels()const{return m_parcels;}
 const std::map<SceneObjectId,std::string>& Errors()const{return m_errors;}
 const std::map<SceneObjectId,bool>& Connections()const{return m_connections;}
private:
 LiquidStepTimes m_stepTimes;
 std::uint64_t m_generation=0,m_next=1,m_nextParcel=1;
 std::map<std::uint64_t,LiquidState> m_states;
 std::map<SceneObjectId,LiquidHandle> m_entities;
 std::map<std::uint64_t,DetachedLiquid> m_parcels;
 std::map<std::string,double> m_expected,m_density;
 std::map<SceneObjectId,std::string> m_errors;
 std::map<SceneObjectId,bool> m_connections;
 struct OpticalTet {glm::dvec3 minimum,maximum,capNormal;double minimumQ=0,maximumQ=0,capOffset=0;std::array<glm::dvec4,4> planes;};
 struct OpticalNode {glm::dvec3 minimum,maximum;unsigned begin=0,end=0,left=0,right=0;};
 struct OpticalCell {std::uint64_t revision=0;SceneTransform pose;GravityEquilibrium equilibrium;glm::dvec3 minimum,maximum;std::vector<OpticalTet> cells;std::vector<unsigned> order;std::vector<OpticalNode> nodes;};
 mutable std::map<std::pair<std::uint64_t,unsigned>,OpticalCell> m_opticalCache;
 LiquidHandle Insert(LiquidState,std::string&);
 void Synchronize(RuntimeWorld&);
};
