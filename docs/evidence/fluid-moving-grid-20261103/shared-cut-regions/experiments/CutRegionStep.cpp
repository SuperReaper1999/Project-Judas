#include "CutRegionStep.h"

#include "CutRegionMomentum.h"
#include "GridHydrostaticReaction.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <queue>

namespace judas::volume_grid {
namespace {

constexpr double densityDifference = 999.;

double WetLength(double low, double high, double plane) {
    return std::clamp(plane - low, 0., high - low);
}

double MaxAbs(const std::vector<double>& values) {
    double largest = 0.;
    for (double value : values)
        largest = std::max(largest, std::abs(value));
    return largest;
}

struct FaceInertia {
    double left, right, gravityImpulse;
};

FaceInertia MeasureInertia(const CutRegionChart& chart, const CutRegionFace& face,
                          const std::vector<double>& waterPlanes,
                          const CutRegionFacePhase& phase, const CutRegionSettings& settings,
                          double dt) {
    const auto& left = chart.regions[face.left];
    const auto& right = chart.regions[face.right];
    const int axis = face.axis;
    const double leftDepth = .5 * (left.hi[axis] - left.lo[axis]);
    const double rightDepth = .5 * (right.hi[axis] - right.lo[axis]);
    double leftWet = leftDepth * phase.wet[0].mean / face.meanArea;
    double rightWet = rightDepth * phase.wet[1].mean / face.meanArea;
    double gravityImpulse = 0.;
    if (axis == settings.upAxis) {
        const double plane = face.lo[axis];
        leftWet = WetLength(plane - leftDepth, plane, waterPlanes[face.left]);
        rightWet = WetLength(plane, plane + rightDepth, waterPlanes[face.right]);
        const double referenceWet = WetLength(plane - leftDepth, plane + rightDepth,
                                               settings.referenceHeight);
        gravityImpulse = dt * face.meanArea * settings.gravity * densityDifference *
            (leftWet + rightWet - referenceWet);
    }
    return {face.meanArea * (leftDepth + densityDifference * leftWet),
            face.meanArea * (rightDepth + densityDifference * rightWet), gravityImpulse};
}

CutRegionPhaseFace PhaseTransfer(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                                 const CutRegionFace& face, const CutRegionFacePhase& phase,
                                 const std::vector<double>& waterPlanes, int upAxis,
                                 double flux, bool relay) {
    const auto side = flux >= 0. ? 0 : 1;
    const auto donor = side == 0 ? face.left : face.right;
    const auto& region = chart.regions[donor];
    const double amount = std::abs(flux);
    const double wetArea = phase.wet[side].mean, dryArea = phase.dry[side].mean;
    const bool exposed = geometry.startCapacity[donor] == 0.;
    bool wet = wetArea > 0., dry = dryArea > 0.;
    double preference = amount * wetArea / face.meanArea;
    if (face.axis == upAxis && amount > 0. && !exposed) {
        // A finite plug can reach across a horizontal interface even when
        // the initial one-sided trace is pure. Clip the actual departure
        // prism in the donor region, including the start solid.
        auto lower = face.lo, upper = face.hi;
        const double depth = amount / face.meanArea;
        if (side == 0)
            lower[upAxis] = std::max(region.lo[upAxis], lower[upAxis] - depth);
        else
            upper[upAxis] = std::min(region.hi[upAxis], upper[upAxis] + depth);
        auto wetUpper = upper, dryLower = lower;
        wetUpper[upAxis] = std::min(wetUpper[upAxis], waterPlanes[donor]);
        dryLower[upAxis] = std::max(dryLower[upAxis], waterPlanes[donor]);
        const double wetVolume = CutRegionClippedVolume(region, geometry.motion, lower, wetUpper);
        const double dryVolume = CutRegionClippedVolume(region, geometry.motion, dryLower, upper);
        wet = wet || wetVolume > 0.;
        dry = dry || dryVolume > 0.;
        if (wetVolume + dryVolume > 0.)
            preference = amount * wetVolume / (wetVolume + dryVolume);
    }
    if (exposed && relay) {
        // The receiver starts as solid. A supported opened face can relay
        // only actual incoming phase, checked by the shared transport graph.
        wet = dry = true;
        preference = 0.;
    }
    const double lower = dry ? 0. : amount;
    const double upper = wet ? amount : 0.;
    return {face.left, face.right, flux, lower, upper,
            lower <= upper ? std::clamp(preference, lower, upper) : 0.};
}

bool FullyOpen(const CutRegionFace& face) {
    const int a = (face.axis + 1) % 3, b = (face.axis + 2) % 3;
    const double area = (face.hi[a] - face.lo[a]) * (face.hi[b] - face.lo[b]);
    return face.startArea == area && face.meanArea == area && face.endArea == area;
}

bool ClearForInterval(const CutRegion& region, const std::optional<BoxMotion>& moving) {
    if (!moving)
        return true;
    for (int axis = 0; axis < 3; ++axis) {
        const double lower = moving->lo[axis] + std::min(0., moving->translation[axis]);
        const double upper = moving->hi[axis] + std::max(0., moving->translation[axis]);
        if (upper <= region.lo[axis] || lower >= region.hi[axis])
            return true;
    }
    return false;
}

void AdmitRelayThroughflow(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                          const std::vector<std::size_t>& indices,
                          const std::vector<CutRegionFacePhase>& phase,
                          CutRegionPhaseInput& transport) {
    // Diagnostic stationary, high-turnover, backward-Euler mixing model.
    (void)phase;
    const auto count = chart.regions.size();
    const auto faceCount = indices.size();
    std::vector<bool> mixing(count);
    std::vector<double> outflow(count);
    for (std::size_t i = 0; i < count; ++i)
        mixing[i] = transport.relayAllowed[i] && transport.startCapacity[i] > 0. &&
            transport.startCapacity[i] == transport.endCapacity[i] &&
            ClearForInterval(chart.regions[i], geometry.motion);
    struct Link { std::size_t from, to; double amount; };
    std::vector<Link> edges;
    edges.reserve(faceCount);
    for (std::size_t i = 0; i < faceCount; ++i) {
        const auto& f = transport.faces[i];
        const bool forward = f.signedVolume >= 0.;
        const auto from = forward ? f.left : f.right;
        const auto to = forward ? f.right : f.left;
        const auto volume = std::abs(f.signedVolume);
        if (volume > 0. && !FullyOpen(geometry.faces[indices[i]])) {
            mixing[from] = false;
            mixing[to] = false;
        }
        outflow[from] += volume;
        edges.push_back({from,to,volume});
    }
    std::vector<double> waterLimit(faceCount), airLimit(faceCount);
    for (const bool water : {false, true}) {
        std::vector<double> fixed(count), fraction(count), next(count), residual(count);
        for (std::size_t i = 0; i < count; ++i)
            if (mixing[i])
                fixed[i] = water ? transport.startWater[i] :
                    transport.startCapacity[i] - transport.startWater[i];
        for (std::size_t i = 0; i < faceCount; ++i) {
            const auto [from,to,volume] = edges[i];
            if (mixing[to] && !mixing[from])
                fixed[to] += water ? transport.faces[i].maxWater :
                    volume - transport.faces[i].minWater;
        }
        for (int iteration = 0; iteration < 1024; ++iteration) {
            next = fixed;
            for (const auto& [from,to,volume] : edges)
                if (mixing[from] && mixing[to])
                    next[to] += volume * fraction[from];
            double maxDelta = 0.;
            for (std::size_t i = 0; i < count; ++i) {
                if (!mixing[i]) continue;
                next[i] = std::clamp(next[i] / (outflow[i] + transport.endCapacity[i]), 0., 1.);
                maxDelta = std::max(maxDelta, std::abs(next[i]-fraction[i]));
            }
            fraction.swap(next);
            if (maxDelta < 16.*std::numeric_limits<double>::epsilon()) break;
        }
        for (std::size_t i = 0; i < faceCount; ++i) {
            const auto [from,to,volume] = edges[i];
            (water ? waterLimit : airLimit)[i] = mixing[from] ? volume*fraction[from] : 0.;
        }
    }
    for (std::size_t i = 0; i < faceCount; ++i) {
        const auto [from,to,volume] = edges[i];
        auto& f = transport.faces[i];
        if (!mixing[from]) continue;
        f.maxWater = std::max(f.maxWater,waterLimit[i]);
        f.minWater = std::min(f.minWater,volume-airLimit[i]);
    }
}

} // namespace

bool AdvanceCutRegionStep(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                          const CutRegionState& state, double dt, const CutRegionSettings& settings,
                          CutRegionStep& output, std::string& error) {
    const auto count = chart.regions.size();
    if (count == 0 || geometry.startCapacity.size() != count || geometry.endCapacity.size() != count ||
        state.water.size() != count || state.velocity.size() != count || !(dt > 0.) || !std::isfinite(dt) ||
        settings.upAxis < 0 || settings.upAxis > 2 || !(settings.gravity <= 0.) ||
        !std::isfinite(settings.gravity) || !std::isfinite(settings.referenceHeight)) {
        error = "invalid cut-region step state or gravity";
        return false;
    }
    const double h = chart.layout.h, cellVolume = h * h * h;
    const double roundoff = 256. * std::numeric_limits<double>::epsilon() * cellVolume;
    std::vector<double> planes, source(count), sweep(count), patchSweep(count);
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, settings.upAxis, state.water, planes, error))
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!(geometry.endCapacity[i] >= 0.) || !std::isfinite(geometry.endCapacity[i]) ||
            std::any_of(state.velocity[i].begin(), state.velocity[i].end(),
                        [](double v) { return !std::isfinite(v); })) {
            error = "invalid cut-region ending capacity or velocity";
            return false;
        }
        source[i] = geometry.startCapacity[i] - geometry.endCapacity[i];
    }
    GridVec travel{};
    if (geometry.motion)
        travel = geometry.motion->translation;
    for (const auto& patch : geometry.patches) {
        if (patch.cell >= count || patch.axis < 0 || patch.axis > 2 ||
            !(patch.area >= 0.) || !std::isfinite(patch.area) ||
            (patch.sign != -1 && patch.sign != 1)) {
            error = "invalid cut-region reaction patch";
            return false;
        }
        if (geometry.motion && patch.owner == geometry.motion->owner)
            patchSweep[patch.cell] -= patch.sign * patch.area * travel[patch.axis];
    }
    std::vector<CutRegionPressureFace> connections;
    std::vector<std::size_t> connectionFaces;
    std::vector<FaceInertia> inertia;
    std::vector<CutRegionFacePhase> phase;
    connections.reserve(geometry.faces.size());
    connectionFaces.reserve(geometry.faces.size());
    inertia.reserve(geometry.faces.size());
    phase.reserve(geometry.faces.size());
    double largestRhs = MaxAbs(source);
    for (std::size_t i = 0; i < geometry.faces.size(); ++i) {
        const auto& face = geometry.faces[i];
        if (face.left >= count || face.right >= count || face.axis < 0 || face.axis > 2 ||
            !(face.meanArea >= 0.) || !std::isfinite(face.meanArea) ||
            !std::isfinite(face.sweptVolume) || !(face.startArea >= 0.) ||
            !std::isfinite(face.startArea) || !(face.length > 0.) || !std::isfinite(face.length)) {
            error = "invalid cut-region shared aperture";
            return false;
        }
        sweep[face.left] += face.sweptVolume;
        sweep[face.right] -= face.sweptVolume;
        if (face.meanArea == 0.)
            continue;
        phase.push_back(MeasureCutRegionFacePhase(face, geometry.motion, settings.upAxis,
                                                planes[face.left], planes[face.right]));
        const auto metric = MeasureInertia(chart, face, planes, phase.back(), settings, dt);
        const double mass = metric.left + metric.right;
        const double projectedArea = dt * face.meanArea;
        const double oldVelocity = (metric.left * state.velocity[face.left][face.axis] +
                                    metric.right * state.velocity[face.right][face.axis]) / mass;
        const double oldShare = std::min(1., face.startArea / face.meanArea);
        const double wallVelocity = travel[face.axis] / dt;
        const double predictor = oldShare * oldVelocity + (1. - oldShare) * wallVelocity +
            metric.gravityImpulse / mass;
        const double prediction = projectedArea * predictor;
        connections.push_back({face.left, face.right, projectedArea * projectedArea / mass,
                               prediction, face.length < h / 128.});
        largestRhs = std::max(largestRhs, std::abs(prediction));
        connectionFaces.push_back(i);
        inertia.push_back(metric);
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (std::abs(source[i] + sweep[i]) > roundoff || std::abs(source[i] + patchSweep[i]) > roundoff) {
            error = "cut-region geometry failed swept volume and patch closure";
            return false;
        }
    }
    const double solveTolerance = std::max(roundoff, largestRhs * 1e-12);
    CutRegionStep next;
    if (!ProjectCutRegionPressure(source, connections, cellVolume, solveTolerance, next.pressure, error))
        return false;
    CutRegionPhaseInput transport;
    transport.startCapacity = geometry.startCapacity;
    transport.endCapacity = geometry.endCapacity;
    transport.startWater = state.water;
    transport.balanceTolerance = solveTolerance;
    transport.relayAllowed.resize(count);
    std::vector<double> outgoing(count);
    for (std::size_t i = 0; i < connections.size(); ++i) {
        const auto& face = connections[i];
        const double flux = next.pressure.volumeFlux[i];
        outgoing[flux >= 0. ? face.left : face.right] += std::abs(flux);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const double capacity = geometry.startCapacity[i];
        // A pressure residual in whole-cell units can exceed a sliver's
        // capacity. It cannot authorize spending that region's initial stock.
        const double allowance = 256. * std::numeric_limits<double>::epsilon() * capacity;
        transport.relayAllowed[i] = capacity == 0. || outgoing[i] - capacity > allowance;
    }
    transport.faces.reserve(connections.size());
    for (std::size_t i = 0; i < connections.size(); ++i) {
        const auto& face = geometry.faces[connectionFaces[i]];
        const double flux = next.pressure.volumeFlux[i];
        const auto donor = flux >= 0. ? face.left : face.right;
        transport.faces.push_back(PhaseTransfer(chart, geometry, face, phase[i], planes,
                                                settings.upAxis, flux, transport.relayAllowed[donor]));
    }
    AdmitRelayThroughflow(chart, geometry, connectionFaces, phase, transport);
    if (!TransportCutRegionPhases(transport, next.phase, error)) {
        struct Bounds { double lowIn=0., highIn=0., lowOut=0., highOut=0., volumeIn=0., volumeOut=0.; };
        std::vector<Bounds> bounds(count);
        for (const auto& f : transport.faces) {
            auto& from = bounds[f.signedVolume >= 0. ? f.left : f.right];
            auto& to = bounds[f.signedVolume >= 0. ? f.right : f.left];
            from.lowOut += f.minWater;
            from.highOut += f.maxWater;
            from.volumeOut += std::abs(f.signedVolume);
            to.lowIn += f.minWater;
            to.highIn += f.maxWater;
            to.volumeIn += std::abs(f.signedVolume);
        }
        struct Deficit { double amount; std::size_t region; const char* test; };
        std::vector<Deficit> deficits;
        for (std::size_t i=0; i<count; ++i) {
            const auto b = bounds[i];
            const auto water = state.water[i], air = geometry.startCapacity[i]-water;
            const auto allowed = transport.relayAllowed[i];
            const double minEnding = water + b.lowIn - b.highOut;
            const double maxEnding = water + b.highIn - b.lowOut;
            const auto note = [&](double amount, const char* test) {
                if (amount > transport.balanceTolerance) deficits.push_back({amount,i,test});
            };
            note(minEnding-geometry.endCapacity[i], "water overflow");
            note(-maxEnding, "water underflow");
            if (!allowed) {
                note(b.lowOut-water, "starting water donor");
                note(b.volumeOut-b.highOut-air, "starting air donor");
            }
        }
        std::sort(deficits.begin(), deficits.end(), [](const auto& a,const auto& b){return a.amount>b.amount;});
        std::cerr << std::setprecision(17) << "PHASE_DIAGNOSTIC residual=" << next.pressure.maximumBalanceError
                  << " tolerance=" << transport.balanceTolerance << " local_deficits=" << deficits.size() << '\n';
        for (std::size_t j=0; j<std::min<std::size_t>(5,deficits.size()); ++j) {
            const auto& d = deficits[j];
            const auto i=d.region;
            const auto b=bounds[i];
            const auto& r=chart.regions[i];
            std::cerr << "DEFICIT region=" << i << " parent=" << r.parent << " lo=(" << r.lo[0] << "," << r.lo[1] << "," << r.lo[2]
                      << ") hi=(" << r.hi[0] << "," << r.hi[1] << "," << r.hi[2]
                      << ") test=" << d.test << " delta_m3=" << d.amount << " initial_capacity=" << geometry.startCapacity[i]
                      << " end_capacity=" << geometry.endCapacity[i] << " initial_water=" << state.water[i]
                      << " water_in=" << b.lowIn << ".." << b.highIn << " water_out=" << b.lowOut << ".." << b.highOut
                      << " mixture_in=" << b.volumeIn << " mixture_out=" << b.volumeOut << " relay=" << transport.relayAllowed[i] << '\n';
        }
        std::vector<std::size_t> target{2566};
        for (std::size_t j=0; j<std::min<std::size_t>(5,deficits.size()); ++j)
            target.push_back(deficits[j].region);
        for (const auto probe : target)
        for (std::size_t j=0; j<transport.faces.size(); ++j) {
            const auto& f = transport.faces[j];
            if (f.left != probe && f.right != probe)
                continue;
            const auto& opening = geometry.faces[connectionFaces[j]];
            const bool outgoing = (f.signedVolume >= 0. && f.left == probe) ||
                                  (f.signedVolume < 0. && f.right == probe);
            const auto donor = f.signedVolume >= 0. ? f.left : f.right;
            const auto dest = f.signedVolume >= 0. ? f.right : f.left;
            std::cerr << "PORT region=" << probe << " " << (outgoing ? "out" : "in")
                      << " axis=" << opening.axis << " from=" << donor << " to=" << dest
                      << " lo=(" << opening.lo[0] << "," << opening.lo[1] << "," << opening.lo[2]
                      << ") hi=(" << opening.hi[0] << "," << opening.hi[1] << "," << opening.hi[2]
                      << ") meanArea=" << opening.meanArea << " flux=" << f.signedVolume
                      << " water=" << f.minWater << ".." << f.maxWater
                      << " donorCapacity=" << geometry.startCapacity[donor] << " donorWater=" << state.water[donor]
                      << " destCapacity=" << geometry.startCapacity[dest] << " destWater=" << state.water[dest]
                      << '\n';
        }
        return false;
        }

    std::vector<GridVec> impulses(count), terminalImpulses(count);
    for (std::size_t i = 0; i < connections.size(); ++i) {
        const auto& face = geometry.faces[connectionFaces[i]];
        const auto metric = inertia[i];
        const double impulse = dt * face.meanArea * next.pressure.pressureJump[i] + metric.gravityImpulse;
        // Momentum belongs to material present at the start. A newly
        // exposed region acquires momentum through the very same transfers.
        const double left = geometry.startCapacity[face.left] > 0. ? metric.left : 0.;
        const double right = geometry.startCapacity[face.right] > 0. ? metric.right : 0.;
        if (left + right == 0. && impulse != 0.) {
            const double finalLeft = geometry.endCapacity[face.left] +
                densityDifference * next.phase.endWater[face.left];
            const double finalRight = geometry.endCapacity[face.right] +
                densityDifference * next.phase.endWater[face.right];
            if (!(finalLeft + finalRight > 0.)) {
                error = "cut-region pressure impulse has no material owner at either endpoint";
                return false;
            }
            terminalImpulses[face.left][face.axis] += impulse * finalLeft / (finalLeft + finalRight);
            terminalImpulses[face.right][face.axis] += impulse * finalRight / (finalLeft + finalRight);
        }
        if (left > 0.)
            impulses[face.left][face.axis] += impulse * left / (left + right);
        if (right > 0.)
            impulses[face.right][face.axis] += impulse * right / (left + right);
    }
    CutRegionMomentumResult momentum;
    if (!TransportCutRegionMomentum(transport, next.phase, state.velocity, impulses,
                                    terminalImpulses, momentum, error))
        return false;
    next.state.water = next.phase.endWater;
    next.state.velocity = std::move(momentum.velocity);
    next.activeFaces = std::move(connectionFaces);
    next.transportMomentumResidual = momentum.momentumResidual;
    next.transportKineticResidual = momentum.kineticResidual;
    try {
        next.loads = AccumulateGridPressureReaction(geometry.patches, next.pressure.pressure);
        double wallWork = 0.;
        for (const auto& load : next.loads)
            if (geometry.motion && load.owner == geometry.motion->owner)
                wallWork += std::inner_product(load.force.begin(), load.force.end(), travel.begin(), 0.);
        next.pressureWorkResidual = wallWork + next.pressure.pressureWork;
        const double workTolerance = 1e-10 * (1. + std::abs(wallWork) + std::abs(next.pressure.pressureWork));
        if (!std::isfinite(next.pressureWorkResidual) || std::abs(next.pressureWorkResidual) > workTolerance) {
            double capacityWork = 0., maximumPressure = 0., maximumSpeed = 0.;
            for (std::size_t i = 0; i < count; ++i) {
                capacityWork += source[i] * next.pressure.pressure[i];
                maximumPressure = std::max(maximumPressure, std::abs(next.pressure.pressure[i]));
                maximumSpeed = std::max({maximumSpeed, std::abs(momentum.velocity.empty() ? next.state.velocity[i][0] : momentum.velocity[i][0]),
                    std::abs(next.state.velocity[i][1]), std::abs(next.state.velocity[i][2])});
            }
            std::cerr << std::setprecision(17) << "WORK wall=" << wallWork
                << " faces=" << next.pressure.pressureWork << " capacity=" << capacityWork
                << " residual=" << next.pressureWorkResidual << " tolerance=" << workTolerance
                << " pressure=" << maximumPressure << " speed=" << maximumSpeed << '\n';
            error = "cut-region pressure and wall reaction failed the work ledger";
            return false;
        }
        if (geometry.motion) {
            const auto hydro = MeasurePlanarHydrostaticMotion(*geometry.motion, settings.upAxis,
                settings.referenceHeight, settings.gravity, 1000., 1.);
            for (auto& load : next.loads) {
                if (load.owner != hydro.owner)
                    continue;
                for (int axis = 0; axis < 3; ++axis) {
                    load.force[axis] += hydro.force[axis];
                    load.torque[axis] += hydro.torque[axis];
                }
            }
        }
    } catch (const std::invalid_argument& invalid) {
        error = invalid.what();
        return false;
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
