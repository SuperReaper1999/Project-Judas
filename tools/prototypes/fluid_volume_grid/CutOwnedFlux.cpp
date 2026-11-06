#include "CutOwnedFlux.h"
#include "CutHydroacousticGraph.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace judas::volume_grid {
namespace {
struct Sum { double v=0.,c=0.; void Add(double x){double n=v+x;c+=std::abs(v)>=std::abs(x)?(v-n)+x:(x-n)+v;v=n;} double Get()const{return v+c;} };
bool Fail(std::string& e,const std::string& s){e=s;return false;}
bool Solve(std::vector<long double> a,std::vector<long double> rhs,std::vector<long double>& x){
 const std::size_t n=rhs.size();std::vector<long double> scale(n);
 for(std::size_t i=0;i<n;++i){if(!(a[i*n+i]>0. && std::isfinite(a[i*n+i])))return false;scale[i]=std::sqrt(a[i*n+i]);}
 for(std::size_t i=0;i<n;++i){rhs[i]/=scale[i];for(std::size_t j=0;j<n;++j)a[i*n+j]/=scale[i]*scale[j];}
 for(std::size_t k=0;k<n;++k){std::size_t pivot=k;for(std::size_t i=k+1;i<n;++i)if(std::abs(a[i*n+k])>std::abs(a[pivot*n+k]))pivot=i;
  if(!(std::abs(a[pivot*n+k])>0. && std::isfinite(a[pivot*n+k])))return false;
  if(pivot!=k){for(std::size_t j=k;j<n;++j)std::swap(a[k*n+j],a[pivot*n+j]);std::swap(rhs[k],rhs[pivot]);}
  for(std::size_t i=k+1;i<n;++i){long double f=a[i*n+k]/a[k*n+k];for(std::size_t j=k+1;j<n;++j)a[i*n+j]-=f*a[k*n+j];rhs[i]-=f*rhs[k];}
 }
 x.assign(n,0.);for(std::size_t i=n;i-->0;){long double b=rhs[i];for(std::size_t j=i+1;j<n;++j)b-=a[i*n+j]*x[j];x[i]=b/a[i*n+i];if(!std::isfinite(x[i]))return false;}
 for(std::size_t i=0;i<n;++i) x[i]/=scale[i];
 return true;
}
}
bool ReconstructCutOwnedFlux(const CutRegionChart& chart,const CutRegionGeometry& geometry,
 const CutConnectedOwners& owners,const std::vector<double>& external,
 const std::vector<CutBoundaryTransfer>& vents,const std::vector<double>& gasVolume,
 const std::vector<double>& predictor,const std::vector<double>& mobility,
 CutOwnedFlux& output,std::string& error,double relativePressureResidual,bool projectExternal,const std::vector<double>* ownerSources,const std::vector<double>* fragmentSources){
 const auto n=chart.regions.size(),nf=geometry.faces.size();
 if((fragmentSources&&fragmentSources->size()!=n)||(ownerSources&&ownerSources->size()!=owners.members.size())||geometry.startCapacity.size()!=n||owners.capacity.size()!=owners.members.size()||!(relativePressureResidual>=0.&&std::isfinite(relativePressureResidual)) || owners.owner.size()!=n || gasVolume.size()!=n || external.size()!=nf || predictor.size()!=nf || mobility.size()!=nf)
  return Fail(error,"internal flux dimensions do not match geometry");
 std::vector<bool> seen(n);
 for(std::size_t o=0;o<owners.members.size();++o){
  if(owners.members[o].empty()||!(owners.capacity[o]>=0.&&std::isfinite(owners.capacity[o])))return Fail(error,"invalid material owner");
  for(auto m:owners.members[o]){if(m>=n||seen[m]||owners.owner[m]!=o)return Fail(error,"inconsistent material mapping");seen[m]=true;}
 }
 if(std::find(seen.begin(),seen.end(),false)!=seen.end())return Fail(error,"unowned geometry fragment");
 for(std::size_t f=0;f<nf;++f)if(!std::isfinite(predictor[f])||!(mobility[f]>=0.&&std::isfinite(mobility[f])))return Fail(error,"invalid kinetic metric");
 CutOwnedFlux next;next.faceVolume=external;next.divergence.assign(n,0.);
 // The instantaneous field must be incompressible in owners without gas.
 // Gas owners retain the proposed pressure. Solve the constrained kinetic
 // correction rather than treating a finite divergence as solver roundoff.
 const auto no=owners.members.size();
 std::vector<Sum> net(no),gasByOwner(no);
 std::vector<std::vector<std::size_t>> internal(no);
 for(std::size_t i=0;i<n;++i){
  if(!(gasVolume[i]>=0.&&std::isfinite(gasVolume[i])))return Fail(error,"invalid gas geometry");
  gasByOwner[owners.owner[i]].Add(gasVolume[i]);
 }
 for(std::size_t f=0;f<nf;++f){const auto& face=geometry.faces[f];
  if(face.left>=n||face.right>=n||!std::isfinite(external[f]))return Fail(error,"invalid external face");
  const auto l=owners.owner[face.left],r=owners.owner[face.right];
  if(l==r){if(face.meanArea>0.)internal[l].push_back(f);next.faceVolume[f]=predictor[f];}
  else {net[l].Add(external[f]);net[r].Add(-external[f]);}
 }
 for(const auto& v:vents){if(v.fragment>=n||!std::isfinite(v.volume))return Fail(error,"invalid internal vent");net[owners.owner[v.fragment]].Add(v.volume);}
 std::vector<std::size_t> row(no,no);std::size_t nr=0;
 for(std::size_t o=0;o<no;++o)if(gasByOwner[o].Get()==0.&&owners.capacity[o]>0.)row[o]=nr++;
 if(nr&&projectExternal){
  std::vector<double> diagonal(nr),rhs(nr),tolerance(nr);
  std::vector<CutHydroacousticGraphLink> links;
  for(std::size_t o=0;o<no;++o)if(row[o]!=no){rhs[row[o]]=-net[o].Get();// Reserve the remaining error budget for disaggregation into spatial fragments.
   tolerance[row[o]]=.01*std::max(relativePressureResidual,4096.*std::numeric_limits<double>::epsilon())*owners.capacity[o];}
  for(std::size_t f=0;f<nf;++f){const auto& face=geometry.faces[f];auto l=owners.owner[face.left],r=owners.owner[face.right];if(l==r||face.meanArea==0.)continue;
   const double w=mobility[f];if(!(w>0.&&std::isfinite(w)))return Fail(error,"invalid external kinetic mobility");
   if(row[l]!=no&&row[r]!=no)links.push_back({row[l],row[r],w});
   else if(row[l]!=no)diagonal[row[l]]+=w;
   else if(row[r]!=no)diagonal[row[r]]+=w;
  }
  CutHydroacousticGraphResult correction;
  if(!SolveCutHydroacousticGraphLocal(diagonal,links,rhs,tolerance,correction,error))return false;
  for(std::size_t f=0;f<nf;++f){const auto& face=geometry.faces[f];auto l=owners.owner[face.left],r=owners.owner[face.right];if(l==r)continue;
   double dq=mobility[f]*((row[l]!=no?correction.pressure[row[l]]:0.)-(row[r]!=no?correction.pressure[row[r]]:0.));
   next.faceVolume[f]+=dq;next.maximumBoundaryCorrection=std::max(next.maximumBoundaryCorrection,std::abs(dq));
  }
 }
 std::vector<Sum> exterior(n);
 for(std::size_t f=0;f<nf;++f){const auto& face=geometry.faces[f];if(owners.owner[face.left]!=owners.owner[face.right]){
  exterior[face.left].Add(next.faceVolume[f]);exterior[face.right].Add(-next.faceVolume[f]);
 }}
 for(const auto& v:vents)exterior[v.fragment].Add(v.volume);
 std::vector<std::size_t> local(n,n);
 for(std::size_t o=0;o<owners.members.size();++o){auto members=owners.members[o];const auto count=members.size();
  if(count)std::iter_swap(members.begin(),std::max_element(members.begin(),members.end(),[&](auto a,auto b){return geometry.startCapacity[a]<geometry.startCapacity[b];}));
  if(count==0)return Fail(error,"empty material owner");
  Sum total,gas;for(std::size_t i=0;i<count;++i){auto m=members[i];local[m]=i;total.Add(exterior[m].Get());if(!(gasVolume[m]>=0. && std::isfinite(gasVolume[m])))return Fail(error,"invalid gas geometry for flux reconstruction");gas.Add(gasVolume[m]);}
  const double volume=ownerSources?(*ownerSources)[o]:total.Get(),gasTotal=gas.Get();
  if(!std::isfinite(volume))return Fail(error,"nonfinite retained owner source");
  if(ownerSources&&std::abs(volume-total.Get())>std::max(relativePressureResidual,4096.*std::numeric_limits<double>::epsilon())*owners.capacity[o])
   return Fail(error,"retained owner source is incompatible with its fixed pressure field");
  if(gasTotal==0.&&!fragmentSources){double magnitude=0.;for(auto m:members)magnitude+=std::abs(exterior[m].Get());
   if(std::abs(volume)>std::max(relativePressureResidual,4096.*std::numeric_limits<double>::epsilon())*(owners.capacity[o]+magnitude))
    {std::ostringstream s;s.precision(17);s<<"pure-water owner="<<o<<" source="<<volume<<" capacity="<<owners.capacity[o]<<" flux="<<magnitude;return Fail(error,s.str());}
  }
  Sum assigned;std::size_t last=n;
  for(auto m:members)if(gasVolume[m]>0.)last=m;
  for(auto m:members){if(m==last)continue;next.divergence[m]=gasTotal>0.?volume*(gasVolume[m]/gasTotal):0.;assigned.Add(next.divergence[m]);}
  if(last!=n)next.divergence[last]=volume-assigned.Get();
  if(fragmentSources){Sum prescribed;
   for(auto m:members){if(!std::isfinite((*fragmentSources)[m]))return Fail(error,"nonfinite spatial divergence");next.divergence[m]=(*fragmentSources)[m];prescribed.Add(next.divergence[m]);}
   if(std::abs(prescribed.Get()-total.Get())>std::max(relativePressureResidual,4096.*std::numeric_limits<double>::epsilon())*owners.capacity[o])return Fail(error,"spatial divergence does not balance actual owner ports");
  }
  if(count>1){const auto k=count-1;std::vector<long double> a(k*k),rhs(k);
   for(std::size_t i=1;i<count;++i)rhs[i-1]=next.divergence[members[i]]-exterior[members[i]].Get();
   for(auto f:internal[o]){const auto& face=geometry.faces[f];auto l=local[face.left],r=local[face.right];const double w=mobility[f];
    if(!(w>0.&&std::isfinite(w)))return Fail(error,"invalid internal kinetic mobility");
    if(l){a[(l-1)*k+l-1]+=w;rhs[l-1]-=predictor[f];}if(r){a[(r-1)*k+r-1]+=w;rhs[r-1]+=predictor[f];}
    if(l&&r){a[(l-1)*k+r-1]-=w;a[(r-1)*k+l-1]-=w;}
   }
   std::vector<long double> p;if(!Solve(a,rhs,p))return Fail(error,"internal kinetic projection is not representable at owner "+std::to_string(o));
   for(auto f:internal[o]){auto l=local[geometry.faces[f].left],r=local[geometry.faces[f].right];next.faceVolume[f]=static_cast<double>(static_cast<long double>(predictor[f])+mobility[f]*((l?p[l-1]:0.L)-(r?p[r-1]:0.L)));}
  }
  for(auto m:members)local[m]=n;
 }
 std::vector<Sum> actual(n);std::vector<double> magnitude(n);
 for(std::size_t f=0;f<nf;++f){auto& face=geometry.faces[f];double q=next.faceVolume[f];if(!std::isfinite(q))return Fail(error,"nonfinite internal flux");actual[face.left].Add(q);actual[face.right].Add(-q);const double arithmetic=std::abs(q)+2.*std::abs(predictor[f])+2.*std::abs(q-predictor[f]);magnitude[face.left]+=arithmetic;magnitude[face.right]+=arithmetic;}
 for(auto& vent:vents){actual[vent.fragment].Add(vent.volume);magnitude[vent.fragment]+=std::abs(vent.volume);}
 for(std::size_t i=0;i<n;++i){double residual=std::abs(actual[i].Get()-next.divergence[i]);next.maximumResidual=std::max(next.maximumResidual,residual);
  if(residual>std::max(relativePressureResidual,4096.*std::numeric_limits<double>::epsilon())*(magnitude[i]+std::abs(next.divergence[i])+geometry.startCapacity[i]))
   {std::ostringstream s;s.precision(17);s<<"internal flux divergence fragment="<<i<<" residual="<<residual<<" magnitude="<<magnitude[i]<<" capacity="<<geometry.startCapacity[i]<<" target="<<next.divergence[i];return Fail(error,s.str());}
 }
 output=std::move(next);error.clear();return true;
}
}
