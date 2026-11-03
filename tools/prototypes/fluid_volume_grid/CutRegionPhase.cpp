#include "CutRegionPhase.h"

#include "GridPhaseFluxRepair.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace judas::volume_grid {
namespace {

bool AuditRelaySources(const GridPhaseFluxRepairInput& input,
                       const GridPhaseFluxRepairResult& output) {
    if (std::none_of(input.relayAllowed.begin(), input.relayAllowed.end(),
                     [](bool relay) { return relay; }))
        return true;

    constexpr unsigned wet = 1, air = 2;
    const auto count = input.startWater.size();
    std::vector<std::vector<std::size_t>> outgoing(count);
    std::vector<unsigned> transported(input.edges.size()), rooted(count);
    for (std::size_t face = 0; face < input.edges.size(); ++face) {
        const auto& edge = input.edges[face];
        const double water = output.waterFlux[face];
        transported[face] = (water > 0. ? wet : 0) | (water < edge.volume ? air : 0);
        if (transported[face] != 0)
            outgoing[edge.from].push_back(face);
    }

    std::vector<std::size_t> pending;
    for (std::size_t region = 0; region < count; ++region) {
        const double water = input.startWater[region];
        rooted[region] = (water > 0. ? wet : 0) |
            (water < input.startCapacity[region] ? air : 0);
        if (rooted[region] != 0)
            pending.push_back(region);
    }
    for (std::size_t entry = 0; entry < pending.size(); ++entry) {
        const auto region = pending[entry];
        for (const auto face : outgoing[region]) {
            const auto receiver = input.edges[face].to;
            const auto added = (transported[face] & rooted[region]) & ~rooted[receiver];
            if (added != 0 && input.relayAllowed[receiver]) {
                rooted[receiver] |= added;
                pending.push_back(receiver);
            }
        }
    }
    for (std::size_t face = 0; face < input.edges.size(); ++face)
        if ((transported[face] & ~rooted[input.edges[face].from]) != 0)
            return false;
    return true;
}

} // namespace

bool TransportCutRegionPhases(const CutRegionPhaseInput& input, CutRegionPhaseResult& output,
                              std::string& error) {
    GridPhaseFluxRepairInput repair;
    repair.startCapacity = input.startCapacity;
    repair.endCapacity = input.endCapacity;
    repair.startWater = input.startWater;
    repair.relayAllowed = input.relayAllowed;
    repair.balanceTolerance = input.balanceTolerance;
    repair.localOrdinaryDensityRatio = CutRegionWaterDensity;
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
    if (!AuditRelaySources(repair, solved)) {
        error = "cut region phase transfer has no initial source along an admitted relay path";
        return false;
    }

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
