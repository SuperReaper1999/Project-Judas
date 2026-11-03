#include "HalfDualVolumeMomentum.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace judas::volume_grid {
namespace {

using Index = std::array<int, 3>;

template<class Function>
void Visit(const Index& counts, Function&& function) {
    for (int z = 0; z < counts[2]; ++z) {
        for (int y = 0; y < counts[1]; ++y) {
            for (int x = 0; x < counts[0]; ++x)
                function(Index{x, y, z});
        }
    }
}

std::size_t Face(const GridLayout& layout, int axis, const Index& i) {
    return layout.Face(axis, i[0], i[1], i[2]);
}

std::size_t Cell(const GridLayout& layout, const Index& i) {
    return layout.Cell(i[0], i[1], i[2]);
}

bool ReadHalfMass(std::size_t count, double cellVolume, const std::vector<double>& capacity,
                  const std::vector<double>& water, std::vector<double>& halfMass) {
    if (capacity.size() != count || water.size() != count)
        return false;
    halfMass.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(capacity[i]) || !std::isfinite(water[i]) ||
            capacity[i] < 0 || capacity[i] > cellVolume || water[i] < 0 || water[i] > capacity[i])
            return false;
        halfMass[i] = .5 * (capacity[i] + 999 * water[i]);
        if (!std::isfinite(halfMass[i]))
            return false;
    }
    return true;
}

bool ReadVelocity(const HalfDualVolumeState& state, std::size_t count) {
    for (const auto& field : state.velocity) {
        if (field.size() != 2 * count ||
            std::any_of(field.begin(), field.end(), [](double speed) { return !std::isfinite(speed); }))
            return false;
    }
    return true;
}

struct PressureFace {
    std::size_t index, leftHalf, rightHalf;
    double leftMass, rightMass, aperture, speed;
};

template<class Function>
bool VisitPressureFaces(const GridLayout& layout, const MovingGridGeometry& geometry,
                        const std::vector<double>& halfMass, const HalfDualVolumeState& state,
                        int axis, Function&& function) {
    auto limits = layout.cells;
    ++limits[axis];
    const double fullArea = layout.h * layout.h;
    const auto& aperture = geometry.averageAperture[axis];
    if (aperture.size() != layout.FaceCount(axis))
        return false;
    bool valid = true;
    Visit(limits, [&](const Index& i) {
        const std::size_t index = Face(layout, axis, i);
        const double area = aperture[index];
        if (!(area >= 0 && area <= fullArea)) {
            valid = false;
            return;
        }
        if (area == 0 || i[axis] == 0 || i[axis] == layout.cells[axis])
            return;
        auto left = i;
        --left[axis];
        const std::size_t l = Cell(layout, left), r = Cell(layout, i);
        const double lm = halfMass[l], rm = halfMass[r], mass = lm + rm;
        if (!(mass > 0) || !std::isfinite(mass)) {
            valid = false;
            return;
        }
        const std::size_t lh = 2 * l + 1, rh = 2 * r;
        const auto& field = state.velocity[axis];
        const double speed = (lm / mass) * field[lh] + (rm / mass) * field[rh];
        valid = valid && std::isfinite(speed) && function(PressureFace{index, lh, rh, lm, rm, area, speed});
    });
    return valid;
}

} // namespace

bool AdvectHalfDualVolumeMomentum(const GridLayout& layout, const MovingGridGeometry& geometry,
                                  const MovingVolumeState& before,
                                  const MovingVolumeStep& projected,
                                  const HalfDualVolumeState& initial,
                                  HalfDualVolumeTransport& output, std::string& error,
                                  bool* donorCapacityExceeded) {
    if (donorCapacityExceeded)
        *donorCapacityExceeded = false;
    const std::size_t count = layout.CheckedCellCount();
    const double area = layout.h * layout.h;
    const double volume = area * layout.h;
    std::vector<double> halfOld, halfNew;
    if (!count || !ReadVelocity(initial, count) ||
        !ReadHalfMass(count, volume, geometry.startCapacity, before.water, halfOld) ||
        !ReadHalfMass(count, volume, geometry.endCapacity, projected.state.water, halfNew) ||
        !std::isfinite(projected.residual) || projected.residual < 0) {
        error = "invalid half-dual material, grid or velocity state";
        return false;
    }
    const double massTolerance = 500 *
        (projected.residual + 4096 * std::numeric_limits<double>::epsilon() * volume);
    if (!std::isfinite(massTolerance)) {
        error = "invalid half-dual mass reconciliation tolerance";
        return false;
    }

    for (int axis = 0; axis < 3; ++axis) {
        const auto& flux = projected.materialMassFlux[axis];
        const auto& apertures = geometry.averageAperture[axis];
        const std::size_t faces = layout.FaceCount(axis);
        if (flux.size() != faces || apertures.size() != faces) {
            error = "half-dual transport requires the shared projected material flux";
            return false;
        }
        bool valid = true;
        auto limits = layout.cells;
        ++limits[axis];
        Visit(limits, [&](const Index& i) {
            const std::size_t face = Face(layout, axis, i);
            const double aperture = apertures[face];
            const bool sealed = i[axis] == 0 || i[axis] == layout.cells[axis] || aperture == 0;
            valid = valid && std::isfinite(flux[face]) && aperture >= 0 && aperture <= area &&
                (!sealed || flux[face] == 0);
        });
        if (!valid) {
            error = "half-dual transport received invalid or obstructed material flux";
            return false;
        }
    }

    HalfDualVolumeTransport step;
    for (int component = 0; component < 3; ++component) {
        const auto& velocity = initial.velocity[component];
        auto& result = step.state.velocity[component];
        result.resize(2 * count);
        std::vector<double> advectedMass(2 * count), momentum(2 * count), outgoing(2 * count);
        double speedScale = 0;
        for (std::size_t half = 0; half < 2 * count; ++half) {
            advectedMass[half] = halfOld[half / 2];
            momentum[half] = advectedMass[half] * velocity[half];
            speedScale = std::max(speedScale, std::abs(velocity[half]));
            step.beforeMomentum[component] += momentum[half];
            step.beforeKineticEnergy += .5 * momentum[half] * velocity[half];
        }
        const auto exchange = [&](std::size_t left, std::size_t right, double flux) {
            const auto donor = flux >= 0 ? left : right;
            outgoing[donor] += std::abs(flux);
            const double impulse = flux * velocity[donor];
            advectedMass[left] -= flux;
            advectedMass[right] += flux;
            momentum[left] -= impulse;
            momentum[right] += impulse;
        };

        Visit(layout.cells, [&](const Index& i) {
            const std::size_t half = 2 * Cell(layout, i);
            auto high = i;
            ++high[component];
            const auto& axial = projected.materialMassFlux[component];
            // At the cell midpoint, average the two axial primal fluxes.
            // Combined with the full flux through each true axial boundary,
            // this gives each half exactly half the primal mass difference.
            exchange(half, half + 1, .5 *
                (axial[Face(layout, component, i)] + axial[Face(layout, component, high)]));
            for (int direction = 0; direction < 3; ++direction) {
                auto neighbor = i;
                if (++neighbor[direction] == layout.cells[direction]) continue;
                const double flux = projected.materialMassFlux[direction][Face(layout, direction, neighbor)];
                const std::size_t other = 2 * Cell(layout, neighbor);
                if (direction == component) {
                    exchange(half + 1, other, flux);
                } else {
                    exchange(half, other, .5 * flux);
                    exchange(half + 1, other + 1, .5 * flux);
                }
            }
        });

        for (std::size_t half = 0; half < 2 * count; ++half) {
            const double start = halfOld[half / 2];
            const double end = halfNew[half / 2];
            const double errorMass = std::abs(advectedMass[half] - end);
            if (!std::isfinite(errorMass) || !std::isfinite(outgoing[half]) ||
                !std::isfinite(momentum[half]) || errorMass > massTolerance) {
                error = "half-dual transported mass does not match endpoint capacity";
                return false;
            }
            if (outgoing[half] > start) {
                if (donorCapacityExceeded)
                    *donorCapacityExceeded = true;
                error = "half-dual donor capacity exhausted; substep required";
                return false;
            }
            step.maximumHalfMassResidual = std::max(step.maximumHalfMassResidual, errorMass);
            if (start > 0)
                step.maximumDonorCfl = std::max(step.maximumDonorCfl, outgoing[half] / start);
            if (end > 0 && !(advectedMass[half] > 0)) {
                error = "half-dual transport could not establish positive new material";
                return false;
            }
            if (end == 0 && speedScale > 0 && std::abs(momentum[half]) / speedScale > massTolerance) {
                error = "half-dual transport leaves momentum in an empty owner";
                return false;
            }
            // The upwind state uses its evolved mass so phase publication
            // roundoff cannot amplify velocity in a nearly empty half. Record
            // the represented momentum change against the new physical mass.
            const double speed = end > 0 ? momentum[half] / advectedMass[half] : 0;
            const double represented = end * speed;
            if (!std::isfinite(speed) || !std::isfinite(represented)) {
                error = "half-dual transport momentum is not representable";
                return false;
            }
            result[half] = speed;
            step.roundoffMomentum[component] += represented - momentum[half];
            step.afterMomentum[component] += represented;
            step.afterKineticEnergy += .5 * represented * speed;
        }
        if (!std::isfinite(step.beforeMomentum[component]) ||
            !std::isfinite(step.afterMomentum[component]) ||
            !std::isfinite(step.roundoffMomentum[component])) {
            error = "half-dual transport momentum totals are not representable";
            return false;
        }
    }
    if (!std::isfinite(step.beforeKineticEnergy) || !std::isfinite(step.afterKineticEnergy)) {
        error = "half-dual transport energy is not representable";
        return false;
    }
    output = std::move(step);
    error.clear();
    return true;
}

bool BuildHalfDualVolumePredictor(const GridLayout& layout, const MovingGridGeometry& geometry,
                                  const std::vector<double>& water,
                                  const HalfDualVolumeState& initial,
                                  MovingVolumeState& output, std::string& error) {
    const std::size_t count = layout.CheckedCellCount();
    std::vector<double> halfMass;
    if (!count || !ReadVelocity(initial, count) ||
        !ReadHalfMass(count, layout.h * layout.h * layout.h, geometry.startCapacity, water, halfMass)) {
        error = "invalid half-dual pressure predictor state";
        return false;
    }
    MovingVolumeState step;
    step.water = water;
    for (int axis = 0; axis < 3; ++axis) {
        auto& field = step.velocity[axis];
        field.resize(layout.FaceCount(axis));
        if (!VisitPressureFaces(layout, geometry, halfMass, initial, axis, [&](const PressureFace& f) {
            field[f.index] = f.speed;
            return true;
        })) {
            error = "half-dual pressure predictor needs start material on every open face";
            return false;
        }
    }
    output = std::move(step);
    error.clear();
    return true;
}

bool ApplyHalfDualVolumePressure(const GridLayout& layout, const MovingGridGeometry& geometry,
                                 const MovingVolumeState& before,
                                 const MovingVolumeStep& projected,
                                 const HalfDualVolumeState& initial, double dt,
                                 HalfDualPressureUpdate& output, std::string& error) {
    const std::size_t count = layout.CheckedCellCount();
    std::vector<double> halfMass;
    if (!count || !std::isfinite(dt) || dt <= 0 || !ReadVelocity(initial, count) ||
        !ReadHalfMass(count, layout.h * layout.h * layout.h, geometry.startCapacity,
                      before.water, halfMass)) {
        error = "invalid half-dual pressure mapping state";
        return false;
    }
    HalfDualPressureUpdate step;
    step.state = initial;
    double workScale = 0;
    for (int axis = 0; axis < 3; ++axis) {
        const auto& flux = projected.flux[axis];
        const auto& gravity = projected.gravityVelocityIncrement[axis];
        auto& field = step.state.velocity[axis];
        const auto& aperture = geometry.averageAperture[axis];
        if (flux.size() != layout.FaceCount(axis) || before.velocity[axis].size() != flux.size() ||
            (!gravity.empty() && gravity.size() != flux.size()) ||
            aperture.size() != flux.size() ||
            std::any_of(flux.begin(), flux.end(), [](double value) { return !std::isfinite(value); })) {
            error = "half-dual pressure mapping requires finite projected face flux";
            return false;
        }
        bool sealed = true;
        auto limits = layout.cells;
        ++limits[axis];
        Visit(limits, [&](const Index& i) {
            const auto index = Face(layout, axis, i);
            if (i[axis] == 0 || i[axis] == layout.cells[axis] || aperture[index] == 0)
                sealed = sealed && flux[index] == 0 && before.velocity[axis][index] == 0 &&
                    (gravity.empty() || gravity[index] == 0);
        });
        if (!sealed) {
            error = "half-dual pressure mapping received flow across a sealed face";
            return false;
        }
        if (!VisitPressureFaces(layout, geometry, halfMass, initial, axis, [&](const PressureFace& f) {
            if (before.velocity[axis][f.index] != f.speed)
                return false;
            const double gravityChange = gravity.empty() ? 0. : gravity[f.index];
            const double freeSpeed = f.speed + gravityChange;
            const double change = flux[f.index] / (dt * f.aperture) - freeSpeed;
            const double mass = f.leftMass + f.rightMass;
            const double impulse = mass * change;
            const double work = (freeSpeed + .5 * change) * impulse;
            if (f.leftMass > 0) field[f.leftHalf] += gravityChange + change;
            if (f.rightMass > 0) field[f.rightHalf] += gravityChange + change;
            step.gravityImpulse[axis] += mass * gravityChange;
            step.impulse[axis] += impulse;
            step.kineticChange += work;
            step.correctionEnergy += .5 * impulse * change;
            workScale += std::abs(work);
            return std::isfinite(field[f.leftHalf]) && std::isfinite(field[f.rightHalf]);
        }) || !std::isfinite(step.impulse[axis]) || !std::isfinite(step.gravityImpulse[axis])) {
            error = "half-dual pressure mapping disagrees with the owned face predictor";
            return false;
        }
    }
    const double tolerance = 4096 * std::numeric_limits<double>::epsilon() *
        (1 + workScale + step.correctionEnergy);
    if (!std::isfinite(tolerance) || !std::isfinite(step.kineticChange) ||
        !std::isfinite(projected.pressureKineticWork) || !std::isfinite(projected.pressureCorrectionEnergy) ||
        std::abs(step.kineticChange - projected.pressureKineticWork) > tolerance ||
        std::abs(step.correctionEnergy - projected.pressureCorrectionEnergy) > tolerance) {
        error = "half-dual pressure mapping requires the matched dual mass metric";
        return false;
    }
    output = std::move(step);
    error.clear();
    return true;
}

bool MergeHalfDualVolumeFaces(const GridLayout& layout, const MovingGridGeometry& geometry,
                              const std::vector<double>& water,
                              const HalfDualVolumeState& initial,
                              HalfDualFaceMerge& output, std::string& error) {
    const std::size_t count = layout.CheckedCellCount();
    const double area = layout.h * layout.h;
    std::vector<double> halfMass;
    if (!count || !ReadVelocity(initial, count) ||
        !ReadHalfMass(count, area * layout.h, geometry.endCapacity, water, halfMass)) {
        error = "invalid half-dual face merge state";
        return false;
    }
    HalfDualFaceMerge step;
    step.state = initial;
    for (int axis = 0; axis < 3; ++axis) {
        const auto& aperture = geometry.endAperture[axis];
        const std::size_t countFaces = layout.FaceCount(axis);
        if (aperture.size() != countFaces) {
            error = "half-dual face merge requires endpoint apertures";
            return false;
        }
        auto& field = step.state.velocity[axis];
        step.faceVelocity[axis].resize(countFaces);
        auto limits = layout.cells;
        ++limits[axis];
        bool valid = true;
        Visit(limits, [&](const Index& i) {
            const std::size_t face = Face(layout, axis, i);
            if (!(aperture[face] >= 0 && aperture[face] <= area)) {
                valid = false;
                return;
            }
            auto left = i;
            --left[axis];
            if (i[axis] == 0 || i[axis] == layout.cells[axis]) {
                const bool low = i[axis] == 0;
                const std::size_t cell = Cell(layout, low ? i : left);
                const std::size_t half = 2 * cell + (low ? 0 : 1);
                const double speed = field[half];
                const double impulse = -halfMass[cell] * speed;
                step.stationaryWallImpulse[axis] += impulse;
                step.stationaryWallKineticLoss -= .5 * impulse * speed;
                field[half] = 0;
                return;
            }
            if (aperture[face] == 0)
                return; // There is no shared material velocity through this solid.
            const std::size_t l = Cell(layout, left);
            const std::size_t r = Cell(layout, i);
            const double lm = halfMass[l], rm = halfMass[r], mass = lm + rm;
            if (!(lm > 0 && rm > 0) || !std::isfinite(mass)) {
                valid = false;
                return;
            }
            const double vl = field[2 * l + 1], vr = field[2 * r];
            const double speed = (lm / mass) * vl + (rm / mass) * vr;
            const double delta = vl - vr;
            step.mergeKineticLoss += .5 * (lm / mass) * rm * delta * delta;
            step.faceVelocity[axis][face] = speed;
            field[2 * l + 1] = speed;
            field[2 * r] = speed;
        });
        if (!valid || !std::isfinite(step.stationaryWallImpulse[axis]) ||
            !std::isfinite(step.stationaryWallKineticLoss) || !std::isfinite(step.mergeKineticLoss)) {
            error = "half-dual face merge has invalid geometry, impulse or kinetic loss";
            return false;
        }
    }
    output = std::move(step);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
