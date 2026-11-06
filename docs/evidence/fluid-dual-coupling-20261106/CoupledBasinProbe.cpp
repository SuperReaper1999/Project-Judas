#include "BasinFixture.h"
#include "CutCoupledPressure.h"
#include "CutOrthogonalField.h"
#include <chrono>
#include <iomanip>
#include <iostream>
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

int main(int argc,char** argv){
 std::cout<<std::setprecision(17);std::string mode=argc>1?argv[1]:"original",which=argc>2?argv[2]:"both";int steps=argc>3?std::stoi(argv[3]):1;
 for(bool rotated:{false,true}){
  if((which=="level"&&rotated)||(which=="rotated"&&!rotated))continue;
  CutSpatialField field;std::string error;if(!BuildFixture(rotated,field,error)){std::cerr<<error;return 2;}
  if(argc>4 && std::string(argv[4])=="orthogonal"){
   CutSpatialField uniform;CutOrthogonalRemap report;
   if(!BuildCutOrthogonalField(field,uniform,report,error)){std::cerr<<error;return 4;}
   std::cout<<"ORTHOGONAL rotated="<<rotated<<" old="<<report.originalCells<<" cells="<<report.orthogonalCells<<" x="<<report.intervals[0]<<" y="<<report.intervals[1]<<" z="<<report.intervals[2]<<" water="<<report.waterError<<" gas="<<report.gasMassError<<std::endl;
   field=std::move(uniform);
  }
  CutDynamicSettings settings;settings.dt=double(tick);settings.gravity={0.,-double(gravity),0.};settings.iterationLimit=85;
  std::vector<double> seed;
  if(mode=="filled"){
   const double height=2.,top=field.chart.layout.cells[1]*field.chart.layout.h,g=double(gravity),c2=settings.soundSpeed*settings.soundSpeed;
   for(std::size_t i=0;i<field.parent.size();++i){const auto& r=field.chart.regions[i];
    auto phases=MeasureCutRegionPhaseVolumes(r,field.geometry.motion,r.lo,r.hi,{{1,false},height});
    const double y=.5*(r.lo[1]+r.hi[1]);
    const double p=y>=height?settings.gasDensity*c2*std::expm1(g*(top-y)/c2):settings.gasDensity*c2*std::expm1(g*(top-height)/c2)+settings.waterDensity*g*(height-y);
    field.water[i]=phases[0];field.gas[i]=phases[1];field.gasMass[i]=phases[1]*(settings.gasDensity+p/c2);seed.push_back(p);
   }
  }
  CutVelocityDual geometry;auto started=std::chrono::steady_clock::now();if(!geometry.Build(field,error)){std::cerr<<error;return 2;}
  std::cout<<"GEOMETRY mode="<<mode<<" rotated="<<rotated<<" nodes="<<geometry.Nodes().size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<std::endl;
  CutCoupledSettings options;CutCoupledState state;Real coldWater,coldGas;
  for(double w:field.water)coldWater=coldWater+Real(w);
  for(double m:field.gasMass)coldGas=coldGas+Real(m);
  for(int step=0;step<steps;++step){
   CutCoupledState next;auto began=std::chrono::steady_clock::now();bool ok=AdvanceCutCoupledPressure(field,geometry,settings,options,next,error,step?&state:nullptr,step?std::vector<double>{}:seed);
   std::cout<<"BASIN mode="<<mode<<" rotated="<<rotated<<" step="<<step+1<<" admitted="<<ok<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()<<" outer="<<next.nonlinearIterations<<" volume="<<next.material.maximumNormalizedResidual<<" momentum="<<next.momentum.maximumNormalizedResidual<<" speed="<<next.momentum.maximumSpeed<<" energy="<<next.momentum.afterEnergy<<" workError="<<next.momentum.energyResidual<<" reason="<<error<<std::endl;
   if(!ok)return 1;
   next.material.externalWater=next.material.externalWater+state.material.externalWater;next.material.externalGasMass=next.material.externalGasMass+state.material.externalGasMass;
   Real w=next.material.externalWater,m=next.material.externalGasMass;for(auto q:next.material.water)w=w+q;for(auto q:next.material.gasMass)m=m+q;
   double we=(w-coldWater).Value(),me=(m-coldGas).Value();
   std::cout<<"LEDGER mode="<<mode<<" rotated="<<rotated<<" step="<<step+1<<" water="<<we<<" gas="<<me<<std::endl;
   if(std::abs(we)>settings.volumeTolerance||std::abs(me)>settings.gasDensity*settings.volumeTolerance)return 3;
   state=std::move(next);
  }
 }
 return 0;
}
