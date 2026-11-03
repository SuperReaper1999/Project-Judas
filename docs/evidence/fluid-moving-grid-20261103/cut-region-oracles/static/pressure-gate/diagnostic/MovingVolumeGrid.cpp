#include <cstdio>
#include "PlanarMovingVolume.h"

#include "ExposureVolumePressure.h"
#include "GridCellPhase.h"
#include "GridConservativeRemap.h"
#include "GridGeometricMomentum.h"
#include "GridHydrostaticReaction.h"
#include "GridPhaseFluxRepair.h"
#include "MovingVolumePressureInternal.h"
#include "OwnedEvolvingVolume.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>

namespace judas::volume_grid {
namespace {

struct FaceFlux {
    std::size_t left, right, index;
    int axis;
    double aperture, mass, conductance, predictor, plane;
};

struct ExposureGroups {
    std::vector<std::size_t> parent, inlet;

    bool Build(const MovingGridGeometry& geometry, const std::vector<FaceFlux>& faces,
               std::string& error) {
        const auto count = geometry.startCapacity.size();
        parent.resize(count);
        std::iota(parent.begin(), parent.end(), 0);
        inlet.assign(count, faces.size());
        const auto admit = [&](std::size_t cell, std::size_t donor, std::size_t link) {
            if (geometry.startCapacity[cell] > 0 || geometry.startCapacity[donor] == 0)
                return;
            if (inlet[cell] == faces.size() || faces[inlet[cell]].aperture < faces[link].aperture) {
                inlet[cell] = link;
                parent[cell] = donor;
            }
        };
        for (std::size_t link = 0; link < faces.size(); ++link) {
            const auto& face = faces[link];
            admit(face.left, face.right, link);
            admit(face.right, face.left, link);
        }
        for (const auto& face : faces) {
            for (auto cell : {face.left, face.right}) {
                if (geometry.startCapacity[cell] == 0 && parent[cell] == cell) {
                    error = "exposed cell lacks a directly connected material owner";
                    return false;
                }
            }
        }
        return true;
    }

    bool Joined(const FaceFlux& face) const {
        return parent[face.left] == parent[face.right];
    }
};

bool RouteExposedPhase(const MovingGridGeometry& geometry, const std::vector<FaceFlux>& faces,
                       double tolerance, MovingVolumeStep& step, std::string& error) {
    const auto count = geometry.startCapacity.size();
    const auto exposed = [&](std::size_t cell) { return geometry.startCapacity[cell] == 0.; };
    ConservativeRemapInput route;
    route.start.resize(count);
    route.end.resize(count);
    route.relayAllowed.resize(count);
    route.balanceTolerance = tolerance;
    for (std::size_t cell = 0; cell < count; ++cell) {
        if (exposed(cell)) {
            route.end[cell] = geometry.endCapacity[cell];
            route.relayAllowed[cell] = true;
        }
    }
    std::vector<double> sourceWater(count);
    std::vector<std::size_t> edges;
    for (std::size_t index = 0; index < faces.size(); ++index) {
        const auto& face = faces[index];
        const double q = step.flux[face.axis][face.index];
        if (q == 0.)
            continue;
        auto donor = q > 0. ? face.left : face.right;
        auto receiver = q > 0. ? face.right : face.left;
        if (!exposed(donor) && !exposed(receiver))
            continue;
        const double amount = std::abs(q);
        if (!exposed(donor)) {
            donor = route.start.size();
            route.start.push_back(amount);
            route.end.push_back(0.);
            route.relayAllowed.push_back(false);
            sourceWater.push_back(std::abs(step.waterFlux[face.axis][face.index]));
        }
        if (!exposed(receiver)) {
            receiver = route.start.size();
            route.start.push_back(0.);
            route.end.push_back(amount);
            route.relayAllowed.push_back(false);
            sourceWater.push_back(0.);
        }
        route.transfers.push_back({donor, receiver, amount});
        edges.push_back(index);
    }
    if (edges.empty())
        return true;
    ConservativeRemap plan;
    RemapScalarResult routed;
    if (!BuildConservativeRemap(route, plan, error) || !plan.RemapScalar(sourceWater, routed, error)) {
        error = "newly exposed phase has no conservative donor route: " + error;
        return false;
    }
    for (std::size_t edge = 0; edge < edges.size(); ++edge) {
        const auto& face = faces[edges[edge]];
        const double q = step.flux[face.axis][face.index];
        const auto donor = q > 0. ? face.left : face.right;
        if (!exposed(donor))
            continue;
        const double water = routed.transfers[edge];
        const double volume = std::abs(q);
        if (!(water >= -tolerance && water <= volume + tolerance)) {
            error = "newly exposed phase exceeds the shared face volume";
            return false;
        }
        step.waterFlux[face.axis][face.index] = std::copysign(std::clamp(water, 0., volume), q);
    }
    return true;
}

double MaxAbs(const std::vector<double>& values) {
    double result = 0;
    for (const double value : values)
        result = std::max(result, std::abs(value));
    return result;
}

bool SolvePressure(const std::vector<FaceFlux>& faces, std::vector<double>& rhs,
                   double tolerance, double cellVolume, std::vector<double>& pressure,
                   int& iterations, std::string& error) {
    const std::size_t count = rhs.size();
    std::vector<std::size_t> parent(count);
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](std::size_t cell) {
        while (parent[cell] != cell) {
            parent[cell] = parent[parent[cell]];
            cell = parent[cell];
        }
        return cell;
    };

    std::vector<double> diagonal(count);
    for (const auto& f : faces) {
        parent[root(f.left)] = root(f.right);
        diagonal[f.left] += f.conductance;
        diagonal[f.right] += f.conductance;
    }
    // A pressure gauge cannot fix an incompatible sealed component. Only
    // remove the representable summation error, independently per component.
    std::vector<std::size_t> size(count);
    std::vector<double> sum(count), absoluteSum(count);
    for (std::size_t cell = 0; cell < count; ++cell) {
        const auto group = root(cell);
        ++size[group];
        sum[group] += rhs[cell];
        absoluteSum[group] += std::abs(rhs[cell]);
    }
    for (std::size_t group = 0; group < count; ++group) {
        const double roundoff = 128 * std::numeric_limits<double>::epsilon() *
            (absoluteSum[group] + double(size[group]) * cellVolume);
        if (std::abs(sum[group]) > roundoff) {
            error = "moving solid leaves an incompatible sealed pressure component";
            return false;
        }
    }
    for (std::size_t cell = 0; cell < count; ++cell) {
        const auto group = root(cell);
        rhs[cell] -= sum[group] / double(size[group]);
    }

    std::vector<double> residual = rhs;
    std::vector<double> direction(count), product(count);
    const auto precondition = [&](std::size_t cell) {
        return diagonal[cell] > 0 ? residual[cell] / diagonal[cell] : 0;
    };
    double energy = 0;
    for (std::size_t cell = 0; cell < count; ++cell) {
        direction[cell] = precondition(cell);
        energy += residual[cell] * direction[cell];
    }

    constexpr int workLimit = 2048;
    while (MaxAbs(residual) > tolerance) {
        if (iterations == workLimit) {
            error = "moving-volume pressure solve exceeded its work limit";
            return false;
        }
        std::fill(product.begin(), product.end(), 0);
        for (const auto& f : faces) {
            const double value = f.conductance * (direction[f.left] - direction[f.right]);
            product[f.left] += value;
            product[f.right] -= value;
        }
        const double curvature =
            std::inner_product(direction.begin(), direction.end(), product.begin(), 0.);
        if (!(curvature > 0) || !std::isfinite(curvature) || !std::isfinite(energy)) {
            error = "moving-volume pressure operator lost a finite positive direction";
            return false;
        }
        const double alpha = energy / curvature;
        double nextEnergy = 0;
        for (std::size_t cell = 0; cell < count; ++cell) {
            pressure[cell] += alpha * direction[cell];
            residual[cell] -= alpha * product[cell];
            nextEnergy += residual[cell] * precondition(cell);
        }
        const double beta = nextEnergy / energy;
        for (std::size_t cell = 0; cell < count; ++cell)
            direction[cell] = precondition(cell) + beta * direction[cell];
        energy = nextEnergy;
        ++iterations;
    }
    std::fill(sum.begin(), sum.end(), 0);
    for (std::size_t cell = 0; cell < count; ++cell)
        sum[root(cell)] += pressure[cell];
    for (std::size_t cell = 0; cell < count; ++cell) {
        const auto group = root(cell);
        pressure[cell] -= sum[group] / double(size[group]);
    }
    return true;
}

struct PlanarPressureReference {
    int axis;
    double height, gravity;
    const std::array<std::vector<double>, 3>& leftWet;
    const std::array<std::vector<double>, 3>& rightWet;
    const std::vector<double>* localHeight = nullptr;

    double DonorHeight(std::size_t cell) const {
        return localHeight ? (*localHeight)[cell] : height;
    }
};

enum class PressureMetric { FacePrism, Dual, OwnedDual };

bool AdvanceVolume(const GridLayout& layout, const MovingGridGeometry& geometry,
                   const MovingVolumeState& state, double dt, const GridVec& gravity,
                   const PlanarPressureReference* reference, PressureMetric metric,
                   MovingVolumeStep& output,
                   std::string& error, bool* transportRetry = nullptr,
                   double verifiedSweepError = 0., std::optional<bool> exposureWater = std::nullopt,
                   const GridGeometricMomentumStart* geometric = nullptr) {
    if (transportRetry)
        *transportRetry = false;
    const std::size_t count = layout.CheckedCellCount();
    if (!std::isfinite(dt) || dt <= 0 || count == 0 ||
        std::any_of(gravity.begin(), gravity.end(), [](double g) { return !std::isfinite(g); })) {
        error = "invalid moving-volume grid, timestep or gravity";
        return false;
    }
    const double area = layout.h * layout.h;
    const double cellVolume = area * layout.h;
    if (!(cellVolume > 0) || !std::isfinite(cellVolume) ||
        geometry.startCapacity.size() != count || geometry.endCapacity.size() != count ||
        state.water.size() != count) {
        error = "moving-volume cell state does not match the grid";
        return false;
    }
    const double roundoff = 256 * std::numeric_limits<double>::epsilon() * cellVolume;
    const bool groupExposure = exposureWater.has_value() || geometric;
    std::vector<double> fraction(count), rhs(count), balance(count), geometricBalance(count);
    for (std::size_t cell = 0; cell < count; ++cell) {
        const double before = geometry.startCapacity[cell];
        const double after = geometry.endCapacity[cell];
        const double water = state.water[cell];
        if (!std::isfinite(before) || !std::isfinite(after) || !std::isfinite(water) ||
            before < 0 || before > cellVolume + roundoff || after < 0 ||
            after > cellVolume + roundoff || water < 0 || water > before) {
            error = "moving-volume material exceeds the available cell capacity";
            return false;
        }
        if (exposureWater && water != (*exposureWater ? before : 0.)) {
            error = "exposure pressure currently requires a uniformly filled water or air chart";
            return false;
        }
        fraction[cell] = before > 0 ? water / before : 0;
        rhs[cell] = before - after;
    }

    MovingVolumeStep step;
    step.pressure.resize(count);
    step.state.water = state.water;
    std::vector<FaceFlux> faces;
    for (int axis = 0; axis < 3; ++axis) {
        const auto faceCount = layout.FaceCount(axis);
        if (geometry.startAperture[axis].size() != faceCount ||
            geometry.averageAperture[axis].size() != faceCount ||
            geometry.endAperture[axis].size() != faceCount ||
            geometry.solidSweep[axis].size() != faceCount ||
            state.velocity[axis].size() != faceCount) {
            error = "moving-volume face state does not match the grid";
            return false;
        }
        step.flux[axis].resize(faceCount);
        step.materialMassFlux[axis].resize(faceCount);
        if (reference)
            step.gravityVelocityIncrement[axis].resize(faceCount);
        if (geometric)
            step.pressureInertialFace[axis].resize(faceCount);
        if (groupExposure)
            step.waterFlux[axis].resize(faceCount);
        step.state.velocity[axis].resize(faceCount);
        auto faceLimits = layout.cells;
        ++faceLimits[axis];
        for (int k = 0; k < faceLimits[2]; ++k) {
            for (int j = 0; j < faceLimits[1]; ++j) {
                for (int i = 0; i < faceLimits[0]; ++i) {
                    const std::array<int, 3> rightCoord{i, j, k};
                    const auto index = layout.Face(axis, i, j, k);
                    const double startAperture = geometry.startAperture[axis][index];
                    const double aperture = geometry.averageAperture[axis][index];
                    const double endAperture = geometry.endAperture[axis][index];
                    const double swept = geometry.solidSweep[axis][index];
                    const double speed = state.velocity[axis][index];
                    if (!std::isfinite(startAperture) || !std::isfinite(aperture) || !std::isfinite(endAperture) ||
                        !std::isfinite(swept) || !std::isfinite(speed) ||
                        startAperture < 0 || startAperture > area ||
                        aperture < 0 || aperture > area || endAperture < 0 || endAperture > area) {
                        error = "invalid moving-volume face aperture, sweep or velocity";
                        return false;
                    }
                    if (rightCoord[axis] == 0 || rightCoord[axis] == layout.cells[axis]) {
                        if (swept != 0) {
                            error = "solid volume crosses the sealed grid boundary";
                            return false;
                        }
                        continue;
                    }
                    auto leftCoord = rightCoord;
                    --leftCoord[axis];
                    const auto left = layout.Cell(leftCoord[0], leftCoord[1], leftCoord[2]);
                    const auto right = layout.Cell(i, j, k);
                    geometricBalance[left] += swept;
                    geometricBalance[right] -= swept;
                    if (aperture == 0)
                        continue;
                    // The same face mass is used in the pressure coefficient and
                    // kinetic-work oracle. A uniform phase retains its density.
                    double waterFraction = exposureWater ? (*exposureWater ? 1. : 0.) :
                        .5 * (fraction[left] + fraction[right]);
                    double acceleration = gravity[axis];
                    if (reference) {
                        acceleration = 0.;
                        if (axis == reference->axis) {
                            const double center = rightCoord[axis] * layout.h;
                            const double lower = center - .5 * layout.h;
                            const double upper = center + .5 * layout.h;
                            const double referenceWet = LiquidLinkLength(lower, upper, reference->height);
                            const double actualWet = reference->localHeight
                                ? LiquidLinkLength(lower, center, reference->DonorHeight(left)) +
                                  LiquidLinkLength(center, upper, reference->DonorHeight(right))
                                : referenceWet;
                            waterFraction = std::clamp(actualWet / layout.h, 0., 1.);
                            // Apply gravity minus the shared reference
                            // pressure gradient. The same path densities
                            // enter both the flux mass and the body force.
                            acceleration = reference->gravity * (999 * (actualWet - referenceWet) /
                                layout.h) / (1 + 999 * waterFraction);
                        } else {
                            const double wet = .5 * (reference->leftWet[axis][index] +
                                                      reference->rightWet[axis][index]);
                            waterFraction = std::clamp(wet / aperture, 0., 1.);
                        }
                    }
                    const double density = 1 + 999 * waterFraction;
                    const bool useDualMass = metric != PressureMetric::FacePrism || geometric;
                    const double metricMass = geometric ? geometric->faceMass[axis][index] : useDualMass
                        ? .5 * (geometry.startCapacity[left] + geometry.startCapacity[right] +
                                999 * (state.water[left] + state.water[right]))
                        : density * aperture * layout.h;
                    if (geometric && metricMass == 0.) {
                        if (geometry.startCapacity[left] > 0. && geometry.startCapacity[right] > 0.) {
                            error = "opening face between existing cells has no initial half-cell inertia";
                            return false;
                        }
                        faces.push_back({left, right, index, axis, aperture, 0., 0., 0.,
                                         rightCoord[axis] * layout.h});
                        continue;
                    }
                    const double fluxScale = dt * aperture;
                    const double conductance = useDualMass
                        ? fluxScale * fluxScale / metricMass
                        : dt * dt * aperture / (density * layout.h);
                    if (reference && useDualMass) {
                        // The reference gravity residual is a force on the
                        // geometric face prism. Divide by the same owned
                        // half-cell mass used by pressure and transport.
                        acceleration *= density * aperture * layout.h / metricMass;
                    }
                    const double increment = dt * acceleration;
                    if (reference)
                        step.gravityVelocityIncrement[axis][index] = increment;
                    const double predictor = (startAperture > 0 || metric == PressureMetric::OwnedDual || geometric
                        ? speed : 0.) + increment;
                    if (!(conductance > 0) || !std::isfinite(conductance) ||
                        !std::isfinite(predictor)) {
                        error = "moving-volume face predictor is not representable";
                        return false;
                    }
                    const double predictedFlux = dt * aperture * predictor;
                    rhs[left] -= predictedFlux;
                    rhs[right] += predictedFlux;
                    faces.push_back({left, right, index, axis, aperture, metricMass,
                                     conductance, predictor, rightCoord[axis] * layout.h});
                }
            }
        }
    }
    for (std::size_t cell = 0; cell < count; ++cell) {
        if (std::abs(geometricBalance[cell] + geometry.startCapacity[cell] -
                     geometry.endCapacity[cell]) > roundoff + verifiedSweepError) {
            error = "moving solid sweep does not reproduce the change in capacity";
            return false;
        }
    }

    const double tolerance = std::max(roundoff / 8, MaxAbs(rhs) * 1e-12);
    ExposureGroups groups, pressureGroups;
    bool hasExposure = false;
    if (groupExposure) {
        if (!groups.Build(geometry, faces, error))
            return false;
        pressureGroups = groups;
        if (geometric) {
            for (std::size_t cell = 0; cell < count; ++cell) {
                const auto parent = groups.parent[cell];
                if (parent == cell)
                    continue;
                hasExposure = true;
                // An exposed cell can borrow carrier from an adjacent
                // material owner. That does not remove an existing normal
                // pressure inertia on their shared face.
                if (faces[groups.inlet[cell]].conductance > 0.)
                    pressureGroups.parent[cell] = cell;
            }
        }
        std::vector<double> groupedRhs(count);
        for (std::size_t cell = 0; cell < count; ++cell)
            groupedRhs[pressureGroups.parent[cell]] += rhs[cell];
        std::vector<FaceFlux> groupedFaces;
        groupedFaces.reserve(faces.size());
        for (auto face : faces) {
            if (pressureGroups.Joined(face) || face.conductance == 0.)
                continue;
            face.left = pressureGroups.parent[face.left];
            face.right = pressureGroups.parent[face.right];
            groupedFaces.push_back(face);
        }
        if (!SolvePressure(groupedFaces, groupedRhs, tolerance, cellVolume, step.pressure,
                           step.iterations, error))
            return false;
        for (std::size_t cell = 0; cell < count; ++cell)
            step.pressure[cell] = step.pressure[pressureGroups.parent[cell]];
    } else if (!SolvePressure(faces, rhs, tolerance, cellVolume, step.pressure,
                              step.iterations, error)) {
        return false;
    }

    std::vector<double> outgoing(count);
    for (const auto& f : faces) {
        if ((groupExposure && pressureGroups.Joined(f)) || f.conductance == 0.)
            continue;
        const double correction = f.conductance *
            (step.pressure[f.left] - step.pressure[f.right]);
        const double correctedSpeed = f.predictor + correction / (dt * f.aperture);
        const double flux = dt * f.aperture * correctedSpeed;
        if (!std::isfinite(flux) || !std::isfinite(correctedSpeed)) {
            error = "nonfinite moving-volume pressure output";
            return false;
        }
        step.flux[f.axis][f.index] = flux;
        if (geometric)
            step.pressureInertialFace[f.axis][f.index] = true;
        if (geometry.endAperture[f.axis][f.index] > 0)
            step.state.velocity[f.axis][f.index] = correctedSpeed;
        balance[f.left] += flux;
        balance[f.right] -= flux;
        auto donor = flux >= 0 ? f.left : f.right;
        if (geometric || !groupExposure || !groups.Joined(f))
            outgoing[geometric || !groupExposure ? donor : groups.parent[donor]] += std::abs(flux);
        const double change = correctedSpeed - f.predictor;
        step.pressureKineticWork += f.mass * (f.predictor + .5 * change) * change;
        step.pressureCorrectionEnergy += .5 * f.mass * change * change;
    }
    if (groupExposure) {
        for (std::size_t cell = 0; cell < count; ++cell) {
            if (pressureGroups.parent[cell] == cell)
                continue;
            const auto& f = faces[pressureGroups.inlet[cell]];
            const double wanted = geometry.startCapacity[cell] - geometry.endCapacity[cell];
            const double flux = (f.left == cell ? 1. : -1.) * (wanted - balance[cell]);
            const double speed = flux / (dt * f.aperture);
            if (!std::isfinite(speed)) {
                error = "exposed cell's internal flux is not representable";
                return false;
            }
            step.flux[f.axis][f.index] = flux;
            if (geometry.endAperture[f.axis][f.index] > 0)
                step.state.velocity[f.axis][f.index] = speed;
            balance[f.left] += flux;
            balance[f.right] -= flux;
            if (geometric)
                outgoing[flux >= 0. ? f.left : f.right] += std::abs(flux);
        }
    }
    const double balanceTolerance = 8 * tolerance + roundoff;
    bool needPhaseRepair = false;
    std::vector<bool> relayPhase(geometric ? count : 0);
    for (std::size_t cell = 0; cell < count; ++cell) {
        const double change = geometry.startCapacity[cell] - geometry.endCapacity[cell];
        step.residual = std::max(step.residual, std::abs(balance[cell] - change));
        step.pressureFluidWork += step.pressure[cell] * change;
        const double capacity = geometry.startCapacity[cell];
        const bool geometricRelay = geometric && (capacity == 0. ||
            (geometry.endCapacity[cell] < capacity && outgoing[cell] > capacity + balanceTolerance));
        if (geometric)
            relayPhase[cell] = geometricRelay;
        if (!geometricRelay && outgoing[cell] > capacity + balanceTolerance) {
            const auto x = cell % layout.cells[0];
            const auto y = (cell / layout.cells[0]) % layout.cells[1];
            const auto z = cell / layout.cells[0] / layout.cells[1];
            std::fprintf(stderr, "DONOR cell=(%zu,%zu,%zu) start=%.17g end=%.17g water=%.17g out=%.17g cfl=%.17g tolerance=%.17g pressure=%.17g\n",
                x, y, z, capacity, geometry.endCapacity[cell], state.water[cell], outgoing[cell], outgoing[cell]/capacity, balanceTolerance, step.pressure[cell]);
            for (const auto& f : faces) {
                if (f.left == cell || f.right == cell)
                    std::fprintf(stderr, "DONOR_FACE axis=%d face=%zu direction=%s aperture=%.17g mass=%.17g conductance=%.17g predictor=%.17g flux=%.17g\n",
                        f.axis, f.index, f.left == cell ? "positive-out" : "negative-out", f.aperture,
                        f.mass, f.conductance, f.predictor, step.flux[f.axis][f.index]);
            }
            if (transportRetry)
                *transportRetry = true;
            error = "moving-volume transport exceeds donor capacity; substep or merge required";
            return false;
        }
        needPhaseRepair = needPhaseRepair || (geometricRelay && capacity > 0.);
        if (capacity > 0)
            step.maxCfl = std::max(step.maxCfl, outgoing[cell] / capacity);
    }
    if (step.residual > balanceTolerance || !std::isfinite(step.pressureFluidWork) ||
        !std::isfinite(step.pressureKineticWork) || !std::isfinite(step.pressureCorrectionEnergy)) {
        error = "moving-volume pressure failed the published balance or work check";
        // A face that first opens late in the interval can have very small
        // integrated conductance. Keep the strict balance and let the coupled
        // clock retry the unresolved topology on a shorter interval.
        if (geometric && hasExposure && transportRetry)
            *transportRetry = true;
        return false;
    }

    // The flat reference knows which parts of the departure slab are wet.
    // Keep its dry and wet exclusions when balancing simultaneous outflows.
    // A limited mixed face may adjust composition, but it cannot supply a
    // phase absent from the donor geometry.
    struct PhaseFlux {
        double lower = 0, target = 0, upper = 0;
    };
    std::vector<PhaseFlux> phaseFlux;
    std::vector<double> minimumWater, targetWater, maximumWater;
    if (reference) {
        phaseFlux.resize(faces.size());
        minimumWater.resize(count);
        targetWater.resize(count);
        maximumWater.resize(count);
        for (std::size_t i = 0; i < faces.size(); ++i) {
            const auto& f = faces[i];
            const double flux = step.flux[f.axis][f.index];
            const double amount = std::abs(flux);
            if (amount == 0)
                continue;
            const auto donor = flux > 0 ? f.left : f.right;
            if (geometric && geometry.startCapacity[donor] == 0.) {
                phaseFlux[i].upper = amount;
                continue;
            }
            double wet = fraction[donor];
            if (wet > 0 && wet < 1) {
                if (f.axis == reference->axis) {
                    const double distance = amount / f.aperture;
                    if (distance > layout.h + balanceTolerance / f.aperture) {
                        if (transportRetry)
                            *transportRetry = true;
                        error = "planar departure slab crosses more than one cell; substep required";
                        return false;
                    }
                    const double endpoint = f.plane - std::copysign(distance, flux);
                    const double height = reference->DonorHeight(donor);
                    const double lower = std::min(f.plane, endpoint);
                    const double upper = std::max(f.plane, endpoint);
                    wet = height <= lower ? 0. : height >= upper ? 1. : (height - lower) / distance;
                } else {
                    const auto& wetSide = flux > 0 ? reference->leftWet : reference->rightWet;
                    wet = std::clamp(wetSide[f.axis][f.index] / f.aperture, 0., 1.);
                }
            }
            auto& phases = phaseFlux[i];
            phases.lower = wet == 1 ? amount : 0;
            phases.target = amount * wet;
            phases.upper = wet > 0 ? amount : 0;
            minimumWater[donor] += phases.lower;
            targetWater[donor] += phases.target;
            maximumWater[donor] += phases.upper;
        }
        for (std::size_t cell = 0; cell < count; ++cell) {
            if (geometric && relayPhase[cell])
                continue;
            const double air = geometry.startCapacity[cell] - state.water[cell];
            const double lower = std::max(minimumWater[cell], outgoing[cell] - air);
            const double upper = std::min(maximumWater[cell], state.water[cell]);
            if (lower > upper + balanceTolerance) {
                if (transportRetry)
                    *transportRetry = true;
                error = "planar departure slabs exhaust one donor phase; substep required";
                return false;
            }
            const double target = targetWater[cell];
            double desired = std::clamp(target, std::min(lower, upper), upper);
            if (geometric) {
                // Near roundoff the inventory and the geometric bounds can
                // disagree by a few ulps. Keep the admissible face interval.
                // In particular, a fixed pure-phase sum has no blend range.
                desired = std::clamp(desired, minimumWater[cell], maximumWater[cell]);
            }
            if (desired > target)
                targetWater[cell] = (desired - target) / (maximumWater[cell] - target);
            else if (desired < target)
                targetWater[cell] = (desired - target) / (target - minimumWater[cell]);
            else
                targetWater[cell] = 0;
        }
    }

    const auto waterForFace = [&](const FaceFlux& f, std::size_t i) {
        const double flux = step.flux[f.axis][f.index];
        const auto donor = flux >= 0 ? f.left : f.right;
        if (!reference)
            return flux * (exposureWater ? (*exposureWater ? 1. : 0.) : fraction[donor]);
        const auto& phases = phaseFlux[i];
        if (geometric && phases.lower == phases.upper)
            return std::copysign(phases.lower, flux);
        const double scale = targetWater[donor];
        const double available = scale > 0 ? phases.upper - phases.target : phases.target - phases.lower;
        const double volume = phases.target + scale * available;
        return std::copysign(geometric ? std::clamp(volume, phases.lower, phases.upper) : volume, flux);
    };
    if (geometric) {
        for (std::size_t i = 0; i < faces.size(); ++i) {
            const auto& f = faces[i];
            step.waterFlux[f.axis][f.index] = waterForFace(f, i);
        }
        if (hasExposure && !RouteExposedPhase(geometry, faces, balanceTolerance, step, error)) {
            needPhaseRepair = true;
        }
        std::vector<double> estimated = state.water;
        for (const auto& f : faces) {
            const double water = step.waterFlux[f.axis][f.index];
            estimated[f.left] -= water;
            estimated[f.right] += water;
        }
        for (std::size_t cell = 0; cell < count; ++cell)
            needPhaseRepair = needPhaseRepair || estimated[cell] < -balanceTolerance ||
                estimated[cell] > geometry.endCapacity[cell] + balanceTolerance;
        if (needPhaseRepair) {
            GridPhaseFluxRepairInput input;
            input.startCapacity = geometry.startCapacity;
            input.endCapacity = geometry.endCapacity;
            input.startWater = state.water;
            input.relayAllowed = relayPhase;
            input.balanceTolerance = balanceTolerance;
            std::vector<std::size_t> repairedFaces;
            for (std::size_t i = 0; i < faces.size(); ++i) {
                const auto& f = faces[i];
                const double q = step.flux[f.axis][f.index];
                if (q == 0.)
                    continue;
                const auto& phase = phaseFlux[i];
                const double preferred = std::clamp(std::abs(step.waterFlux[f.axis][f.index]),
                                                    phase.lower, phase.upper);
                input.edges.push_back({q > 0. ? f.left : f.right, q > 0. ? f.right : f.left,
                                       std::abs(q), phase.lower, phase.upper, preferred});
                repairedFaces.push_back(i);
            }
            GridPhaseFluxRepairResult repaired;
            if (!RepairGridPhaseFlux(input, repaired, error)) {
                if (transportRetry)
                    *transportRetry = true;
                return false;
            }
            for (std::size_t edge = 0; edge < repairedFaces.size(); ++edge) {
                const auto& f = faces[repairedFaces[edge]];
                const double q = step.flux[f.axis][f.index];
                step.waterFlux[f.axis][f.index] = std::copysign(repaired.waterFlux[edge], q);
            }
        }
    }
    std::vector<unsigned char> incomingPhase(geometric ? count : 0);
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto& f = faces[i];
        const double flux = step.flux[f.axis][f.index];
        const double water = geometric ? step.waterFlux[f.axis][f.index] : waterForFace(f, i);
        step.materialMassFlux[f.axis][f.index] = flux + 999 * water;
        if (groupExposure)
            step.waterFlux[f.axis][f.index] = water;
        if (geometric) {
            const auto receiver = flux >= 0. ? f.right : f.left;
            if (water != 0.)
                incomingPhase[receiver] |= 1;
            if (flux != water)
                incomingPhase[receiver] |= 2;
        }
        step.state.water[f.left] -= water;
        step.state.water[f.right] += water;
    }
    for (std::size_t cell = 0; cell < count; ++cell) {
        const double capacity = geometry.endCapacity[cell];
        const double amount = step.state.water[cell];
        if (!std::isfinite(amount) || amount < -balanceTolerance ||
            amount > capacity + balanceTolerance) {
            error = "moving-volume phase transport exceeds receiver capacity";
            if (geometric && transportRetry)
                *transportRetry = true;
            return false;
        }
        // Roundoff only. Any physical overfill was rejected above. Report the
        // total correction so it cannot be mistaken for conservative flux.
        double represented = std::clamp(amount, 0., capacity);
        if (geometric) {
            // A truly absent phase stays absent in the absence of incoming
            // phase flux. Pressure residual is accounted as roundoff, never
            // turned into an unsupported material stream on a later step.
            if (state.water[cell] == 0. && (incomingPhase[cell] & 1) == 0 &&
                represented <= balanceTolerance) {
                represented = 0.;
            } else if (state.water[cell] == geometry.startCapacity[cell] &&
                       (incomingPhase[cell] & 2) == 0 &&
                       capacity - represented <= balanceTolerance) {
                represented = capacity;
            }
        }
        step.waterRoundoffCorrection += std::abs(represented - amount);
        step.state.water[cell] = represented;
    }
    output = std::move(step);
    error.clear();
    return true;
}

} // namespace

bool AdvanceMovingVolume(const GridLayout& layout, const MovingGridGeometry& geometry,
                         const MovingVolumeState& state, double dt, const GridVec& gravity,
                         MovingVolumeStep& output, std::string& error) {
    return AdvanceVolume(layout, geometry, state, dt, gravity, nullptr, PressureMetric::FacePrism,
                         output, error);
}

bool AdvanceExposureVolumePressure(const GridLayout& layout, const MovingGridGeometry& geometry,
                                    const MovingVolumeState& state, double dt, bool water,
                                    MovingVolumeStep& output, std::string& error) {
    return AdvanceVolume(layout, geometry, state, dt, {}, nullptr, PressureMetric::FacePrism,
                         output, error, nullptr, 0., water);
}

namespace {

bool AdvancePhaseVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                        const MovingVolumeState& state, double dt, int axis, double height,
                        double signedGravity, bool requireFlat, PlanarMovingStep& output,
                        std::string& error, bool* transportRetry = nullptr,
                        const HalfDualVolumeState* owned = nullptr,
                        OwnedEvolvingVolumeStep* ownedResult = nullptr, bool geometric = false) {
    if (transportRetry)
        *transportRetry = false;
    if (axis < 0 || axis > 2 || !std::isfinite(height) || !std::isfinite(signedGravity) ||
        signedGravity > 0) {
        error = "invalid planar gravity or water level";
        return false;
    }
    std::vector<BoxMotion> initialBoxes = boxes;
    for (auto& box : initialBoxes)
        box.translation = {};

    MovingGridGeometry geometry;
    if (!BuildMovingGridGeometry(layout, boxes, geometry, error))
        return false;
    if (state.water.size() != geometry.startCapacity.size()) {
        error = "phase material state does not match the grid";
        return false;
    }
    double inputRoundoff = 0;
    const double tolerance = 1e-12 * layout.h * layout.h * layout.h;
    MovingVolumeState canonical = state;
    for (std::size_t i = 0; i < state.water.size(); ++i) {
        const double water = state.water[i];
        const double capacity = geometry.startCapacity[i];
        if (water < 0 || !std::isfinite(water) || water > capacity + tolerance) {
            error = "phase material exceeds the verified cut capacity";
            return false;
        }
        if (water > capacity) {
            canonical.water[i] = capacity;
            inputRoundoff += water - capacity;
        }
    }

    if (!requireFlat) {
        const double ceiling = layout.cells[axis] * layout.h;
        double capacity = 0;
        if (!FlatWaterVolume(layout, initialBoxes, axis, ceiling, capacity, error))
            return false;
        double water = 0, rounding = 0;
        for (double value : canonical.water) {
            const double next = water + (value - rounding);
            rounding = (next - water) - (value - rounding);
            water = next;
        }
        if (water > capacity && water - capacity > tolerance * canonical.water.size()) {
            error = "phase water total exceeds the verified tank capacity";
            return false;
        }
        if (!FlatWaterHeight(layout, initialBoxes, axis, std::min(water, capacity), height, error))
            return false;
    }

    GridPhaseGeometry phase;
    if (!BuildGridPhaseGeometry(layout, initialBoxes, axis, height, phase, error))
        return false;
    bool flat = true;
    for (std::size_t i = 0; i < state.water.size(); ++i) {
        const double target = std::min(phase.water[i], geometry.startCapacity[i]);
        flat = flat && std::abs(canonical.water[i] - target) <= tolerance;
    }
    if (requireFlat && !flat) {
        error = "material state does not match the flat hydrostatic interface";
        return false;
    }

    std::array<std::vector<double>, 3> wetLeft, wetRight;
    GridCellPhase localPhase;
    if (flat) {
        if (!AverageWetAperture(layout, boxes, axis, height, wetLeft, error))
            return false;
        for (std::size_t i = 0; i < state.water.size(); ++i) {
            const double target = std::min(phase.water[i], geometry.startCapacity[i]);
            inputRoundoff += std::abs(canonical.water[i] - target);
            canonical.water[i] = target;
        }
    } else if (!ReconstructGridCellPhase(layout, initialBoxes, axis, canonical.water,
                                         localPhase, error) ||
               !AverageCellPhaseApertures(layout, boxes, axis, localPhase.waterPlane,
                                          wetLeft, wetRight, error)) {
        return false;
    }

    PlanarMovingStep step;
    step.referenceHeight = height;
    const PlanarPressureReference reference{axis, height, signedGravity, wetLeft,
        flat ? wetLeft : wetRight, flat ? nullptr : &localPhase.waterPlane};
    GridGeometricMomentumStart geometricStart;
    if (geometric) {
        if ((flat && !ReconstructGridCellPhase(layout, initialBoxes, axis, canonical.water, localPhase, error)) ||
            !BuildGridGeometricMomentumStart(layout, geometry, initialBoxes, axis, localPhase.waterPlane,
                                             canonical, *owned, geometricStart, error))
            return false;
    } else if (owned && !BuildHalfDualVolumePredictor(layout, geometry, canonical.water, *owned,
                                                      canonical, error)) {
        return false;
    }
    const auto metric = owned ? PressureMetric::OwnedDual : PressureMetric::FacePrism;
    const auto& predictor = geometric ? geometricStart.predictor : canonical;
    if (!AdvanceVolume(layout, geometry, predictor, dt, {}, &reference, metric, step.fluid,
                       error, transportRetry, 0., std::nullopt, geometric ? &geometricStart : nullptr))
        return false;
    step.fluid.waterRoundoffCorrection += inputRoundoff;
    try {
        step.loads = AccumulateGridPressureReaction(geometry.averagePatches, step.fluid.pressure);
        for (const auto& box : boxes) {
            const auto hydro = MeasurePlanarHydrostaticMotion(box, axis, height, signedGravity,
                                                             1000., 1.);
            const auto load = std::find_if(step.loads.begin(), step.loads.end(),
                [&](const auto& value) { return value.owner == hydro.owner; });
            if (load == step.loads.end()) {
                error = "planar pressure reaction lost its solid owner";
                return false;
            }
            for (int a = 0; a < 3; ++a) {
                load->force[a] += hydro.force[a];
                load->torque[a] += hydro.torque[a];
                if (!std::isfinite(load->force[a]) || !std::isfinite(load->torque[a])) {
                    error = "nonfinite planar pressure reaction";
                    return false;
                }
            }
        }
    } catch (const std::invalid_argument& exception) {
        error = exception.what();
        return false;
    }
    if (geometric) {
        HalfDualPressureUpdate pressure;
        GridGeometricTransport transport;
        if (!ApplyGridGeometricPressure(layout, geometry, geometricStart, step.fluid, *owned,
                                        dt, pressure, error))
            return false;
        if (!AdvectGridGeometricMomentum(layout, geometry, boxes, axis, localPhase.waterPlane,
                                          geometricStart, step.fluid, pressure.state, transport, error)) {
            if (transportRetry)
                *transportRetry = true;
            return false;
        }
        step.fluid.state.velocity = std::move(transport.merged.faceVelocity);
        step.fluid.state.water = std::move(transport.water);
        step.fluid.waterRoundoffCorrection += transport.waterVolumeRoundoff;
        ownedResult->halfVelocity = std::move(transport.merged.state);
        ownedResult->residualGravityImpulse = pressure.gravityImpulse;
        ownedResult->pressureImpulse = pressure.impulse;
        ownedResult->stationaryWallImpulse = transport.merged.stationaryWallImpulse;
        ownedResult->stationaryWallKineticLoss = transport.merged.stationaryWallKineticLoss;
        ownedResult->mergeKineticLoss = transport.merged.mergeKineticLoss;
        ownedResult->advectiveKineticChange = transport.kineticChange;
        ownedResult->geometricPhaseFluxRoundoff = transport.phaseFluxRoundoff;
        ownedResult->geometricPressureMixingLoss = pressure.participationMixingLoss;
        ownedResult->transportMomentumError = transport.momentumResidual;
        ownedResult->geometricTransportRoundoffImpulse = transport.phaseMomentumRoundoff;
    } else if (owned) {
        HalfDualPressureUpdate pressure;
        HalfDualVolumeTransport advection;
        HalfDualFaceMerge merged;
        if (!ApplyHalfDualVolumePressure(layout, geometry, canonical, step.fluid, *owned,
                                         dt, pressure, error) ||
            !AdvectHalfDualVolumeMomentum(layout, geometry, canonical, step.fluid, pressure.state,
                                           advection, error, transportRetry) ||
            !MergeHalfDualVolumeFaces(layout, geometry, step.fluid.state.water, advection.state,
                                       merged, error))
            return false;
        step.fluid.state.velocity = std::move(merged.faceVelocity);
        ownedResult->halfVelocity = std::move(merged.state);
        ownedResult->residualGravityImpulse = pressure.gravityImpulse;
        ownedResult->pressureImpulse = pressure.impulse;
        ownedResult->stationaryWallImpulse = merged.stationaryWallImpulse;
        ownedResult->stationaryWallKineticLoss = merged.stationaryWallKineticLoss;
        ownedResult->mergeKineticLoss = merged.mergeKineticLoss;
        ownedResult->advectiveKineticChange = advection.afterKineticEnergy -
                                               advection.beforeKineticEnergy;
        for (int a = 0; a < 3; ++a) {
            ownedResult->transportMomentumError[a] = advection.afterMomentum[a] -
                advection.beforeMomentum[a] - advection.roundoffMomentum[a];
        }
    }
    output = std::move(step);
    error.clear();
    return true;
}

} // namespace

bool AdvancePlanarMovingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                               const MovingVolumeState& state, double dt, int axis, double height,
                               double signedGravity, PlanarMovingStep& output, std::string& error) {
    return AdvancePhaseVolume(layout, boxes, state, dt, axis, height, signedGravity, true,
                              output, error);
}

bool AdvanceEvolvingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                           const MovingVolumeState& state, double dt, int axis,
                           double signedGravity, PlanarMovingStep& output, std::string& error) {
    return AdvancePhaseVolume(layout, boxes, state, dt, axis, 0., signedGravity, false,
                              output, error);
}

bool AdvanceOwnedEvolvingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                const MovingVolumeState& state, const HalfDualVolumeState& owned,
                                double dt, int axis, double signedGravity,
                                OwnedEvolvingVolumeStep& output, std::string& error) {
    OwnedEvolvingVolumeStep next;
    if (!AdvancePhaseVolume(layout, boxes, state, dt, axis, 0., signedGravity, false,
                            next.phase, error, nullptr, &owned, &next))
        return false;
    output = std::move(next);
    return true;
}

bool AdvanceGeometricEvolvingVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                    const MovingVolumeState& state, const HalfDualVolumeState& owned,
                                    double dt, int axis, double signedGravity,
                                    OwnedEvolvingVolumeStep& output, std::string& error,
                                    bool* transportRetry) {
    OwnedEvolvingVolumeStep next;
    if (!AdvancePhaseVolume(layout, boxes, state, dt, axis, 0., signedGravity, false,
                            next.phase, error, transportRetry, &owned, &next, true))
        return false;
    output = std::move(next);
    return true;
}

namespace {

const PlanarMovingStep& PhaseOf(const PlanarMovingStep& step) { return step; }
const PlanarMovingStep& PhaseOf(const OwnedEvolvingVolumeStep& step) { return step.phase; }

template<class Step, class Interval, class Advance>
bool AdvanceInterval(const std::vector<BoxMotion>& boxes, const MovingVolumeState& state,
                     double dt, int subdivisionLimit, Advance advanceOne, Interval& output,
                     std::string& error) {
    if (subdivisionLimit < 0 || subdivisionLimit > 10) {
        error = "invalid moving-volume subdivision limit";
        return false;
    }
    Interval next;
    std::map<std::uint32_t, GridPressureLoad> means;
    MovingVolumeState current = state;
    const auto advance = [&](const auto& self, double begin, double end, int depth) -> bool {
        std::vector<BoxMotion> substep = boxes;
        for (auto& box : substep) {
            for (int a = 0; a < 3; ++a) {
                box.lo[a] = std::fma(begin, box.translation[a], box.lo[a]);
                box.hi[a] = std::fma(begin, box.translation[a], box.hi[a]);
                box.translation[a] *= end - begin;
            }
        }
        Step result;
        bool retry = false;
        if (!advanceOne(substep, current, dt * (end - begin), result, retry)) {
            ++next.rejectedAttempts;
            if (!retry || depth == subdivisionLimit)
                return false;
            const double middle = .5 * (begin + end);
            return self(self, begin, middle, depth + 1) && self(self, middle, end, depth + 1);
        }
        ++next.substeps;
        const auto& phase = PhaseOf(result);
        next.maximumResidual = std::max(next.maximumResidual, phase.fluid.residual);
        next.maximumCfl = std::max(next.maximumCfl, phase.fluid.maxCfl);
        for (const auto& load : phase.loads) {
            auto& mean = means[load.owner];
            mean.owner = load.owner;
            for (int a = 0; a < 3; ++a) {
                mean.force[a] += (end - begin) * load.force[a];
                mean.torque[a] += (end - begin) * load.torque[a];
            }
        }
        current = phase.fluid.state;
        next.final = std::move(result);
        return true;
    };
    if (!advance(advance, 0., 1., 0))
        return false;
    for (const auto& entry : means) {
        const auto& mean = entry.second;
        for (int a = 0; a < 3; ++a) {
            if (!std::isfinite(mean.force[a]) || !std::isfinite(mean.torque[a])) {
                error = "nonfinite mean pressure reaction over subdivided interval";
                return false;
            }
        }
        next.meanLoads.push_back(mean);
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace

bool AdvanceEvolvingVolumeInterval(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                   const MovingVolumeState& state, double dt, int axis,
                                   double signedGravity, int subdivisionLimit,
                                   EvolvingVolumeInterval& output, std::string& error) {
    const auto step = [&](const auto& motion, const auto& material, double h,
                          PlanarMovingStep& next, bool& retry) {
        return AdvancePhaseVolume(layout, motion, material, h, axis, 0., signedGravity,
                                  false, next, error, &retry);
    };
    return AdvanceInterval<PlanarMovingStep>(boxes, state, dt, subdivisionLimit, step, output,
                                              error);
}

namespace {

bool AdvanceOwnedInterval(const GridLayout& layout,
                                        const std::vector<BoxMotion>& boxes,
                                        const MovingVolumeState& state,
                                        const HalfDualVolumeState& owned,
                                        double dt, int axis, double signedGravity,
                                        int subdivisionLimit, OwnedEvolvingVolumeInterval& output,
                                        std::string& error, bool geometric) {
    auto currentOwned = owned;
    const auto step = [&](const auto& motion, const auto& material, double h,
                          OwnedEvolvingVolumeStep& next, bool& retry) {
        if (!AdvancePhaseVolume(layout, motion, material, h, axis, 0., signedGravity, false,
                                next.phase, error, &retry, &currentOwned, &next, geometric))
            return false;
        currentOwned = next.halfVelocity;
        return true;
    };
    return AdvanceInterval<OwnedEvolvingVolumeStep>(boxes, state, dt, subdivisionLimit, step,
                                                     output, error);
}

} // namespace

bool AdvanceOwnedEvolvingVolumeInterval(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                        const MovingVolumeState& state, const HalfDualVolumeState& owned,
                                        double dt, int axis, double signedGravity, int subdivisionLimit,
                                        OwnedEvolvingVolumeInterval& output, std::string& error) {
    return AdvanceOwnedInterval(layout, boxes, state, owned, dt, axis, signedGravity, subdivisionLimit,
                                output, error, false);
}

bool AdvanceGeometricEvolvingVolumeInterval(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                                            const MovingVolumeState& state, const HalfDualVolumeState& owned,
                                            double dt, int axis, double signedGravity, int subdivisionLimit,
                                            OwnedEvolvingVolumeInterval& output, std::string& error) {
    return AdvanceOwnedInterval(layout, boxes, state, owned, dt, axis, signedGravity, subdivisionLimit,
                                output, error, true);
}

bool detail::AdvanceVolumeWithDualMass(const GridLayout& layout, const MovingGridGeometry& geometry,
                                        const MovingVolumeState& state, double dt,
                                        MovingVolumeStep& output, std::string& error) {
    return AdvanceVolume(layout, geometry, state, dt, {}, nullptr, PressureMetric::Dual, output,
                         error);
}

bool detail::AdvanceVolumeWithOwnedDualMass(const GridLayout& layout,
                                           const MovingGridGeometry& geometry,
                                           const MovingVolumeState& state, double dt,
                                           MovingVolumeStep& output, std::string& error) {
    return AdvanceVolume(layout, geometry, state, dt, {}, nullptr, PressureMetric::OwnedDual,
                         output, error);
}

bool detail::AdvanceVolumeForVerifiedSweep(const GridLayout& layout,
                                           const MovingGridGeometry& geometry,
                                           const MovingVolumeState& state, double dt,
                                           double verifiedSweepError, MovingVolumeStep& output,
                                           std::string& error) {
    if (!std::isfinite(verifiedSweepError) || verifiedSweepError < 0) {
        error = "invalid swept geometry verification error";
        return false;
    }
    return AdvanceVolume(layout, geometry, state, dt, {}, nullptr, PressureMetric::FacePrism,
                         output, error, nullptr, verifiedSweepError);
}

} // namespace judas::volume_grid
