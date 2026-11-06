#include "WaterMimeticCell.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {int checks=0,failures=0;void Check(bool ok,const char* s){++checks;if(!ok){++failures;std::cerr<<s<<'\n';}}}
int main(){
 const double rho=1000.,dt=1./60.;GridVec gravity{1.2,-9.81,.7};std::string error;
 for(double height:{1.,.01,1e-8}){
  std::vector<WaterCellPatch> faces;
  GridVec length{1.,height,.8};double volume=length[0]*length[1]*length[2];
  for(int a=0;a<3;++a)for(int sign:{-1,1}){GridVec r{};r[a]=sign*length[a]*.5;faces.push_back({a,sign,volume/length[a],r,false});}
  // Split a face into fragments with distinct tangential centroids. The
  // analytic affine pressure should still leave exactly zero velocity.
  auto f=faces[0];faces.erase(faces.begin());f.area*=.5;f.offset[1]=height*.25;faces.push_back(f);f.offset[1]=-height*.25;faces.push_back(f);
  WaterCellProjection result;Check(PrepareWaterMimeticCell(volume,rho,dt,gravity,faces,result,error),"mimetic liquid cell with exact split boundary prepares");
  double max=0.;for(std::size_t i=0;i<result.free.size();++i){double q=result.predictor[i];for(std::size_t j=0;j<result.free.size();++j){auto b=faces[result.free[j]];double p=0.;for(int a=0;a<3;++a)p+=rho*gravity[a]*b.offset[a];q-=dt*result.mobility[i*result.free.size()+j].Value()*(p-result.gravityHead[j]);}max=std::max(max,std::abs(q));}
  std::cout<<"HYDROSTATIC height="<<height<<" flux="<<max<<'\n';Check(max<1e-11*volume,"affine pressure and vector gravity cancel on actual patch geometry");
  auto saved=result;faces[0].area*=.9;Check(!PrepareWaterMimeticCell(volume,rho,dt,gravity,faces,result,error)&&result.free.size()==saved.free.size(),"unclosed geometry rejects without replacing a prepared operator");
 }
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
