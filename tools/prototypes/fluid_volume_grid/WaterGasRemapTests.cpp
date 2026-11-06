#include "WaterGasRemap.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;int checks=0,failures=0;void Check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
}
int main(){
    CutSpatialField f;f.water={0.,.9,0.};f.gas={1.,.1,1.};f.geometry.startCapacity={1.,1.,1.};
    WaterGasGeometry g;g.nodeCell={0,1,2};g.cellNode={0,1,2};WaterGasProjectionResult p;
    p.gasVolume={1.05,.1,1.};for(auto v:p.gasVolume)p.gasMass.push_back(Real(1.2)*v);
    WaterGasRemapResult result;std::string error;
    Check(RemapWaterGas(f,g,p,{0.,.85,0.},{{0,1,.05}},{},result,error),"a receding parcel remaps into an existing connected dry slab: "+error);
    if(result.volume.empty())return 1;
    Check(std::abs(result.volume[1].Value()-.15)<1e-14&&std::abs(result.mass[1].Value()-.18)<1e-14,"gas mass follows its real displaced parcel");
    Check(result.maximumPartitionResidual<=1.&&result.freeEnergyChange<=1e-10,"connected local remap preserves spatial partition and is passive");
    auto saved=result;f.water={0.,1.,0.};f.gas={1.,0.,1.};g.nodeCell={0,2};g.cellNode={0,SIZE_MAX,1};p.gasVolume={1.05,1.05};p.gasMass={1.26,1.26};
    Check(!RemapWaterGas(f,g,p,{0.,.9,0.},{{0,1,.05},{1,1,.05}},{},result,error)&&error.find("disconnected")!=std::string::npos,"opposed new air fronts cannot jump through a water separator");
    Check(result.mass[1].hi==saved.mass[1].hi,"failed front remap leaves the prior state intact");
    f.water={1.,.1};f.gas={1e-25,1.9};f.geometry.startCapacity={1.,2.};g.nodeCell={0,1};g.cellNode={0,1};
    p.gasVolume={Real(.1)+Real(1e-25),Real(1.9)};p.gasMass.clear();for(auto v:p.gasVolume)p.gasMass.push_back(Real(1.2)*v);
    Check(RemapWaterGas(f,g,p,{1.,0.},{{0,1,.1}},{},result,error),"sub-ULP native air is retained through a much larger remap: "+error);
    Check(result.volume[0].Value()>0.&&result.mass[0].Value()>0.&&std::abs(result.volume[0].Value()/1e-25-1.)<1e-12,"largest-last ledger construction does not erase the native minority parcel");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
