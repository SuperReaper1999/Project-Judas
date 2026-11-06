#include "CutTransparentGrid.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
namespace {
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::cerr<<text<<'\n';}}
CutSpatialField Slabs(const std::vector<double>& x){
    CutSpatialField f;f.chart.layout={{1,1,1},1.};
    for(std::size_t i=0;i+1<x.size();++i){double c=x[i+1]-x[i];f.parent.push_back(i);f.chart.regions.push_back({{x[i],0.,0.},{x[i+1],1.,1.},0});f.geometry.startCapacity.push_back(c);f.water.push_back(0.);f.gas.push_back(c);f.gasMass.push_back(1.2*c);
        if(i)f.geometry.faces.push_back({i-1,i,0,{x[i],0.,0.},{x[i],1.,1.},1.,1.,1.,0.,.5*(x[i+1]-x[i-1])});}
    f.geometry.endCapacity=f.geometry.startCapacity;f.flux.resize(f.geometry.faces.size());return f;
}
}
int main(){
    auto f=Slabs({0.,1e-8,.5,1.});CutSpatialField result;CutTransparentGridReport report;std::string error;
    Check(CoarsenCutTransparentGrid(f,result,report,error),"remove only algorithmic near-coincident plane");
    Check(report.removedPlanes==1&&report.outputCells==2,"tiny redundant slab is merged at fixed declared resolution");
    Check(report.minimumCapacityAfter==.5,"representative mass cells no longer contain algorithmic sliver");
    Check(std::abs(report.waterError)<1e-14&&std::abs(report.gasMassError)<1.2e-14,"independent conserved quantities survive projection");
    Check(std::abs(report.gasFirstMomentChange[0])<1e-15,"uniform field first moment preserved");
    auto saved=result;auto savedReport=report;
    f.flux[0]=1.;Check(!CoarsenCutTransparentGrid(f,result,report,error),"existing physical flow cannot be discarded");
    Check(result.parent==saved.parent&&report.outputCells==savedReport.outputCells,"failed projection leaves prior publication intact");
    f=Slabs({0.,1e-8,.5,1.});f.geometry.faces.erase(f.geometry.faces.begin());f.flux.erase(f.flux.begin());
    Check(!CoarsenCutTransparentGrid(f,result,report,error),"missing internal aperture is a barrier, not an invitation to merge");
    f=Slabs({0.,1e-8,.5,1.});f.geometry.faces[0].meanArea=.5;
    Check(!CoarsenCutTransparentGrid(f,result,report,error),"partial internal face rejects instead of widening");
    f=Slabs({.5,.50000001});
    Check(CoarsenCutTransparentGrid(f,result,report,error),"retain a genuine tiny fluid slit between solids");
    Check(report.removedPlanes==0&&result.chart.regions[0].lo[0]==.5&&result.chart.regions[0].hi[0]==.50000001,"both physical sides of a sub-grid gap remain exact");
    f=Slabs({0.,1e-8,.5,1.});f.boundaries={{0,1,1,0.,0.,true}};
    Check(CoarsenCutTransparentGrid(f,result,report,error),"different exterior port coverage retains its partition");
    Check(report.removedPlanes==0&&result.boundaries.size()==1,"gas-only boundary is not spread into sealed faces");
    f=Slabs({0.,1e-8,.5,1.});f.water={1e-8,.5-1e-8,0.};f.gas={1e-30,0.,.5};f.gasMass={1.2e-30,0.,.6};
    Check(CoarsenCutTransparentGrid(f,result,report,error),"independent minority gas remains represented during merging");
    Check(result.gas[0]==1e-30&&result.gasMass[0]==1.2e-30,"tiny positive gas is not subtracted from rounded dominant water");
    Check(std::abs(report.waterError)<std::numeric_limits<double>::epsilon()*.5,"minority-phase projection changes water only by bounded final binary64 rounding");
    f.chart.regions[0].lo[0]=std::numeric_limits<double>::quiet_NaN();
    Check(!CoarsenCutTransparentGrid(f,result,report,error),"invalid geometry rejects");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
