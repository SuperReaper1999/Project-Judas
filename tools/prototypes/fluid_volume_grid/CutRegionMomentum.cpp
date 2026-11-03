#include "CutRegionMomentum.h"

#include "GridConservativeRemap.h"

#include <utility>

namespace judas::volume_grid {
namespace {

constexpr double kWaterDensity = 1000.;
constexpr double kRoundoff = 256. * std::numeric_limits<double>::epsilon();

struct CompensatedSum {
    double sum = 0., correction = 0.;

    void Add(double value) {
        const double next = sum + value;
        correction += std::abs(sum) >= std::abs(value) ? (sum - next) + value
                                                       : (value - next) + sum;
        sum = next;
    }

    double Value() const { return sum + correction; }
};

struct ImpulseLedger {
    CompensatedSum before, after, work, workMagnitude;

    bool Kick(double mass, double momentum, double speed, double push, double& next,
              double& nextSpeed) {
        if (!std::isfinite(speed) || !std::isfinite(push))
            return false;
        if (mass == 0.) {
            next = nextSpeed = 0.;
            return push == 0.;
        }
        next = momentum + push;
        nextSpeed = next / mass;
        const double impulseWork = push * (.5 * speed + .5 * nextSpeed);
        if (!std::isfinite(momentum) || !std::isfinite(nextSpeed) || !std::isfinite(impulseWork))
            return false;
        work.Add(impulseWork);
        workMagnitude.Add(std::abs(impulseWork));
        before.Add((.5 * momentum) * speed);
        after.Add((.5 * next) * nextSpeed);
        return true;
    }

    bool Close(double& residual) const {
        const double initial = before.Value(), final = after.Value();
        residual = (final - initial) - work.Value();
        const double limit = kRoundoff * std::max(initial, final) + kRoundoff * workMagnitude.Value();
        return std::isfinite(residual) && std::isfinite(limit) && std::abs(residual) <= limit;
    }
};

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool Nonnegative(double value) {
    return std::isfinite(value) && value >= 0.;
}

double MixtureMass(double capacity, double water) {
    return kWaterDensity * water + (capacity - water);
}

} // namespace

bool TransportCutRegionMomentum(const CutRegionPhaseInput& input,
                                 const CutRegionPhaseResult& phases,
                                 const std::vector<GridVec>& startVelocity,
                                 const std::vector<GridVec>& impulse,
                                 const std::vector<GridVec>& endImpulse,
                                 CutRegionMomentumResult& output, std::string& error) {
    const auto count = input.startCapacity.size();
    if (input.endCapacity.size() != count || input.startWater.size() != count ||
        phases.endWater.size() != count || startVelocity.size() != count || impulse.size() != count ||
        phases.waterFlux.size() != input.faces.size() || phases.airFlux.size() != input.faces.size() ||
        (!endImpulse.empty() && endImpulse.size() != count) ||
        (!input.relayAllowed.empty() && input.relayAllowed.size() != count) ||
        !Nonnegative(input.balanceTolerance))
        return Fail(error, "invalid cut region momentum dimensions or tolerance");

    ConservativeRemapInput remap;
    remap.start.resize(count);
    remap.end.resize(count);
    remap.relayAllowed = input.relayAllowed;
    remap.relayAllowed.resize(count);
    remap.balanceTolerance = kWaterDensity * input.balanceTolerance;
    remap.allowDonorRoundoff = true;
    remap.transfers.reserve(input.faces.size());
    std::vector<GridVec> before(count), kicked(count);
    std::vector<std::array<CompensatedSum, 2>> outgoing(count);
    std::vector<CompensatedSum> receivedVolume(count);
    CompensatedSum massResidual, massBudget;
    ImpulseLedger startWork, endWork;
    for (std::size_t region = 0; region < count; ++region) {
        const double start = input.startCapacity[region], end = input.endCapacity[region];
        const double wetStart = input.startWater[region], wetEnd = phases.endWater[region];
        if (!Nonnegative(wetStart) || !Nonnegative(wetEnd) || !Nonnegative(start) ||
            !Nonnegative(end) || wetStart > start || wetEnd > end)
            return Fail(error, "invalid cut region phase inventory for momentum");

        const double startMass = remap.start[region] = MixtureMass(start, wetStart);
        const double endMass = remap.end[region] = MixtureMass(end, wetEnd);
        if (!std::isfinite(startMass) || !std::isfinite(endMass))
            return Fail(error, "cut region mass is not representable");
        massResidual.Add(endMass - startMass);
        massBudget.Add(kRoundoff * std::max(startMass, endMass));
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const double speed = startVelocity[region][axis], push = impulse[region][axis];
            if (startMass == 0. && push != 0.)
                return Fail(error, "cut region impulse has no initial material owner");
            const double momentum = before[region][axis] = startMass * speed;
            double nextSpeed;
            if (!startWork.Kick(startMass, momentum, speed, push, kicked[region][axis], nextSpeed))
                return Fail(error, "cut region impulse cannot be represented");
        }
    }
    for (std::size_t face = 0; face < input.faces.size(); ++face) {
        const auto& opening = input.faces[face];
        const double wet = phases.waterFlux[face], dry = phases.airFlux[face];
        const double volume = opening.signedVolume;
        if (opening.left >= count || opening.right >= count || opening.left == opening.right ||
            !std::isfinite(volume) || !std::isfinite(wet) || !std::isfinite(dry) ||
            (volume >= 0. ? wet < 0. || dry < 0. : wet > 0. || dry > 0.) ||
            std::abs((wet + dry) - volume) > kRoundoff * std::abs(volume) ||
            !(opening.minWater <= std::abs(wet) && std::abs(wet) <= opening.maxWater))
            return Fail(error, "cut region momentum requires the accepted signed phase flux");
        const double mass = kWaterDensity * wet + dry;
        if (!std::isfinite(mass))
            return Fail(error, "cut region face mass transfer is not representable");
        remap.transfers.push_back({opening.left, opening.right, mass});
        massBudget.Add(2. * kRoundoff * std::abs(mass));
        const auto donor = volume < 0. ? opening.right : opening.left;
        const auto receiver = volume < 0. ? opening.left : opening.right;
        outgoing[donor][0].Add(std::abs(wet));
        outgoing[donor][1].Add(std::abs(dry));
        receivedVolume[receiver].Add(std::abs(volume));
    }
    for (std::size_t region = 0; region < count; ++region) {
        if (remap.relayAllowed[region])
            continue;
        const double capacity = input.startCapacity[region];
        const double wet = input.startWater[region], dry = capacity - wet;
        const double wetOut = outgoing[region][0].Value(), dryOut = outgoing[region][1].Value();
        // Air can be the difference of two almost equal volumes. Bound that
        // subtraction by the mixture scale and this region's represented
        // mixture residual. The allowance must remain roundoff in mass.
        const double limit = kRoundoff * capacity;
        const double volumeResidual = std::abs((capacity + receivedVolume[region].Value()) -
            (input.endCapacity[region] + (wetOut + dryOut)));
        const double airLimit = limit + std::min(volumeResidual, kRoundoff * remap.start[region]);
        if (!std::isfinite(wetOut) || !std::isfinite(dryOut) ||
            (wet == 0. && wetOut > 0.) || (dry == 0. && dryOut > 0.) ||
            wetOut - wet > limit || dryOut - dry > airLimit)
            return Fail(error, "cut region momentum phase leaves an ordinary donor before receipt");
    }

    CutRegionMomentumResult result;
    result.massResidual = massResidual.Value();
    const double massLimit = massBudget.Value() + remap.balanceTolerance * double(count);
    if (!std::isfinite(result.massResidual) || !std::isfinite(massLimit) ||
        std::abs(result.massResidual) > massLimit)
        return Fail(error, "cut region global mass balance failed");
    result.beforeKineticEnergy = startWork.before.Value();
    result.afterImpulseKineticEnergy = startWork.after.Value();
    result.impulseWork = startWork.work.Value();
    if (!startWork.Close(result.impulseWorkResidual))
        return Fail(error, "cut region kinetic change disagrees with impulse work");

    ConservativeRemap plan;
    RemapMomentumResult mapped;
    if (!BuildConservativeRemap(remap, plan, error) || !plan.RemapMomentum(kicked, mapped, error))
        return false;

    result.afterTransportKineticEnergy = mapped.afterKineticEnergy;
    result.afterEndImpulseKineticEnergy = mapped.afterKineticEnergy;
    result.mixingLoss = mapped.mixingLoss;

    result.velocity.resize(count);
    std::array<CompensatedSum, 3> momentum;
    for (std::size_t region = 0; region < count; ++region) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const double mass = remap.end[region];
            double final = mapped.end[region][axis];
            double speed = mass > 0. ? final / mass : 0.;
            if (!std::isfinite(speed))
                return Fail(error, "cut region endpoint velocity is not representable");
            if (!endImpulse.empty()) {
                const double push = endImpulse[region][axis];
                if (mass == 0. && push != 0.)
                    return Fail(error, "cut region terminal impulse has no ending material owner");
                if (!endWork.Kick(mass, final, speed, push, final, speed))
                    return Fail(error, "cut region terminal impulse cannot be represented");
                momentum[axis].Add(-push);
            }
            result.velocity[region][axis] = speed;
            momentum[axis].Add(final - before[region][axis]);
            momentum[axis].Add(-impulse[region][axis]);
        }
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        result.momentumResidual[axis] = momentum[axis].Value();
        if (!std::isfinite(result.momentumResidual[axis]))
            return Fail(error, "cut region momentum ledger is not representable");
    }
    if (!endImpulse.empty()) {
        result.afterEndImpulseKineticEnergy = endWork.after.Value();
        result.endImpulseWork = endWork.work.Value();
        if (!endWork.Close(result.endImpulseWorkResidual))
            return Fail(error, "cut region kinetic change disagrees with terminal impulse work");
    }
    result.kineticResidual = (result.afterEndImpulseKineticEnergy - result.beforeKineticEnergy) -
        result.impulseWork - result.endImpulseWork + result.mixingLoss;
    if (!std::isfinite(result.kineticResidual))
        return Fail(error, "cut region kinetic ledger is not representable");
    output = std::move(result);
    error.clear();
    return true;
}

bool TransportCutRegionMomentum(const CutRegionPhaseInput& input,
                                 const CutRegionPhaseResult& phases,
                                 const std::vector<GridVec>& startVelocity,
                                 const std::vector<GridVec>& impulse,
                                 CutRegionMomentumResult& output, std::string& error) {
    return TransportCutRegionMomentum(input, phases, startVelocity, impulse, {}, output, error);
}

} // namespace judas::volume_grid
