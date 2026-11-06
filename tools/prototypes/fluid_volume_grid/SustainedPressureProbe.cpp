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
int main(int argc,char** argv){
 bool fast=true;int steps=argc>1?std::stoi(argv[1]):120;if(steps<1)return 2;
 std::cout<<std::setprecision(17);int failed=0;
 for(bool rotated:{false,true}){
  if(argc>2&&((std::string(argv[2])=="rotated"&&!rotated)||(std::string(argv[2])=="level"&&rotated)))continue;
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
  CutLocalPressureResult result;result.water={CutCompensated(-7.)};auto start=std::chrono::steady_clock::now();
  bool ok=AdvanceCutLocalPressure(field,settings,{},result,error,fast?CutLocalTransport::Fast:CutLocalTransport::Reference);failed+=!ok;
  std::cout<<"BASIN rotated="<<rotated<<" admitted="<<ok<<" dt="<<settings.dt<<" iterations="<<result.iterations<<" normalized="<<result.maximumNormalizedResidual<<" residual_m3="<<result.maximumVolumeResidual<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" reason="<<error<<std::endl;
  if(!ok){std::cout<<"OUTPUT_UNCHANGED "<<(result.water.size()==1&&result.water[0].Value()==-7.)<<std::endl;continue;}
  CutCompensated coldW,coldM;for(double w:field.water)coldW=coldW+CutCompensated(w);for(double m:field.gasMass)coldM=coldM+CutCompensated(m);
  auto prefix=[&](const CutLocalPressureResult& state,int step){
   CutCompensated w=state.externalWater,m=state.externalGasMass;double minDensity=1e300,maxPressure=0.,maxSpeed=0.;
   for(std::size_t i=0;i<state.water.size();++i){w=w+state.water[i];m=m+state.gasMass[i];minDensity=std::min(minDensity,(CutCompensated(settings.gasDensity)+state.pressure[i]/CutCompensated(settings.soundSpeed*settings.soundSpeed)).Value());maxPressure=std::max(maxPressure,std::abs(state.pressure[i].Value()));}
   for(std::size_t f=0;f<state.flux.size();++f)if(field.geometry.faces[f].meanArea>0.)maxSpeed=std::max(maxSpeed,std::abs(state.flux[f].Value())/(settings.dt*field.geometry.faces[f].meanArea));
   double we=(w-coldW).Value(),me=(m-coldM).Value();bool admitted=std::abs(we)<=settings.volumeTolerance&&std::abs(me)<=settings.gasDensity*settings.volumeTolerance&&minDensity>0.;
   std::cout<<"PREFIX rotated="<<rotated<<" step="<<step<<" admitted="<<admitted<<" waterError="<<we<<" gasError="<<me<<" minDensity="<<minDensity<<" maxPressure="<<maxPressure<<" maxNormalSpeed="<<maxSpeed<<std::endl;return admitted;
  };
  if(!prefix(result,1)){++failed;continue;}
  for(int step=1;ok&&step<steps;++step){
   CutLocalPressureResult next;auto started=std::chrono::steady_clock::now();
   ok=AdvanceCutLocalPressure(field,settings,{},next,error,CutLocalTransport::Fast,&result);
   std::cout<<"CONTINUATION rotated="<<rotated<<" step="<<step+1<<" admitted="<<ok<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<" iterations="<<next.iterations<<" normalized="<<next.maximumNormalizedResidual<<" transport_ms="<<next.transportMilliseconds<<" tangent_ms="<<next.tangentMilliseconds<<" linear_ms="<<next.linearMilliseconds<<" prep_ms="<<next.pressurePreparationMilliseconds<<" pressureFallbacks="<<next.pressurePreconditionerFallbacks<<" rejectedTrials="<<next.rejectedTransportTrials<<" reason="<<error<<std::endl;
   if(ok){next.externalWater=next.externalWater+result.externalWater;next.externalGasMass=next.externalGasMass+result.externalGasMass;result=std::move(next);if(!prefix(result,step+1)){++failed;ok=false;}}else ++failed;
  }
  if(!ok)continue;
  if(argc>3){std::string file=std::string(argv[3])+(rotated?"-rotated.bin":"-level.bin");if(!Save(file.c_str(),result))return 3;}
  std::cout<<"END rotated="<<rotated<<" intervals="<<steps<<" simulatedSeconds="<<steps*settings.dt<<std::endl;
  CutCompensated initialW,initialM,water,mass,originalW,originalM;double minimumWater=1e300,minimumMass=1e300,over=0.;
  for(std::size_t i=0;i<field.parent.size();++i){initialW=initialW+CutCompensated(field.water[i]);initialM=initialM+CutCompensated(field.gasMass[i]);water=water+result.water[i];mass=mass+result.gasMass[i];minimumWater=std::min(minimumWater,result.water[i].Value());minimumMass=std::min(minimumMass,result.gasMass[i].Value());over=std::max(over,(result.water[i]-CutCompensated(field.geometry.startCapacity[i])).Value());}
  for(double w:source.water)originalW=originalW+CutCompensated(w);
  for(double m:source.gasMass)originalM=originalM+CutCompensated(m);
  double waterError=(water+result.externalWater-initialW).Value(),massError=(mass+result.externalGasMass-initialM).Value();
  bool accepted=minimumWater>=0.&&minimumMass>=0.&&std::abs(waterError)<=settings.volumeTolerance&&std::abs(massError)<=1.2*settings.volumeTolerance&&std::abs((initialW-originalW).Value())<=settings.volumeTolerance&&std::abs((initialM-originalM).Value())<=1.2*settings.volumeTolerance&&result.externalWater.Value()==0.;
  failed+=!accepted;
  std::cout<<"QUANTITIES rotated="<<rotated<<" independentAdmission="<<accepted<<" waterLedgerError_m3="<<waterError<<" gasLedgerError_kg="<<massError<<" sourceWaterRemapError_m3="<<(initialW-originalW).Value()<<" sourceGasRemapError_kg="<<(initialM-originalM).Value()<<" minWater="<<minimumWater<<" minGasMass="<<minimumMass<<" maxOverCapacity_m3="<<over<<std::endl;
  std::cout<<"FAST solves="<<result.fastSolves<<" corrections="<<result.fastCorrections<<" sweeps="<<result.fastSweeps<<" fallbacks="<<result.fastFallbacks<<std::endl;
  std::cout<<"COST rotated="<<rotated<<" transport_ms="<<result.transportMilliseconds<<" tangent_ms="<<result.tangentMilliseconds<<" pressureLinear_ms="<<result.linearMilliseconds<<" transportEvaluations="<<result.transportEvaluations<<" linearApplications="<<result.linearApplications<<std::endl;
 }
 return failed?1:0;
}
