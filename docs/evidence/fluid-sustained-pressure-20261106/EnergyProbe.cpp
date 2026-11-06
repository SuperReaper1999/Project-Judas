#include "BasinFixture.h"
#include "CutLocalPressure.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
// Diagnostic state only; this is not the engine cold-save format.
bool Save(const char* path,const CutLocalPressureResult& s){
 std::ofstream f(path,std::ios::binary);
 auto scalar=[&](auto x){f.write(reinterpret_cast<const char*>(&x),sizeof(x));};
 scalar(s.dt);scalar(s.waterDensity);scalar(s.gasDensity);scalar(s.soundSpeed);scalar(s.frozenGeometryStamp);
 for(auto* v:{&s.pressure,&s.water,&s.gasMass,&s.flux,&s.portFlux}){scalar(std::uint64_t(v->size()));f.write(reinterpret_cast<const char*>(v->data()),v->size()*sizeof(CutCompensated));}
 return bool(f);
}
bool Load(const char* path,CutLocalPressureResult& s){
 std::ifstream f(path,std::ios::binary);
 auto scalar=[&](auto& x){f.read(reinterpret_cast<char*>(&x),sizeof(x));};
 scalar(s.dt);scalar(s.waterDensity);scalar(s.gasDensity);scalar(s.soundSpeed);scalar(s.frozenGeometryStamp);
 for(auto* v:{&s.pressure,&s.water,&s.gasMass,&s.flux,&s.portFlux}){std::uint64_t count=0;scalar(count);if(count>1000000)return false;v->resize(count);f.read(reinterpret_cast<char*>(v->data()),v->size()*sizeof(CutCompensated));}
 return bool(f);
}
int main(int argc,char** argv){
 std::cout<<std::setprecision(17);int failed=0;
 for(bool rotated:{true}){
  auto fixture=Fixture::Authored(rotated);CutRegionChart chart;CutRegionGeometry geometry;std::string error;
  if(!PrepareCutRegionChart(fixture.layout,fixture.Walls(),fixture.InitialBox(),{-.005,-.015,-.005},{.005,.025,.005},chart,error)||!BuildCutRegionGeometry(chart,{},geometry,error)){std::cerr<<error;return 2;}
  CutDynamicState source;SeedImport import;
  if(!WaterByRegion(fixture,chart,geometry,rotated,source.water,import,error)){std::cerr<<error;return 2;}
  std::vector<double> gas;for(std::size_t i=0;i<source.water.size();++i){gas.push_back(geometry.startCapacity[i]-source.water[i]);source.gasMass.push_back(1.2*gas.back());}
  CutConnectedOwners owners;if(!BuildCutConnectedOwners(chart,geometry,.125,owners,error)){std::cerr<<error;return 2;}
  std::vector<CutRegionWaterPlane> planes;if(!ReconstructCutRegionPhasePlanes(chart,geometry,1,source.water,gas,{},planes,error)){std::cerr<<error;return 2;}
  auto vent=FullCutRegionGasVent(chart,1,1);std::vector<CutPhaseBoundary> ports;
  for(std::size_t i=0;i<chart.regions.size();++i)if(MeasureCutRegionGasVent(chart.regions[i],geometry.motion,vent,planes[i]).mean>0.)ports.push_back({i,1,1,0.,0.,true});
  CutSpatialField field;std::vector<double> zero(geometry.faces.size());auto buildStart=std::chrono::steady_clock::now();
  if(!BuildCutSpatialField(chart,geometry,owners,source,zero,ports,field,error,false)){std::cerr<<error;return 2;}
  std::cout<<"BUILD rotated="<<rotated<<" cells="<<field.parent.size()<<" faces="<<field.flux.size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-buildStart).count()<<std::endl;
  CutDynamicSettings settings;settings.dt=double(tick);settings.gravity={0.,-double(gravity),0.};settings.iterationLimit=85;
  CutLocalPressureResult state;if(argc<2||!Load(argv[1],state))return 3;
  const auto n=state.water.size();std::vector<double> w(n),g(n),rho(n);std::vector<CutRegionWaterPlane> shapes;
  for(std::size_t i=0;i<n;++i){CutCompensated density=CutCompensated(settings.gasDensity)+state.pressure[i]/CutCompensated(settings.soundSpeed*settings.soundSpeed),gas=state.gasMass[i]/density,capacity=field.geometry.startCapacity[i];rho[i]=density.Value();if(state.water[i].Value()<=gas.Value()){w[i]=state.water[i].Value();g[i]=(capacity-state.water[i]).Value();}else{g[i]=gas.Value();w[i]=(capacity-gas).Value();}}
  if(!ReconstructCutRegionPhasePlanes(field.chart,field.geometry,1,w,g,{},shapes,error)){std::cerr<<error;return 1;}
  struct Row{std::size_t face;double speed,area,wm,gm,we,ge;};std::vector<Row> rows;long double wm=0.,gm=0.,we=0.,ge=0.;
  for(std::size_t f=0;f<state.flux.size();++f){auto face=field.geometry.faces[f];if(face.meanArea==0.)continue;
   auto l=MeasureCutRegionFaceInertiaVolumes(face,field.geometry.motion,field.chart.regions[face.left],true,shapes[face.left]);auto rr=MeasureCutRegionFaceInertiaVolumes(face,field.geometry.motion,field.chart.regions[face.right],false,shapes[face.right]);
   double waterMass=settings.waterDensity*(l[0]+rr[0]),gasMass=rho[face.left]*l[1]+rho[face.right]*rr[1],speed=std::abs(state.flux[f].Value())/(state.dt*face.meanArea),waterEnergy=.5*waterMass*speed*speed,gasEnergy=.5*gasMass*speed*speed;
   wm+=waterMass;gm+=gasMass;we+=waterEnergy;ge+=gasEnergy;rows.push_back({f,speed,face.meanArea,waterMass,gasMass,waterEnergy,gasEnergy});
  }
  std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.speed>b.speed;});
  std::cout<<"ENERGY waterJ="<<double(we)<<" gasJ="<<double(ge)<<" waterDualMass="<<double(wm)<<" gasDualMass="<<double(gm)<<" waterWeightedNormalRms="<<std::sqrt(double(2*we/wm))<<" gasWeightedNormalRms="<<std::sqrt(double(2*ge/gm))<<std::endl;
  for(std::size_t j=0;j<std::min<std::size_t>(10,rows.size());++j){auto v=rows[j];std::cout<<"FAST_FACE index="<<v.face<<" speed="<<v.speed<<" area="<<v.area<<" waterMass="<<v.wm<<" gasMass="<<v.gm<<" waterJ="<<v.we<<" gasJ="<<v.ge<<std::endl;}
 }
 return failed?1:0;
}
