#include "BasinFixture.h"
#include "CutLocalPressure.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>
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
  CutLocalPressureResult state;
  if(argc>2&&std::string(argv[1])=="replay"){
   if(!Load(argv[2],state))return 3;
   CutLocalPressureResult output;auto started=std::chrono::steady_clock::now();
   bool ok=AdvanceCutLocalPressure(field,settings,{},output,error,CutLocalTransport::Fast,&state);
   std::cout<<"REPLAY admitted="<<ok<<" iterations="<<output.iterations<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<" transport_ms="<<output.transportMilliseconds<<" tangent_ms="<<output.tangentMilliseconds<<" linear_ms="<<output.linearMilliseconds<<" prep_ms="<<output.pressurePreparationMilliseconds<<" reason="<<error<<std::endl;
   if(ok&&argc>3&&!Save(argv[3],output))return 3;
   return ok?0:1;
  }
  std::cerr<<"Usage: replay STATE_FILE [OUTPUT_STATE]\n";return 2;
 }
 return failed?1:0;
}
