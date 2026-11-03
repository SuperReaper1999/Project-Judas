#include "GridStaticMovingPhase.h"

namespace judas::volume_grid {
namespace {

using moving_geometry_detail::Index;
using moving_geometry_detail::AdmitRoundoff;
using moving_geometry_detail::AverageBlockedFace;
using moving_geometry_detail::Overlap;
using moving_geometry_detail::VisitSweptBounds;

using Owners = std::vector<std::size_t>;
constexpr std::size_t maxCellReferences = 4'000'000;

struct Bounds { GridVec lo, hi; };

Bounds Unit(const Index& cell) {
    return {{double(cell[0]), double(cell[1]), double(cell[2])},
            {cell[0] + 1., cell[1] + 1., cell[2] + 1.}};
}

Bounds Half(Bounds region, int axis, int side) {
    if (side == 0)
        region.hi[axis] -= .5;
    else
        region.lo[axis] += .5;
    return region;
}

Bounds Wet(Bounds region, int axis, double height) {
    region.hi[axis] = std::clamp(height, region.lo[axis], region.hi[axis]);
    return region;
}

GridVec Metres(GridVec x, double h) {
    for (double& coordinate : x)
        coordinate *= h;
    return x;
}

double Measure(const Bounds& b, int normal = -1) {
    double volume = 1.;
    for (int axis = 0; axis < 3; ++axis)
        if (axis != normal)
            volume *= b.hi[axis] - b.lo[axis];
    return volume;
}

double CellVolume(const GridLayout& layout) { return (layout.h * layout.h) * layout.h; }

std::size_t Cell(const GridLayout& layout, const Index& i) { return layout.Cell(i[0], i[1], i[2]); }

bool Axis(int axis) { return axis >= 0 && axis < 3; }

bool Admit(const GridStaticUnion& fixed, const std::vector<BoxMotion>& motion, bool snapshot,
           std::vector<BoxMotion>& normalized, std::string& error) {
    const auto count = fixed.layout.CheckedCellCount();
    if (count == 0 || fixed.capacity.size() != count || fixed.cellPartition.size() != count) {
        error = "static/moving phase geometry requires the original static partition cache";
        return false;
    }
    if (!moving_geometry_detail::NormalizeBoxes(fixed.layout, motion, normalized, error))
        return false;
    for (const auto& box : motion) {
        if (snapshot && box.translation != GridVec{}) {
            error = "static/moving phase endpoint storage requires a stationary snapshot";
            return false;
        }
        for (const auto& wall : fixed.stationary) {
            if (moving_geometry_detail::SweptBoxesTouch(wall, box)) {
                error = "moving phase box touches static geometry during its swept path";
                return false;
            }
        }
    }
    return true;
}

double Occupied(const BoxMotion& box, const Bounds& clip, double scale) {
    for (int axis = 0; axis < 3; ++axis)
        scale *= Overlap(clip.lo[axis], clip.hi[axis], box.lo[axis], box.hi[axis]);
    return scale;
}

bool BinSnapshot(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                 std::vector<Owners>& bins, std::string& error) {
    bins.resize(layout.CellCount());
    std::size_t count = 0;
    for (std::size_t id = 0; id < boxes.size(); ++id) {
        VisitSweptBounds(layout, boxes[id], -1, [&](const Index& cell) {
            if (count <= maxCellReferences && Occupied(boxes[id], Unit(cell), 1.) > 0.) {
                bins[Cell(layout, cell)].push_back(id);
                ++count;
            }
        });
        if (count > maxCellReferences) {
            error = "static/moving phase cell reference budget exceeded";
            return false;
        }
    }
    return true;
}

double CanonicalCapacity(const GridStaticUnion& fixed, std::size_t index, const Bounds& clip,
                         const std::vector<BoxMotion>& boxes, const Owners& owners) {
    const double full = CellVolume(fixed.layout);
    double dynamic = full;
    for (const auto id : owners)
        dynamic -= Occupied(boxes[id], clip, full);
    // Match BuildGridStaticMovingUnion, including the moving builder's
    // endpoint arithmetic and its bounded roundoff near full occupation.
    return fixed.capacity[index] - (full - std::clamp(dynamic, 0., full));
}

bool FreeVolume(const GridStaticUnion& fixed, const Index& cell, const Bounds& clip,
                const std::vector<BoxMotion>& boxes, const Owners& owners,
                double& volume, std::string& error) {
    const double full = CellVolume(fixed.layout);
    volume = full * Measure(clip);
    if (volume == 0.)
        return true;
    if (fixed.cellPartition[Cell(fixed.layout, cell)] != GridStaticUnion::uncut &&
        !ClipGridStaticCellVolume(fixed, cell, Metres(clip.lo, fixed.layout.h),
                                 Metres(clip.hi, fixed.layout.h), volume, error))
        return false;
    for (const auto id : owners)
        volume -= Occupied(boxes[id], clip, full);
    if (!AdmitRoundoff(volume, full)) {
        error = "static/moving clipped free volume is not representable";
        return false;
    }
    return true;
}

bool Plane(const GridStaticUnion& fixed, const Index& cell, std::size_t index, int axis,
           const std::vector<BoxMotion>& boxes, const Owners& owners, double water, double cap,
           double& plane, std::string& error) {
    const double lo = cell[axis], hi = lo + 1., h = fixed.layout.h;
    if (water == 0. || water == cap) {
        plane = (water == 0. ? lo : hi) * h;
        return true;
    }
    const auto part = fixed.cellPartition[index];
    if (part == GridStaticUnion::uncut && owners.empty()) {
        plane = (lo + water / CellVolume(fixed.layout)) * h;
        return true;
    }
    std::vector<double> cuts = part == GridStaticUnion::uncut ? std::vector<double>{lo, hi}
                                                            : fixed.partitions[part].edges[axis];
    for (const auto id : owners) {
        cuts.push_back(std::max(lo, boxes[id].lo[axis]));
        cuts.push_back(std::min(hi, boxes[id].hi[axis]));
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    auto slab = Unit(cell);
    for (std::size_t i = 1; i < cuts.size(); ++i) {
        slab.lo[axis] = cuts[i - 1];
        slab.hi[axis] = cuts[i];
        double volume;
        if (!FreeVolume(fixed, cell, slab, boxes, owners, volume, error))
            return false;
        if (water <= volume) {
            const double height = water == volume ? cuts[i]
                : std::fma(water / volume, cuts[i] - cuts[i - 1], cuts[i - 1]);
            plane = height * h;
            return true;
        }
        water -= volume;
    }
    if (water > 256. * std::numeric_limits<double>::epsilon() * CellVolume(fixed.layout)) {
        error = "static/moving phase inverse does not close against cell capacity";
        return false;
    }
    plane = hi * h;
    return true;
}

bool ReadPlanes(const GridLayout& layout, const GridCellPhase& phase, std::vector<double>& planes,
                std::string& error) {
    if (!Axis(phase.axis) || phase.waterPlane.size() != layout.CellCount()) {
        error = "static/moving phase plane field does not match the grid";
        return false;
    }
    planes.resize(phase.waterPlane.size());
    bool valid = true;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
        const auto c = Cell(layout, cell);
        const double lo = cell[phase.axis], h = layout.h, height = phase.waterPlane[c];
        if (!(height >= lo * h && height <= (lo + 1.) * h)) {
            valid = false;
            return;
        }
        planes[c] = height == lo*h ? lo : height == (lo + .5)*h ? lo + .5
            : height == (lo + 1.)*h ? lo + 1. : std::clamp(height / h, lo, lo + 1.);
    });
    if (!valid)
        error = "static/moving local phase plane is outside its cell";
    return valid;
}

struct FaceBounds { Bounds open, leftWet, rightWet; };

FaceBounds Face(const GridLayout& layout, const std::vector<double>& planes, int phaseAxis,
                const Index& face, int normal, int split, int side, bool midpoint) {
    auto open = split < 0 ? Unit(face) : Half(Unit(face), split, side);
    const double plane = face[normal] + (midpoint ? .5 : 0.);
    open.lo[normal] = open.hi[normal] = plane;
    auto dry = open;
    dry.hi[(normal + 1) % 3] = dry.lo[(normal + 1) % 3];
    const auto phase = [&](bool left) {
        auto owner = face;
        owner[normal] -= left && !midpoint;
        if (owner[normal] < 0 || owner[normal] >= layout.cells[normal])
            return dry;
        const double height = planes[Cell(layout, owner)];
        if (normal == phaseAxis)
            return (left ? height >= plane : height > plane) ? open : dry;
        return Wet(open, phaseAxis, height);
    };
    return {open, phase(true), phase(false)};
}

bool StaticAperture(const GridStaticUnion& fixed, Index owner, int normal, const Bounds& rect,
                    double& area, std::string& error) {
    const double whole = Measure(rect, normal);
    area = 0.;
    if (whole == 0.)
        return true;
    const auto& layout = fixed.layout;
    const int plane = int(rect.lo[normal]);
    const bool shared = plane == rect.lo[normal];
    auto face = owner;
    face[normal] = plane;
    const auto& indices = shared ? fixed.facePartition[normal] : fixed.cellPartition;
    const auto index = shared ? layout.Face(normal, face[0], face[1], face[2]) : Cell(layout, owner);
    if (index < indices.size() && indices[index] == GridStaticUnion::uncut) {
        area = whole * (layout.h * layout.h);
        return true;
    }
    if (owner[normal] == fixed.layout.cells[normal])
        --owner[normal];
    return ClipGridStaticFaceArea(fixed, owner, normal, rect.lo[normal] * fixed.layout.h,
        Metres(rect.lo, fixed.layout.h), Metres(rect.hi, fixed.layout.h), area, error);
}

bool FillFaces(const GridStaticUnion& fixed, const std::vector<BoxMotion>& boxes,
               const std::vector<double>& planes, int phaseAxis, int normal, int split, bool midpoint,
               std::vector<GridHalfCellAperture>& output, std::string& error) {
    const auto& layout = fixed.layout;
    const int parts = split < 0 ? 1 : 2;
    const double faceArea = layout.h * layout.h;
    output.resize((midpoint ? layout.CellCount() : layout.FaceCount(normal)) * parts);
    const auto index = [&](const Index& i) {
        return (midpoint ? Cell(layout, i) : layout.Face(normal, i[0], i[1], i[2])) * parts;
    };
    bool valid = true;
    grid_phase_detail::VisitGrid(layout, midpoint ? -1 : normal, [&](const Index& i) {
        if (!valid)
            return;
        for (int side = 0; side < parts; ++side) {
            const auto f = Face(layout, planes, phaseAxis, i, normal, split, side, midpoint);
            auto& a = output[index(i) + side];
            const auto clip = [&](const Bounds& wet, double& wetArea) {
                if (wet.lo == f.open.lo && wet.hi == f.open.hi) {
                    wetArea = a.open;
                    return true;
                }
                return StaticAperture(fixed, i, normal, wet, wetArea, error);
            };
            valid = StaticAperture(fixed, i, normal, f.open, a.open, error) &&
                clip(f.leftWet, a.leftWet) && clip(f.rightWet, a.rightWet);
            if (!valid)
                return;
        }
    });
    if (!valid)
        return false;
    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, midpoint ? -1 : normal, [&](const Index& i) {
            const auto blocked = [&](const Bounds& rect) {
                return Measure(rect, normal) == 0. ? 0. :
                    faceArea * AverageBlockedFace(box, normal, rect.lo, rect.hi);
            };
            for (int side = 0; side < parts; ++side) {
                const auto f = Face(layout, planes, phaseAxis, i, normal, split, side, midpoint);
                auto& a = output[index(i) + side];
                a.open -= blocked(f.open);
                a.leftWet -= blocked(f.leftWet);
                a.rightWet -= blocked(f.rightWet);
            }
        });
    }
    for (auto& a : output) {
        const double roundoff = 128. * std::numeric_limits<double>::epsilon() * faceArea;
        if (!AdmitRoundoff(a.open, faceArea / parts) || !AdmitRoundoff(a.leftWet, faceArea / parts) ||
            !AdmitRoundoff(a.rightWet, faceArea / parts) || a.leftWet > a.open + roundoff ||
            a.rightWet > a.open + roundoff) {
            error = "static/moving wet aperture exceeds its open rectangle";
            return false;
        }
        a.leftWet = std::min(a.leftWet, a.open);
        a.rightWet = std::min(a.rightWet, a.open);
    }
    return true;
}

} // namespace

bool ReconstructStaticMovingPhase(const GridStaticUnion& fixed, const std::vector<BoxMotion>& snapshot,
                                  int phaseAxis, const std::vector<double>& water,
                                  GridCellPhase& output, std::string& error) {
    const auto& layout = fixed.layout;
    if (!Axis(phaseAxis) || water.size() != layout.CheckedCellCount()) {
        error = "static/moving phase reconstruction requires matching inventory and axis";
        return false;
    }
    std::vector<BoxMotion> boxes;
    std::vector<Owners> bins;
    if (!Admit(fixed, snapshot, true, boxes, error) || !BinSnapshot(layout, boxes, bins, error))
        return false;
    GridCellPhase next;
    next.axis = phaseAxis;
    next.capacity.resize(water.size());
    next.waterPlane.resize(water.size());
    bool valid = true;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
        if (!valid)
            return;
        const auto c = Cell(layout, cell);
        auto& cap = next.capacity[c];
        cap = CanonicalCapacity(fixed, c, Unit(cell), boxes, bins[c]);
        if (!AdmitRoundoff(cap, CellVolume(layout)) || !(water[c] >= 0. && water[c] <= cap)) {
            valid = false;
            error = "static/moving water inventory exceeds its exact cell capacity";
            return;
        }
        valid = Plane(fixed, cell, c, phaseAxis, boxes, bins[c], water[c], cap, next.waterPlane[c], error);
    });
    if (!valid)
        return false;
    output = std::move(next);
    error.clear();
    return true;
}

bool BuildStaticMovingHalfStorage(const GridStaticUnion& fixed, const std::vector<BoxMotion>& snapshot,
                                  const GridCellPhase& phase, int splitAxis,
                                  GridHalfCellStorage& output, std::string& error) {
    const auto& layout = fixed.layout;
    if (!Axis(splitAxis)) {
        error = "invalid static/moving half split axis";
        return false;
    }
    std::vector<BoxMotion> boxes;
    std::vector<Owners> bins;
    std::vector<double> planes;
    if (!Admit(fixed, snapshot, true, boxes, error) || !ReadPlanes(layout, phase, planes, error) ||
        !BinSnapshot(layout, boxes, bins, error))
        return false;
    GridHalfCellStorage next;
    next.capacity.resize(layout.CellCount());
    next.water.resize(layout.CellCount());
    const double roundoff = 256. * std::numeric_limits<double>::epsilon() * CellVolume(layout);
    bool valid = true;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
        if (!valid)
            return;
        const auto c = Cell(layout, cell);
        for (int side = 0; side < 2; ++side) {
            const auto half = Half(Unit(cell), splitAxis, side);
            auto& capacity = next.capacity[c][side];
            auto& water = next.water[c][side];
            valid = FreeVolume(fixed, cell, half, boxes, bins[c], capacity, error) &&
                FreeVolume(fixed, cell, Wet(half, phase.axis, planes[c]), boxes, bins[c], water, error);
            if (!valid)
                return;
            if (water > capacity + roundoff) {
                error = "static/moving half water exceeds the available half volume";
                valid = false;
                return;
            }
            water = std::min(water, capacity);
        }
    });
    if (!valid)
        return false;
    output = std::move(next);
    error.clear();
    return true;
}

bool AverageStaticMovingWetApertures(const GridStaticUnion& fixed, const std::vector<BoxMotion>& motion,
                                     const GridCellPhase& phase,
                                     std::array<std::vector<GridHalfCellAperture>, 3>& output,
                                     std::string& error) {
    std::vector<BoxMotion> boxes;
    std::vector<double> planes;
    if (!Admit(fixed, motion, false, boxes, error) || !ReadPlanes(fixed.layout, phase, planes, error))
        return false;
    std::array<std::vector<GridHalfCellAperture>, 3> next;
    for (int normal = 0; normal < 3; ++normal)
        if (!FillFaces(fixed, boxes, planes, phase.axis, normal, -1, false, next[normal], error))
            return false;
    output = std::move(next);
    error.clear();
    return true;
}

bool AverageStaticMovingHalfApertures(const GridStaticUnion& fixed, const std::vector<BoxMotion>& motion,
                                      const GridCellPhase& phase, int splitAxis,
                                      GridHalfCellApertures& output, std::string& error) {
    if (!Axis(splitAxis)) {
        error = "invalid static/moving half aperture split axis";
        return false;
    }
    std::vector<BoxMotion> boxes;
    std::vector<double> planes;
    if (!Admit(fixed, motion, false, boxes, error) || !ReadPlanes(fixed.layout, phase, planes, error))
        return false;
    GridHalfCellApertures next;
    if (!FillFaces(fixed, boxes, planes, phase.axis, splitAxis, -1, true, next.midpoint, error))
        return false;
    std::vector<GridHalfCellAperture> parts;
    for (int normal = 0; normal < 3; ++normal) {
        if (normal == splitAxis)
            continue;
        if (!FillFaces(fixed, boxes, planes, phase.axis, normal, splitAxis, false, parts, error))
            return false;
        auto& transverse = next.transverse[normal];
        transverse.resize(parts.size() / 2);
        for (std::size_t i = 0; i < transverse.size(); ++i)
            transverse[i] = {parts[2 * i], parts[2 * i + 1]};
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
