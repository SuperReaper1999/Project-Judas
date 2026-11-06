#include "CutCoupledPressure.h"
#include <iostream>
#include <cmath>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}}
CutSpatialField Column(bool ports){
    CutSpatialField f;f.chart.layout={{1,2,1},1.};f.parent={0,1};
    f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{0.,1.,0.},{1.,2.,1.},1}};
    f.geometry.startCapacity=f.geometry.endCapacity={1.,1.};
    f.geometry.faces={{0,1,1,{0.,1.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};
    f.flux={0.};f.water={1.,1.};f.gas={0.,0.};f.gasMass={0.,0.};
    if(ports)f.boundaries={{1,1,1,0.,0.,true}};
    return f;
}
}
int main(int argc,char** argv){
    const auto backend=argc>1&&std::string(argv[1])=="--reference"?CutMomentumBackend::Reference:CutMomentumBackend::Fast;
    std::string error;auto field=Column(false);CutVelocityDual geometry;
    Check(geometry.Build(field,error),"compile compatible velocity-dual geometry");
    CutDynamicSettings settings;settings.dt=.01;settings.gravity={0.,-9.81,0.};settings.iterationLimit=80;
    CutCoupledSettings options;options.momentumBackend=backend;CutCoupledState state;
    Check(AdvanceCutCoupledPressure(field,geometry,settings,options,state,error,nullptr,{4905.,-4905.}),"prefilled hydrostatic water admitted");
    Check(state.momentum.maximumSpeed<1e-10,"hydrostatic water remains stationary");
    Check(state.momentum.maximumNormalizedResidual<1e-10,"pressure and ending momentum close together");
    Check(std::abs(state.momentum.energyResidual)<1e-14,"pressure/transport kinetic work closes");
    for(int step=0;step<10;++step){
        CutCoupledState next;bool ok=AdvanceCutCoupledPressure(field,geometry,settings,options,next,error,&state);
        Check(ok,"sustained hydrostatic continuation admitted");if(!ok){std::cerr<<error<<'\n';break;}state=std::move(next);
    }
    Check(state.momentum.maximumSpeed<1e-9,"quiet water is not stirred by zero-flow ownership transfer");
    field=Column(true);field.water={1.,0.};field.gas={0.,1.};
    const double c2=settings.soundSpeed*settings.soundSpeed;
    const double density=settings.gasDensity/(1.-.5*9.81/c2);
    field.gasMass={0.,density};Check(geometry.Build(field,error),"build filled water with a dry atmosphere port");
    const double top=.5*9.81*density,bottom=top+.5*9.81*(1000.+density);
    Check(AdvanceCutCoupledPressure(field,geometry,settings,options,state,error,nullptr,{bottom,top}),"hydrostatic gas and water share the EOS and pressure gradient");
    Check(state.momentum.maximumSpeed<1e-8,"prefilled compressible atmosphere stays quiet");
    // A supplied coefficient state with no advection independently exercises
    // the full off-diagonal response derivative, not a diagonal stand-in.
    std::vector<Real> mass{1000.,density},zero(geometry.Nodes().size());CutDualMomentum response;
    Check(response.Build(geometry,field,settings,mass,mass,{0.},{0.},zero,error,backend),"prepare checked dual pressure response");
    std::vector<Real> q,v,dq,dv,plus,minus,pv,mv;
    Check(response.Flow({bottom,top},q,v,error)&&response.Derivative({.3,-.2},dq,dv,error),"evaluate pressure and analytic derivative");
    const double h=.01;
    Check(response.Flow({bottom+.3*h,top-.2*h},plus,pv,error)&&response.Flow({bottom-.3*h,top+.2*h},minus,mv,error),"independent pressure perturbations solve");
    Check(std::abs(((plus[0]-minus[0])/Real(2.*h)-dq[0]).Value())<1e-12,"momentum-response derivative matches finite difference");
    auto copy=field;
    Check(!response.Matches(copy,settings),"response cannot be reused on a different geometry owner");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
