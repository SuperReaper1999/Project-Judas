#include "WaterGasGeometry.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace judas::volume_grid {
namespace {
bool Fail(std::string& e,const char* s){e=s;return false;}
bool Conductance(int axis,int phaseAxis,const GridVec& lo,const GridVec& hi,
    double area,double length,const WaterGasSettings& settings,double viscosity,
    double& conductance,std::string& error){
    int widthAxis=(axis+1)%3;if(widthAxis==phaseAxis&&axis!=phaseAxis)widthAxis=(axis+2)%3;
    double width=hi[widthAxis]-lo[widthAxis],height=area/width;
    return RectangularGasMassConductance(width,height,length,settings.referenceDensity,
        settings.referencePressure,viscosity,conductance,error);
}
}
bool PrepareWaterGasGeometry(const CutSpatialField& field,const std::vector<CutRegionWaterPlane>& planes,
    const WaterGasSettings& settings,double viscosity,WaterGasGeometry& output,std::string& error){
    const auto n=field.parent.size();
    if(!n||field.chart.regions.size()!=n||field.gas.size()!=n||field.gasMass.size()!=n||planes.size()!=n||
       field.geometry.startCapacity.size()!=n||field.geometry.endCapacity!=field.geometry.startCapacity||field.geometry.motion||!(viscosity>0.&&std::isfinite(viscosity)))
        return Fail(error,"gas geometry requires frozen rectilinear phase cells");
    const int phaseAxis=planes[0].orientation.axis;
    if(phaseAxis<0||phaseAxis>2)return Fail(error,"invalid gas geometry phase axis");
    WaterGasGeometry next;next.cellNode.assign(n,SIZE_MAX);
    for(std::size_t i=0;i<n;++i){
        if(planes[i].orientation.axis!=phaseAxis)return Fail(error,"mixed gas interface axes require a native aperture-shape compiler");
        if(!(field.gas[i]>=0.&&std::isfinite(field.gas[i])&&field.gasMass[i]>=0.&&std::isfinite(field.gasMass[i])))return Fail(error,"invalid independent gas geometry quantity");
        if(field.gas[i]==0.){if(field.gasMass[i]!=0.)return Fail(error,"gas mass has no represented physical space");continue;}
        auto r=field.chart.regions[i];double full=1.;for(int a=0;a<3;++a)full*=r.hi[a]-r.lo[a];
        if(!(full>0.)||std::abs(full-field.geometry.startCapacity[i])>256.*std::numeric_limits<double>::epsilon()*full)return Fail(error,"gas geometry contains an unresolved cut or overlapping owner");
        auto measured=MeasureCutRegionPhaseVolumes(r,{},r.lo,r.hi,planes[i]);
        if(std::abs(measured[1]-field.gas[i])>1e-12*field.gas[i])
            return Fail(error,"gas interface does not preserve independently owned dry capacity");
        next.cellNode[i]=next.nodeCell.size();next.nodeCell.push_back(i);next.volume.push_back(field.gas[i]);next.mass.push_back(field.gasMass[i]);next.containingCapacity.push_back(field.geometry.startCapacity[i]);
    }

    for(const auto& face:field.geometry.faces){
        if(face.left>=n||face.right>=n||face.axis<0||face.axis>2)return Fail(error,"invalid gas geometry face");
        if(face.meanArea==0.)continue;
        double physicalArea=1.;for(int d=0;d<3;++d)if(d!=face.axis)physicalArea*=face.hi[d]-face.lo[d];
        if(!(physicalArea>0.&&std::isfinite(physicalArea)&&std::isfinite(face.meanArea))||std::abs(face.meanArea-physicalArea)>256.*std::numeric_limits<double>::epsilon()*physicalArea)
            return Fail(error,"partially blocked gas faces require an explicit aperture shape");
        auto a=next.cellNode[face.left],b=next.cellNode[face.right];
        if(a==SIZE_MAX||b==SIZE_MAX)continue;
        const auto& l=field.chart.regions[face.left];const auto& r=field.chart.regions[face.right];
        for(int d=0;d<3;++d)if(d!=face.axis&&(l.lo[d]!=r.lo[d]||l.hi[d]!=r.hi[d]||face.lo[d]!=l.lo[d]||face.hi[d]!=l.hi[d]))return Fail(error,"gas geometry requires matching rectangular shared faces");
        double area=MeasureCutRegionSharedPhase(face,{},planes[face.left],planes[face.right],false,false).start;
        if(!(area>=0.&&std::isfinite(area)))return Fail(error,"invalid jointly dry gas opening");
        if(area==0.)continue;
        double g=0.;if(!Conductance(face.axis,phaseAxis,face.lo,face.hi,area,face.length,settings,viscosity,g,error))return false;
        next.links.push_back({a,b,g});next.sharedDryArea.push_back(area);
    }
    for(auto port:field.boundaries){
        if(port.fragment>=n||port.axis<0||port.axis>2||(port.sign!=1&&port.sign!=-1)||!port.gasOnly||port.volume!=0.||port.incomingWater!=0.)return Fail(error,"gas geometry requires explicit gas-only exterior ports");
        auto node=next.cellNode[port.fragment];if(node==SIZE_MAX)continue;
        auto r=field.chart.regions[port.fragment];
        double edge=port.sign>0?r.hi[port.axis]:r.lo[port.axis];
        if(edge!=(port.sign>0?field.chart.layout.cells[port.axis]*field.chart.layout.h:0.))return Fail(error,"atmospheric gas port is not on a real exterior boundary");
        GridVec lo=r.lo,hi=r.hi;lo[port.axis]=hi[port.axis]=edge;
        double area=MeasureCutRegionGasVent(r,{},CutRegionGasVent{port.axis,port.sign,lo,hi},planes[port.fragment]).start;
        if(area==0.)continue;
        double g=0.;if(!Conductance(port.axis,phaseAxis,lo,hi,area,.5*(r.hi[port.axis]-r.lo[port.axis]),settings,viscosity,g,error))return false;
        next.vents.push_back({node,g,0.});
    }
    output=std::move(next);error.clear();return true;
}
}
