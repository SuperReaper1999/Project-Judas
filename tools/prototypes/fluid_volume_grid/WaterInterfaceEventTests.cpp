#include "WaterFragmentAdvance.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace judas::volume_grid;
namespace {
using Real = CutCompensated;
int checks = 0, failures = 0;
void Check(bool ok, const std::string& why) {
    ++checks;
    if (!ok) { ++failures; std::cerr << why << '\n'; }
}
CutSpatialField Cell() {
    CutSpatialField field;
    field.chart.layout = {{1,1,1},1.};
    field.chart.regions = {{{0.,0.,0.},{1.,1.,1.},0}};
    field.geometry.startCapacity = field.geometry.endCapacity = {1.};
    return field;
}
WaterMaterialFragment Box(Real lower, Real upper, bool water, std::size_t carrier) {
    WaterMaterialFragment box;
    box.lo = {lower,0.,0.}; box.hi = {upper,1.,1.};
    box.water = water; box.carrier = carrier;
    if (!water) box.gasMass = Real(1.2)*MaterialVolume(box);
    return box;
}
WaterInterfaceSweep Sweep(std::size_t water, std::size_t gas, int sign,
                          Real position, Real amount) {
    WaterInterfaceSweep sweep;
    sweep.face.water = water; sweep.face.gas = gas;
    sweep.face.axis = 0; sweep.face.sign = sign;
    sweep.face.lo = {position,0.,0.}; sweep.face.hi = {position,1.,1.};
    sweep.volume = amount;
    return sweep;
}
bool Partition(const CutSpatialField& field, const WaterMaterialState& state) {
    std::vector<Real> volumes(field.chart.regions.size());
    for (std::size_t i = 0; i < state.fragments.size(); ++i) {
        auto a = state.fragments[i];
        if (a.cell >= volumes.size() || !(MaterialVolume(a).Value() > 0.)) return false;
        volumes[a.cell] = volumes[a.cell]+MaterialVolume(a);
        for (std::size_t j = 0; j < i; ++j) {
            auto b = state.fragments[j]; if (a.cell != b.cell) continue;
            bool overlap = true;
            for (int d = 0; d < 3; ++d)
                overlap &= (a.hi[d]-b.lo[d]).Value() > 0. && (b.hi[d]-a.lo[d]).Value() > 0.;
            if (overlap) return false;
        }
    }
    for (std::size_t i=0; i<volumes.size(); ++i)
        if (std::abs((volumes[i]-Real(field.geometry.startCapacity[i])).Value()) > 1e-28) return false;
    return true;
}
void Film() {
    auto field = Cell(); WaterMaterialState state, after; std::string error;
    state.fragments = {Box(0.,.3,true,0), Box(.3,.4,false,1),
                       Box(.4,.7,true,2), Box(.7,1.,false,3)};
    std::vector<Real> mass(state.fragments.size());
    for (std::size_t i=0; i<mass.size(); ++i) mass[i]=state.fragments[i].gasMass;
    // The gas film first grows into the upper liquid, then its lower edge
    // follows. Growth must see the newly created air, not its old bounds.
    auto sweeps = std::vector<WaterInterfaceSweep>{
        Sweep(0,1,1,.3,.15), Sweep(2,1,-1,.4,-.15)};
    WaterInterfaceEventReport report;
    bool ok = ApplyWaterInterfaceEvents(field,state,sweeps,mass,after,report,error);
    Check(ok,"translating film crosses its OLD boundary: "+error);
    if (!ok) return;
    Check(Partition(field,after),"translated film partitions the cell without overlap or holes");
    Real gas, water, first, last;
    bool initialized=false;
    for (auto f:after.fragments) {
        if (f.water) water=water+MaterialVolume(f);
        else {gas=gas+f.gasMass;if(f.carrier==1){if(!initialized){first=f.lo[0];last=f.hi[0];initialized=true;}else{if((f.lo[0]-first).Value()<0.)first=f.lo[0];if((f.hi[0]-last).Value()>0.)last=f.hi[0];}}}
    }
    Check(std::abs((first-Real(.45)).Value())<1e-16&&std::abs((last-Real(.55)).Value())<1e-16,
          "gas retains a spatial film instead of instantly mixing in its owner");
    Check(std::abs((gas-mass[1]-mass[3]).Value())<1e-28&&
          std::abs((water-Real(.3)-(Real(.7)-Real(.4))).Value())<1e-28,
          "film crossing preserves independently measured water and air mass");
    auto saved=after;
    sweeps[0].volume=1.;
    Check(!ApplyWaterInterfaceEvents(field,state,sweeps,mass,after,report,error)&&
          after.fragments.size()==saved.fragments.size(),
          "exhausted material rejects without publishing a partial crossing");
}
void ThinMetric() {
    auto field=Cell(); WaterMaterialState state; std::string error;
    Real start(.1),end=start+Real(1e-12),split(.123456789);
    state.fragments={Box(0.,start,false,0),Box(start,end,true,1),Box(end,1.,false,2),Box(end,1.,false,3),Box(end,1.,false,4)};
    state.fragments[2].hi[1]=split;state.fragments[3].lo[1]=split;
    state.fragments[3].hi[1]=Real(.876543210123);state.fragments[4].lo[1]=Real(.876543210123);
    for(auto& f:state.fragments)if(!f.water)f.gasMass=Real(1.2)*MaterialVolume(f);
    WaterGasSettings settings;WaterFragmentGeometry geometry;
    bool ok=PrepareWaterFragmentGeometry(field,state,1000.,{0.,-9.81,0.},settings,geometry,error);
    Check(ok,"thin slab retains exact partitioned boundary moments: "+error);
    if(!ok)return;
    Check(geometry.liquid.regions.size()==1&&geometry.air.nodeCell.size()==4,
          "thin water separator does not join the gas compartments");
    auto patches=geometry.liquid.regions[0].patches;
    for(auto& patch:patches)patch.compensatedGeometry=false;
    WaterCellProjection rejected;
    Check(!PrepareWaterMimeticCell(MaterialVolume(state.fragments[1]).Value(),1000.,settings.dt,
          {0.,-9.81,0.},patches,rejected,error),
          "binary64 boundary-rounding negative control still fails the unchanged geometry gate");
}
void IntegrationPieces() {
    auto field=Cell();WaterMaterialState state,after;std::string error;
    auto water=Box(0.,1.,true,0);water.hi[1]=Real(.5);state.fragments.push_back(water);
    constexpr int pieces=64;
    for(int i=0;i<pieces;++i){auto gas=Box(Real(double(i)/pieces),Real(double(i+1)/pieces),false,1);gas.lo[1]=Real(.5);gas.gasMass=Real(1.2)*MaterialVolume(gas);state.fragments.push_back(gas);}
    state.importedWater=Real(.5);state.importedGasMass=Real(.6);
    WaterGasSettings settings;WaterFragmentGeometry geometry;
    bool ok=PrepareWaterFragmentGeometry(field,state,1000.,{0.,-9.81,0.},settings,geometry,error);
    Check(ok,"64 surface integration pieces prepare: "+error);if(!ok)return;
    Check(geometry.liquid.regions[0].patches.size()==6&&geometry.air.nodeCell.size()==1&&geometry.interfaces.size()==64,
          "physical owner and six normal velocities remain separate from 64 surface pieces");
    WaterGasProjectionResult projected,repeated;
    bool projectedOk=ProjectWaterGasPressure(geometry.liquid,geometry.air,settings,projected,error);
    Check(projectedOk,"quiet fragmented pressure proposal prepares: "+error);
    if(projectedOk){auto seed=projected;seed.gasMass.assign(seed.gasMass.size(),Real(1e20));
        bool repeatedOk=ProjectWaterGasPressure(geometry.liquid,geometry.air,settings,repeated,error,&seed);
        Check(repeatedOk&&!repeated.gasMass.empty()&&std::abs((repeated.gasMass[0]-projected.gasMass[0]).Value())<1e-28,
              "numerical seed never imports stale physical air mass");
        auto saved=repeated;seed.gasAbsolutePressure.clear();
        Check(!ProjectWaterGasPressure(geometry.liquid,geometry.air,settings,repeated,error,&seed)&&repeated.gasMass.size()==saved.gasMass.size(),
              "dimensionally incompatible pressure seed rejects transactionally");
        seed=projected;seed.gasAbsolutePressure[0]=Real(std::numeric_limits<double>::quiet_NaN());
        Check(!ProjectWaterGasPressure(geometry.liquid,geometry.air,settings,repeated,error,&seed)&&repeated.gasMass.size()==saved.gasMass.size(),
              "nonfinite numerical pressure seed rejects transactionally");
    }
    std::cout<<"INTEGRATION_PIECES rectangles="<<pieces<<" velocityDofs="<<geometry.liquid.regions[0].patches.size()<<" gasOwners="<<geometry.air.nodeCell.size()<<"\n";
    WaterFragmentAdvanceStats stats;
    Check(AdvanceWaterFragments(field,state,1000.,{0.,-9.81,0.},settings,after,stats,error)&&Partition(field,after),
          "integration-piece reduction retains a quiet, completely partitioned pool: "+error);
    // The same label does not join air separated by actual water.
    WaterMaterialState separated;separated.fragments={Box(0.,.3,false,0),Box(.3,.7,true,1),Box(.7,1.,false,0)};
    Check(PrepareWaterFragmentGeometry(field,separated,1000.,{},settings,geometry,error)&&geometry.air.nodeCell.size()==2,
          "an old carrier label cannot join disconnected air through water");
    state.fragments[1].carrier=2;
    Check(PrepareWaterFragmentGeometry(field,state,1000.,{},settings,geometry,error)&&geometry.air.nodeCell.size()==2&&!geometry.air.links.empty(),
          "different air owners retain a finite physical connection rather than instant pressure union");
}

void AcrossCells() {
    auto field=Cell();field.chart.layout.cells[0]=2;field.chart.regions.push_back({{1.,0.,0.},{2.,1.,1.},1});
    field.geometry.startCapacity.push_back(1.);field.geometry.endCapacity.push_back(1.);
    field.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};
    WaterMaterialState state,after;state.fragments={Box(0.,.9,true,0),Box(.9,1.,false,1),Box(0.,.5,true,2),Box(.5,1.,false,3)};
    state.fragments[2].cell=state.fragments[3].cell=1;
    std::vector<Real> mass(state.fragments.size());for(std::size_t i=0;i<mass.size();++i)mass[i]=state.fragments[i].gasMass;
    std::vector<WaterInterfaceSweep> sweeps={Sweep(0,1,1,.9,.3),Sweep(2,1,-1,0.,-.3)};
    WaterInterfaceEventReport report;std::string error;
    bool ok=ApplyWaterInterfaceEvents(field,state,sweeps,mass,after,report,error);
    Check(ok&&Partition(field,after),"translating film continues through an actual containing-cell aperture: "+error);
    auto saved=after;
    field.geometry.faces.clear();
    Check(!ApplyWaterInterfaceEvents(field,state,sweeps,mass,after,report,error)&&after.fragments.size()==saved.fragments.size(),
          "the same ray cannot cross a closed physical wall");
    auto invalid=sweeps;invalid[0].face.axis=3;
    Check(!ApplyWaterInterfaceEvents(field,state,invalid,mass,after,report,error),"malformed sweep axes reject before accessing geometry");
}

}
int main() {
    Film(); ThinMetric(); IntegrationPieces(); AcrossCells();
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
