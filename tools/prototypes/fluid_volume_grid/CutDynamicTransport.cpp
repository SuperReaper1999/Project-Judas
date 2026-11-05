#include "CutDynamicTransport.h"

#include "CutHydroacousticGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace judas::volume_grid {
namespace {

struct Port {
    std::size_t face, left, right;
    double conductance, predictor;
};

struct VentPort {
    std::size_t cell, index;
    double area, conductance, predictor;
};

struct Sum {
    double value = 0., correction = 0.;
    void Add(double term) {
        const double next = value + term;
        correction += std::abs(value) >= std::abs(term)
            ? (value - next) + term : (term - next) + value;
        value = next;
    }
    double Value() const { return value + correction; }
};

bool Fail(std::string& error, const std::string& message) {
    error = message;
    return false;
}

// Clip a donor's swept departure prism. Both phases use this same volume;
// an initially pure face can therefore carry an interface during the step.
bool SplitFlux(const CutRegionChart& chart, const CutRegionGeometry& geometry,
               const std::vector<CutRegionWaterPlane>& planes, const Port& port,
               double flux, double& water, double& crossingRatio, std::string& error) {
    if (flux == 0.) {
        water = 0.;
        return true;
    }
    const auto& face = geometry.faces[port.face];
    const bool fromLeft = flux > 0.;
    const auto donor = fromLeft ? port.left : port.right;
    const auto& region = chart.regions[donor];
    // Work in a face-local frame. Adding a 1e-19 m departure to a
    // metre-scale world coordinate would otherwise erase its thickness.
    const auto origin = face.lo;
    auto localRegion = region;
    auto motion = geometry.motion;
    auto plane = planes[donor];
    auto lo = face.lo, hi = face.hi;
    for (int axis = 0; axis < 3; ++axis) {
        lo[axis] -= origin[axis];
        hi[axis] -= origin[axis];
        localRegion.lo[axis] -= origin[axis];
        localRegion.hi[axis] -= origin[axis];
        if (motion) {
            motion->lo[axis] -= origin[axis];
            motion->hi[axis] -= origin[axis];
        }
    }
    plane.coordinate -= origin[plane.orientation.axis];
    const double depth = std::abs(flux) / face.meanArea;
    if (fromLeft)
        lo[face.axis] -= depth;
    else
        hi[face.axis] += depth;
    if (lo[face.axis] < localRegion.lo[face.axis] || hi[face.axis] > localRegion.hi[face.axis]) {
        // A pressure iterate is not an accepted material transfer. Extend
        // its residual using the donor trace; only geometrically owned
        // departures may pass the final publication gate below.
        const auto trace = MeasureCutRegionFacePhase(face, geometry.motion,
            planes[port.left], planes[port.right]);
        water = flux * trace.wet[fromLeft ? 0 : 1].mean / face.meanArea;
        const double width = fromLeft ? -localRegion.lo[face.axis] : localRegion.hi[face.axis];
        crossingRatio = std::max(crossingRatio, depth / width);
        return true;
    }
    const auto volumes = MeasureCutRegionPhaseVolumes(localRegion, motion, lo, hi, plane);
    const double sum = volumes[0] + volumes[1];
    if (!(sum > 0. && std::isfinite(sum))) {
        std::ostringstream detail;
        detail.precision(17);
        detail << "dynamic departure has no geometric owner: face=" << port.face
               << " donor=" << donor << " q=" << flux << " depth=" << depth
               << " lo=" << lo[face.axis] << " hi=" << hi[face.axis]
               << " aperture=" << face.meanArea;
        return Fail(error, detail.str());
    }
    if (std::abs(sum - std::abs(flux)) >
        128. * std::numeric_limits<double>::epsilon() * std::abs(flux))
        crossingRatio = std::max(crossingRatio, std::abs(flux) / sum);
    water = flux * (volumes[0] / sum);
    return true;
}

} // namespace

bool AdvanceCutDynamic(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                       const CutDynamicState& initial, const CutDynamicSettings& settings,
                       CutDynamicStep& output, std::string& error,
                       const std::vector<CutRegionGasVent>& vents) {
    const auto n = chart.regions.size(), nf = geometry.faces.size();
    if (n == 0 || initial.water.size() != n || initial.gasMass.size() != n ||
        chart.faces.size() != nf ||
        geometry.startCapacity.size() != n || geometry.endCapacity.size() != n ||
        (!initial.pressure.empty() && initial.pressure.size() != n) ||
        (!initial.faceSpeed.empty() && initial.faceSpeed.size() != nf) ||
        vents.size() > 16 || (vents.size() && n > (1u << 22) / vents.size()) ||
        (!initial.ventSpeed.empty() && initial.ventSpeed.size() != n * vents.size()) ||
        !(settings.dt > 0. && std::isfinite(settings.dt) &&
          settings.waterDensity > 0. && std::isfinite(settings.waterDensity) &&
          settings.gasDensity > 0. && std::isfinite(settings.gasDensity) &&
          settings.soundSpeed > 0. && std::isfinite(settings.soundSpeed) &&
          settings.volumeTolerance > 0. && std::isfinite(settings.volumeTolerance) &&
          settings.relativeVolumeTolerance > 0. && settings.relativeVolumeTolerance < 1. &&
          std::isfinite(settings.relativeVolumeTolerance)) ||
        settings.iterationLimit < 1)
        return Fail(error, "invalid dynamic pressure state or settings");
    for (double g : settings.gravity)
        if (!std::isfinite(g))
            return Fail(error, "nonfinite dynamic gravity");
    if (!ValidateCutRegionGasVents(chart, vents, error))
        return false;
    // Swept phase ownership requires a separate remap; never silently reuse
    // a static donor formula for a moving solid.
    if (geometry.motion && geometry.motion->translation != GridVec{})
        return Fail(error, "dynamic moving-solid remap is not implemented");
    const double c2 = settings.soundSpeed * settings.soundSpeed;
    if (!std::isfinite(c2))
        return Fail(error, "dynamic sound-speed square is not representable");
    std::vector<double> pressure = initial.pressure;
    pressure.resize(n);
    std::vector<double> initialGasVolume(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double capacity = geometry.startCapacity[i], water = initial.water[i];
        const double roundoff = 128. * std::numeric_limits<double>::epsilon() * capacity;
        if (!(capacity >= 0. && std::isfinite(capacity) &&
              capacity == geometry.endCapacity[i] && water >= 0. && water <= capacity + roundoff &&
              initial.gasMass[i] >= 0. && std::isfinite(initial.gasMass[i]) &&
              std::isfinite(pressure[i])))
            return Fail(error, "invalid dynamic phase inventory");
        const double density = settings.gasDensity + pressure[i] / c2;
        if (!(density > 0. && std::isfinite(density)))
            return Fail(error, "invalid initial dynamic gas pressure");
        // Retain a small gas owner independently of subtracting two large
        // volumes. In a merged cut its volume can be below one water ULP.
        initialGasVolume[i] = initial.gasMass[i] / density;
        if (!(std::abs((capacity - water) - initialGasVolume[i]) <= roundoff))
            return Fail(error, "dynamic initial EOS and capacity disagree");
    }
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionPhasePlanes(chart, geometry, 1, initial.water, initialGasVolume,
                                         initial.interfaces, planes, error))
        return false;
    std::vector<Port> ports;
    std::vector<VentPort> boundary;
    std::vector<CutHydroacousticGraphLink> links;
    for (std::size_t i = 0; i < nf; ++i) {
        const auto& face = geometry.faces[i];
        if (face.left >= n || face.right >= n || face.left == face.right ||
            face.axis < 0 || face.axis > 2 ||
            !(face.meanArea >= 0. && std::isfinite(face.meanArea)))
            return Fail(error, "invalid dynamic face ownership");
        if (face.meanArea == 0.)
            continue;
        const auto a = MeasureCutRegionFaceInertiaVolumes(face, geometry.motion,
            chart.regions[face.left], true, planes[face.left]);
        const auto b = MeasureCutRegionFaceInertiaVolumes(face, geometry.motion,
            chart.regions[face.right], false, planes[face.right]);
        const double mass = settings.waterDensity * (a[0] + b[0]) + settings.gasDensity * (a[1] + b[1]);
        const double velocity = (initial.faceSpeed.empty() ? 0. : initial.faceSpeed[i]) +
                                settings.dt * settings.gravity[face.axis];
        const double impulseArea = settings.dt * face.meanArea;
        if (!(mass > 0. && std::isfinite(mass) && std::isfinite(velocity)))
            return Fail(error, "dynamic face has invalid inertia or velocity");
        const double g = impulseArea * impulseArea / mass;
        if (!(g > 0. && std::isfinite(g)))
            return Fail(error, "dynamic face conductance is not representable");
        ports.push_back({i, face.left, face.right, g, impulseArea * velocity});
        links.push_back({face.left, face.right, g});
    }
    for (std::size_t j = 0; j < vents.size(); ++j) {
        const auto& vent = vents[j];
        for (std::size_t i = 0; i < n; ++i) {
            const auto& region = chart.regions[i];
            const double area = MeasureCutRegionGasVent(region, geometry.motion, vent, planes[i]).mean;
            if (area == 0.)
                continue;
            const auto index = i * vents.size() + j;
            const double velocity = (initial.ventSpeed.empty() ? 0. : initial.ventSpeed[index]) +
                settings.dt * settings.gravity[vent.axis] * vent.sign;
            const double length = .5 * (region.hi[vent.axis] - region.lo[vent.axis]);
            const double g = settings.dt * settings.dt * area / (settings.gasDensity * length);
            if (!(area > 0. && g > 0. && std::isfinite(g) && std::isfinite(velocity)))
                return Fail(error, "invalid dynamic gas vent inertia");
            boundary.push_back({i, index, area, g, settings.dt * area * velocity});
        }
    }
    CutDynamicStep next;
    next.totalFlux.resize(nf);
    next.waterFlux.resize(nf);
    next.gasMassFlux.resize(nf);
    next.state.faceSpeed.resize(nf);
    next.state.ventSpeed.resize(n * vents.size());
    next.state.interfaces = initial.interfaces;
    next.state.interfaces.resize(n);
    std::vector<double> density(n), residual(n), storage(n), rhs(n);
    std::vector<std::array<double, 2>> departures(n);
    std::vector<std::array<double, 2>> arrivals(n);
    std::vector<double> crossings(n);
    std::vector<Sum> waterOut(n), massOut(n);
    for (int iteration = 0; iteration < settings.iterationLimit; ++iteration) {
        for (std::size_t i = 0; i < n; ++i) {
            density[i] = settings.gasDensity + pressure[i] / c2;
            if (!(density[i] > 0. && std::isfinite(density[i])))
                return Fail(error, "dynamic gas pressure leaves EOS domain");
        }
        next.state.water = initial.water;
        next.state.gasMass = initial.gasMass;
        std::fill(departures.begin(), departures.end(), std::array<double, 2>{});
        std::fill(arrivals.begin(), arrivals.end(), std::array<double, 2>{});
        std::fill(crossings.begin(), crossings.end(), 0.);
        std::fill(waterOut.begin(), waterOut.end(), Sum{});
        std::fill(massOut.begin(), massOut.end(), Sum{});
        next.externalGasMass = 0.;
        for (const auto& port : ports) {
            const double q = port.predictor + port.conductance * (pressure[port.left] - pressure[port.right]);
            double w = 0.;
            const auto donor = q >= 0. ? port.left : port.right;
            const auto receiver = q >= 0. ? port.right : port.left;
            if (!SplitFlux(chart, geometry, planes, port, q, w, crossings[donor], error))
                return false;
            const double gasVolume = q - w, gasMass = density[donor] * gasVolume;
            next.totalFlux[port.face] = q;
            next.waterFlux[port.face] = w;
            next.gasMassFlux[port.face] = gasMass;
            next.state.faceSpeed[port.face] = q / (settings.dt * geometry.faces[port.face].meanArea);
            waterOut[port.left].Add(w);
            waterOut[port.right].Add(-w);
            massOut[port.left].Add(gasMass);
            massOut[port.right].Add(-gasMass);
            departures[donor][0] += std::abs(w);
            departures[donor][1] += std::abs(gasMass);
            arrivals[receiver][0] += std::abs(w);
            arrivals[receiver][1] += std::abs(gasMass);
        }
        for (const auto& vent : boundary) {
            const double q = vent.predictor + vent.conductance * pressure[vent.cell];
            const double mass = q * (q >= 0. ? density[vent.cell] : settings.gasDensity);
            massOut[vent.cell].Add(mass);
            next.externalGasMass += mass;
            next.state.ventSpeed[vent.index] = q / (settings.dt * vent.area);
            if (mass >= 0.)
                departures[vent.cell][1] += mass;
            else
                arrivals[vent.cell][1] -= mass;
        }
        double largest = 0.;
        for (std::size_t i = 0; i < n; ++i) {
            next.state.water[i] = initial.water[i] - waterOut[i].Value();
            next.state.gasMass[i] = initial.gasMass[i] - massOut[i].Value();
            const double gas = initialGasVolume[i] +
                (geometry.endCapacity[i] - geometry.startCapacity[i]) + waterOut[i].Value();
            residual[i] = gas - next.state.gasMass[i] / density[i];
            largest = std::max(largest, std::abs(residual[i]));
            storage[i] = std::max(0., next.state.gasMass[i]) / (density[i] * density[i] * c2);
            rhs[i] = -residual[i];
        }
        for (const auto& vent : boundary)
            storage[vent.cell] += vent.conductance;
        next.maximumVolumeResidual = largest;
        next.nonlinearIterations = iteration + 1;
        if (largest <= settings.volumeTolerance) {
            for (std::size_t i = 0; i < n; ++i) {
                const double localTolerance = settings.relativeVolumeTolerance * geometry.endCapacity[i];
                if (!(std::abs(residual[i]) <= localTolerance)) {
                    std::ostringstream detail;
                    detail.precision(17);
                    detail << "global pressure tolerance masks a cut-local EOS residual: region=" << i
                           << " capacity=" << geometry.endCapacity[i] << " residual=" << residual[i]
                           << " required=" << localTolerance;
                    return Fail(error, detail.str());
                }
                // A connected static cut initially full of one phase can
                // carry that same phase through. No interface is advanced
                // using this permission, and no foreign-phase arrival is
                // ignored (even if it is tiny). Ordinary mixed donors keep
                // the finite departure-prism requirement.
                const bool pureWater = geometry.startCapacity[i] > 0. &&
                    initial.gasMass[i] == 0. && arrivals[i][1] == 0.;
                const bool pureGas = geometry.startCapacity[i] > 0. &&
                    initial.water[i] == 0. && arrivals[i][0] == 0.;
                if (crossings[i] > 0. && !(pureWater || pureGas)) {
                    std::ostringstream detail;
                    detail.precision(17);
                    detail << "dynamic mixed departure has no owned finite prism: region=" << i
                           << " ratio=" << crossings[i] << " nonlinear=" << iteration + 1
                           << " linear=" << next.linearIterations << " residual=" << largest;
                    return Fail(error, detail.str());
                }
                // Same geometric arithmetic envelope as water-plane
                // reconstruction, not a material clamp. Keep the actual
                // quantities and report the independently checked residual.
                const double roundoff = 128. * std::numeric_limits<double>::epsilon() *
                    (geometry.endCapacity[i] + departures[i][0]);
                if (!(next.state.water[i] >= 0. && next.state.water[i] <= geometry.endCapacity[i] + roundoff &&
                      next.state.gasMass[i] >= 0. &&
                      departures[i][0] <= initial.water[i] + (pureWater ? arrivals[i][0] : 0.) + roundoff &&
                      departures[i][1] <= (initial.gasMass[i] + (pureGas ? arrivals[i][1] : 0.)) *
                          (1. + 128. * std::numeric_limits<double>::epsilon()))) {
                    std::ostringstream detail;
                    detail.precision(17);
                    detail << "dynamic pressure spends an unsupported phase donor: region=" << i
                           << " capacity=" << geometry.endCapacity[i]
                           << " initialWater=" << initial.water[i] << " endWater=" << next.state.water[i]
                           << " initialGasMass=" << initial.gasMass[i] << " endGasMass=" << next.state.gasMass[i]
                           << " waterDepartures=" << departures[i][0] << " gasDepartures=" << departures[i][1]
                           << " nonlinear=" << iteration + 1 << " residual=" << largest;
                    return Fail(error, detail.str());
                }
                next.pureThroughFlowOwners += crossings[i] > 0.;
            }
            next.state.pressure = std::move(pressure);
            std::vector<std::array<GridVec, 2>> incoming(n);
            for (const auto& port : ports) {
                const auto& face = geometry.faces[port.face];
                const std::array volumes{next.waterFlux[port.face],
                    next.totalFlux[port.face] - next.waterFlux[port.face]};
                for (int phase = 0; phase < 2; ++phase)
                    if (volumes[phase] != 0.)
                        incoming[volumes[phase] > 0. ? port.right : port.left][phase][face.axis] += volumes[phase];
            }
            for (std::size_t i = 0; i < n; ++i) {
                const bool wasDry = initial.water[i] == 0.;
                const bool wasWet = initial.gasMass[i] == 0.;
                if ((!wasDry && !wasWet) || next.state.water[i] == 0. || next.state.gasMass[i] == 0.)
                    continue;
                const auto& arrival = incoming[i][wasDry ? 0 : 1];
                int axis = 0;
                for (int j = 1; j < 3; ++j)
                    if (std::abs(arrival[j]) > std::abs(arrival[axis]))
                        axis = j;
                if (arrival[axis] != 0.)
                    next.state.interfaces[i] = {axis, wasDry ? arrival[axis] < 0. : arrival[axis] > 0.};
            }
            output = std::move(next);
            error.clear();
            return true;
        }
        CutHydroacousticGraphResult correction;
        if (!SolveCutHydroacousticGraph(storage, links, rhs,
                                        settings.volumeTolerance * .1, correction, error))
            return false;
        next.linearIterations += correction.iterations;
        for (std::size_t i = 0; i < n; ++i)
            pressure[i] += correction.pressure[i];
    }
    std::ostringstream detail;
    detail.precision(17);
    detail << "dynamic pressure/EOS iteration did not converge: iterations="
           << settings.iterationLimit << " residual=" << next.maximumVolumeResidual;
    return Fail(error, detail.str());
}

} // namespace judas::volume_grid
