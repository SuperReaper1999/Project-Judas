#include "CutVelocityDual.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}}
CutSpatialField Pair(){
    CutSpatialField f;f.chart.layout={{2,1,1},1.};f.parent={0,1};
    f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1}};
    f.geometry.startCapacity=f.geometry.endCapacity={1.,1.};
    f.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};
    f.flux={0.};return f;
}
}
int main(){
    auto field=Pair();std::string error;CutVelocityDual dual;
    Check(dual.Build(field,error),"build exact aperture-connected velocity ownership");
    for(const auto& shares:dual.Shares()){
        Real sum;for(const auto& s:shares)sum=sum+s.fraction;
        Check(std::abs((sum-Real(1.)).Value())<1e-28,"each component partitions its entire physical mass");
    }
    std::vector<Real> start,end;std::vector<CutTransportFace> edges;std::vector<CutTransportGasPort> ports;
    Check(dual.Mass({2.,1.},start,error)&&dual.Mass({1.,2.},end,error),"assemble both endpoint masses from the same partition");
    Check(dual.Carriers({1.},{},edges,ports,error),"route actual primal carrier to velocity owners");
    auto balance=end;for(std::size_t i=0;i<start.size();++i)balance[i]=balance[i]-start[i];
    for(const auto& edge:edges){balance[edge.left]=balance[edge.left]+edge.volume;balance[edge.right]=balance[edge.right]-edge.volume;}
    for(auto r:balance)Check(std::abs(r.Value())<1e-28,"all velocity owners independently close their mass ledger");
    auto saved=dual.Capacity();auto bad=field;bad.geometry.faces.push_back(bad.geometry.faces[0]);
    Check(!dual.Build(bad,error)&&dual.Capacity()==saved,"duplicate columns in one half reject transactionally");
    bad=field;bad.geometry.faces[0].meanArea=0.;Check(dual.Build(bad,error),"sealed face retains only stationary wall owners");
    Check(dual.FaceNodes()[0]==SIZE_MAX,"closed wall does not create a velocity or mass path");
    Check(dual.Build(field,error),"rebuild valid topology");
    auto savedMass=start;Check(!dual.Mass({-1.,2.},start,error)&&start.size()==savedMass.size(),"invalid material rejects without publishing");
    Check(!dual.Carriers({std::numeric_limits<double>::quiet_NaN()},{},edges,ports,error),"nonfinite carrier rejects");
    field.boundaries={{0,0,-1,0.,0.,true},{1,0,1,0.,0.,true}};
    Check(dual.Build(field,error)&&dual.PortNodes().size()==2,"explicit gas ports participate in the same dual partition");
    for(const auto& shares:dual.Shares()){
        Real sum;for(const auto& s:shares)sum=sum+s.fraction;
        Check(std::abs((sum-Real(1.)).Value())<1e-28,"port partition preserves physical mass");
    }
    Check(dual.Mass({1.,1.},start,error)&&dual.Mass({.75,1.25},end,error)&&dual.Carriers({.5},{-.25,.25},edges,ports,error),"boundary and interior mass carriers assembled together");
    balance=end;for(std::size_t i=0;i<start.size();++i)balance[i]=balance[i]-start[i];
    for(const auto& edge:edges){balance[edge.left]=balance[edge.left]+edge.volume;balance[edge.right]=balance[edge.right]-edge.volume;}
    for(const auto& port:ports)balance[port.cell]=balance[port.cell]+port.volume;
    for(auto r:balance)Check(std::abs(r.Value())<1e-28,"open-boundary velocity mass closes locally");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
