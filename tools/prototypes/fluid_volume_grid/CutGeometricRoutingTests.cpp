#include "CutDualMomentum.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;
int checks=0,failures=0;
void Check(bool ok,const char* s){++checks;if(!ok){++failures;std::cerr<<s<<'\n';}}
CutSpatialField Fixture(){
 CutSpatialField f;f.chart.layout={{2,1,3},1.};
 for(int z=0;z<3;++z)for(int x=0;x<2;++x){std::size_t i=f.parent.size();f.parent.push_back(i);f.chart.regions.push_back({{double(x),0.,double(z)},{double(x+1),1.,double(z+1)},i});}
 f.geometry.startCapacity=f.geometry.endCapacity=std::vector<double>(6,1.);
 for(int z=0;z<3;++z)f.geometry.faces.push_back({std::size_t(2*z),std::size_t(2*z+1),0,{1.,0.,double(z)},{1.,1.,double(z+1)},1.,1.,1.,0.,1.});
 for(int z=0;z<2;++z)for(int x=0;x<2;++x)f.geometry.faces.push_back({std::size_t(2*z+x),std::size_t(2*z+x+2),2,{double(x),0.,double(z+1)},{double(x+1),1.,double(z+1)},1.,1.,1.,0.,1.});
 f.flux.resize(f.geometry.faces.size());return f;
}
}
int main(){
 auto f=Fixture();std::string error;double referenceError=0.;
 for(bool product:{false,true}){
  CutVelocityDual geometry;Check(geometry.Build(f,error,product),"build velocity geometry");
  Check(geometry.UsesGeometricRouting()!=product,"requested routing is explicit");
  const auto shared=geometry.FaceNodes()[1];bool skip=false,cross=false;
  for(auto route:geometry.FaceScatter()[1]){
   auto a=geometry.Nodes()[route.from],b=geometry.Nodes()[route.to];
   if(a.axis==0&&route.from!=shared&&route.to!=shared)skip=true;
   if(a.axis==2&&!a.fixed&&!b.fixed&&f.geometry.faces[a.face].lo[2]!=f.geometry.faces[b.face].lo[2])cross=true;
  }
  Check(skip==product&&cross==product,"negative control identifies reference skip and cross-half paths");
  std::vector<Real> start(6,Real(1.)),end=start,flux(f.geometry.faces.size()),u(geometry.Nodes().size());end[2]=.8;end[3]=1.2;flux[1]=.2;
  // Express endpoints from the same extensive transfer, not separately rounded
  // decimals: this independently tests momentum, not a source-mass typo.
  end[2]=start[2]-flux[1];end[3]=start[3]+flux[1];
  for(std::size_t j=0;j<u.size();++j){auto node=geometry.Nodes()[j];if(node.axis==2&&!node.fixed)u[j]=f.geometry.faces[node.face].lo[2]==1.?1.:-1.;}
  CutDynamicSettings settings;settings.gravity={0.,0.,0.};CutDualMomentum response;std::vector<Real> q,v,actual;
  Check(response.Build(geometry,f,settings,start,end,flux,{},u,error)&&response.Flow(std::vector<Real>(6),q,v,error,&actual),"advance prescribed extensive carrier with transverse velocity profile");
  double maximum=0.;for(std::size_t j=0;j<u.size();++j)maximum=std::max(maximum,std::abs((actual[j]-u[j]).Value()));
  CutDualMomentumAudit audit;Check(response.Audit(std::vector<Real>(6),q,v,audit,error),"audit original momentum and kinetic work");
  Check(audit.maximumNormalizedResidual<1e-12&&std::abs(audit.energyResidual)<1e-12,"both methods retain their conservation obligations");
  std::cout<<"ROUTING product="<<product<<" error="<<maximum<<" mixing="<<audit.mixingLoss<<std::endl;
  if(product){referenceError=maximum;Check(maximum>.01&&audit.mixingLoss>.01,"reference demonstrably diffuses a profile with no transverse material transport");}
  else Check(maximum<1e-24&&std::abs(audit.mixingLoss)<1e-24,"geometric transport preserves the profile without invented mixing");
 }
 Check(referenceError>.01,"negative control remains active");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
