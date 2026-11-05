#include "BasinFixture.h"
#include "CutOwnedProjection.h"
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
  CutOwnedProjection result;result.water={-7.}; CutHydroacousticGraphFailure diagnostic;
  auto start=std::chrono::steady_clock::now();
  bool ok=ProjectCutOwnedMaterial(chart,geometry,owners,source,settings,{FullCutRegionGasVent(chart,1,1)},result,error,&diagnostic);
  failed+=!ok;
  std::cout<<"PROJECTION rotated="<<rotated<<" admitted="<<ok<<" unchanged="<<(result.water==std::vector<double>{-7.})<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" reason="<<error<<std::endl;
  if(diagnostic.tolerance>0.){
   std::cout<<"ROW rotated="<<rotated<<" iterations="<<diagnostic.iterations<<" owner="<<diagnostic.row<<" normalized="<<diagnostic.normalizedResidual<<std::hexfloat
    <<" storage="<<diagnostic.storage<<" rhs="<<diagnostic.source<<" tolerance="<<diagnostic.tolerance<<" pressure="<<diagnostic.pressure<<" residual="<<diagnostic.residual<<std::defaultfloat<<std::endl;
   for(const auto& neighbor:diagnostic.neighbors)std::cout<<"EDGE rotated="<<rotated<<std::hexfloat<<" g="<<neighbor.conductance<<" p="<<neighbor.pressure<<std::defaultfloat<<std::endl;
  }

 }
 return failed ? 1 : 0;
}
