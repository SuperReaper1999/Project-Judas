#include "MovingVolumeMomentum.h"

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
            for (int x = 0; x < counts[0]; ++x) {
                function(Index{x, y, z});
            }
        }
    }
}

bool Contains(const Index& index, const Index& counts) {
    for (int axis = 0; axis < 3; ++axis) {
        if (index[axis] < 0 || index[axis] >= counts[axis]) {
            return false;
        }
    }
    return true;
}

std::size_t Face(const GridLayout& layout, int axis, const Index& face) {
    return layout.Face(axis, face[0], face[1], face[2]);
}

std::size_t Cell(const GridLayout& layout, const Index& cell) {
    return layout.Cell(cell[0], cell[1], cell[2]);
}

double DualMass(const GridLayout& layout, const std::vector<double>& primalMass,
                int axis, const Index& face) {
    auto left = face;
    --left[axis];
    const double leftMass = Contains(left, layout.cells) ? primalMass[Cell(layout, left)] : 0;
    const double rightMass = Contains(face, layout.cells) ? primalMass[Cell(layout, face)] : 0;
    return .5 * (leftMass + rightMass);
}

double DualFaceMassFlux(const GridLayout& layout, const std::vector<double>& primalFlux,
                        int component, int direction, const Index& from, const Index& to) {
    if (component == direction) {
        // The dual interface bisects a primal cell.
        return .5 * (primalFlux[Face(layout, direction, from)] + primalFlux[Face(layout, direction, to)]);
    }

    // Each transverse dual interface consists of two half-primal faces.
    // At an exterior boundary only one of the halves exists.
    auto left = to;
    --left[component];
    auto limits = layout.cells;
    ++limits[direction];
    const double leftFlux = Contains(left, limits) ? primalFlux[Face(layout, direction, left)] : 0;
    const double rightFlux = Contains(to, limits) ? primalFlux[Face(layout, direction, to)] : 0;
    return .5 * (leftFlux + rightFlux);
}

bool ValidPhase(double water, double capacity, double fullCellVolume, double roundoff) {
    return std::isfinite(capacity) && std::isfinite(water) && capacity > 0 &&
        capacity <= fullCellVolume + roundoff && water >= 0 && water <= capacity;
}

} // namespace

bool AdvectMovingVolumeMomentum(const GridLayout& layout, const MovingGridGeometry& geometry,
                                const MovingVolumeState& before,
                                const MovingVolumeStep& projected,
                                MovingVolumeMomentumStep& output, std::string& error) {
    const std::size_t cells = layout.CheckedCellCount();
    const auto& after = projected.state;
    const double fullAperture = layout.h * layout.h;
    const auto openAperture = [fullAperture](double area) {
        return area > 0 && area <= fullAperture;
    };
    const double volume = fullAperture * layout.h;
    const double volumeRoundoff = 256 * std::numeric_limits<double>::epsilon() * volume;
    if (cells == 0 || before.water.size() != cells || after.water.size() != cells ||
        geometry.startCapacity.size() != cells || geometry.endCapacity.size() != cells ||
        !std::isfinite(projected.residual) || projected.residual < 0) {
        error = "invalid moving momentum grid or phase state";
        return false;
    }
    // Reconcile the algebraic dual mass against the published primal state
    // within the measured pressure-volume residual and binary64 additions.
    const double tolerance = 1000 * (projected.residual + 16 * volumeRoundoff);
    if (!std::isfinite(tolerance)) {
        error = "invalid moving momentum mass reconciliation tolerance";
        return false;
    }
    std::vector<double> oldMass(cells), newMass(cells), advectedMass(cells);
    for (std::size_t c = 0; c < cells; ++c) {
        const double start = geometry.startCapacity[c];
        const double end = geometry.endCapacity[c];
        if (!ValidPhase(before.water[c], start, volume, volumeRoundoff) ||
            !ValidPhase(after.water[c], end, volume, volumeRoundoff)) {
            error = "moving momentum needs finite phases in nonempty primal cells";
            return false;
        }
        oldMass[c] = start + 999 * before.water[c];
        newMass[c] = end + 999 * after.water[c];
        advectedMass[c] = oldMass[c];
        if (!std::isfinite(oldMass[c]) || !std::isfinite(newMass[c])) {
            error = "moving momentum material mass is not representable";
            return false;
        }
    }
    MovingVolumeMomentumStep next;
    for (int axis = 0; axis < 3; ++axis) {
        const std::size_t faces = layout.FaceCount(axis);
        const auto& flux = projected.materialMassFlux[axis];
        if (flux.size() != faces || after.velocity[axis].size() != faces ||
            geometry.startAperture[axis].size() != faces || geometry.endAperture[axis].size() != faces ||
            geometry.averageAperture[axis].size() != faces) {
            error = "moving momentum face fields do not match the grid";
            return false;
        }

        auto limits = layout.cells;
        ++limits[axis];
        bool valid = true;
        Visit(limits, [&](const Index& face) {
            const auto f = Face(layout, axis, face);
            if (!std::isfinite(flux[f]) || !std::isfinite(after.velocity[axis][f])) {
                valid = false;
                return;
            }
            if (face[axis] == 0 || face[axis] == layout.cells[axis]) {
                valid = valid && flux[f] == 0;
                return;
            }
            // Until face momentum has explicit birth/death ownership, use
            // only a fixed open interior-face graph through the step.
            if (!openAperture(geometry.startAperture[axis][f]) ||
                !openAperture(geometry.endAperture[axis][f]) ||
                !openAperture(geometry.averageAperture[axis][f])) {
                valid = false;
                return;
            }
            auto left = face;
            --left[axis];
            advectedMass[Cell(layout, left)] -= flux[f];
            advectedMass[Cell(layout, face)] += flux[f];
        });
        if (!valid) {
            error = "moving momentum needs open interior faces and finite sealed material flux";
            return false;
        }
    }
    for (std::size_t c = 0; c < cells; ++c) {
        const double residual = std::abs(advectedMass[c] - newMass[c]);
        if (!std::isfinite(residual) || residual > tolerance) {
            error = "moving momentum primal material flux does not match the published phases";
            return false;
        }
        next.maximumPrimalMassResidual = std::max(next.maximumPrimalMassResidual, residual);
    }

    for (int component = 0; component < 3; ++component) {
        const std::size_t faces = layout.FaceCount(component);
        std::vector<double> dualOld(faces), dualNew(faces), dualAdvected(faces),
            momentum(faces), outgoing(faces);
        const auto& velocity = after.velocity[component];
        auto limits = layout.cells;
        ++limits[component];
        Visit(limits, [&](const Index& face) {
            const auto f = Face(layout, component, face);
            dualOld[f] = DualMass(layout, oldMass, component, face);
            dualNew[f] = DualMass(layout, newMass, component, face);
            dualAdvected[f] = dualOld[f];
            momentum[f] = dualOld[f] * velocity[f];
            next.beforeMomentum[component] += momentum[f];
            next.beforeKineticEnergy += .5 * momentum[f] * velocity[f];
        });
        Visit(limits, [&](const Index& face) {
            const auto f = Face(layout, component, face);
            for (int direction = 0; direction < 3; ++direction) {
                auto neighbor = face;
                if (++neighbor[direction] == limits[direction]) continue;
                const auto g = Face(layout, component, neighbor);
                const double flux = DualFaceMassFlux(layout, projected.materialMassFlux[direction],
                                                     component, direction, face, neighbor);
                const auto donor = flux >= 0 ? f : g;
                outgoing[donor] += std::abs(flux);
                const double transportedMomentum = flux * velocity[donor];
                dualAdvected[f] -= flux;
                dualAdvected[g] += flux;
                momentum[f] -= transportedMomentum;
                momentum[g] += transportedMomentum;
            }
        });
        next.velocity[component].resize(faces);
        for (std::size_t f = 0; f < faces; ++f) {
            if (!(dualOld[f] > 0 && dualNew[f] > 0) || !std::isfinite(dualNew[f]) ||
                !std::isfinite(outgoing[f]) || outgoing[f] > dualOld[f] + tolerance) {
                error = "moving momentum exceeds dual donor capacity; substep or merge required";
                return false;
            }
            const double residual = std::abs(dualAdvected[f] - dualNew[f]);
            const double transported = momentum[f] / dualNew[f];
            if (!std::isfinite(residual) || residual > tolerance || !std::isfinite(transported)) {
                error = "moving momentum dual mass or velocity is not representable";
                return false;
            }
            next.maximumDualMassResidual = std::max(next.maximumDualMassResidual, residual);
            next.maximumDonorCfl = std::max(next.maximumDonorCfl, outgoing[f] / dualOld[f]);
            next.velocity[component][f] = transported;
            next.afterMomentum[component] += momentum[f];
            next.afterKineticEnergy += .5 * momentum[f] * transported;
        }
        if (!std::isfinite(next.beforeMomentum[component]) || !std::isfinite(next.afterMomentum[component])) {
            error = "moving momentum totals are not representable";
            return false;
        }
    }
    if (!std::isfinite(next.beforeKineticEnergy) || !std::isfinite(next.afterKineticEnergy)) {
        error = "moving momentum energy is not representable";
        return false;
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
