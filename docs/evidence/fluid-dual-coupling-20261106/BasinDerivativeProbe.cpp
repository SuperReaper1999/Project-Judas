#include "BasinFixture.h"
#include "CutCoupledResidual.h"
#include "CutOrthogonalField.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <algorithm>
using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
using Real=CutCompensated;
bool BuildFixture(bool rotated, CutSpatialField& field, std::string& error) {
    const auto fixture = Fixture::Authored(rotated);
    CutRegionChart chart;
    CutRegionGeometry geometry;
    if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(), fixture.InitialBox(),
        {-.005,-.015,-.005}, {.005,.025,.005}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) return false;
    CutDynamicState source;
    SeedImport imported;
    if (!WaterByRegion(fixture, chart, geometry, rotated, source.water, imported, error)) return false;
    std::vector<double> gas;
    for (std::size_t i = 0; i < source.water.size(); ++i) {
        gas.push_back(geometry.startCapacity[i] - source.water[i]);
        source.gasMass.push_back(1.2 * gas.back());
    }
    CutConnectedOwners owners;
    if (!BuildCutConnectedOwners(chart, geometry, .125, owners, error)) return false;
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionPhasePlanes(chart, geometry, 1, source.water, gas, {}, planes, error)) return false;
    auto vent = FullCutRegionGasVent(chart, 1, 1);
    std::vector<CutPhaseBoundary> ports;
    for (std::size_t i = 0; i < chart.regions.size(); ++i)
        if (MeasureCutRegionGasVent(chart.regions[i], geometry.motion, vent, planes[i]).mean > 0.)
            ports.push_back({i,1,1,0.,0.,true});
    return BuildCutSpatialField(chart, geometry, owners, source,
        std::vector<double>(geometry.faces.size()), ports, field, error, false);
}

int main(){
 for(bool rotated:{false,true}){
  CutSpatialField old,f;std::string e;CutOrthogonalRemap report;if(!BuildFixture(rotated,old,e)||!BuildCutOrthogonalField(old,f,report,e)){std::cerr<<e;return 2;}
  CutVelocityDual dual;if(!dual.Build(f,e)){std::cerr<<e;return 2;}CutDynamicSettings s;s.dt=double(tick);
  std::vector<Real> sw(f.water.begin(),f.water.end()),sm(f.gasMass.begin(),f.gasMass.end()),zero(dual.Nodes().size()),u=zero,p(f.parent.size());std::vector<double> dp(p.size()),du(u.size());
  for(std::size_t i=0;i<p.size();++i)dp[i]=1000.*std::sin(double(i)*.31);
  for(std::size_t j=0;j<u.size();++j)if(!dual.Nodes()[j].fixed){u[j]=dual.Nodes()[j].axis==1?-.1635:.02;du[j]=u[j].Value()*.1*std::cos(double(j)*.13);}
  CutCoupledResidual base,plus,minus;std::vector<Real> v,m;
  if(!base.Build(f,dual,s,sw,sm,zero,p,u,CutMomentumBackend::Fast,e)||!base.Tangent(dp,du,v,m,e)){std::cerr<<e;return 3;}
  const double h=1e-4;auto pp=p,pm=p,up=u,um=u;
  for(std::size_t i=0;i<p.size();++i){pp[i]=p[i]+Real(h*dp[i]);pm[i]=p[i]-Real(h*dp[i]);}
  for(std::size_t j=0;j<u.size();++j){up[j]=u[j]+Real(h*du[j]);um[j]=u[j]-Real(h*du[j]);}
  if(!plus.Build(f,dual,s,sw,sm,zero,pp,up,CutMomentumBackend::Fast,e)||!minus.Build(f,dual,s,sw,sm,zero,pm,um,CutMomentumBackend::Fast,e)){std::cerr<<e;return 4;}
  double verror=0.,merror=0.,mr=0.;
  for(std::size_t i=0;i<v.size();++i)verror=std::max(verror,std::abs(((plus.volumeRows[i]-minus.volumeRows[i])/Real(2.*h)-v[i]).Value())/f.geometry.startCapacity[i]);
  for(auto j:base.momentum.FreeNodes()){double d=std::abs(((plus.momentumRows[j]-minus.momentumRows[j])/Real(2.*h)-m[j]).Value())/base.momentum.Diagonal()[j].Value();merror=std::max(merror,d);mr=std::max(mr,std::abs(m[j].Value())/base.momentum.Diagonal()[j].Value());}
  double minC=*std::min_element(f.geometry.startCapacity.begin(),f.geometry.startCapacity.end());
  std::cout<<"BASIN_DERIVATIVE rotated="<<rotated<<" minCapacity="<<minC<<" volumePerCapacity="<<verror<<" momentumPerMass="<<merror<<" derivativePerMass="<<mr<<std::endl;
 }
}
