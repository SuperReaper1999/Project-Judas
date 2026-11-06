#include "CutCoupledResidual.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;
int checks=0,failures=0;
void Check(bool x,const char* name){++checks;if(!x){++failures;std::cerr<<name<<'\n';}}
CutSpatialField Square(){
    CutSpatialField f;f.chart.layout={{2,2,1},1.};
    for(int y=0;y<2;++y)for(int x=0;x<2;++x){auto i=f.parent.size();f.parent.push_back(i);f.chart.regions.push_back({{double(x),double(y),0.},{double(x+1),double(y+1),1.},i});}
    f.geometry.startCapacity=f.geometry.endCapacity=std::vector<double>(4,1.);
    f.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.},{2,3,0,{1.,1.,0.},{1.,2.,1.},1.,1.,1.,0.,1.},
        {0,2,1,{0.,1.,0.},{1.,1.,1.},1.,1.,1.,0.,1.},{1,3,1,{1.,1.,0.},{2.,1.,1.},1.,1.,1.,0.,1.}};
    f.flux=std::vector<double>(4);f.water={.2,.3,.7,.6};for(auto w:f.water){f.gas.push_back(1.-w);f.gasMass.push_back(1.2*(1.-w));}
    f.boundaries={{2,1,1,0.,0.,true},{3,1,1,0.,0.,true}};return f;
}
}
int main(){
    auto f=Square();CutVelocityDual dual;std::string e;
    Check(dual.Build(f,e),"build derivative fixture");CutDynamicSettings s;s.dt=.02;
    std::vector<Real> sw(f.water.begin(),f.water.end()),sm(f.gasMass.begin(),f.gasMass.end()),zero(dual.Nodes().size()),p{30.,10.,20.,15.},u=zero;
    std::vector<double> dp{.3,-.4,.5,-.2},du(u.size());
    for(std::size_t j=0;j<u.size();++j)if(!dual.Nodes()[j].fixed){u[j]=j%2?.12:-.18;du[j]=j%2?-.7:.4;}
    for(bool atZero:{false,true}){
        if(atZero)u=zero;
        CutCoupledResidual base;Check(base.Build(f,dual,s,sw,sm,zero,p,u,CutMomentumBackend::Fast,e),"build material/momentum residual together");
        std::vector<Real> v,m;Check(base.Tangent(dp,du,v,m,e),"evaluate full coupled analytic tangent");
        const double h=atZero?1e-6:1e-4;auto pp=p,pm=p,up=u,um=u;
        for(std::size_t i=0;i<p.size();++i){pp[i]=p[i]+Real(h*dp[i]);pm[i]=p[i]-Real(h*dp[i]);}
        for(std::size_t j=0;j<u.size();++j){up[j]=u[j]+Real(h*du[j]);um[j]=u[j]-Real(h*du[j]);}
        CutCoupledResidual plus,minus;
        Check(plus.Build(f,dual,s,sw,sm,zero,pp,up,CutMomentumBackend::Fast,e)&&minus.Build(f,dual,s,sw,sm,zero,pm,um,CutMomentumBackend::Fast,e),"independent nonlinear perturbations admitted");
        double ve=0.,me=0.;
        for(std::size_t i=0;i<v.size();++i)ve=std::max(ve,std::abs(((plus.volumeRows[i]-minus.volumeRows[i])/Real(2.*h)-v[i]).Value()));
        for(auto j:base.momentum.FreeNodes())me=std::max(me,std::abs(((plus.momentumRows[j]-minus.momentumRows[j])/Real(2.*h)-m[j]).Value()));
        std::cout<<"TANGENT zero="<<atZero<<" volume="<<ve<<" momentum="<<me<<'\n';
        Check(ve<(atZero?1e-7:1e-9),"material and EOS derivative matches nonlinear difference");
        Check(me<(atZero?2e-5:1e-6),"mass/advection/pressure/gravity momentum derivative matches nonlinear difference");
    }
    f=Square();f.boundaries.clear();
    Check(dual.Build(f,e),"compile disturbed mixed-phase fixture");
    CutCoupledState converged;
    for(auto linearization:{CutCoupledLinearization::Newton,CutCoupledLinearization::Oseen}){
        CutCoupledSettings mixedOptions;mixedOptions.linearization=linearization;CutCoupledState mixedState;
        for(int step=0;step<3;++step){CutCoupledState next;bool ok=AdvanceCutCoupledPressure(f,dual,s,mixedOptions,next,e,step?&mixedState:nullptr);
            Check(ok,"mixed-phase gravity and momentum advance together");if(!ok){std::cerr<<e<<'\n';break;}mixedState=std::move(next);}
        Check(mixedState.momentum.maximumSpeed>0.&&mixedState.momentum.maximumNormalizedResidual<1e-10,"nontrivial motion closes original momentum rows");
        if(linearization==CutCoupledLinearization::Newton)converged=mixedState;
        else{
            double velocityError=0.,quantityError=0.;
            for(std::size_t j=0;j<mixedState.velocity.size();++j)velocityError=std::max(velocityError,std::abs((mixedState.velocity[j]-converged.velocity[j]).Value()));
            for(std::size_t i=0;i<mixedState.material.water.size();++i)quantityError=std::max(quantityError,std::abs((mixedState.material.water[i]-converged.material.water[i]).Value()));
            Check(velocityError<1e-10&&quantityError<1e-13,"different nonlinear linearizations reach the same admitted state");
        }
    }
    CutSelectiveTransport prepared;std::vector<Real> a{7.},b{8.},c{9.},d{10.};
    Check(prepared.Build({1.,1.},{{0,1,Real(.01)}},{},{Real(.2),Real(.3)},{Real(.96),Real(.84)},1.2,1e-14,e),"prepare transactional tangent test");
    Check(!prepared.MassTangent({}, {},1000.,a,b,c,d,e)&&a[0].Value()==7.&&b[0].Value()==8.&&c[0].Value()==9.&&d[0].Value()==10.,"failed mass tangent leaves every output intact");
    Check(!prepared.MassTangent({Real(.01)}, {},1000.,a,b,a,d,e),"mass tangent rejects aliased publications");
    // A nonzero source velocity makes mass transfer carry actual momentum.
    CutCoupledState state;CutCoupledSettings options;f=Square();f.boundaries.clear();f.water={0.,0.,0.,0.};f.gas={1.,1.,1.,1.};f.gasMass={1.2,1.2,1.2,1.2};
    Check(dual.Build(f,e),"compile ordinary gas coupling fixture");s.gravity={0.,0.,0.};
    Check(AdvanceCutCoupledPressure(f,dual,s,options,state,e),"uniform quiescent gas admitted");
    auto saved=state;auto invalid=options;invalid.iterationLimit=0;
    Check(!AdvanceCutCoupledPressure(f,dual,s,invalid,state,e,&saved),"invalid solve rejected");
    Check(state.material.water.size()==saved.material.water.size()&&state.material.water[0].hi==saved.material.water[0].hi&&state.material.water[0].lo==saved.material.water[0].lo,"failed advance leaves publication intact");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
