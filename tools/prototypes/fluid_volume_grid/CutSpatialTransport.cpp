#include "CutSpatialTransport.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
namespace judas::volume_grid {
namespace {
bool Fail(std::string& e,const std::string& s){e=s;return false;}

CutRegionFace Face(std::size_t l,std::size_t r,int axis,GridVec lo,GridVec hi){
 double a=(hi[(axis+1)%3]-lo[(axis+1)%3])*(hi[(axis+2)%3]-lo[(axis+2)%3]);
 return {l,r,axis,lo,hi,a,a,a,0.,0.};
}
}
bool BuildCutSpatialField(const CutRegionChart& chart,const CutRegionGeometry& geometry,
 const CutConnectedOwners& owners,const CutDynamicState& source,const std::vector<double>& flux,
 const std::vector<CutPhaseBoundary>& boundaries,CutSpatialField& output,std::string& error,bool splitMidplanes){
 const auto n=chart.regions.size();
 if(n==0||owners.lo.size()!=owners.members.size()||owners.hi.size()!=owners.members.size())return Fail(error,"invalid spatial ownership");
 if(source.water.size()!=n||source.gasMass.size()!=n||owners.owner.size()!=n||flux.size()!=geometry.faces.size()||
    (!source.pressure.empty()&&source.pressure.size()!=n))return Fail(error,"spatial source dimensions mismatch");
 for(std::size_t i=0;i<n;++i)if(owners.owner[i]>=owners.members.size())return Fail(error,"invalid spatial owner index");
 for(const auto& face:geometry.faces)if(face.left>=n||face.right>=n||face.axis<0||face.axis>2)return Fail(error,"invalid spatial face");
 for(const auto& b:boundaries)if(b.fragment>=n||b.axis<0||b.axis>2||(b.sign!=1&&b.sign!=-1)||!std::isfinite(b.volume)||!(b.incomingWater>=0.&&b.incomingWater<=1.))return Fail(error,"invalid spatial boundary");
 if(geometry.motion&&geometry.motion->translation!=GridVec{})return Fail(error,"spatial field currently requires frozen geometry");
 CutSpatialField next;next.chart.layout=chart.layout;next.chart.moving=geometry.motion;
 std::vector<std::array<std::vector<double>,3>> cuts(n);
 for(std::size_t i=0;i<n;++i)for(int a=0;a<3;++a){const auto& r=chart.regions[i];cuts[i][a]={r.lo[a],r.hi[a]};if(splitMidplanes&&r.hi[a]-r.lo[a]>=.25*chart.layout.h)cuts[i][a].push_back(r.lo[a]+.5*(r.hi[a]-r.lo[a]));}
 for(const auto& f:geometry.faces)for(auto i:{f.left,f.right})for(int a=0;a<3;++a)if(a!=f.axis){cuts[i][a].push_back(f.lo[a]);cuts[i][a].push_back(f.hi[a]);}
 std::vector<double> gas(n);
 for(std::size_t i=0;i<n;++i){double rho=1.2+(source.pressure.empty()?0.:source.pressure[i])/(343.*343.);if(!(rho>0.&&std::isfinite(rho)))return Fail(error,"invalid spatial source density");gas[i]=source.gasMass[i]/rho;}
 std::vector<CutRegionWaterPlane> planes;
 if(!ReconstructCutRegionPhasePlanes(chart,geometry,1,source.water,gas,source.interfaces,planes,error))return false;
 std::vector<std::vector<std::size_t>> children(n);
 for(std::size_t i=0;i<n;++i){auto& c=cuts[i];
  for(int a=0;a<3;++a){auto& v=c[a];std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());}
  for(std::size_t x=1;x<c[0].size();++x)for(std::size_t y=1;y<c[1].size();++y)for(std::size_t z=1;z<c[2].size();++z){
   GridVec lo{c[0][x-1],c[1][y-1],c[2][z-1]},hi{c[0][x],c[1][y],c[2][z]};
   auto v=MeasureCutRegionPhaseVolumes(chart.regions[i],geometry.motion,lo,hi,planes[i]);
   if(v[0]+v[1]==0.)continue;
   if(next.parent.size()>=1000000)return Fail(error,"spatial prototype cell budget exceeded");
   auto j=next.parent.size();children[i].push_back(j);next.parent.push_back(i);next.chart.regions.push_back({lo,hi,chart.regions[i].parent});next.water.push_back(v[0]);next.gas.push_back(v[1]);next.gasMass.push_back(v[1]*(1.2+(source.pressure.empty()?0.:source.pressure[i])/(343.*343.)));
  }
 }
 // Internal child faces are derived from shared bounds, never from a parent
 // bounding-box connection across a solid or a missing fragment.
 for(std::size_t p=0;p<n;++p)for(auto l:children[p])for(auto r:children[p])if(l<r){
  const auto& a=next.chart.regions[l];const auto& b=next.chart.regions[r];
  for(int axis=0;axis<3;++axis){bool forward=a.hi[axis]==b.lo[axis],reverse=b.hi[axis]==a.lo[axis];if(!forward&&!reverse)continue;
   GridVec lo{},hi{};bool open=true;for(int t=0;t<3;++t)if(t!=axis){lo[t]=std::max(a.lo[t],b.lo[t]);hi[t]=std::min(a.hi[t],b.hi[t]);open=open&&hi[t]>lo[t];}
   if(!open)continue;
   lo[axis]=hi[axis]=forward?a.hi[axis]:b.hi[axis];next.chart.faces.push_back(Face(forward?l:r,forward?r:l,axis,lo,hi));next.originalFace.push_back(geometry.faces.size());
  }
 }
 for(std::size_t f=0;f<geometry.faces.size();++f){const auto& face=geometry.faces[f];if(face.meanArea==0.)continue;
  for(auto l:children[face.left])for(auto r:children[face.right]){
   const auto& a=next.chart.regions[l];const auto& b=next.chart.regions[r];if(a.hi[face.axis]!=face.lo[face.axis]||b.lo[face.axis]!=face.lo[face.axis])continue;
   GridVec lo=face.lo,hi=face.hi;bool open=true;for(int t=0;t<3;++t)if(t!=face.axis){lo[t]=std::max({lo[t],a.lo[t],b.lo[t]});hi[t]=std::min({hi[t],a.hi[t],b.hi[t]});open=open&&hi[t]>lo[t];}
   if(open){next.chart.faces.push_back(Face(l,r,face.axis,lo,hi));next.originalFace.push_back(f);}
  }
 }
 next.chart.movingRegions.resize(next.parent.size());std::iota(next.chart.movingRegions.begin(),next.chart.movingRegions.end(),0);
 next.chart.movingFaces.resize(next.chart.faces.size());std::iota(next.chart.movingFaces.begin(),next.chart.movingFaces.end(),0);
 if(!BuildCutRegionGeometry(next.chart,{},next.geometry,error))return false;
 next.flux.resize(next.chart.faces.size());next.mobility.resize(next.flux.size());
 for(std::size_t f=0;f<next.flux.size();++f){auto& a=next.geometry.faces[f];double dl=next.chart.regions[a.left].hi[a.axis]-next.chart.regions[a.left].lo[a.axis],dr=next.chart.regions[a.right].hi[a.axis]-next.chart.regions[a.right].lo[a.axis];
  if(a.meanArea>0.)next.mobility[f]=a.meanArea/(.5*(dl+dr));
  if(next.originalFace[f]<flux.size())next.flux[f]=flux[next.originalFace[f]]*a.meanArea/geometry.faces[next.originalFace[f]].meanArea;
 }
 for(const auto& b:boundaries){const auto& r=chart.regions[b.fragment];int a=b.axis;double totalArea=(r.hi[(a+1)%3]-r.lo[(a+1)%3])*(r.hi[(a+2)%3]-r.lo[(a+2)%3]);
  for(auto j:children[b.fragment]){const auto& child=next.chart.regions[j];if((b.sign>0?child.hi[a]:child.lo[a])!=(b.sign>0?r.hi[a]:r.lo[a]))continue;
   double area=(child.hi[(a+1)%3]-child.lo[(a+1)%3])*(child.hi[(a+2)%3]-child.lo[(a+2)%3]);auto port=b;port.fragment=j;port.volume*=area/totalArea;next.boundaries.push_back(port);
  }
 }
 auto& map=next.fragments;map.owner.resize(next.parent.size());
 std::vector<std::size_t> compact(owners.members.size(),owners.members.size());
 for(std::size_t i=0;i<next.parent.size();++i){auto physical=owners.owner[next.parent[i]];
  if(compact[physical]==owners.members.size()){compact[physical]=map.members.size();map.members.emplace_back();map.capacity.push_back(0.);map.lo.push_back(owners.lo[physical]);map.hi.push_back(owners.hi[physical]);}
  auto o=compact[physical];map.owner[i]=o;map.members[o].push_back(i);map.capacity[o]+=next.geometry.startCapacity[i];
 }
 std::vector<CutBoundaryTransfer> vents;for(auto& b:next.boundaries)vents.push_back({b.fragment,b.volume});
 CutOwnedFlux field;std::vector<double> predictor(next.flux.size());
 std::vector<double> net(map.members.size()),sourceTerms(next.parent.size());
 for(std::size_t f=0;f<next.flux.size();++f){auto face=next.geometry.faces[f];if(map.owner[face.left]!=map.owner[face.right]){net[map.owner[face.left]]+=next.flux[f];net[map.owner[face.right]]-=next.flux[f];}}
 for(auto& v:vents)net[map.owner[v.fragment]]+=v.volume;
 for(std::size_t o=0;o<map.members.size();++o){double g=0.;for(auto i:map.members[o])g+=next.gas[i];double assigned=0.;auto last=map.members[o].back();
  for(auto i:map.members[o])if(i!=last){sourceTerms[i]=net[o]*(g>0.?next.gas[i]/g:next.geometry.startCapacity[i]/map.capacity[o]);assigned+=sourceTerms[i];}
  sourceTerms[last]=net[o]-assigned;
 }
 if(!ReconstructCutOwnedFlux(next.chart,next.geometry,map,next.flux,vents,next.gas,predictor,next.mobility,field,error,1e-10,false,nullptr,&sourceTerms))return false;
 next.flux=std::move(field.faceVolume);output=std::move(next);error.clear();return true;
}
bool AdvanceCutSpatial(const CutSpatialField& field,const CutConnectedOwners& owners,
 double h,CutSpatialResult& output,std::string& error,CutSpatialResult* diagnostic){
 if(diagnostic==&output)return Fail(error,"spatial diagnostic must not alias transport output");
 const auto n=field.parent.size();if(!(h>0.&&std::isfinite(h)))return Fail(error,"invalid spatial transport interval");
 struct Incoming{std::size_t from;double q;};std::vector<std::vector<Incoming>> incoming(n);
 std::vector<double> diag=field.geometry.startCapacity,rhs=field.water,massRhs=field.gasMass,alpha(n),gamma(n);
 for(std::size_t i=0;i<n;++i){alpha[i]=field.water[i]/diag[i];gamma[i]=field.gasMass[i]/diag[i];}
 for(std::size_t f=0;f<field.flux.size();++f){auto face=field.geometry.faces[f];double q=h*field.flux[f];auto from=q>=0.?face.left:face.right,to=q>=0.?face.right:face.left;q=std::abs(q);diag[from]+=q;incoming[to].push_back({from,q});}
 for(auto& b:field.boundaries)if(b.volume>=0.)diag[b.fragment]+=h*b.volume;else {rhs[b.fragment]-=h*b.volume*b.incomingWater;massRhs[b.fragment]-=h*b.volume*1.2*(1.-b.incomingWater);}
 CutSpatialResult next;next.fraction=alpha;
 for(std::size_t iteration=0;iteration<20000;++iteration){
  for(std::size_t i=0;i<n;++i){double v=rhs[i];for(auto& e:incoming[i])v+=e.q*alpha[e.from];alpha[i]=v/diag[i];double m=massRhs[i];for(auto& e:incoming[i])m+=e.q*gamma[e.from];gamma[i]=m/diag[i];}
  double residual=0.;for(std::size_t i=0;i<n;++i){double r=rhs[i]-diag[i]*alpha[i];for(auto& e:incoming[i])r+=e.q*alpha[e.from];residual=std::max(residual,std::abs(r)/diag[i]);double rm=massRhs[i]-diag[i]*gamma[i];for(auto& e:incoming[i])rm+=e.q*gamma[e.from];residual=std::max(residual,std::abs(rm)/(1.2*diag[i]));}
  next.iterations=iteration+1;next.residual=residual;if(residual<1e-12)break;
  if(iteration==19999)return Fail(error,"spatial transport linear solve exhausted");
 }
 next.water.resize(n);next.gasMass.resize(n);for(std::size_t i=0;i<n;++i)next.gasMass[i]=gamma[i]*field.geometry.startCapacity[i];next.fraction=alpha;next.ownerWater.assign(owners.members.size(),0.);next.ownerGasMass.assign(owners.members.size(),0.);
 bool admissible=true;
 for(std::size_t i=0;i<n;++i){
  next.maximumFraction=std::max(next.maximumFraction,alpha[i]);
  if(!(alpha[i]>=0.&&alpha[i]<=1.+1e-10&&std::isfinite(alpha[i])))admissible=false;
  next.water[i]=alpha[i]*field.geometry.startCapacity[i];auto o=owners.owner[field.parent[i]];next.ownerWater[o]+=field.water[i];next.ownerGasMass[o]+=field.gasMass[i];}
 for(std::size_t f=0;f<field.flux.size();++f){auto face=field.geometry.faces[f];double q=h*field.flux[f];auto from=q>=0.?face.left:face.right;double w=q*alpha[from],m=q*gamma[from];auto l=owners.owner[field.parent[face.left]],r=owners.owner[field.parent[face.right]];

  if(l!=r){next.ownerWater[l]-=w;next.ownerWater[r]+=w;next.ownerGasMass[l]-=m;next.ownerGasMass[r]+=m;}}
 for(auto& b:field.boundaries){auto o=owners.owner[field.parent[b.fragment]];double q=h*b.volume;double w=q*(q>=0.?alpha[b.fragment]:b.incomingWater),m=q*(q>=0.?gamma[b.fragment]:1.2*(1.-b.incomingWater));
  if(b.gasOnly&&w>0.)admissible=false;

  next.ownerWater[o]-=w;next.ownerGasMass[o]-=m;next.externalWater+=w;next.externalGasMass+=m;}
 std::vector<double> spatialWater(next.ownerWater.size()),spatialMass(spatialWater.size());
 for(std::size_t i=0;i<n;++i){auto o=owners.owner[field.parent[i]];spatialWater[o]+=next.water[i];spatialMass[o]+=next.gasMass[i];}
 for(std::size_t o=0;o<next.ownerWater.size();++o){
  double w=spatialWater[o],m=spatialMass[o];
  const double budget=1e-10*owners.capacity[o];
  if(std::abs(w-next.ownerWater[o])>budget||std::abs(m-next.ownerGasMass[o])>1.2*budget||next.ownerWater[o]<0.||next.ownerWater[o]>owners.capacity[o]+budget||next.ownerGasMass[o]<0.)admissible=false;
 }
 if(diagnostic)*diagnostic=next;
 if(!admissible)return Fail(error,"spatial candidate fails capacity or gas-only port admission");
 output=std::move(next);error.clear();return true;
}
}
