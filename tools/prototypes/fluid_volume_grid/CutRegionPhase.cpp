#include "CutRegionPhase.h"

#include "GridPhaseFluxRepair.h"

#include <cmath>
#include <utility>

namespace judas::volume_grid {
bool TransportCutRegionPhases(const CutRegionPhaseInput& input, CutRegionPhaseResult& output,
                              std::string& error) {
    GridPhaseFluxRepairInput repair;
    repair.startCapacity = input.startCapacity;
    repair.endCapacity = input.endCapacity;
    repair.startWater = input.startWater;
    repair.relayAllowed = input.relayAllowed;
    repair.balanceTolerance = input.balanceTolerance;
    repair.localOrdinaryDensityRatio = CutRegionWaterDensity;
    repair.repairUnrootedRelays = true;
    repair.edges.reserve(input.faces.size());
    for (const auto& face : input.faces) {
        const bool reverse = face.signedVolume < 0.;
        repair.edges.push_back({reverse ? face.right : face.left, reverse ? face.left : face.right,
                               std::abs(face.signedVolume), face.minWater, face.maxWater,
                               face.preferredWater});
    }

    GridPhaseFluxRepairResult solved;
    if (!RepairGridPhaseFlux(repair, solved, error))
        return false;

    CutRegionPhaseResult result;
    result.endWater = std::move(solved.endWater);
    result.waterFlux = std::move(solved.waterFlux);
    result.airFlux.resize(input.faces.size());
    for (std::size_t face = 0; face < input.faces.size(); ++face) {
        const double mixture = input.faces[face].signedVolume;
        result.airFlux[face] = std::copysign(repair.edges[face].volume - result.waterFlux[face],
                                           mixture);
        result.waterFlux[face] = std::copysign(result.waterFlux[face], mixture);
    }
    result.maximumVolumeResidual = solved.maximumVolumeResidual;
    result.maximumWaterResidual = solved.maximumWaterResidual;
    result.endpointWaterCorrection = solved.endpointWaterCorrection;
    result.conservationResidual = solved.conservationResidual;
    output = std::move(result);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
