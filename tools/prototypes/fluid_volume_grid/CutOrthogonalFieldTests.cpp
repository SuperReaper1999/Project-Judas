#include "CutOrthogonalField.h"
#include "CutCoupledPressure.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
int checks=0,failures=0;
void Check(bool x,const char* name){++checks;if(!x){++failures;std::cerr<<name<<'\n';}}
CutSpatialField Split(){
    CutSpatialField f;f.chart.layout={{2,1,1},1.};f.parent={0,0,1,1};
    f.chart.regions={{{0.,0.,0.},{1.,.3,1.},0},{{0.,.3,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,.7,1.},1},{{1.,.7,0.},{2.,1.,1.},1}};
    for(const auto& r:f.chart.regions){double c=r.hi[1]-r.lo[1],w=std::max(0.,std::min(.5,r.hi[1])-r.lo[1]);
        f.geometry.startCapacity.push_back(c);f.water.push_back(w);f.gas.push_back(c-w);f.gasMass.push_back(1.2*(c-w));}
    f.geometry.endCapacity=f.geometry.startCapacity;
    f.geometry.faces={{0,2,0,{1.,0.,0.},{1.,.3,1.},.3,.3,.3,0.,1.},
        {1,2,0,{1.,.3,0.},{1.,.7,1.},.4,.4,.4,0.,1.},{1,3,0,{1.,.7,0.},{1.,1.,1.},.3,.3,.3,0.,1.},
        {0,1,1,{0.,.3,0.},{1.,.3,1.},1.,1.,1.,0.,.5},{2,3,1,{1.,.7,0.},{2.,.7,1.},1.,1.,1.,0.,.5}};
    f.flux=std::vector<double>(5);return f;
}
}
int main(){
    auto source=Split();std::string error;CutSpatialField cooked;CutOrthogonalRemap report;
    Check(BuildCutOrthogonalField(source,cooked,report,error),"cook nonconforming initial chart");
    Check(cooked.parent.size()==6,"retain both original horizontal subdivisions");
    Check(std::abs(report.waterError)<1e-15&&std::abs(report.gasMassError)<1e-15,"source phases conserved independently");
    bool orthogonal=true;double maximumFalseHead=0.;
    for(auto face:source.geometry.faces)if(face.axis==0){auto l=source.chart.regions[face.left],r=source.chart.regions[face.right];maximumFalseHead=std::max(maximumFalseHead,std::abs(.5*(l.lo[1]+l.hi[1]-r.lo[1]-r.hi[1])));}
    Check(maximumFalseHead>.1,"negative control: old two-point head is nonzero for vertical affine pressure across a horizontal face");
    for(auto face:cooked.geometry.faces){const auto& l=cooked.chart.regions[face.left];const auto& r=cooked.chart.regions[face.right];for(int a=0;a<3;++a)if(a!=face.axis)orthogonal=orthogonal&&l.lo[a]==r.lo[a]&&l.hi[a]==r.hi[a];}
    Check(orthogonal,"all cooked pressure differences align with actual normals");
    auto saved=cooked;auto savedReport=report;
    Check(!BuildCutOrthogonalField(source,cooked,report,error,5),"budget rejection does not delete a positive cell");
    Check(cooked.parent==saved.parent&&report.orthogonalCells==savedReport.orthogonalCells,"failed cook leaves old publication intact");
    auto moving=source;moving.flux[0]=.01;
    Check(!BuildCutOrthogonalField(moving,cooked,report,error),"existing velocity cannot be silently discarded by initial-state cooking");
    CutVelocityDual dual;Check(dual.Build(source,error),"old chart still representable by mass routing");
    CutCoupledState state;CutDynamicSettings settings;CutCoupledSettings options;
    Check(!AdvanceCutCoupledPressure(source,dual,settings,options,state,error),"normal pressure solver rejects nonorthogonal geometry explicitly");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
