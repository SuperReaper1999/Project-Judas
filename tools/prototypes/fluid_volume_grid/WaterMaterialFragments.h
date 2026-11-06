#pragma once
#include "WaterGasProjection.h"
namespace judas::volume_grid {
using WaterCoordinate = std::array<CutCompensated,3>;
struct WaterMaterialFragment {
    std::size_t cell=0,carrier=0;
    bool water=false;
    WaterCoordinate lo{},hi{}; // local to the immutable containing cell
    CutCompensated gasMass;
    GridVec velocity{};
};
struct WaterMaterialState {
    std::vector<WaterMaterialFragment> fragments;
    CutCompensated importedWater,importedGasMass,waterImportError,externalGasMass;
};
struct WaterMaterialFace {
    std::size_t left=0,right=0;
    int axis=0;
    WaterCoordinate lo{},hi{}; // face in left containing-cell coordinates
};
CutCompensated MaterialVolume(const WaterMaterialFragment&);
bool ImportWaterMaterial(const CutSpatialField&,const std::vector<GridVec>&,
    WaterMaterialState&,std::string&);
// Positive-area adjacency, including nonconforming LOCAL fragments. No gas
// owner union, global cut-plane propagation or tolerance-based gap closure.
bool BuildWaterMaterialFaces(const CutSpatialField&,const WaterMaterialState&,
    std::vector<WaterMaterialFace>&,std::string&);
}
