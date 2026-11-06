#include "WaterGasAperture.h"
#include "WaterFragmentAdvance.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using R=CutCompensated;
int checks=0,failures=0;
void Check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
WaterMaterialFragment Rectangle(R u0,R u1,R v0,R v1){WaterMaterialFragment p;p.lo={0.,u0,v0};p.hi={1.,u1,v1};return p;}
void Opening(){
    WaterGasSettings settings;std::string error;double area=0.,g=0.,expected=0.,split=0.;
    std::vector<WaterMaterialFragment> pieces;
    const int count=16;for(int i=0;i<count;++i)pieces.push_back(Rectangle(0.,.01,R(.01)*R(double(i)/count),R(.01)*R(double(i+1)/count)));
    bool ok=MeasureWaterGasAperture(pieces,0,1.,settings,area,g,error)&&RectangularGasMassConductance(.01,.01,1.,settings.referenceDensity,settings.referencePressure,1.8e-5,expected,error);
    Check(ok,"native fragmented opening compiles: "+error);
    Check(std::abs(g-expected)<1e-14*expected&&std::abs(area-1e-4)<1e-18,"physical rectangular resistance is invariant under integration cuts");
    for(int i=0;i<count;++i){double part=0.;RectangularGasMassConductance(.01,.01/count,1.,settings.referenceDensity,settings.referencePressure,1.8e-5,part,error);split+=part;}
    Check(split<.5*expected,"old independent-pipe negative control still changes a physically identical opening");
    std::reverse(pieces.begin(),pieces.end());double reversed=0.;
    Check(MeasureWaterGasAperture(pieces,0,1.,settings,area,reversed,error)&&reversed==g,"aperture resistance is independent of source order");
    pieces={Rectangle(0.,.01,0.,.01),Rectangle(0.,.01,.02,.03)};
    Check(MeasureWaterGasAperture(pieces,0,1.,settings,area,g,error)&&std::abs(area-2e-4)<1e-18&&std::abs(g-2.*expected)<1e-14*expected,
          "two separated openings remain two parallel channels without filling their gap");
    pieces={Rectangle(0.,1e-7,0.,1e-7)};
    Check(MeasureWaterGasAperture(pieces,0,1.,settings,area,g,error)&&g>0.&&g<expected*1e-10,
          "a microscopic actual aperture retains finite nonzero resistance");
}
void SharedWater(){
    CutSpatialField field;field.chart.layout={{2,1,1},1.};field.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1}};
    field.geometry.startCapacity=field.geometry.endCapacity={1.,1.};field.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};
    WaterMaterialState state,after;
    WaterMaterialFragment water;water.water=true;water.lo={0.,0.,0.};water.hi={1.,.5,1.};state.fragments.push_back(water);
    auto gas=water;gas.water=false;gas.lo[1]=R(.5);gas.hi[1]=R(1.);gas.carrier=1;gas.gasMass=R(.6);state.fragments.push_back(gas);
    for(int i=0;i<16;++i){auto p=water;p.cell=1;p.carrier=2+i;p.lo[2]=R(double(i)/16);p.hi[2]=R(double(i+1)/16);state.fragments.push_back(p);}
    gas.cell=1;gas.carrier=100;state.fragments.push_back(gas);state.importedWater=R(1.);state.importedGasMass=R(1.2);
    WaterGasSettings settings;WaterFragmentGeometry geometry;std::string error;
    bool ok=PrepareWaterFragmentGeometry(field,state,1000.,{0.,-9.81,0.},settings,geometry,error);
    Check(ok,"shared-water integration pieces compile: "+error);if(!ok)return;
    Check(geometry.liquid.sharedFaces==1&&geometry.liquid.regions[0].patches.size()==6,
          "sixteen integration rectangles share one physical pressure trace");
    WaterFragmentAdvanceStats stats;
    Check(AdvanceWaterFragments(field,state,1000.,{0.,-9.81,0.},settings,after,stats,error)&&after.fragments.size()==4,
          "shared trace reduction completes a quiet, fully partitioned hydrostatic interval: "+error);
}
void GasIntegration(){
    CutSpatialField field;field.chart.layout={{2,1,1},1.};
    field.chart.regions={{{0.,0.,0.},{1.,.01,.01},0},{{1.,0.,0.},{2.,.01,.01},1}};
    field.geometry.startCapacity=field.geometry.endCapacity={.0001,.0001};
    field.geometry.faces={{0,1,0,{1.,0.,0.},{1.,.01,.01},.0001,.0001,.0001,0.,1.}};
    WaterMaterialState state,after;R initialMass;
    for(std::size_t cell=0;cell<2;++cell)for(int i=0;i<16;++i){
        auto p=Rectangle(0.,.01,R(.01)*R(double(i)/16),R(.01)*R(double(i+1)/16));
        p.cell=cell;p.carrier=cell+1;p.gasMass=R(cell?1.2:2.4)*MaterialVolume(p);
        initialMass=initialMass+p.gasMass;state.fragments.push_back(p);
    }
    state.importedGasMass=initialMass;WaterGasSettings settings;std::string error;WaterFragmentGeometry geometry;
    bool ok=PrepareWaterFragmentGeometry(field,state,1000.,{},settings,geometry,error);
    Check(ok&&geometry.air.nodeCell.size()==2&&geometry.air.links.size()==1,
          "fragment compiler uses one actual opening for 32 air integration pieces: "+error);if(!ok)return;
    WaterGasProjectionResult projected;WaterGasResult reference;
    ok=ProjectWaterGasPressure(geometry.liquid,geometry.air,settings,projected,error)&&
       AdvanceWaterGasPockets(geometry.air.mass,geometry.air.volume,geometry.air.links,geometry.air.vents,settings,reference,error);
    Check(ok&&std::abs((projected.gasMass[0]-reference.mass[0]).Value())<2.*settings.massTolerance,
          "integrated gas solve matches the independent finite-resistance pocket solve: "+error);
    if(ok)Check((projected.gasAbsolutePressure[0]-projected.gasAbsolutePressure[1]).Value()>1.,
          "the compiled connection does not instantly equalize independent pressures");
    WaterFragmentAdvanceStats stats;ok=AdvanceWaterFragments(field,state,1000.,{},settings,after,stats,error);
    R finalMass;for(auto p:after.fragments)finalMass=finalMass+p.gasMass;
    Check(ok&&after.fragments.size()==2&&std::abs((finalMass-initialMass).Value())<1e-25,
          "finite exchange and exact retiling complete together with a cold air-mass ledger: "+error);
}

}
int main(){Opening();SharedWater();GasIntegration();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}
