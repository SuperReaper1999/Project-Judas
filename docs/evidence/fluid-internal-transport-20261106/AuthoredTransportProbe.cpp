#include "BasinFixture.h"
#include "CutOwnedProjection.h"
#include "CutPhaseFronts.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <limits>
using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
int main(){
 int failed=0;
 std::cout<<std::setprecision(17);
 for(bool rotated:{false,true}){
  auto fixture=Fixture::Authored(rotated);CutRegionChart chart;CutRegionGeometry geometry;std::string error;
  if(!PrepareCutRegionChart(fixture.layout,fixture.Walls(),fixture.InitialBox(),{-.005,-.015,-.005},{.005,.025,.005},chart,error)||!BuildCutRegionGeometry(chart,{},geometry,error)){std::cerr<<error;return 2;}
  CutDynamicState source;SeedImport imp;
  if(!WaterByRegion(fixture,chart,geometry,rotated,source.water,imp,error)){std::cerr<<error;return 2;}
  for(std::size_t i=0;i<source.water.size();++i)source.gasMass.push_back(1.2*(geometry.startCapacity[i]-source.water[i]));
  CutConnectedOwners owners;
  if(!BuildCutConnectedOwners(chart,geometry,.125,owners,error)){std::cerr<<error;return 2;}
  double min=std::numeric_limits<double>::infinity();std::size_t largest=0;
  for(std::size_t i=0;i<owners.members.size();++i){if(owners.capacity[i]>0.)min=std::min(min,owners.capacity[i]);largest=std::max(largest,owners.members[i].size());}
  std::cout<<"OWNERS rotated="<<rotated<<" fragments="<<chart.regions.size()<<" owners="<<owners.members.size()<<" minCapacity="<<min<<" maxMembers="<<largest<<std::endl;
  CutDynamicSettings settings;settings.dt=double(tick);settings.gravity={0.,-double(gravity),0.};
  CutOwnedProjection result;result.water={-7.};CutOwnedProjection trial; CutHydroacousticGraphFailure diagnostic;
  auto start=std::chrono::steady_clock::now();
  bool ok=ProjectCutOwnedMaterial(chart,geometry,owners,source,settings,{FullCutRegionGasVent(chart,1,1)},result,error,&diagnostic,&trial);
  failed+=!ok;
  std::cout<<"PROJECTION rotated="<<rotated<<" admitted="<<ok<<" unchanged="<<(result.water==std::vector<double>{-7.})<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" reason="<<error<<std::endl;
  if(!trial.pressure.empty()){
   std::vector<double> fragmentGas(source.water.size()),rho(owners.members.size());
   for(std::size_t i=0;i<fragmentGas.size();++i)fragmentGas[i]=source.gasMass[i]/1.2;
   std::vector<CutRegionWaterPlane> planes;
   if(!ReconstructCutRegionPhasePlanes(chart,geometry,1,source.water,fragmentGas,{},planes,error)){std::cerr<<error;return 2;}
   std::vector<double> mobility(geometry.faces.size()),predictor(mobility.size());
   for(std::size_t f=0;f<mobility.size();++f){auto& face=geometry.faces[f];if(face.meanArea==0.)continue;
    auto a=MeasureCutRegionFaceInertiaVolumes(face,geometry.motion,chart.regions[face.left],true,planes[face.left]);
    auto b=MeasureCutRegionFaceInertiaVolumes(face,geometry.motion,chart.regions[face.right],false,planes[face.right]);
    double mass=1000.*(a[0]+b[0])+1.2*(a[1]+b[1]);
    mobility[f]=face.meanArea*face.meanArea/mass;
    predictor[f]=settings.dt*face.meanArea*settings.dt*settings.gravity[face.axis];
   }
   for(std::size_t i=0;i<rho.size();++i)rho[i]=1.2+trial.pressure[i]/(343.*343.);
   std::vector<CutPhaseBoundary> boundaries;
   for(auto& v:trial.ventVolume)boundaries.push_back({v.fragment,1,1,v.volume,0.,true});
   if(!rotated){
    CutOwnedFlux field;std::string why;
    if(!ReconstructCutOwnedFlux(chart,geometry,owners,trial.faceVolume,trial.ventVolume,fragmentGas,predictor,mobility,field,why)){std::cout<<"FIELD "<<why<<std::endl;}
    else {
     const std::size_t cell=2844;const auto& r=chart.regions[cell];
     std::cout<<"FILM fragment="<<cell<<" owner="<<owners.owner[cell]<<" members="<<owners.members[owners.owner[cell]].size()<<" bounds="<<r.lo[0]<<","<<r.hi[0]<<";"<<r.lo[1]<<","<<r.hi[1]<<";"<<r.lo[2]<<","<<r.hi[2]<<" water="<<source.water[cell]<<" gas="<<fragmentGas[cell]<<std::endl;
     for(std::size_t f=0;f<geometry.faces.size();++f){auto& face=geometry.faces[f];if(face.left!=cell&&face.right!=cell)continue;
      auto phase=MeasureCutRegionFacePhase(face,geometry.motion,planes[face.left],planes[face.right]);double q=field.faceVolume[f];
      std::cout<<"FILM_FACE index="<<f<<" left="<<face.left<<" right="<<face.right<<" axis="<<face.axis<<" q="<<q<<" donorWet="<<phase.wet[q>=0.?0:1].mean/face.meanArea<<" sameOwner="<<(owners.owner[face.left]==owners.owner[face.right])<<std::endl;
     }
    }
   }
   CutPhaseFrontSettings ps;ps.maximumSteps=128;CutPhaseFrontResult phased;phased.water={-11.};
   auto begin=std::chrono::steady_clock::now();
   bool physical=AdvanceCutPhaseFronts(chart,geometry,owners,source,rho,trial.faceVolume,boundaries,predictor,mobility,ps,phased,error);
   std::cout<<"PHASE rotated="<<rotated<<" admitted="<<physical<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<" outputUnchanged="<<(!physical&&phased.water==std::vector<double>{-11.})<<" reason="<<error<<std::endl;
  }
  if(diagnostic.tolerance>0.){
   std::cout<<"ROW rotated="<<rotated<<" iterations="<<diagnostic.iterations<<" owner="<<diagnostic.row<<" normalized="<<diagnostic.normalizedResidual<<std::hexfloat
    <<" storage="<<diagnostic.storage<<" rhs="<<diagnostic.source<<" tolerance="<<diagnostic.tolerance<<" pressure="<<diagnostic.pressure<<" residual="<<diagnostic.residual<<std::defaultfloat<<std::endl;
   for(const auto& neighbor:diagnostic.neighbors)std::cout<<"EDGE rotated="<<rotated<<std::hexfloat<<" g="<<neighbor.conductance<<" p="<<neighbor.pressure<<std::defaultfloat<<std::endl;
  }

 }
 return failed ? 1 : 0;
}
