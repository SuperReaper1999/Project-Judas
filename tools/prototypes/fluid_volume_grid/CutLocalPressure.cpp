#include "CutLocalPressure.h"
#include "CutDirectedTransport.h"
#include "CutFastDirectedTransport.h"
#include "CutConservativeBalance.h"
#include "CutPressurePreconditioner.h"
#include <memory>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <chrono>
#include <cstring>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& error,const char* reason){error=reason;return false;}
// Mutation diagnostic for owner-local continuation, not a cryptographic ID.
// The caller must retain the same frozen chart; moving charts reject above.
std::uint64_t FrozenGeometryStamp(const CutSpatialField& field){
 std::uint64_t stamp=1469598103934665603ull;
 auto integer=[&](std::uint64_t x){stamp^=x;stamp*=1099511628211ull;};
 auto scalar=[&](double x){std::uint64_t bits=0;if(x!=0.)std::memcpy(&bits,&x,sizeof(bits));integer(bits);};
 integer(field.chart.regions.size());integer(field.geometry.faces.size());
 for(auto& r:field.chart.regions)for(int a=0;a<3;++a){scalar(r.lo[a]);scalar(r.hi[a]);}
 for(double c:field.geometry.startCapacity)scalar(c);
 for(double c:field.geometry.endCapacity)scalar(c);
 for(auto& f:field.geometry.faces){integer(f.left);integer(f.right);integer(f.axis);scalar(f.meanArea);for(int a=0;a<3;++a){scalar(f.lo[a]);scalar(f.hi[a]);}}
 for(auto& b:field.boundaries){integer(b.fragment);integer(b.axis);integer(b.sign);integer(b.gasOnly);}
 return stamp;
}
struct Timing {double& milliseconds;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();~Timing(){milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}};
void AddStats(CutLocalPressureResult& result,const CutFastTransportStats& before,const CutFastTransportStats& after){
 result.fastSolves+=after.solves-before.solves;result.fastCorrections+=after.corrections-before.corrections;result.fastSweeps+=after.sweeps-before.sweeps;result.fastFallbacks+=after.fallbacks-before.fallbacks;
}
bool DryGasPorts(const CutSpatialField& field,const std::vector<Real>& water,const std::vector<Real>& mass,const std::vector<Real>& density,std::string& error){
 const auto n=water.size();std::vector<double> w(n),g(n);
 for(std::size_t i=0;i<n;++i){Real capacity=field.geometry.startCapacity[i],gas=mass[i]/density[i];if(water[i].Value()<=gas.Value()){w[i]=water[i].Value();g[i]=(capacity-water[i]).Value();}else{g[i]=gas.Value();w[i]=(capacity-gas).Value();}}
 std::vector<CutRegionWaterPlane> planes;
 if(!ReconstructCutRegionPhasePlanes(field.chart,field.geometry,1,w,g,{},planes,error))return false;
 for(auto& port:field.boundaries){
  const auto& region=field.chart.regions[port.fragment];CutRegionGasVent vent{port.axis,port.sign,region.lo,region.hi};vent.lo[port.axis]=vent.hi[port.axis]=port.sign>0?region.hi[port.axis]:region.lo[port.axis];
  CutRegionFace face{};face.axis=port.axis;face.lo=vent.lo;face.hi=vent.hi;
  auto opening=MeasureCutRegionAperture(face,field.geometry.motion,face.lo,face.hi);
  auto dry=MeasureCutRegionGasVent(region,field.geometry.motion,vent,planes[port.fragment]);
  if(dry.mean!=opening.mean||dry.start!=opening.start)return Fail(error,"water covers a gas-only port; boundary topology change unsupported");
 }
 return true;
}
struct Port{std::size_t i;int axis,sign;Real g,predictor;};
bool Transport(const CutSpatialField& field,const std::vector<Real>& q,const std::vector<Port>& ports,
 const std::vector<Real>& v,std::vector<Real>& water,std::vector<Real>& mass,std::string& error,double referenceGasDensity,CutLocalTransport mode,CutLocalPressureResult& result,const CutLocalPressureResult* previous,double physicalBudget,std::unique_ptr<CutSelectiveTransport>* prepared=nullptr){
 const auto n=field.parent.size();
 if(mode==CutLocalTransport::Fast){
  // Reserve accumulation headroom without modifying any physical quantity
  // or relaxing the public per-cell/whole-step admission thresholds.
  constexpr double persistentLedgerReserve=1e-4;
  auto context=std::make_unique<CutSelectiveTransport>();std::vector<CutTransportFace> faces;std::vector<CutTransportGasPort> boundaries;std::vector<Real> sourceWater(n),sourceMass(n);
  for(std::size_t i=0;i<n;++i){sourceWater[i]=previous?previous->water[i]:Real(field.water[i]);sourceMass[i]=previous?previous->gasMass[i]:Real(field.gasMass[i]);}
  for(std::size_t f=0;f<q.size();++f)faces.push_back({field.geometry.faces[f].left,field.geometry.faces[f].right,q[f]});
  for(std::size_t j=0;j<ports.size();++j)boundaries.push_back({ports[j].i,v[j]});
  if(!context->Build(field.geometry.startCapacity,faces,boundaries,sourceWater,sourceMass,referenceGasDensity,physicalBudget*persistentLedgerReserve,error))return false;
  water=context->Water();mass=context->Mass();AddStats(result,{},context->Stats());if(prepared)*prepared=std::move(context);return true;
 }

 std::vector<CutDirectedEdge> edges;std::vector<Real> exterior(n),wrhs(n),mrhs(n),alpha,gamma;
 for(std::size_t i=0;i<n;++i){wrhs[i]=previous?previous->water[i]:Real(field.water[i]);mrhs[i]=previous?previous->gasMass[i]:Real(field.gasMass[i]);}
 for(std::size_t f=0;f<q.size();++f){auto face=field.geometry.faces[f];if(q[f].Value()>=0.)edges.push_back({face.left,face.right,q[f]});else edges.push_back({face.right,face.left,Real(0.)-q[f]});}
 for(std::size_t j=0;j<ports.size();++j){auto i=ports[j].i;if(v[j].Value()>=0.)exterior[i]=exterior[i]+v[j];else mrhs[i]=mrhs[i]-v[j]*Real(referenceGasDensity);}
 CutDirectedTransport linear;
 if(!linear.Build(field.geometry.startCapacity,edges,exterior,error))return false;

 if(!linear.SolveFraction(wrhs,1e-12,alpha,error)||!linear.SolveFraction(mrhs,referenceGasDensity*1e-12,gamma,error))return false;
 water.resize(n);mass.resize(n);for(std::size_t i=0;i<n;++i){water[i]=alpha[i]*Real(field.geometry.startCapacity[i]);mass[i]=gamma[i]*Real(field.geometry.startCapacity[i]);}return true;
}
}
std::uint64_t CutLocalGeometryStamp(const CutSpatialField& field){return FrozenGeometryStamp(field);}
bool CutLocalDryPorts(const CutSpatialField& field,const std::vector<Real>& w,const std::vector<Real>& m,
 const std::vector<Real>& p,const CutDynamicSettings& s,std::string& error){
 if(w.size()!=field.parent.size()||m.size()!=w.size()||p.size()!=w.size()||!(s.gasDensity>0.&&s.soundSpeed>0.))return Fail(error,"invalid dry-port admission dimensions or law");
 std::vector<Real> rho;for(auto x:p){Real density=Real(s.gasDensity)+x/Real(s.soundSpeed*s.soundSpeed);if(!(density.Value()>0.&&std::isfinite(density.hi)&&std::isfinite(density.lo)))return Fail(error,"invalid dry-port admission density");rho.push_back(density);}
 return DryGasPorts(field,w,m,rho,error);
}
bool AdvanceCutLocalPressure(const CutSpatialField& field,const CutDynamicSettings& settings,
 const std::vector<double>& seed,CutLocalPressureResult& output,std::string& error,CutLocalTransport mode,const CutLocalPressureResult* previous){
 const auto n=field.parent.size();if(n==0||(!seed.empty()&&seed.size()!=n)||!(settings.dt>0.))return Fail(error,"invalid local pressure input");
 if(field.chart.regions.size()!=n||field.geometry.startCapacity.size()!=n||field.water.size()!=n||field.gas.size()!=n||field.gasMass.size()!=n||field.geometry.faces.size()!=field.flux.size()||!(std::isfinite(settings.dt)&&settings.waterDensity>0.&&std::isfinite(settings.waterDensity)&&settings.gasDensity>0.&&std::isfinite(settings.gasDensity)&&settings.soundSpeed>0.&&std::isfinite(settings.soundSpeed*settings.soundSpeed)&&settings.volumeTolerance>0.&&std::isfinite(settings.volumeTolerance)&&settings.relativeVolumeTolerance>0.&&settings.relativeVolumeTolerance<1.&&settings.iterationLimit>0))return Fail(error,"invalid local pressure state or settings");
 if(field.geometry.motion&&field.geometry.motion->translation!=GridVec{})return Fail(error,"local pressure currently requires frozen solid geometry");
 for(const auto& face:field.geometry.faces)if(face.left>=n||face.right>=n||face.axis<0||face.axis>2||!(face.meanArea>=0.&&std::isfinite(face.meanArea)))return Fail(error,"invalid local pressure face");
 for(double g:settings.gravity)if(!std::isfinite(g))return Fail(error,"nonfinite local gravity");
 for(std::size_t i=0;i<n;++i){double c=field.geometry.startCapacity[i];if(!(c>0.&&std::isfinite(c)&&field.water[i]>=0.&&std::isfinite(field.water[i])&&field.gas[i]>=0.&&std::isfinite(field.gas[i])&&field.gasMass[i]>=0.&&std::isfinite(field.gasMass[i]))||(!seed.empty()&&!std::isfinite(seed[i])))return Fail(error,"invalid local material state");}
 for(auto& b:field.boundaries)if(b.fragment>=n||b.axis<0||b.axis>2||(b.sign!=1&&b.sign!=-1)||!b.gasOnly)return Fail(error,"local pressure supports explicit gas ports only");
 if(mode==CutLocalTransport::Fast)for(auto& b:field.boundaries){const auto& r=field.chart.regions[b.fragment];double boundary=b.sign>0?r.hi[b.axis]:r.lo[b.axis],exterior=b.sign>0?field.chart.layout.cells[b.axis]*field.chart.layout.h:0.;if(boundary!=exterior)return Fail(error,"gas-only port is not on the exterior chart boundary");}
 auto geometryStamp=FrozenGeometryStamp(field);
 if(previous){
  if(previous->frozenGeometryStamp!=geometryStamp||previous->waterDensity!=settings.waterDensity||previous->gasDensity!=settings.gasDensity||previous->soundSpeed!=settings.soundSpeed)return Fail(error,"continuation geometry or material law changed");
  if(previous==&output||previous->water.size()!=n||previous->gasMass.size()!=n||previous->pressure.size()!=n||previous->flux.size()!=field.flux.size()||previous->portFlux.size()!=field.boundaries.size()||!(previous->dt>0.&&std::isfinite(previous->dt)))return Fail(error,"invalid local continuation state or alias");
  for(const auto* values:{&previous->water,&previous->gasMass,&previous->pressure,&previous->flux,&previous->portFlux})for(auto x:*values)if(!std::isfinite(x.hi)||!std::isfinite(x.lo))return Fail(error,"nonfinite local continuation state");
  for(std::size_t i=0;i<n;++i)if(previous->water[i].Value()<0.||previous->gasMass[i].Value()<0.)return Fail(error,"negative local continuation quantity");
 }
 const double c2=settings.soundSpeed*settings.soundSpeed;
 std::vector<double> gas(n),phaseWater=field.water;
 for(std::size_t i=0;i<n;++i){
  gas[i]=field.gas[i];if(!previous)continue;
  Real rho=Real(settings.gasDensity)+previous->pressure[i]/Real(c2);
  if(!(rho.Value()>0.))return Fail(error,"invalid continuation EOS");
  Real actualGas=previous->gasMass[i]/rho,capacity=field.geometry.startCapacity[i];
  double tolerance=std::min(settings.volumeTolerance,settings.relativeVolumeTolerance*capacity.Value());
  if(std::abs((capacity-previous->water[i]-actualGas).Value())>tolerance)return Fail(error,"continuation source exceeds original volume tolerance");
  // One interface fills geometric capacity. Retain the smaller independent
  // phase exactly; reconcile only the dominant geometric complement within
  // the already accepted EOS defect. Physical W and M are never modified.
  if(previous->water[i].Value()<=actualGas.Value()){
   phaseWater[i]=previous->water[i].Value();gas[i]=(capacity-previous->water[i]).Value();
  }else{
   gas[i]=actualGas.Value();phaseWater[i]=(capacity-actualGas).Value();
  }
 }

 std::vector<CutRegionWaterPlane> planes;
 if(!ReconstructCutRegionPhasePlanes(field.chart,field.geometry,1,phaseWater,gas,{},planes,error))return false;
 std::vector<Real> conductance(field.flux.size()),predictor(field.flux.size());
 std::vector<CutHydroacousticGraphLink> links;
 std::vector<std::size_t> active;
 for(std::size_t f=0;f<field.flux.size();++f){auto face=field.geometry.faces[f];if(face.meanArea==0.)continue;
  auto a=MeasureCutRegionFaceInertiaVolumes(face,field.geometry.motion,field.chart.regions[face.left],true,planes[face.left]);auto b=MeasureCutRegionFaceInertiaVolumes(face,field.geometry.motion,field.chart.regions[face.right],false,planes[face.right]);
  double inertia=settings.waterDensity*(a[0]+b[0])+settings.gasDensity*(a[1]+b[1]);
  double g=settings.dt*settings.dt*face.meanArea*face.meanArea/inertia;
  conductance[f]=g;predictor[f]=settings.dt*settings.dt*face.meanArea*settings.gravity[face.axis];if(previous)predictor[f]=predictor[f]+previous->flux[f]*Real(settings.dt/previous->dt);links.push_back({face.left,face.right,g});active.push_back(f);
 }
 std::vector<Port> ports;
 for(auto& b:field.boundaries){auto r=field.chart.regions[b.fragment];double area=(r.hi[(b.axis+1)%3]-r.lo[(b.axis+1)%3])*(r.hi[(b.axis+2)%3]-r.lo[(b.axis+2)%3]);if(mode==CutLocalTransport::Fast){CutRegionFace aperture{};aperture.axis=b.axis;aperture.lo=r.lo;aperture.hi=r.hi;aperture.lo[b.axis]=aperture.hi[b.axis]=b.sign>0?r.hi[b.axis]:r.lo[b.axis];area=MeasureCutRegionAperture(aperture,field.geometry.motion,aperture.lo,aperture.hi).mean;}double g=settings.dt*settings.dt*area/(settings.gasDensity*.5*(r.hi[b.axis]-r.lo[b.axis]));ports.push_back({b.fragment,b.axis,b.sign,g,settings.dt*settings.dt*area*settings.gravity[b.axis]*b.sign});}
 if(previous)for(std::size_t j=0;j<ports.size();++j)ports[j].predictor=ports[j].predictor+previous->portFlux[j]*Real(settings.dt/previous->dt);
 std::vector<std::size_t> component(n);std::iota(component.begin(),component.end(),0);
 auto componentRoot=[&](std::size_t i){while(component[i]!=i){component[i]=component[component[i]];i=component[i];}return i;};
 for(auto& edge:links){auto a=componentRoot(edge.left),b=componentRoot(edge.right);if(a!=b)component[b]=a;}
 for(std::size_t i=0;i<n;++i)component[i]=componentRoot(i);
 CutLocalPressureResult next;next.dt=settings.dt;next.waterDensity=settings.waterDensity;next.gasDensity=settings.gasDensity;next.soundSpeed=settings.soundSpeed;next.frozenGeometryStamp=geometryStamp;next.pressure.resize(n);if(previous)next.pressure=previous->pressure;for(std::size_t i=0;i<n;++i)if(!seed.empty())next.pressure[i]=seed[i];
 std::vector<double> tolerance(n);std::vector<bool> small(n);std::vector<double> sorted=field.geometry.startCapacity;std::sort(sorted.begin(),sorted.end());double median=sorted[n/2];
 for(std::size_t i=0;i<n;++i){tolerance[i]=std::min(settings.volumeTolerance,settings.relativeVolumeTolerance*field.geometry.startCapacity[i]);small[i]=field.geometry.startCapacity[i]<.125*median;}
 std::unique_ptr<CutPressurePreconditioner> pressureAccelerator;
 std::unique_ptr<CutSelectiveTransport> physicalContext;
 bool acceptedTransport=false;
 for(int iteration=0;iteration<settings.iterationLimit;++iteration){
  next.flux.resize(field.flux.size());for(auto f:active){auto face=field.geometry.faces[f];next.flux[f]=predictor[f]+conductance[f]*(next.pressure[face.left]-next.pressure[face.right]);}
  std::vector<Real> v(ports.size());for(std::size_t j=0;j<ports.size();++j)v[j]=ports[j].predictor+ports[j].g*next.pressure[ports[j].i];
  if(!acceptedTransport){Timing time{next.transportMilliseconds};++next.transportEvaluations;if(!Transport(field,next.flux,ports,v,next.water,next.gasMass,error,settings.gasDensity,mode,next,previous,settings.volumeTolerance,&physicalContext))return false;}
  acceptedTransport=false;
  std::vector<Real> residual(n),rho(n);std::vector<double> rhs(n),storage(n);
  next.maximumVolumeResidual=next.maximumNormalizedResidual=0.;
  for(std::size_t i=0;i<n;++i){rho[i]=Real(settings.gasDensity)+next.pressure[i]/Real(c2);if(!(rho[i].Value()>0.))return Fail(error,"local EOS outside domain");residual[i]=Real(field.geometry.startCapacity[i])-next.water[i]-next.gasMass[i]/rho[i];rhs[i]=-residual[i].Value();storage[i]=(next.gasMass[i]/(rho[i]*rho[i]*Real(c2))).Value();next.maximumVolumeResidual=std::max(next.maximumVolumeResidual,std::abs(residual[i].Value()));next.maximumNormalizedResidual=std::max(next.maximumNormalizedResidual,std::abs(residual[i].Value())/tolerance[i]);}
  next.iterations=iteration+1;
  std::cout<<"LOCAL_PRESSURE iteration="<<iteration<<" normalized="<<next.maximumNormalizedResidual<<" residual="<<next.maximumVolumeResidual<<std::endl;
  if(next.maximumNormalizedResidual<=1.){for(std::size_t i=0;i<n;++i)if(next.water[i].Value()<0.||next.gasMass[i].Value()<0.||(next.water[i]-Real(field.geometry.startCapacity[i])).Value()>tolerance[i])return Fail(error,"local phase admission failed");
   Real totalW,totalM,initialW,initialM;
   for(std::size_t i=0;i<n;++i){totalW=totalW+next.water[i];totalM=totalM+next.gasMass[i];initialW=initialW+(previous?previous->water[i]:Real(field.water[i]));initialM=initialM+(previous?previous->gasMass[i]:Real(field.gasMass[i]));}
   if(mode==CutLocalTransport::Fast){
    if(!DryGasPorts(field,next.water,next.gasMass,rho,error))return false;
    for(auto w:physicalContext->WaterPortVolume()){if(w.Value()!=0.)return Fail(error,"water left a gas-only port");totalW=totalW+w;next.externalWater=next.externalWater+w;}
    auto gasFlux=physicalContext->GasPortMass();for(auto m:gasFlux){totalM=totalM+m;next.externalGasMass=next.externalGasMass+m;}
   }else{
   for(std::size_t j=0;j<ports.size();++j){auto i=ports[j].i;
    if(v[j].Value()>0.){Real w=v[j]*next.water[i]/Real(field.geometry.startCapacity[i]);if(w.Value()>0.)return Fail(error,"water reached gas-only pressure boundary");totalW=totalW+w;next.externalWater=next.externalWater+w;Real m=v[j]*next.gasMass[i]/Real(field.geometry.startCapacity[i]);totalM=totalM+m;next.externalGasMass=next.externalGasMass+m;}
    else {Real m=v[j]*Real(settings.gasDensity);totalM=totalM+m;next.externalGasMass=next.externalGasMass+m;}
   }
   }
   if(std::abs((totalW-initialW).Value())>settings.volumeTolerance||std::abs((totalM-initialM).Value())>settings.gasDensity*settings.volumeTolerance)return Fail(error,"local pressure physical quantity ledger failed");
   next.portFlux=std::move(v);output=std::move(next);error.clear();return true;}
  for(auto& port:ports)storage[port.i]+=port.g.Value();
  // Right-preconditioned GMRES applies the derivative of the actual
  // implicit two-material transport, not a frozen phase pressure graph.
  std::vector<CutTransportFace> balanceFaces;std::vector<CutTransportGasPort> balancePorts;std::vector<Real> equationResidual=residual;
  for(std::size_t f=0;f<next.flux.size();++f)balanceFaces.push_back({field.geometry.faces[f].left,field.geometry.faces[f].right,next.flux[f]});
  for(std::size_t j=0;j<ports.size();++j)balancePorts.push_back({ports[j].i,v[j]});
  auto weightedLinks=links;
  if(mode==CutLocalTransport::Fast){
   if(!ApplyCutConservativeBalance(field.geometry.startCapacity,balanceFaces,balancePorts,residual,equationResidual,error))return false;
   for(std::size_t i=0;i<n;++i){Real sourceMass=previous?previous->gasMass[i]:Real(field.gasMass[i]);storage[i]=(sourceMass/(rho[i]*rho[i]*Real(c2))).Value();}
   for(auto& port:ports)storage[port.i]+=port.g.Value();
  }else{
  for(std::size_t j=0;j<active.size();++j){auto f=active[j];auto face=field.geometry.faces[f];auto donor=next.flux[f].Value()>=0.?face.left:face.right;
   Real inverseRho=(Real(1.)/rho[face.left]+Real(1.)/rho[face.right])*Real(.5);
   auto volumeConcentration=[&](std::size_t i){return (next.water[i]+next.gasMass[i]*inverseRho).Value()/field.geometry.startCapacity[i];};
   double weight=next.flux[f].Value()==0.?.5*(volumeConcentration(face.left)+volumeConcentration(face.right)):volumeConcentration(donor);
   weightedLinks[j].conductance*=weight;
  }
  }
  std::vector<double> rowScale=storage;
  for(const auto& edge:weightedLinks){rowScale[edge.left]+=edge.conductance;rowScale[edge.right]+=edge.conductance;}
  double absoluteKrylov=1.;for(std::size_t i=0;i<n;++i){if(rowScale[i]==0.)rowScale[i]=field.geometry.startCapacity[i];absoluteKrylov=std::min(absoluteKrylov,.01*tolerance[i]/rowScale[i]);}
  if(mode==CutLocalTransport::Fast){
   Timing time{next.pressurePreparationMilliseconds};auto prepared=std::make_unique<CutPressurePreconditioner>();
   if(prepared->Build(storage,weightedLinks,small,error)){++next.pressurePreconditionerBuilds;pressureAccelerator=std::move(prepared);}else {pressureAccelerator.reset();error.clear();}
  }
  auto jacobian=[&](const std::vector<double>& direction,std::vector<double>& applied)->bool{
   Timing time{next.tangentMilliseconds};
   if(mode==CutLocalTransport::Fast){
    std::vector<Real> dq(next.flux.size()),dv(ports.size()),dw,dm,raw(n),total;
    for(auto f:active){auto face=field.geometry.faces[f];dq[f]=conductance[f]*(Real(direction[face.left])-Real(direction[face.right]));}
    for(std::size_t j=0;j<ports.size();++j)dv[j]=ports[j].g*Real(direction[ports[j].i]);
    auto before=physicalContext->Stats();bool ok=physicalContext->Tangent(dq,dv,dw,dm,error);AddStats(next,before,physicalContext->Stats());if(!ok)return false;
    for(std::size_t i=0;i<n;++i)raw[i]=Real(0.)-dw[i]-dm[i]/rho[i]+next.gasMass[i]*Real(direction[i])/(rho[i]*rho[i]*Real(c2));
    if(!ApplyCutConservativeBalanceTangent(field.geometry.startCapacity,balanceFaces,balancePorts,residual,raw,dq,dv,total,error))return false;
    applied.resize(n);for(std::size_t i=0;i<n;++i)applied[i]=total[i].Value()/rowScale[i];return true;
   }

   struct In{std::size_t donor;Real q;};std::vector<std::vector<In>> incoming(n);
   std::vector<Real> diagonal(n),dw(n),dm(n),aw(n),am(n),alpha(n),gamma(n);
   for(std::size_t i=0;i<n;++i){diagonal[i]=field.geometry.startCapacity[i];alpha[i]=next.water[i]/diagonal[i];gamma[i]=next.gasMass[i]/diagonal[i];}
   for(auto f:active){auto face=field.geometry.faces[f];Real q=next.flux[f],dq=conductance[f]*(Real(direction[face.left])-Real(direction[face.right]));
    auto from=q.Value()>=0.?face.left:face.right,to=q.Value()>=0.?face.right:face.left;
    if(q.Value()<0.){q=Real(0.)-q;dq=Real(0.)-dq;}
    diagonal[from]=diagonal[from]+q;incoming[to].push_back({from,q});
    Real upWater=q.Value()==0.?(alpha[face.left]+alpha[face.right])*Real(.5):alpha[from];Real upMass=q.Value()==0.?(gamma[face.left]+gamma[face.right])*Real(.5):gamma[from];
    dw[from]=dw[from]-dq*upWater;dw[to]=dw[to]+dq*upWater;dm[from]=dm[from]-dq*upMass;dm[to]=dm[to]+dq*upMass;
   }
   for(std::size_t j=0;j<ports.size();++j){auto i=ports[j].i;Real dq=ports[j].g*Real(direction[i]);
    if(v[j].Value()>=0.){diagonal[i]=diagonal[i]+v[j];dw[i]=dw[i]-dq*alpha[i]*Real(v[j].Value()==0.?.5:1.);dm[i]=dm[i]-dq*(v[j].Value()==0.?(gamma[i]+Real(settings.gasDensity))*Real(.5):gamma[i]);}else dm[i]=dm[i]-dq*Real(settings.gasDensity);
   }
   std::vector<CutDirectedEdge> tangentEdges;std::vector<Real> exterior(n);
   for(std::size_t i=0;i<n;++i)for(auto& edge:incoming[i])tangentEdges.push_back({edge.donor,i,edge.q});
   for(std::size_t j=0;j<ports.size();++j)if(v[j].Value()>=0.)exterior[ports[j].i]=exterior[ports[j].i]+v[j];
   CutDirectedTransport linear;if(!linear.Build(field.geometry.startCapacity,tangentEdges,exterior,error))return false;

   if(!linear.SolveFraction(dw,1e-12,aw,error)||!linear.SolveFraction(dm,settings.gasDensity*1e-12,am,error))return false;
   applied.resize(n);for(std::size_t i=0;i<n;++i){Real derivative=(Real(0.)-aw[i]-am[i]/rho[i])*Real(field.geometry.startCapacity[i])+next.gasMass[i]*Real(direction[i])/(rho[i]*rho[i]*Real(c2));applied[i]=derivative.Value()/rowScale[i];}return true;
  };
  auto dot=[](const std::vector<double>& a,const std::vector<double>& b){long double sum=0.;for(std::size_t i=0;i<a.size();++i)sum+=static_cast<long double>(a[i])*b[i];return double(sum);};
  std::vector<double> b(n);for(std::size_t i=0;i<n;++i)b[i]=-equationResidual[i].Value()/rowScale[i];
  const double norm=std::sqrt(dot(b,b));
  std::vector<std::vector<double>> basis(1,b),directions;for(auto& value:basis[0])value/=norm;
  constexpr int width=30;std::vector<std::vector<double>> hessenberg(width+1,std::vector<double>(width));
  std::vector<double> cosine(width),sine(width),projection(width+1);projection[0]=norm;int used=0;
  for(int k=0;k<width;++k){
   std::vector<double> preRhs(n),preTol(n);for(std::size_t i=0;i<n;++i){preRhs[i]=basis[k][i]*rowScale[i];}
   if(mode==CutLocalTransport::Reference){
   std::vector<Real> transportedRhs(n),concentration(n);
   for(std::size_t i=0;i<n;++i){transportedRhs[i]=preRhs[i];concentration[i]=Real(preRhs[i])/Real(field.geometry.startCapacity[i]);}
   for(auto f:active){auto face=field.geometry.faces[f];auto from=next.flux[f].Value()>=0.?face.left:face.right,to=next.flux[f].Value()>=0.?face.right:face.left;Real amount=next.flux[f].Value()>=0.?next.flux[f]:Real(0.)-next.flux[f];Real transported=amount*concentration[from];transportedRhs[from]=transportedRhs[from]+transported;transportedRhs[to]=transportedRhs[to]-transported;}
   for(std::size_t j=0;j<ports.size();++j)if(v[j].Value()>0.)transportedRhs[ports[j].i]=transportedRhs[ports[j].i]+v[j]*concentration[ports[j].i];
   for(std::size_t i=0;i<n;++i)preRhs[i]=transportedRhs[i].Value();

   }

   double rhsMax=0.;for(double value:preRhs)rhsMax=std::max(rhsMax,std::abs(value));
   for(std::size_t i=0;i<n;++i)preTol[i]=std::max(1e-300,std::max(std::abs(preRhs[i]),rhsMax*1e-6)*1e-4);
   std::vector<long double> componentRhs(n);std::vector<std::size_t> componentCount(n);std::vector<bool> grounded(n);
   for(std::size_t i=0;i<n;++i){componentRhs[component[i]]+=preRhs[i];++componentCount[component[i]];grounded[component[i]]=grounded[component[i]]||storage[i]>0.;}
   for(std::size_t i=0;i<n;++i)if(!grounded[component[i]])preRhs[i]-=double(componentRhs[component[i]]/componentCount[component[i]]);

   std::vector<double> direction(n),w;
   {Timing time{next.linearMilliseconds};++next.linearApplications;
    bool accelerated=pressureAccelerator&&pressureAccelerator->Apply(preRhs,direction,error);
    if(!accelerated){if(mode==CutLocalTransport::Fast)++next.pressurePreconditionerFallbacks;CutCondensedResult preconditioned;if(!SolveCutCondensedPressure(storage,weightedLinks,preRhs,preTol,small,preconditioned,error))return false;for(std::size_t i=0;i<n;++i)direction[i]=preconditioned.pressure[i].Value();}
   }
   if(!jacobian(direction,w))return false;
   directions.push_back(std::move(direction));
   for(int orthogonalize=0;orthogonalize<2;++orthogonalize)for(int j=0;j<=k;++j){double d=dot(w,basis[j]);hessenberg[j][k]+=d;for(std::size_t i=0;i<n;++i)w[i]-=d*basis[j][i];}
   hessenberg[k+1][k]=std::sqrt(dot(w,w));
   if(hessenberg[k+1][k]>0.){for(auto& x:w)x/=hessenberg[k+1][k];basis.push_back(std::move(w));}
   for(int j=0;j<k;++j){double a=hessenberg[j][k],bb=hessenberg[j+1][k];hessenberg[j][k]=cosine[j]*a+sine[j]*bb;hessenberg[j+1][k]=-sine[j]*a+cosine[j]*bb;}
   double length=std::hypot(hessenberg[k][k],hessenberg[k+1][k]);if(!(length>0.)){if(k>0){used=k;break;}return Fail(error,"singular coupled pressure Krylov direction");}
   cosine[k]=hessenberg[k][k]/length;sine[k]=hessenberg[k+1][k]/length;hessenberg[k][k]=length;hessenberg[k+1][k]=0.;
   projection[k+1]=-sine[k]*projection[k];projection[k]*=cosine[k];used=k+1;
   if(std::abs(projection[k+1])<std::max(absoluteKrylov,norm*.01)||basis.size()<=std::size_t(k+1))break;
  }
  std::vector<double> weights(used);for(int k=used;k-->0;){double value=projection[k];for(int j=k+1;j<used;++j)value-=hessenberg[k][j]*weights[j];weights[k]=value/hessenberg[k][k];}
  std::vector<Real> dp(n);for(int k=0;k<used;++k)for(std::size_t i=0;i<n;++i)dp[i]=dp[i]+Real(directions[k][i])*Real(weights[k]);
  double line=1.;for(std::size_t i=0;i<n;++i)if(dp[i].Value()<0.)line=std::min(line,(mode==CutLocalTransport::Fast?(Real(.9)*rho[i]*Real(c2)/(Real(0.)-dp[i])).Value():.9*(next.pressure[i].Value()+settings.gasDensity*c2)/(-dp[i].Value())));
  bool accepted=false;
  for(int backtrack=0;backtrack<24;++backtrack){
   std::vector<Real> trialP(n),trialQ(next.flux.size()),trialV(ports.size()),trialW,trialM;std::unique_ptr<CutSelectiveTransport> trialContext;
   for(std::size_t i=0;i<n;++i)trialP[i]=next.pressure[i]+dp[i]*Real(line);
   for(auto f:active){auto face=field.geometry.faces[f];trialQ[f]=predictor[f]+conductance[f]*(trialP[face.left]-trialP[face.right]);}
   for(std::size_t j=0;j<ports.size();++j)trialV[j]=ports[j].predictor+ports[j].g*trialP[ports[j].i];
   {Timing time{next.transportMilliseconds};++next.transportEvaluations;if(!Transport(field,trialQ,ports,trialV,trialW,trialM,error,settings.gasDensity,mode,next,previous,settings.volumeTolerance,&trialContext)){if(mode==CutLocalTransport::Fast){++next.rejectedTransportTrials;line*=.5;continue;}return false;}}
   std::vector<Real> trialResidual(n),trialEquation;
   for(std::size_t i=0;i<n;++i)trialResidual[i]=Real(field.geometry.startCapacity[i])-trialW[i]-trialM[i]/(Real(settings.gasDensity)+trialP[i]/Real(c2));
   if(mode==CutLocalTransport::Fast){auto trialFaces=balanceFaces;auto trialPorts=balancePorts;for(std::size_t f=0;f<trialFaces.size();++f)trialFaces[f].volume=trialQ[f];for(std::size_t j=0;j<trialPorts.size();++j)trialPorts[j].volume=trialV[j];if(!ApplyCutConservativeBalance(field.geometry.startCapacity,trialFaces,trialPorts,trialResidual,trialEquation,error))return false;}else trialEquation=std::move(trialResidual);
   double merit=0.;for(std::size_t i=0;i<n;++i){double normalized=trialEquation[i].Value()/rowScale[i];merit+=normalized*normalized;}
   if(std::sqrt(merit)<norm*(1.-1e-4*line)){next.pressure=std::move(trialP);if(mode==CutLocalTransport::Fast){next.water=std::move(trialW);next.gasMass=std::move(trialM);acceptedTransport=true;physicalContext=std::move(trialContext);}accepted=true;break;}line*=.5;
  }
  std::cout<<"NEWTON_STEP krylov="<<used<<" predictedResidual="<<std::abs(projection[used])/norm<<" line="<<line<<std::endl;
  if(!accepted)return Fail(error,"coupled pressure line search exhausted");

 }
 return Fail(error,"local pressure nonlinear closure exhausted");
}
}
