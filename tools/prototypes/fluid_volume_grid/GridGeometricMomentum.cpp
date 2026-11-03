#include "GridGeometricMomentum.h"
#include "GridConservativeRemap.h"

namespace judas::volume_grid {
namespace {

using Index = std::array<int, 3>;

template<class Function>
bool VisitFaces(const GridLayout& layout, int axis, const Function& visit) {
    auto limits = layout.cells;
    ++limits[axis];
    for (int z = 0; z < limits[2]; ++z) {
        for (int y = 0; y < limits[1]; ++y) {
            for (int x = 0; x < limits[0]; ++x) {
                const Index i{x, y, z};
                if (!visit(layout.Face(axis, x, y, z), i))
                    return false;
            }
        }
    }
    return true;
}

template<class Function>
bool VisitInterior(const GridLayout& layout, int axis, const Function& visit) {
    return VisitFaces(layout, axis, [&](std::size_t face, const Index& i) {
        if (i[axis] == 0 || i[axis] == layout.cells[axis])
            return true;
        auto left = i;
        --left[axis];
        return visit(face, layout.Cell(left[0], left[1], left[2]), layout.Cell(i[0], i[1], i[2]));
    });
}

double Mass(const GridHalfCellStorage& storage, std::size_t cell, int half) {
    return storage.capacity[cell][half] + 999. * storage.water[cell][half];
}

bool CheckOwned(const HalfDualVolumeState& owned, std::size_t count) {
    for (const auto& values : owned.velocity) {
        if (values.size() != 2 * count ||
            std::any_of(values.begin(), values.end(), [](double v) { return !std::isfinite(v); }))
            return false;
    }
    return true;
}

double RemoveEmptyPhaseRoundoff(ConservativeRemapInput& route, double massRoundoff) {
    const auto count = route.start.size();
    std::vector<std::vector<std::size_t>> incident(count);
    for (std::size_t edge = 0; edge < route.transfers.size(); ++edge) {
        const auto& transfer = route.transfers[edge];
        // Only material absent at the start with at most a representational
        // endpoint amount can trigger this reconciliation.
        for (auto cell : {transfer.from, transfer.to}) {
            if (route.start[cell] == 0. && route.end[cell] <= massRoundoff)
                incident[cell].push_back(edge);
        }
    }
    std::vector<std::size_t> pending;
    for (std::size_t cell = 0; cell < count; ++cell)
        if (!incident[cell].empty() ||
            (route.start[cell] == 0. && route.end[cell] > 0. && route.end[cell] <= massRoundoff))
            pending.push_back(cell);
    double correction = 0.;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto cell = pending[i];
        double in = 0., out = 0.;
        for (auto edge : incident[cell]) {
            const auto& transfer = route.transfers[edge];
            const bool outgoing = (transfer.from == cell) == (transfer.amount >= 0.);
            (outgoing ? out : in) += std::abs(transfer.amount);
        }
        const double end = route.end[cell];
        const bool emptyRoundoff = end == 0. && in + out > 0. && in + out <= massRoundoff;
        const bool unsupportedRoundoff = in == 0. && out > 0. && end + out <= massRoundoff;
        if (!emptyRoundoff && !unsupportedRoundoff)
            continue;
        for (auto edge : incident[cell]) {
            auto& transfer = route.transfers[edge];
            if (transfer.amount == 0.)
                continue;
            correction += std::abs(transfer.amount);
            transfer.amount = 0.;
            const auto neighbor = transfer.from == cell ? transfer.to : transfer.from;
            if (!incident[neighbor].empty())
                pending.push_back(neighbor);
        }
    }
    return correction;
}

bool ComputeTransportEndpoint(ConservativeRemapInput& route, const std::vector<double>& geometric,
                              double& correction, std::string& error) {
    std::vector<std::array<double, 2>> traffic(route.start.size());
    for (const auto& edge : route.transfers) {
        const auto donor = edge.amount >= 0. ? edge.from : edge.to;
        const auto receiver = edge.amount >= 0. ? edge.to : edge.from;
        traffic[donor][1] += std::abs(edge.amount);
        traffic[receiver][0] += std::abs(edge.amount);
    }
    for (std::size_t half = 0; half < route.start.size(); ++half) {
        const auto [incoming, outgoing] = traffic[half];
        const double available = route.start[half] + incoming;
        const double remapped = available - outgoing;
        const double difference = std::abs(remapped - geometric[half]);
        if (!std::isfinite(difference) || difference > route.balanceTolerance) {
            error = "geometric transport cannot reconcile its half inventory with the phase flux";
            return false;
        }
        route.end[half] = std::max(0., remapped);
        correction += difference;
    }
    return true;
}

bool ValidatePrimalFlux(const GridLayout& layout, const MovingGridGeometry& geometry,
                        const MovingVolumeStep& projected, double rounding, std::string& error) {
    for (int axis = 0; axis < 3; ++axis) {
        const auto count = layout.FaceCount(axis);
        const auto& volume = projected.flux[axis];
        const auto& water = projected.waterFlux[axis];
        const auto& mass = projected.materialMassFlux[axis];
        const auto& aperture = geometry.averageAperture[axis];
        if (volume.size() != count || water.size() != count || mass.size() != count ||
            aperture.size() != count) {
            error = "geometric momentum requires the accepted shared primal fluxes";
            return false;
        }
        if (!VisitFaces(layout, axis, [&](std::size_t face, const Index& i) {
            const double q = volume[face];
            const double w = std::copysign(1., q) * water[face];
            if (!std::isfinite(q) || !std::isfinite(w) || !std::isfinite(mass[face]) ||
                !std::isfinite(aperture[face]) || aperture[face] < 0. || w < 0. || w > std::abs(q) ||
                std::abs(mass[face] - (q + 999. * water[face])) > 1000. * rounding ||
                (q == 0. && mass[face] != 0.)) {
                error = "geometric momentum received an invalid primal phase partition";
                return false;
            }
            if (q != 0. && (i[axis] == 0 || i[axis] == layout.cells[axis] || aperture[face] == 0.)) {
                error = "geometric momentum received flux across a sealed or obstructed face";
                return false;
            }
            return true;
        }))
            return false;
    }
    return true;
}

} // namespace

bool BuildGridGeometricMomentumStart(const GridLayout& layout, const MovingGridGeometry& geometry,
                                     const std::vector<BoxMotion>& stationary, int phaseAxis,
                                     const std::vector<double>& waterPlane,
                                     const MovingVolumeState& material,
                                     const HalfDualVolumeState& owned,
                                     GridGeometricMomentumStart& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || material.water.size() != count || geometry.startCapacity.size() != count ||
        !CheckOwned(owned, count)) {
        error = "geometric momentum ownership does not match its pressure grid";
        return false;
    }
    GridGeometricMomentumStart next;
    next.predictor.water = material.water;
    const double roundoff = 256 * std::numeric_limits<double>::epsilon() * layout.h * layout.h * layout.h;
    for (int axis = 0; axis < 3; ++axis) {
        const auto faces = layout.FaceCount(axis);
        if (geometry.startAperture[axis].size() != faces ||
            geometry.averageAperture[axis].size() != faces ||
            geometry.endAperture[axis].size() != faces) {
            error = "geometric momentum faces do not match their pressure grid";
            return false;
        }
        auto& storage = next.storage[axis];
        if (!BuildGridHalfCellStorage(layout, stationary, phaseAxis, axis, waterPlane, storage, error))
            return false;
        for (std::size_t cell = 0; cell < count; ++cell) {
            const auto& capacity = storage.capacity[cell];
            const auto& water = storage.water[cell];
            if (!std::isfinite(geometry.startCapacity[cell]) ||
                !(material.water[cell] >= 0.) || material.water[cell] > geometry.startCapacity[cell] ||
                std::abs(capacity[0] + capacity[1] - geometry.startCapacity[cell]) > roundoff ||
                std::abs(water[0] + water[1] - material.water[cell]) > roundoff) {
                error = "geometric half storage disagrees with its primal inventory";
                return false;
            }
        }
        auto& mass = next.faceMass[axis];
        auto& predictor = next.predictor.velocity[axis];
        auto& participation = next.faceParticipation[axis];
        mass.resize(faces);
        predictor.resize(faces);
        participation.resize(faces);
        const auto& u = owned.velocity[axis];
        if (!VisitInterior(layout, axis, [&](std::size_t face, std::size_t left, std::size_t right) {
            const double start = geometry.startAperture[axis][face];
            const double average = geometry.averageAperture[axis][face];
            const double end = geometry.endAperture[axis][face];
            if (!std::isfinite(start) || !std::isfinite(average) || !std::isfinite(end) ||
                start < 0. || average < 0. || end < 0.)
                return false;
            if (average == 0.)
                return true;
            const double leftMass = Mass(storage, left, 1), rightMass = Mass(storage, right, 0);
            const double combined = leftMass + rightMass;
            // An opening that exists for a fraction of the interval must not
            // recruit the entire old dual mass for that whole interval. For
            // a step-open face this is its exact exposure duration fraction.
            // With varying area it is a bounded area-time approximation.
            participation[face] = average / std::max({start, average, end});
            mass[face] = combined * participation[face];
            if (combined > 0)
                predictor[face] = (leftMass / combined) * u[2 * left + 1] +
                    (rightMass / combined) * u[2 * right];
            return std::isfinite(combined) && std::isfinite(predictor[face]);
        })) {
            error = "geometric pressure predictor is not representable";
            return false;
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool ApplyGridGeometricPressure(const GridLayout& layout, const MovingGridGeometry& geometry,
                                const GridGeometricMomentumStart& start,
                                const MovingVolumeStep& projected,
                                const HalfDualVolumeState& owned, double dt,
                                HalfDualPressureUpdate& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || !(dt > 0) || !std::isfinite(dt) || !CheckOwned(owned, count)) {
        error = "invalid geometric momentum pressure update";
        return false;
    }
    HalfDualPressureUpdate next;
    next.state = owned;
    double workScale = 0.;
    for (int axis = 0; axis < 3; ++axis) {
        const auto faces = layout.FaceCount(axis);
        const auto& gravity = projected.gravityVelocityIncrement[axis];
        const auto& inertial = projected.pressureInertialFace[axis];
        const auto& storage = start.storage[axis];
        if (geometry.averageAperture[axis].size() != faces || start.faceMass[axis].size() != faces ||
            start.faceParticipation[axis].size() != faces ||
            start.predictor.velocity[axis].size() != faces || projected.flux[axis].size() != faces ||
            gravity.size() != faces || inertial.size() != faces || storage.capacity.size() != count ||
            storage.water.size() != count) {
            error = "geometric pressure update lost its pressure face ownership";
            return false;
        }
        if (!VisitInterior(layout, axis, [&](std::size_t face, std::size_t left, std::size_t right) {
            if (!inertial[face])
                return start.faceMass[axis][face] == 0. ||
                    (projected.flux[axis][face] == 0. && gravity[face] == 0.);
            const double scale = dt * geometry.averageAperture[axis][face];
            const double mass = start.faceMass[axis][face];
            const double participating = start.faceParticipation[axis][face];
            if (!(scale > 0) || !(mass > 0) || !(participating > 0. && participating <= 1.))
                return false;
            const double predicted = start.predictor.velocity[axis][face] + gravity[face];
            const double delta = projected.flux[axis][face] / scale - predicted;
            const double impulse = mass * delta;
            const double work = impulse * (predicted + .5 * delta);
            workScale += std::abs(work);
            next.kineticChange += work;
            next.correctionEnergy += .5 * impulse * delta;
            next.gravityImpulse[axis] += mass * gravity[face];
            next.impulse[axis] += impulse;
            auto& u = next.state.velocity[axis];
            const double totalIncrement = gravity[face] + delta;
            const double ownedIncrement = participating * totalIncrement;
            // Merge the material that participated in the projection with
            // its unaccelerated remainder. Impulse is unchanged by the merge.
            next.participationMixingLoss += .5 * mass * (1. - participating) *
                totalIncrement * totalIncrement;
            if (Mass(storage, left, 1) > 0)
                u[2 * left + 1] += ownedIncrement;
            if (Mass(storage, right, 0) > 0)
                u[2 * right] += ownedIncrement;
            return std::isfinite(u[2 * left + 1]) && std::isfinite(u[2 * right]);
        })) {
            error = "geometric pressure impulse is not representable";
            return false;
        }
    }
    const double tolerance = 4096 * std::numeric_limits<double>::epsilon() *
        (1 + workScale + next.correctionEnergy);
    if (!std::isfinite(tolerance) || !std::isfinite(next.kineticChange) ||
        !std::isfinite(next.participationMixingLoss) ||
        !std::isfinite(projected.pressureKineticWork) ||
        !std::isfinite(projected.pressureCorrectionEnergy) ||
        std::abs(next.kineticChange - projected.pressureKineticWork) > tolerance ||
        std::abs(next.correctionEnergy - projected.pressureCorrectionEnergy) > tolerance) {
        error = "geometric momentum update disagrees with the pressure work metric";
        return false;
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool AdvectGridGeometricMomentum(const GridLayout& layout, const MovingGridGeometry& geometry,
                                   const std::vector<BoxMotion>& moving, int phaseAxis,
                                   const std::vector<double>& startPlane,
                                   const GridGeometricMomentumStart& start,
                                   const MovingVolumeStep& projected, const HalfDualVolumeState& pressed,
                                   GridGeometricTransport& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || !CheckOwned(pressed, count) || startPlane.size() != count ||
        start.predictor.water.size() != count ||
        geometry.startCapacity.size() != count || geometry.endCapacity.size() != count ||
        !std::isfinite(projected.residual) || projected.residual < 0.) {
        error = "invalid geometric momentum transport state";
        return false;
    }
    auto endpoints = moving;
    for (auto& box : endpoints) {
        for (int axis = 0; axis < 3; ++axis) {
            box.lo[axis] += box.translation[axis];
            box.hi[axis] += box.translation[axis];
        }
        box.translation = {};
    }
    GridCellPhase endPhase;
    GridCellPhaseReconciliation endpoint{geometry.endCapacity, {}};
    if (!ReconstructGridCellPhase(layout, endpoints, phaseAxis, projected.state.water, endPhase,
                                  error, &endpoint))
        return false;

    // A phase pair stores water and air volume in the same oriented face.
    using Phases = std::array<double, 2>;
    const double cellVolume = layout.h * layout.h * layout.h;
    const double rounding = 4096 * std::numeric_limits<double>::epsilon() * cellVolume;
    const double volumeTolerance = projected.residual + rounding;
    if (!ValidatePrimalFlux(layout, geometry, projected, rounding, error))
        return false;
    constexpr Phases density{1000., 1.};
    GridGeometricTransport next;
    for (int component = 0; component < 3; ++component) {
        const auto& before = start.storage[component];
        GridHalfCellStorage after;
        GridHalfCellApertures apertures;
        if (before.capacity.size() != count || before.water.size() != count ||
            !BuildGridHalfCellStorage(layout, endpoints, phaseAxis, component, endPhase.waterPlane,
                                      after, error) ||
            !AverageGridHalfCellApertures(layout, moving, phaseAxis, component, startPlane, apertures, error))
            return false;
        std::array<ConservativeRemapInput, 2> routes;
        for (int phase = 0; phase < 2; ++phase) {
            auto& route = routes[phase];
            route.start.resize(2 * count);
            route.end.resize(2 * count);
            route.relayAllowed.resize(2 * count);
            route.balanceTolerance = density[phase] * volumeTolerance;
        }
        const auto phaseVolume = [](const GridHalfCellStorage& storage, std::size_t cell,
                                    int side, int phase) {
            const double water = storage.water[cell][side];
            return phase == 0 ? water : storage.capacity[cell][side] - water;
        };
        std::vector<Phases> net(2 * count);
        for (std::size_t cell = 0; cell < count; ++cell) {
            const double startWater = start.predictor.water[cell];
            const double endWater = endpoint.water[cell];
            const bool interface = (startWater > 0. && startWater < geometry.startCapacity[cell]) ||
                (endWater > 0. && endWater < geometry.endCapacity[cell]);
            for (int side = 0; side < 2; ++side) {
                const auto index = 2 * cell + side;
                for (int phase = 0; phase < 2; ++phase) {
                    auto& route = routes[phase];
                    route.start[index] = density[phase] * phaseVolume(before, cell, side, phase);
                    route.end[index] = density[phase] * phaseVolume(after, cell, side, phase);
                    // Cut cells and moving phase boundaries can have a material
                    // pass through during the interval even when its initial
                    // inventory in this half is zero. The phase's own graph
                    // still has to prove the received carrier and its source.
                    route.relayAllowed[index] = before.capacity[cell][side] < .5 * cellVolume ||
                        after.capacity[cell][side] < .5 * cellVolume ||
                        (interface && apertures.midpoint[cell].open > 0.);
                }
            }
        }
        const auto exchange = [&](std::size_t left, std::size_t right, const Phases& q) {
            for (int phase = 0; phase < 2; ++phase) {
                net[left][phase] -= q[phase];
                net[right][phase] += q[phase];
                if (q[phase] != 0.)
                    routes[phase].transfers.push_back({left, right, density[phase] * q[phase]});
            }
        };
        for (int axis = 0; axis < 3; ++axis) {
            if (!VisitInterior(layout, axis, [&](std::size_t face, std::size_t left, std::size_t right) {
                const double q = projected.flux[axis][face];
                if (q == 0.)
                    return true;
                const double waterFlux = projected.waterFlux[axis][face];
                Phases flux{waterFlux, q - waterFlux};
                if (axis == component) {
                    exchange(2 * left + 1, 2 * right, flux);
                    return true;
                }
                const auto& halves = apertures.transverse[axis][face];
                const auto donor = q > 0. ? left : right;
                const double donorCapacity = geometry.startCapacity[donor];
                std::array<Phases, 2> support{};
                for (int phase = 0; phase < 2; ++phase) {
                    for (int side = 0; side < 2; ++side) {
                        const double trace = q > 0. ? halves[side].leftWet : halves[side].rightWet;
                        support[phase][side] = phase == 0 ? trace : halves[side].open - trace;
                    }
                    // The coarse projection already certified the phase transfer
                    // through a normal departure slab or from an exposed donor.
                    // The split axis is transverse here, so intersect its two
                    // geometric openings when the old trace was not yet filled.
                    if (support[phase][0] + support[phase][1] == 0. &&
                        (axis == phaseAxis || donorCapacity == 0.)) {
                        support[phase] = {halves[0].open, halves[1].open};
                    }
                }
                for (int phase = 0; phase < 2; ++phase) {
                    if (flux[phase] == 0. || support[phase][0] + support[phase][1] > 0.)
                        continue;
                    const auto other = 1 - phase;
                    const double partitionRoundoff = 8 * std::numeric_limits<double>::epsilon() * std::abs(q);
                    if (std::abs(flux[phase]) > partitionRoundoff ||
                        !(support[other][0] + support[other][1] > 0.)) {
                        error = "primal phase transport has no transverse half-face support";
                        return false;
                    }
                    // Complement subtraction can leave an ulp-sized trace
                    // where both geometric halves are exclusively one phase.
                    // Keep the shared volume and account for the phase residual.
                    next.phaseFluxRoundoff += std::abs(flux[phase]);
                    flux[other] = q;
                    flux[phase] = 0.;
                }
                Phases lower{};
                for (int phase = 0; phase < 2; ++phase) {
                    if (flux[phase] == 0.)
                        continue;
                    const auto [lo, hi] = support[phase];
                    if (!(lo + hi > 0.)) {
                        error = "primal phase transport has no transverse half-face support";
                        return false;
                    }
                    lower[phase] = lo == 0. ? 0. : hi == 0. ? flux[phase] : flux[phase] * (lo / (lo + hi));
                }
                exchange(2 * left, 2 * right, lower);
                exchange(2 * left + 1, 2 * right + 1,
                         {flux[0] - lower[0], flux[1] - lower[1]});
                return true;
            })) {
                if (error.empty())
                    error = "geometric momentum received mass without matching volume";
                return false;
            }
        }
        GridHalfCellApertures endApertures;
        for (std::size_t cell = 0; cell < count; ++cell) {
            Phases mid{};
            for (int phase = 0; phase < 2; ++phase) {
                const double lowChange = phaseVolume(before, cell, 0, phase) -
                    phaseVolume(after, cell, 0, phase);
                const double highChange = phaseVolume(after, cell, 1, phase) -
                    phaseVolume(before, cell, 1, phase);
                const double low = lowChange + net[2 * cell][phase];
                const double high = highChange - net[2 * cell + 1][phase];
                if (std::abs(low - high) > volumeTolerance) {
                    error = "geometric phase transfer disagrees with half-cell endpoint storage";
                    return false;
                }
                if (apertures.midpoint[cell].open == 0.) {
                    if (std::abs(low) > volumeTolerance || std::abs(high) > volumeTolerance) {
                        error = "geometric phase transfer requires flow across a solid midpoint";
                        return false;
                    }
                } else {
                    // The two balances are equivalent. Use the less
                    // cancellation-prone side. An exactly empty half with
                    // no external phase transfer then determines exact zero,
                    // rather than a fictitious unsupported roundoff stream.
                    const double lowScale = std::abs(lowChange) + std::abs(net[2 * cell][phase]);
                    const double highScale = std::abs(highChange) + std::abs(net[2 * cell + 1][phase]);
                    mid[phase] = lowScale <= highScale ? low : high;
                }
                if (mid[phase] != 0. && apertures.midpoint[cell].open < layout.h * layout.h) {
                    const auto support = [&](const GridHalfCellAperture& a) {
                        const double wet = mid[phase] > 0. ? a.leftWet : a.rightWet;
                        return phase == 0 ? wet : a.open - wet;
                    };
                    // A verified stream entering from the side can cross a
                    // vertical partition in the direction of phase separation
                    // before either endpoint plane reaches that partition.
                    // Tangential flow cannot use this to jump a dry obstacle.
                    const bool separates = component == phaseAxis &&
                        (phase == 0 ? mid[phase] < 0. : mid[phase] > 0.);
                    const auto donor = 2 * cell + (mid[phase] < 0. ? 1 : 0);
                    const bool incomingStream = separates && net[donor][phase] > 0. &&
                        std::abs(mid[phase]) <= net[donor][phase] + rounding;
                    const int side = int(donor % 2);
                    // A half that was entirely solid has no old phase trace.
                    // Its swept opening can relay the phase that has actually
                    // arrived there, up to the exposed endpoint volume.
                    const bool exposureStream = before.capacity[cell][side] == 0. &&
                        after.capacity[cell][side] > 0. && net[donor][phase] > 0. &&
                        std::abs(mid[phase]) <= std::min(net[donor][phase], after.capacity[cell][side]) +
                            rounding;
                    const double swept = before.capacity[cell][side] - after.capacity[cell][side];
                    const double displacedPhase = phaseVolume(before, cell, side, phase) -
                        phaseVolume(after, cell, side, phase);
                    // A closing piston can force a real phase out through a
                    // momentarily open midpoint, even against its buoyant
                    // direction. Bound the transfer by both the solid's swept
                    // capacity and the measured phase expelled from that side.
                    const bool squeezedStream = component == phaseAxis && swept > 0. &&
                        displacedPhase > 0. && std::abs(mid[phase]) <=
                            std::min(swept, displacedPhase) + rounding;
                    if (!(support(apertures.midpoint[cell]) > 0.) &&
                        !incomingStream && !exposureStream && !squeezedStream) {
                        if (endApertures.midpoint.empty() &&
                            !AverageGridHalfCellApertures(layout, endpoints, phaseAxis, component,
                                                         endPhase.waterPlane, endApertures, error))
                            return false;
                        if (!(support(endApertures.midpoint[cell]) > 0.)) {
                            if (std::abs(mid[phase]) <= rounding) {
                                next.phaseFluxRoundoff += std::abs(mid[phase]);
                                mid[phase] = 0.;
                            } else {
                                error = "geometric phase transfer has no wet or dry midpoint support";
                                return false;
                            }
                        }
                    }
                }
            }
            exchange(2 * cell, 2 * cell + 1, mid);
        }
        std::vector<double> afterMomentum(2 * count), afterMass(2 * count), waterSpeed(2 * count);
        double energyBefore = 0., remapLoss = 0., roundoffEnergy = 0., phaseMixingLoss = 0.;
        for (int phase = 0; phase < 2; ++phase) {
            auto& route = routes[phase];
            next.phaseFluxRoundoff += RemoveEmptyPhaseRoundoff(route, density[phase] * rounding) /
                density[phase];
            const auto geometricEnd = route.end;
            double endpointCorrection = 0.;
            if (!ComputeTransportEndpoint(route, geometricEnd, endpointCorrection, error))
                return false;
            next.phaseFluxRoundoff += endpointCorrection / density[phase];
            ConservativeRemap plan;
            std::vector<GridVec> beforeMomentum(2 * count);
            for (std::size_t i = 0; i < beforeMomentum.size(); ++i)
                beforeMomentum[i][component] = route.start[i] * pressed.velocity[component][i];
            RemapMomentumResult transport;
            if (!BuildConservativeRemap(route, plan, error) ||
                !plan.RemapMomentum(beforeMomentum, transport, error)) {
                error = "geometric half-cell " + std::string(phase == 0 ? "water: " : "air: ") + error;
                return false;
            }
            next.momentumResidual[component] += transport.conservationResidual[component];
            energyBefore += transport.beforeKineticEnergy;
            remapLoss += transport.mixingLoss;
            for (std::size_t half = 0; half < afterMass.size(); ++half) {
                const double moved = transport.end[half][component];
                const double speed = route.end[half] > 0. ? moved / route.end[half] : 0.;
                const double represented = speed * geometricEnd[half];
                const double defect = represented - moved;
                roundoffEnergy += .5 * defect * speed;
                next.phaseMomentumRoundoff[component] += defect;
                next.absolutePhaseMomentumRoundoff += std::abs(defect);
                if (phase == 0) {
                    waterSpeed[half] = speed;
                } else if (geometricEnd[half] > 0. && afterMass[half] > 0.) {
                    const double relative = speed - waterSpeed[half];
                    phaseMixingLoss += .5 * (afterMass[half] / (afterMass[half] + geometricEnd[half])) *
                        geometricEnd[half] * relative * relative;
                }
                afterMomentum[half] += represented;
                afterMass[half] += geometricEnd[half];
            }
        }
        auto& u = next.merged.state.velocity[component];
        u.resize(2 * count);
        double energyAfter = 0.;
        for (std::size_t half = 0; half < 2 * count; ++half) {
            if (afterMass[half] > 0.)
                u[half] = afterMomentum[half] / afterMass[half];
            energyAfter += .5 * afterMomentum[half] * u[half];
        }
        const double energyError = energyAfter - energyBefore + remapLoss + phaseMixingLoss - roundoffEnergy;
        const double energyTolerance = 32768 * std::numeric_limits<double>::epsilon() *
            (1. + energyAfter + energyBefore + remapLoss + phaseMixingLoss + std::abs(roundoffEnergy));
        if (!std::isfinite(energyError) || !std::isfinite(energyTolerance) ||
            std::abs(energyError) > energyTolerance) {
            error = "geometric phase momentum failed its kinetic energy ledger";
            return false;
        }
        next.kineticChange += energyAfter - energyBefore;
        next.phaseRoundoffEnergy += roundoffEnergy;
        next.phaseMixingLoss += phaseMixingLoss;
        auto& faceVelocity = next.merged.faceVelocity[component];
        faceVelocity.resize(layout.FaceCount(component));
        if (geometry.endAperture[component].size() != faceVelocity.size()) {
            error = "geometric momentum lacks the endpoint apertures";
            return false;
        }
        if (!VisitInterior(layout, component, [&](std::size_t face, std::size_t left, std::size_t right) {
            if (geometry.endAperture[component][face] == 0.)
                return true;
            const auto lo = 2 * left + 1, hi = 2 * right;
            const double mLo = afterMass[lo], mHi = afterMass[hi];
            const double mass = mLo + mHi;
            if (!(mass > 0.)) {
                error = "open endpoint face has no geometric half-cell material";
                return false;
            }
            const double speed = (mLo / mass) * u[lo] + (mHi / mass) * u[hi];
            const double relative = u[lo] - u[hi];
            next.merged.mergeKineticLoss += .5 * (mLo / mass) * mHi * relative * relative;
            faceVelocity[face] = u[lo] = u[hi] = speed;
            return std::isfinite(speed);
        }))
            return false;
        for (int z = 0; z < layout.cells[2]; ++z) {
            for (int y = 0; y < layout.cells[1]; ++y) {
                for (int x = 0; x < layout.cells[0]; ++x) {
                    const Index i{x, y, z};
                    const auto cell = layout.Cell(x, y, z);
                    for (int side = 0; side < 2; ++side) {
                        if (i[component] != side * (layout.cells[component] - 1))
                            continue;
                        const auto half = 2 * cell + side;
                        const double impulse = afterMass[half] * u[half];
                        next.merged.stationaryWallImpulse[component] -= impulse;
                        next.merged.stationaryWallKineticLoss += .5 * impulse * u[half];
                        u[half] = 0.;
                    }
                }
            }
        }
    }
    next.water = std::move(endpoint.water);
    next.waterVolumeRoundoff = endpoint.volumeRoundoff;
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
