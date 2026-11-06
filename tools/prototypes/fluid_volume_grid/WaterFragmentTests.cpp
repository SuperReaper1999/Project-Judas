#include "WaterFragmentAdvance.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;int checks=0,failures=0;
void Check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
CutSpatialField Columns(double a,double b){CutSpatialField f;f.chart.layout={{2,1,1},1.};f.parent={0,1};f.chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1}};f.geometry.startCapacity=f.geometry.endCapacity={1.,1.};f.geometry.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,1.}};f.water={a,b};f.gas={1.-a,1.-b};f.gasMass={1.2*(1.-a),1.2*(1.-b)};f.flux={0.};return f;}
double Energy(const WaterMaterialState& state) {
    double energy=0.;
    for(auto f:state.fragments){double v=MaterialVolume(f).Value();
        if(f.water){double speed2=0.;for(auto component:f.velocity)speed2+=component*component;
            double y=((f.lo[1]+f.hi[1])*Real(.5)).Value();
            energy+=.5*1000.*v*speed2+1000.*9.81*v*y;
        }else{double mass=f.gasMass.Value();energy+=101325./1.2*mass*(std::log(mass/(1.2*v))-1.)+101325.*v;}
    }
    return energy;
}
void Sustained() {
    auto field=Columns(.6,.4);WaterMaterialState state;std::string error;WaterGasSettings settings;
    if(!ImportWaterMaterial(field,{},state,error)){Check(false,error);return;}
    double before=Energy(state);bool advancing=true,energy=true,quantity=true;
    for(int step=0;step<240;++step){WaterMaterialState after;WaterFragmentAdvanceStats stats;
        if(!AdvanceWaterFragments(field,state,1000.,{0.,-9.81,0.},settings,after,stats,error)){std::cerr<<"sustained step="<<step<<" ";advancing=false;break;}
        double next=Energy(after);if(next>before+1e-8*std::max(before,1.))energy=false;before=next;
        Real water,mass;for(auto f:after.fragments){if(f.water)water=water+MaterialVolume(f);else mass=mass+f.gasMass;}
        quantity &= std::abs((water-Real(1.)).Value())<1e-12&&std::abs((mass-Real(1.2)).Value())<1e-12;
        state=std::move(after);
    }
    Check(advancing,"240 spatial-fragment flow intervals: "+error);
    Check(advancing&&energy,"adjacent-interval cold total energy cannot grow");
    Check(advancing&&quantity,"cold water and finite air ledgers survive sustained flow");
}

}
int main(){
    auto field=Columns(.5,.5);WaterMaterialState state,after;std::string error;WaterGasSettings settings;
    Check(ImportWaterMaterial(field,{},state,error),"import conserved local pieces: "+error);
    Check(state.fragments.size()==4&&std::abs(state.waterImportError.Value())<1e-30,"one independent gas and water piece per half-full cell");
    WaterFragmentGeometry geometry;Check(PrepareWaterFragmentGeometry(field,state,1000.,{0.,-9.81,0.},settings,geometry,error),"compile fragment apertures and liquid inertia: "+error);
    Check(geometry.liquid.regions.size()==2&&geometry.air.mass.size()==2&&geometry.air.links.size()==1,"physical compartments and finite dry connection stay distinct");
    WaterFragmentAdvanceStats stats;Check(AdvanceWaterFragments(field,state,1000.,{0.,-9.81,0.},settings,after,stats,error),"quiet complete interval: "+error);
    Real water,mass;double speed=0.;for(auto f:after.fragments){if(f.water){water=water+MaterialVolume(f);for(double v:f.velocity)speed=std::max(speed,std::abs(v));}else mass=mass+f.gasMass;}
    Check(std::abs((water-Real(1.)).Value())<1e-25&&std::abs((mass-Real(1.2)).Value())<1e-15&&speed<1e-12,"quiet fragment interval conserves materials and stays at rest");
    field=Columns(.6,.4);Check(ImportWaterMaterial(field,{},state,error),"import sloped free surface");
    Check(AdvanceWaterFragments(field,state,1000.,{0.,-9.81,0.},settings,after,stats,error),"unequal columns complete geometry crossing: "+error);
    if(!after.fragments.empty()){Real left;for(auto f:after.fragments)if(f.water&&f.cell==0)left=left+MaterialVolume(f);Check(left.Value()<.6,"real spatial water moves to the lower column");}
    auto saved=after;field.gasMass[0]=-1.;WaterMaterialState invalid;
    Check(!ImportWaterMaterial(field,{},invalid,error),"negative air mass rejects");
    Check(after.fragments.size()==saved.fragments.size(),"failed independent import cannot overwrite published material");
    Sustained();
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
