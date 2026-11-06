#include "WaterGasAperture.h"
#include <cmath>

namespace judas::volume_grid {
bool MeasureWaterGasAperture(const std::vector<WaterMaterialFragment>& rectangles,
    int axis,double length,const WaterGasSettings& settings,double& area,
    double& conductance,std::string& error) {
    if(axis<0||axis>2||!(length>0.&&std::isfinite(length))){error="invalid physical gas aperture length or axis";return false;}
    std::vector<WaterMaterialFragment> canonical;
    if(!CanonicalWaterMaterialBoxes(rectangles,canonical,error))return false;
    CutCompensated sumArea,sumConductance;
    const int u=(axis+1)%3,v=(axis+2)%3;
    for(const auto& rectangle:canonical){
        double width=(rectangle.hi[u]-rectangle.lo[u]).Value();
        double height=(rectangle.hi[v]-rectangle.lo[v]).Value(),g=0.;
        if(!RectangularGasMassConductance(width,height,length,settings.referenceDensity,
            settings.referencePressure,1.8e-5,g,error))return false;
        sumArea=sumArea+(rectangle.hi[u]-rectangle.lo[u])*(rectangle.hi[v]-rectangle.lo[v]);
        sumConductance=sumConductance+CutCompensated(g);
    }
    if(!(sumArea.Value()>0.&&sumConductance.Value()>0.&&std::isfinite(sumConductance.Value()))){error="physical gas opening has unrepresentable finite resistance";return false;}
    area=sumArea.Value();conductance=sumConductance.Value();error.clear();return true;
}
}
