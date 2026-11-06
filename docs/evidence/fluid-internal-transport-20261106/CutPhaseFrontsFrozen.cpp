#include "CutPhaseFronts.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace judas::volume_grid {
namespace {
struct Sum {double v=0.,c=0.;void Add(double x){double n=v+x;c+=std::abs(v)>=std::abs(x)?(v-n)+x:(x-n)+v;v=n;}double Get()const{return v+c;}};
struct State {std::vector<double>w,g;std::vector<CutRegionInterface> orientation;};
struct Rate {std::vector<double>w,faceW,ownerW,ownerM;double externalW=0.,externalM=0.;};
bool Fail(std::string& e,const std::string& s){e=s;return false;}
struct Context {
 const CutRegionChart& chart;const CutRegionGeometry& geometry;const CutConnectedOwners& owners;
 const std::vector<double>& density;const std::vector<double>& external;
 const std::vector<CutPhaseBoundary>& boundary;const std::vector<double>& predictor;const std::vector<double>& mobility;
 double referenceGasDensity;
 const std::vector<double>& ownerSources;
 bool Evaluate(State& state,Rate& rate,std::string& error,const State* leftLimit=nullptr)const{
  const auto n=state.w.size(),no=owners.members.size();
  std::vector<CutRegionWaterPlane> planes;
  if(!ReconstructCutRegionPhasePlanes(chart,geometry,1,state.w,state.g,state.orientation,planes,error))return false;
  std::vector<CutBoundaryTransfer> vents;for(auto& b:boundary)vents.push_back({b.fragment,b.volume});
  CutOwnedFlux field;
  if(!ReconstructCutOwnedFlux(chart,geometry,owners,external,vents,state.g,predictor,mobility,field,error,1e-10,false,&ownerSources))return false;
  // At an analytically scheduled phase endpoint, the final RK stage samples
  // the limit from inside this material episode, not the next episode.
  if(leftLimit)for(std::size_t i=0;i<n;++i){
   const bool above=planes[i].orientation.wetAbove;
   if(state.g[i]==0. && leftLimit->g[i]>0.)planes[i].remainder+=above?std::numeric_limits<double>::denorm_min():-std::numeric_limits<double>::denorm_min();
   if(state.w[i]==0. && leftLimit->w[i]>0.)planes[i].remainder+=above?-std::numeric_limits<double>::denorm_min():std::numeric_limits<double>::denorm_min();
  }
  rate.w.assign(n,0.);rate.faceW.assign(geometry.faces.size(),0.);rate.ownerW.assign(no,0.);rate.ownerM.assign(no,0.);
  rate.externalW=rate.externalM=0.;
  std::vector<Sum> outW(n),outG(n);std::vector<GridVec> inW(n),inG(n);
  const auto transfer=[&](std::size_t from,std::size_t to,int axis,double q,double fw,double fg){
   const double w=q*fw,g=q*fg;
   outW[from].Add(w);outW[to].Add(-w);outG[from].Add(g);outG[to].Add(-g);
   const auto donor=q>=0.?from:to,receiver=q>=0.?to:from;
   inW[receiver][axis]+=std::copysign(std::abs(w),q);inG[receiver][axis]+=std::copysign(std::abs(g),q);
   const auto a=owners.owner[from],b=owners.owner[to];
   const double m=g*density[owners.owner[donor]];
   if(a!=b){rate.ownerW[a]-=w;rate.ownerW[b]+=w;rate.ownerM[a]-=m;rate.ownerM[b]+=m;}
   return w;
  };
  for(std::size_t f=0;f<geometry.faces.size();++f){const auto& face=geometry.faces[f];const double q=field.faceVolume[f];if(q==0.)continue;
   const auto phase=MeasureCutRegionFacePhase(face,geometry.motion,planes[face.left],planes[face.right]);
   const int side=q>=0.?0:1;
   double fw=0.*phase.wet[side].mean/face.meanArea,fg=1.+0.*phase.dry[side].mean/face.meanArea;
   if(!(fw>=0.&&fg>=0.&&std::abs(fw+fg-1.)<=512.*std::numeric_limits<double>::epsilon()))return Fail(error,"phase face fractions do not partition an actual aperture");
   if(fw<fg)fg=1.-fw;else fw=1.-fg;
   rate.faceW[f]=transfer(face.left,face.right,face.axis,q,fw,fg);
  }
  for(const auto& b:boundary){const auto i=b.fragment;double fw=b.incomingWater,fg=1.-fw;
   if(b.volume>0.){
    const auto& r=chart.regions[i];CutRegionFace face{};face.axis=b.axis;face.lo=r.lo;face.hi=r.hi;
    face.lo[b.axis]=face.hi[b.axis]=b.sign>0?r.hi[b.axis]:r.lo[b.axis];
    const double area=(face.hi[(b.axis+1)%3]-face.lo[(b.axis+1)%3])*(face.hi[(b.axis+2)%3]-face.lo[(b.axis+2)%3]);
    face.meanArea=face.startArea=face.endArea=area;
    const auto p=MeasureCutRegionFacePhase(face,geometry.motion,planes[i],planes[i]);
    fw=p.wet[b.sign>0?0:1].mean/area;fg=p.dry[b.sign>0?0:1].mean/area;
    if(b.gasOnly && fw>0.)return Fail(error,"water reached a gas-only exterior port; opening must be rebuilt");
   }
   double w=b.volume*fw,g=b.volume*fg;outW[i].Add(w);outG[i].Add(g);
   const auto o=owners.owner[i];rate.ownerW[o]-=w;
   const double rho=b.volume>=0.?density[o]:referenceGasDensity;
   rate.ownerM[o]-=g*rho;rate.externalW+=w;rate.externalM+=g*rho;
   if(b.volume<0.){inW[i][b.axis]+=w*b.sign;inG[i][b.axis]+=g*b.sign;}
  }
  for(std::size_t i=0;i<n;++i){
   // Evaluate the minority phase to avoid subtracting two water-size flows
   // to recover a real, much smaller gas volume.
   rate.w[i]=state.w[i]<=state.g[i]?-outW[i].Get():outG[i].Get()-field.divergence[i];
   bool iw=false,ig=false;for(int a=0;a<3;++a){iw=iw||inW[i][a]!=0.;ig=ig||inG[i][a]!=0.;}
   const bool phaseEndpoint=leftLimit&&((state.g[i]==0.&&leftLimit->g[i]>0.)||(state.w[i]==0.&&leftLimit->w[i]>0.));
   if(!phaseEndpoint && state.g[i]==0.&&!ig)rate.w[i]=0.;
   if(!phaseEndpoint && state.w[i]==0.&&!iw)rate.w[i]=0.;
   if(phaseEndpoint)continue;
   const GridVec* arrival=state.w[i]==0.&&iw?&inW[i]:(state.g[i]==0.&&ig?&inG[i]:nullptr);
   if(arrival){int axis=0;for(int a=1;a<3;++a)if(std::abs((*arrival)[a])>std::abs((*arrival)[axis]))axis=a;
    state.orientation[i]={axis,state.w[i]==0.?(*arrival)[axis]<0.:(*arrival)[axis]>0.};}
  }
  return true;
 }
};
bool Add(const State& start,const Rate& a,const Rate* b,double h,State& end,std::string& error){
 end=start;
 for(std::size_t i=0;i<start.w.size();++i){double d=h*(b?.5*(a.w[i]+b->w[i]):a.w[i]);
  const double available=d>=0.?start.g[i]:start.w[i];
  if(std::abs(d)>available){const double bound=32.*std::numeric_limits<double>::epsilon()*available;
   if(std::abs(d)-available>bound){std::ostringstream s;s.precision(17);s<<"fragment="<<i<<" water="<<start.w[i]<<" gas="<<start.g[i]<<" deltaWater="<<d<<" firstRate="<<a.w[i]<<" endpointRate="<<(b?b->w[i]:a.w[i])<<" interval="<<h;return Fail(error,s.str());}
   // Certified endpoint canonicalization affects only the geometric view.
   // Primary owner quantities still come from paired integrated face fluxes.
   d=std::copysign(available,d);
  }
  end.w[i]=start.w[i]+d;end.g[i]=start.g[i]-d;
  if(!(end.w[i]>=0.&&end.g[i]>=0.&&std::isfinite(end.w[i]+end.g[i])))return false;
 }
 return true;
}
bool Heun(const Context& c,State start,double h,State& end,Rate& integrated,std::string& error){
 Rate a,b;if(!c.Evaluate(start,a,error))return false;State e;
 if(!Add(start,a,nullptr,h,e,error))return false;
 if(!c.Evaluate(e,b,error,&start))return false;
 if(!Add(start,a,&b,h,end,error))return false;
 integrated=a;
 for(std::size_t i=0;i<a.faceW.size();++i)integrated.faceW[i]=h*.5*(a.faceW[i]+b.faceW[i]);
 for(std::size_t i=0;i<a.ownerW.size();++i){integrated.ownerW[i]=h*.5*(a.ownerW[i]+b.ownerW[i]);integrated.ownerM[i]=h*.5*(a.ownerM[i]+b.ownerM[i]);}
 integrated.externalW=h*.5*(a.externalW+b.externalW);integrated.externalM=h*.5*(a.externalM+b.externalM);
 return true;
}
}
bool AdvanceCutPhaseFronts(const CutRegionChart& chart,const CutRegionGeometry& geometry,
 const CutConnectedOwners& owners,const CutDynamicState& source,const std::vector<double>& density,
 const std::vector<double>& external,const std::vector<CutPhaseBoundary>& boundary,
 const std::vector<double>& predictor,const std::vector<double>& mobility,const CutPhaseFrontSettings& settings,
 CutPhaseFrontResult& output,std::string& error){
 const auto n=chart.regions.size(),no=owners.members.size();
 if(source.water.size()!=n||source.gasMass.size()!=n||density.size()!=no||(!source.pressure.empty()&&source.pressure.size()!=n)||source.interfaces.size()>n||!(settings.relativeError>0.&&std::isfinite(settings.relativeError)&&settings.relativeError<1.&&settings.maximumSteps>0&&settings.referenceGasDensity>0.&&std::isfinite(settings.referenceGasDensity)&&settings.soundSpeed>0.&&std::isfinite(settings.soundSpeed*settings.soundSpeed)))return Fail(error,"invalid phase-front dimensions");
 for(double rho:density)if(!(rho>0.&&std::isfinite(rho)))return Fail(error,"invalid final owner gas density");
 for(const auto& b:boundary)if(b.fragment>=n||b.axis<0||b.axis>2||(b.sign!=1&&b.sign!=-1)||!std::isfinite(b.volume)||!(b.incomingWater>=0.&&b.incomingWater<=1.))return Fail(error,"invalid phase boundary");
 State state;state.w=source.water;state.g.resize(n);state.orientation=source.interfaces;state.orientation.resize(n);
 CutPhaseFrontResult next;next.waterFlux.assign(geometry.faces.size(),0.);next.ownerWater.assign(no,0.);next.ownerGasMass.assign(no,0.);
 for(std::size_t i=0;i<n;++i){const auto o=owners.owner[i];
  const double rho=settings.referenceGasDensity+(source.pressure.empty()?0.:source.pressure[i])/(settings.soundSpeed*settings.soundSpeed);
  if(!(rho>0.&&std::isfinite(rho)))return Fail(error,"invalid initial gas density");
  state.g[i]=source.gasMass[i]/rho;next.ownerWater[o]+=source.water[i];next.ownerGasMass[o]+=source.gasMass[i];}
 std::vector<CutBoundaryTransfer> vents;for(const auto& b:boundary)vents.push_back({b.fragment,b.volume});
 CutOwnedFlux fixed;
 if(!ReconstructCutOwnedFlux(chart,geometry,owners,external,vents,state.g,predictor,mobility,fixed,error))return false;
 // A trial has one pressure field. Do not release an acoustic constraint
 // discontinuously when a geometric fragment acquires its first gas sliver.
 // A whole-owner phase exhaustion still requires a new pressure episode.
 std::vector<double> ownerSources(no);
 for(std::size_t o=0;o<no;++o){Sum d;for(auto i:owners.members[o])d.Add(fixed.divergence[i]);ownerSources[o]=d.Get();}
 Context c{chart,geometry,owners,density,fixed.faceVolume,boundary,predictor,mobility,settings.referenceGasDensity,ownerSources};
 const auto initialWater=next.ownerWater,initialMass=next.ownerGasMass;
 std::vector<Sum> accumulatedWater(no),accumulatedMass(no);
 double time=0.,h=1.;std::string lastFailure;
 while(time<1.){
  if(next.steps+next.rejectedSteps>=settings.maximumSteps){std::ostringstream s;s.precision(17);s<<"phase-front temporal budget exhausted at fraction="<<time<<" h="<<h<<" steps="<<next.steps<<" rejected="<<next.rejectedSteps<<" last="<<lastFailure;return Fail(error,s.str());}
  Rate initial;if(!c.Evaluate(state,initial,error))return false;
  h=std::min(h,1.-time);
  for(std::size_t i=0;i<n;++i)if(initial.w[i]!=0.)h=std::min(h,(initial.w[i]>0.?state.g[i]:state.w[i])/std::abs(initial.w[i]));
  if(!(h>0.&&time+h>time)){std::ostringstream s;s.precision(17);s<<"phase event has no representable time increment at fraction="<<time<<" h="<<h;return Fail(error,s.str());}
  State full,half,end;Rate fullFlux,a,b;std::string trialError;
  const bool ok=Heun(c,state,h,full,fullFlux,trialError)&&Heun(c,state,.5*h,half,a,trialError)&&Heun(c,half,.5*h,end,b,trialError);
  std::vector<double> errors(no);
  if(ok)for(std::size_t i=0;i<n;++i)errors[owners.owner[i]]+=std::max(std::abs(full.w[i]-end.w[i]),std::abs(full.g[i]-end.g[i]));
  bool accurate=ok;
  for(std::size_t o=0;o<no;++o)accurate=accurate&&errors[o]<=settings.relativeError*h*owners.capacity[o];
  if(!accurate){lastFailure=trialError.empty()?"embedded temporal error exceeds tolerance":trialError;++next.rejectedSteps;h*=.5;continue;}
  for(std::size_t f=0;f<next.waterFlux.size();++f)next.waterFlux[f]+=a.faceW[f]+b.faceW[f];
  for(std::size_t o=0;o<no;++o){accumulatedWater[o].Add(a.ownerW[o]+b.ownerW[o]);accumulatedMass[o].Add(a.ownerM[o]+b.ownerM[o]);}
  next.externalWater+=a.externalW+b.externalW;next.externalGasMass+=a.externalM+b.externalM;
  state=std::move(end);time+=h;++next.steps;h*=1.5;
 }
 next.water=std::move(state.w);next.gas=std::move(state.g);next.interfaces=std::move(state.orientation);
 for(std::size_t o=0;o<no;++o){
  next.ownerWater[o]=initialWater[o]+accumulatedWater[o].Get();
  next.ownerGasMass[o]=initialMass[o]+accumulatedMass[o].Get();
  Sum partition;for(auto i:owners.members[o])partition.Add(next.water[i]);
  const double errorVolume=std::abs(partition.Get()-next.ownerWater[o]);next.maximumPartitionError=std::max(next.maximumPartitionError,errorVolume);
  if(errorVolume>settings.relativeError*owners.capacity[o])return Fail(error,"phase-front spatial partition does not match its conservative owner ledger");
 }
 output=std::move(next);error.clear();return true;
}
}
