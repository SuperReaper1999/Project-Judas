#include "GridStaticUnion.h"

#include <iterator>

namespace judas::volume_grid {
namespace {

using Index = std::array<int, 3>;
using Owners = std::vector<std::size_t>;

constexpr std::size_t maxBoxes = 256;
constexpr std::size_t maxBinReferences = 4'000'000;
constexpr std::size_t maxLocalPieces = 4096;
constexpr std::size_t maxOccupancyChecks = 32'000'000;
constexpr std::size_t maxCachedPieces = 1'000'000;

template<class Function>
bool Visit(const Index& first, const Index& end, const Function& function) {
    for (int z = first[2]; z < end[2]; ++z)
        for (int y = first[1]; y < end[1]; ++y)
            for (int x = first[0]; x < end[0]; ++x)
                if (!function(Index{x, y, z}))
                    return false;
    return true;
}

bool ReadBoxes(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
               std::vector<BoxMotion>& boxes, std::string& error) {
    if (stationary.size() > maxBoxes) {
        error = "static union box count exceeds the prototype limit";
        return false;
    }
    boxes.reserve(stationary.size());
    for (auto box : stationary) {
        if (box.translation != GridVec{}) {
            error = "static union received a moving box";
            return false;
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (!(box.lo[axis] >= 0. && box.hi[axis] > box.lo[axis] &&
                  box.hi[axis] <= layout.cells[axis] * layout.h)) {
                error = "static union requires finite, chart-contained positive box extents";
                return false;
            }
            box.lo[axis] /= layout.h;
            box.hi[axis] = box.hi[axis] == layout.cells[axis] * layout.h
                ? layout.cells[axis] : box.hi[axis] / layout.h;
            if (!(box.hi[axis] > box.lo[axis])) {
                error = "static union box collapses at chart precision";
                return false;
            }
        }
        boxes.push_back(box);
    }
    return true;
}

struct Bounds {
    GridVec lo, hi;
};

Bounds Region(const Index& index, int normal) {
    Bounds region{};
    for (int axis = 0; axis < 3; ++axis) {
        region.lo[axis] = index[axis];
        region.hi[axis] = index[axis] + (axis != normal);
    }
    return region;
}

bool Covers(const BoxMotion& box, const Bounds& piece) {
    for (int axis = 0; axis < 3; ++axis)
        if (box.lo[axis] > piece.lo[axis] || box.hi[axis] < piece.hi[axis])
            return false;
    return true;
}

bool OneFreeComponent(const Index& spans, std::vector<unsigned char>& free) {
    const auto first = std::find(free.begin(), free.end(), 1);
    if (first == free.end())
        return true;
    const std::array<std::size_t, 3> stride{1, std::size_t(spans[0]),
        std::size_t(spans[0]) * std::size_t(spans[1])};
    std::vector<std::size_t> frontier{std::size_t(first - free.begin())};
    *first = 0;
    for (std::size_t visit = 0; visit < frontier.size(); ++visit) {
        const auto node = frontier[visit];
        const auto mark = [&](std::size_t neighbor) {
            if (free[neighbor]) {
                free[neighbor] = 0;
                frontier.push_back(neighbor);
            }
        };
        for (int axis = 0; axis < 3; ++axis) {
            const auto coordinate = (node / stride[axis]) % std::size_t(spans[axis]);
            if (coordinate > 0)
                mark(node - stride[axis]);
            if (coordinate + 1 < std::size_t(spans[axis]))
                mark(node + stride[axis]);
        }
    }
    return std::find(free.begin(), free.end(), 1) == free.end();
}

bool Measure(const Index& index, int normal, const std::vector<BoxMotion>& boxes,
             const Owners& owners, GridStaticUnion& stats, std::size_t& cacheIndex,
             double& open, std::string& error) {
    if (owners.empty()) {
        open = 1.;
        return true;
    }
    const auto region = Region(index, normal);
    GridStaticPartition partition;
    auto& edges = partition.edges;
    Index spans{};
    std::size_t pieces = 1;
    for (int axis = 0; axis < 3; ++axis) {
        auto& cuts = edges[axis];
        cuts = {region.lo[axis], region.hi[axis]};
        if (axis != normal) {
            for (const auto owner : owners) {
                const auto& box = boxes[owner];
                cuts.push_back(std::clamp(box.lo[axis], region.lo[axis], region.hi[axis]));
                cuts.push_back(std::clamp(box.hi[axis], region.lo[axis], region.hi[axis]));
            }
            std::sort(cuts.begin(), cuts.end());
            cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        }
        spans[axis] = int(cuts.size()) - 1;
        pieces *= std::size_t(spans[axis]);
        if (pieces > maxLocalPieces) {
            error = "static union local cut partition exceeds its piece budget";
            return false;
        }
    }
    const auto checkBound = pieces * owners.size();
    if (checkBound > maxOccupancyChecks - stats.occupancyChecksBound) {
        error = "static union exceeds its global occupancy-check budget";
        return false;
    }
    if (pieces > maxCachedPieces - stats.cachedPieces) {
        error = "static union exceeds its cached partition budget";
        return false;
    }
    stats.maximumLocalPieces = std::max(stats.maximumLocalPieces, pieces);
    stats.occupancyChecksBound += checkBound;

    partition.open.resize(pieces);
    std::size_t linear = 0;
    double sum = 0., compensation = 0.;
    Visit({}, spans, [&](const Index& subcell) {
        Bounds piece{};
        double size = 1.;
        for (int axis = 0; axis < 3; ++axis) {
            piece.lo[axis] = edges[axis][subcell[axis]];
            piece.hi[axis] = edges[axis][subcell[axis] + 1];
            if (axis != normal)
                size *= piece.hi[axis] - piece.lo[axis];
        }
        const bool openPiece = std::none_of(owners.begin(), owners.end(), [&](std::size_t owner) {
            return Covers(boxes[owner], piece);
        });
        partition.open[linear++] = openPiece;
        if (openPiece) {
            const double addend = size - compensation;
            const double updated = sum + addend;
            compensation = (updated - sum) - addend;
            sum = updated;
        }
        return true;
    });
    if (normal < 0) {
        auto free = partition.open;
        if (!OneFreeComponent(spans, free)) {
            error = "static union leaves disconnected free pockets in cell (" + std::to_string(index[0]) + "," +
                std::to_string(index[1]) + "," + std::to_string(index[2]) + ")";
            return false;
        }
    }
    if (!moving_geometry_detail::AdmitRoundoff(sum, 1.)) {
        error = "static union free measure is outside the parent control volume";
        return false;
    }
    cacheIndex = stats.partitions.size();
    stats.partitions.push_back(std::move(partition));
    stats.cachedPieces += pieces;
    open = sum;
    return true;
}

bool ValidCell(const GridLayout& layout, const Index& cell) {
    if (layout.CheckedCellCount() == 0)
        return false;
    for (int axis = 0; axis < 3; ++axis)
        if (cell[axis] < 0 || cell[axis] >= layout.cells[axis])
            return false;
    return true;
}

double GridCoordinate(double h, double value, double lo, double hi) {
    if (value <= lo * h)
        return lo;
    if (value >= hi * h)
        return hi;
    return std::clamp(value / h, lo, hi);
}

bool ReadClip(const GridLayout& layout, const Bounds& region, int normal, const GridVec& lo,
              const GridVec& hi, Bounds& clip, std::string& error) {
    clip = region;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == normal)
            continue;
        if (!std::isfinite(lo[axis]) || !std::isfinite(hi[axis]) || lo[axis] > hi[axis]) {
            error = "static union clip bounds are nonfinite or reversed";
            return false;
        }
        clip.lo[axis] = GridCoordinate(layout.h, lo[axis], region.lo[axis], region.hi[axis]);
        clip.hi[axis] = GridCoordinate(layout.h, hi[axis], region.lo[axis], region.hi[axis]);
    }
    return true;
}

bool ReadPartition(const GridStaticUnion& stationary, const std::vector<std::size_t>& indices,
                   std::size_t index, const GridStaticPartition*& partition, std::string& error) {
    if (index < indices.size()) {
        const auto cached = indices[index];
        if (cached == GridStaticUnion::uncut || cached < stationary.partitions.size()) {
            partition = cached == GridStaticUnion::uncut ? nullptr : &stationary.partitions[cached];
            return true;
        }
    }
    error = "static union clip requires an intact cached partition";
    return false;
}

std::size_t Linear(const Index& spans, const Index& piece) {
    return (std::size_t(piece[2]) * std::size_t(spans[1]) + std::size_t(piece[1])) *
        std::size_t(spans[0]) + std::size_t(piece[0]);
}

double ClipMeasure(const GridStaticPartition* partition, const Bounds& clip,
                   int normal, double plane = 0., bool shared = false) {
    if (!partition) {
        double measure = 1.;
        for (int axis = 0; axis < 3; ++axis)
            if (axis != normal)
                measure *= clip.hi[axis] - clip.lo[axis];
        return measure;
    }
    const auto& edges = partition->edges;
    Index spans{};
    for (int axis = 0; axis < 3; ++axis)
        spans[axis] = int(edges[axis].size()) - 1;

    int near = 0, far = 0;
    if (normal >= 0 && !shared) {
        const auto& cut = edges[normal];
        const auto at = std::lower_bound(cut.begin(), cut.end(), plane);
        near = int(at - cut.begin()) - 1;
        far = *at == plane ? near + 1 : near;
    }
    auto limits = spans;
    if (normal >= 0)
        limits[normal] = 1;
    double sum = 0., compensation = 0.;
    Visit({}, limits, [&](Index piece) {
        double measure = 1.;
        for (int axis = 0; axis < 3; ++axis) {
            if (axis != normal) {
                measure *= moving_geometry_detail::Overlap(clip.lo[axis], clip.hi[axis],
                    edges[axis][piece[axis]], edges[axis][piece[axis] + 1]);
            }
        }
        if (normal >= 0) {
            piece[normal] = near;
            if (!partition->open[Linear(spans, piece)])
                return true;
            piece[normal] = far;
        }
        if (partition->open[Linear(spans, piece)]) {
            const double addend = measure - compensation;
            const double updated = sum + addend;
            compensation = (updated - sum) - addend;
            sum = updated;
        }
        return true;
    });
    return sum;
}

} // namespace

bool BuildGridStaticUnion(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                          GridStaticUnion& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0) {
        error = "static union grid is invalid or oversized";
        return false;
    }
    std::vector<BoxMotion> boxes;
    if (!ReadBoxes(layout, stationary, boxes, error))
        return false;

    const auto cell = [&](const Index& i) { return layout.Cell(i[0], i[1], i[2]); };
    std::vector<Owners> bins(count);
    std::size_t references = 0;
    for (std::size_t owner = 0; owner < boxes.size(); ++owner) {
        Index first{}, end{};
        for (int axis = 0; axis < 3; ++axis) {
            first[axis] = int(std::floor(boxes[owner].lo[axis]));
            end[axis] = std::min(layout.cells[axis], int(std::ceil(boxes[owner].hi[axis])));
        }
        if (!Visit(first, end, [&](const Index& i) {
            if (++references > maxBinReferences)
                return false;
            bins[cell(i)].push_back(owner);
            return true;
        })) {
            error = "static union exceeds its spatial-bin reference budget";
            return false;
        }
    }

    GridStaticUnion next;
    next.layout = layout;
    next.stationary = stationary;
    const double area = layout.h * layout.h, volume = area * layout.h;
    next.capacity.resize(count);
    next.cellPartition.assign(count, GridStaticUnion::uncut);
    if (!Visit({}, layout.cells, [&](const Index& i) {
        const auto index = cell(i);
        double available = 0.;
        if (!Measure(i, -1, boxes, bins[index], next, next.cellPartition[index], available, error))
            return false;
        next.capacity[index] = available * volume;
        return true;
    }))
        return false;

    const Owners empty;
    Owners candidates;
    candidates.reserve(boxes.size());
    for (int axis = 0; axis < 3; ++axis) {
        auto& faces = next.aperture[axis];
        faces.resize(layout.FaceCount(axis));
        next.facePartition[axis].assign(faces.size(), GridStaticUnion::uncut);
        auto end = layout.cells;
        ++end[axis];
        if (!Visit({}, end, [&](const Index& i) {
            auto previous = i;
            --previous[axis];
            const auto& left = i[axis] > 0 ? bins[cell(previous)] : empty;
            const auto& right = i[axis] < layout.cells[axis] ? bins[cell(i)] : empty;
            candidates.clear();
            std::set_union(left.begin(), left.end(), right.begin(), right.end(),
                           std::back_inserter(candidates));
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](std::size_t owner) {
                return boxes[owner].lo[axis] > i[axis] || boxes[owner].hi[axis] < i[axis];
            }), candidates.end());
            double open = 0.;
            const auto face = layout.Face(axis, i[0], i[1], i[2]);
            if (!Measure(i, axis, boxes, candidates, next, next.facePartition[axis][face], open, error))
                return false;
            faces[face] = open * area;
            return true;
        }))
            return false;
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool BuildGridStaticMovingUnion(const GridStaticUnion& stationary,
                                const std::vector<BoxMotion>& moving,
                                MovingGridGeometry& output, std::string& error) {
    const auto& layout = stationary.layout;
    if (layout.CheckedCellCount() == 0 || stationary.capacity.size() != layout.CellCount()) {
        error = "static/moving composition requires a built stationary grid";
        return false;
    }
    MovingGridGeometry next;
    if (!BuildMovingGridGeometry(layout, moving, next, error))
        return false;
    for (const auto& wall : stationary.stationary) {
        for (const auto& body : moving) {
            if (moving_geometry_detail::SweptBoxesTouch(wall, body)) {
                error = "moving box touches the stationary union during the swept path";
                return false;
            }
        }
    }

    const auto compose = [](const std::vector<double>& fixed, double full, std::vector<double>& dynamic) {
        if (fixed.size() != dynamic.size())
            return false;
        for (std::size_t i = 0; i < fixed.size(); ++i) {
            dynamic[i] = fixed[i] - (full - dynamic[i]);
            if (!moving_geometry_detail::AdmitRoundoff(dynamic[i], full))
                return false;
        }
        return true;
    };
    const double area = layout.h * layout.h, volume = area * layout.h;
    if (!compose(stationary.capacity, volume, next.startCapacity) ||
        !compose(stationary.capacity, volume, next.endCapacity)) {
        error = "invalid static/moving composed cell volume";
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!compose(stationary.aperture[axis], area, next.startAperture[axis]) ||
            !compose(stationary.aperture[axis], area, next.endAperture[axis]) ||
            !compose(stationary.aperture[axis], area, next.averageAperture[axis])) {
            error = "invalid static/moving composed shared aperture";
            return false;
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool ClipGridStaticCellVolume(const GridStaticUnion& stationary, const Index& cell,
                              const GridVec& clipLo, const GridVec& clipHi,
                              double& volume, std::string& error) {
    const auto& layout = stationary.layout;
    if (!ValidCell(layout, cell)) {
        error = "static union volume clip is outside the grid";
        return false;
    }
    Bounds clip;
    const GridStaticPartition* partition;
    if (!ReadClip(layout, Region(cell, -1), -1, clipLo, clipHi, clip, error) ||
        !ReadPartition(stationary, stationary.cellPartition, layout.Cell(cell[0], cell[1], cell[2]),
                       partition, error))
        return false;

    volume = ClipMeasure(partition, clip, -1) * ((layout.h * layout.h) * layout.h);
    error.clear();
    return true;
}

bool ClipGridStaticFaceArea(const GridStaticUnion& stationary, const Index& cell,
                            int normal, double chartPlane, const GridVec& clipLo,
                            const GridVec& clipHi, double& area, std::string& error) {
    const auto& layout = stationary.layout;
    if (!ValidCell(layout, cell) || normal < 0 || normal > 2) {
        error = "static union face clip has an invalid cell or normal";
        return false;
    }
    const double lower = cell[normal] * layout.h, upper = (cell[normal] + 1) * layout.h;
    if (!(chartPlane >= lower && chartPlane <= upper)) {
        error = "static union face clip plane is outside its coarse cell";
        return false;
    }
    const bool shared = chartPlane == lower || chartPlane == upper;
    const double plane = GridCoordinate(layout.h, chartPlane, cell[normal], cell[normal] + 1.);
    if (!shared && !(plane > cell[normal] && plane < cell[normal] + 1.)) {
        error = "static union interior clip plane is unresolved in grid coordinates";
        return false;
    }
    const GridStaticPartition* partition;
    if (shared) {
        auto face = cell;
        face[normal] += chartPlane == upper;
        const auto index = layout.Face(normal, face[0], face[1], face[2]);
        if (!ReadPartition(stationary, stationary.facePartition[normal], index, partition, error))
            return false;
    } else if (!ReadPartition(stationary, stationary.cellPartition, layout.Cell(cell[0], cell[1], cell[2]),
                              partition, error)) {
        return false;
    }
    Bounds clip;
    if (!ReadClip(layout, Region(cell, -1), normal, clipLo, clipHi, clip, error))
        return false;
    area = ClipMeasure(partition, clip, normal, plane, shared) * (layout.h * layout.h);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
