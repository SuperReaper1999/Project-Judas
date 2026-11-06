#include "CutPhaseFronts.h"
#include "CutOwnedProjection.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

using namespace judas::volume_grid;
namespace {
int checks=0, failures=0;
void Check(bool value,const char* name){++checks;if(!value)++failures;std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';}
bool Near(double a,double b){return std::abs(a-b)<2e-12;}
struct Channel {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    CutConnectedOwners owners;
    CutDynamicState source;
    std::vector<double> external,predictor,mobility;
    std::vector<CutPhaseBoundary> boundary;
    Channel(int cells,double flow,bool reverse=false){
        std::string error;
        Check(PrepareCutRegionChart({{cells,1,1},1.},{},{},{},{},chart,error)&&
              BuildCutRegionGeometry(chart,{},geometry,error),"compile actual channel geometry");
        owners.owner.assign(cells,0);owners.members.resize(1);owners.capacity={double(cells)};
        owners.lo={{0,0,0}};owners.hi={{double(cells),1,1}};
        for(int i=0;i<cells;++i)owners.members[0].push_back(i);
        source.water.assign(cells,0.);source.gasMass.assign(cells,1.2);
        external.resize(geometry.faces.size());predictor=external;mobility.assign(external.size(),1.);
        boundary={{std::size_t(reverse?cells-1:0),0,reverse?1:-1,-flow,1.,false},
                  {std::size_t(reverse?0:cells-1),0,reverse?-1:1,flow,0.,false}};
    }
    bool Run(CutPhaseFrontResult& out,std::string& error,CutPhaseFrontSettings settings={}){
        return AdvanceCutPhaseFronts(chart,geometry,owners,source,{1.2},external,boundary,predictor,mobility,settings,out,error);
    }
};
}
int main(){
    std::string error;
    for(int cells:{1,3})for(bool reverse:{false,true}){
        Channel c(cells,4.,reverse);CutPhaseFrontResult out;
        Check(c.Run(out,error),"finite front advances through connected fragments in either direction");
        if(out.ownerWater.empty()){std::cerr<<error<<'\n';continue;}
        Check(Near(out.ownerWater[0],cells)&&Near(out.ownerGasMass[0],0.),"outlet switches from gas to water after front arrival");
        Check(Near(out.externalWater,-cells)&&Near(out.externalGasMass,1.2*cells),"independent inlet/outlet ledgers balance both phases");
        Check(std::all_of(out.water.begin(),out.water.end(),[](double w){return w==1.;}),"geometric fragments are full without negative gas");
        Check(out.maximumPartitionError==0.,"spatial partition equals primary owner ledger");
        CutPhaseFrontResult repeat;
        Check(c.Run(repeat,error)&&repeat.water==out.water&&repeat.ownerGasMass==out.ownerGasMass&&repeat.waterFlux==out.waterFlux&&repeat.steps==out.steps,"phase advancement repeats deterministically");
    }
    {
        Channel c(1,.25);CutPhaseFrontResult out;
        Check(c.Run(out,error),"partial filling interval advances");
        Check(out.ownerWater.size()==1&&Near(out.ownerWater[0],.25)&&Near(out.ownerGasMass[0],.9),"partial front retains unexhausted air");
    }
    {
        Channel c(3,4.);CutOwnedFlux field;
        std::vector<CutBoundaryTransfer> ports={{0,-4.},{2,4.}};
        Check(ReconstructCutOwnedFlux(c.chart,c.geometry,c.owners,c.external,ports,{1.,1.,1.},c.predictor,c.mobility,field,error),"reconstruct retained internal kinetic flux");
        Check(field.faceVolume.size()==2&&Near(field.faceVolume[0],4.)&&Near(field.faceVolume[1],4.),"internal flux transmits whole boundary flow");
        Check(std::all_of(field.divergence.begin(),field.divergence.end(),[](double d){return d==0.;}),"balanced channel has no compressibility source");
        auto singleton=c.owners;singleton.owner={0,1,2};singleton.members={{0},{1},{2}};singleton.capacity={1.,1.,1.};
        Check(ReconstructCutOwnedFlux(c.chart,c.geometry,singleton,{1.,2.},{{2,2.}},{1.,0.,0.},c.predictor,c.mobility,field,error),"pure-water correction uses neighboring gas pressure as anchor");
        Check(Near(field.faceVolume[0],2.)&&Near(field.faceVolume[1],2.),"kinetic projection enforces actual incompressibility");
        field.faceVolume={-7.};
        Check(!ReconstructCutOwnedFlux(c.chart,c.geometry,c.owners,c.external,{{2,1.}},{0.,0.,0.},c.predictor,c.mobility,field,error)&&field.faceVolume==std::vector<double>{-7.},"incompatible sealed water source rejects transactionally");
        const std::vector<double> invalidSources={std::numeric_limits<double>::quiet_NaN()};
        Check(!ReconstructCutOwnedFlux(c.chart,c.geometry,c.owners,c.external,ports,{1.,1.,1.},c.predictor,c.mobility,field,error,1e-10,false,&invalidSources),"nonfinite retained pressure source rejects");
        auto bad=c.owners;bad.members[0][0]=99;
        Check(!ReconstructCutOwnedFlux(c.chart,c.geometry,bad,c.external,ports,{1.,1.,1.},c.predictor,c.mobility,field,error),"invalid ownership is rejected before indexing");
    }
    {
        CutRegionChart chart;chart.layout={{2,2,1},1.};
        chart.regions={{{0.,0.,0.},{1.,1.,1.},0},{{1.,0.,0.},{2.,1.,1.},1},{{1.,1.,0.},{2.,2.,1.},3}};
        chart.faces={{0,1,0,{1.,0.,0.},{1.,1.,1.},1.,1.,1.,0.,.5},
                     {1,2,1,{1.,1.,0.},{2.,1.,1.},1.,1.,1.,0.,.5}};
        CutRegionGeometry geometry;
        Check(BuildCutRegionGeometry(chart,{},geometry,error),"compile actual nonrectangular turning owner");
        CutConnectedOwners owners;owners.owner={0,0,0};owners.members={{0,1,2}};owners.capacity={3.};
        CutOwnedFlux field;
        Check(ReconstructCutOwnedFlux(chart,geometry,owners,{0.,0.},{{0,-4.},{2,4.}},{1.,1.,1.},{0.,0.},{1.,1.},field,error),"recover interior flow through a right-angle connected path");
        Check(field.faceVolume.size()==2&&Near(field.faceVolume[0],4.)&&Near(field.faceVolume[1],4.),"turning path carries both boundary transfers without a bounding-box shortcut");
    }
    {
        Channel c(1,4.);CutPhaseFrontResult out;out.water={-7.};CutPhaseFrontSettings settings;settings.maximumSteps=1;
        Check(!c.Run(out,error,settings)&&out.water==std::vector<double>{-7.},"exhausted temporal budget publishes no partial state");
        CutOwnedProjection physical;physical.water={-7.};CutDynamicSettings projectionSettings;
        Check(!ProjectCutOwnedMaterial(c.chart,c.geometry,c.owners,c.source,projectionSettings,{},physical,error,nullptr,&physical)&&physical.water==std::vector<double>{-7.},"diagnostic candidate cannot alias physical output");
        c.boundary[1].gasOnly=true;
        Check(!c.Run(out,error)&&out.water==std::vector<double>{-7.},"gas-only port cannot silently discharge water");
        c.boundary[0].axis=3;
        Check(!c.Run(out,error),"invalid boundary axis rejects");
        c.boundary[0].axis=0;c.source.pressure={0.,0.};
        Check(!c.Run(out,error),"invalid pressure dimensions reject");
        c.source.pressure={-2.*1.2*343.*343.};
        Check(!c.Run(out,error),"invalid initial gas EOS rejects");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
