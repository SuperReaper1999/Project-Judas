#include "GridParcelSeed.h"

#include <cfenv>

namespace judas::volume_grid {
namespace {

// A short nonoverlapping expansion keeps the exact binary residual of each
// source. Adding a rounded amount to an already seeded cell may transfer
// less than requested. Retain that residual for another destination.
class ExactVolume {
public:
    void Add(double value) {
        std::size_t written = 0;
        for (const double part : parts_) {
            const double sum = value + part;
            const double partVirtual = sum - value;
            const double residual = (value - (sum - partVirtual)) + (part - partVirtual);
            if (residual != 0.)
                parts_[written++] = residual;
            value = sum;
        }
        parts_.resize(written);
        if (value != 0.)
            parts_.push_back(value);
    }

    bool Empty() const { return parts_.empty(); }
    bool Negative() const { return !Empty() && parts_.back() < 0.; }

    double Rounded() const {
        double value = 0.;
        for (const double part : parts_)
            value += part;
        return value;
    }

private:
    std::vector<double> parts_;
};

bool Deposit(double capacity, double& water, ExactVolume& remaining) {
    if (water == capacity)
        return false;
    const auto residualAt = [&](double endpoint) {
        auto residual = remaining;
        residual.Add(water);
        residual.Add(-endpoint);
        return residual;
    };

    double endpoint = std::min(capacity, water + remaining.Rounded());
    auto residual = residualAt(endpoint);
    while (residual.Negative()) {
        endpoint = std::nextafter(endpoint, water);
        residual = residualAt(endpoint);
    }
    // The estimated sum can round down by one representable endpoint.
    const double above = std::nextafter(endpoint, capacity);
    if (above > endpoint) {
        auto nextResidual = residualAt(above);
        if (!nextResidual.Negative()) {
            endpoint = above;
            residual = std::move(nextResidual);
        }
    }
    if (endpoint == water)
        return false;
    water = endpoint;
    remaining = std::move(residual);
    return true;
}

template<typename Visit>
void VisitNeighbors(const GridLayout& layout, const MovingGridGeometry& geometry,
                    std::size_t cell, const Visit& visit) {
    const auto width = std::size_t(layout.cells[0]);
    const auto height = std::size_t(layout.cells[1]);
    const std::array<int, 3> index{int(cell % width), int((cell / width) % height),
                                   int((cell / width) / height)};
    for (int axis = 0; axis < 3; ++axis) {
        for (const int direction : {-1, 1}) {
            auto neighbor = index;
            neighbor[axis] += direction;
            if (neighbor[axis] < 0 || neighbor[axis] >= layout.cells[axis])
                continue;
            auto face = index;
            if (direction > 0)
                ++face[axis];
            const auto aperture = geometry.startAperture[axis][layout.Face(axis, face[0], face[1], face[2])];
            if (aperture > 0.)
                visit(layout.Cell(neighbor[0], neighbor[1], neighbor[2]));
        }
    }
}

} // namespace

bool ImportGridParcelSeed(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                          const std::vector<double>& authoredWater, int maxGraphRadius,
                          GridParcelSeed& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || authoredWater.size() != count || maxGraphRadius < -1 ||
        std::fegetround() != FE_TONEAREST) {
        error = "parcel seed requires a matching grid, valid radius and nearest rounding";
        return false;
    }
    const double cellVolume = (layout.h * layout.h) * layout.h;
    for (double seed : authoredWater) {
        if (!(seed >= 0. && seed <= cellVolume)) {
            error = "authored parcel water is outside the cell volume";
            return false;
        }
    }
    for (const auto& box : stationary) {
        if (box.translation != GridVec{}) {
            error = "parcel seed requires stationary solid geometry";
            return false;
        }
    }
    MovingGridGeometry geometry;
    if (!BuildMovingGridGeometry(layout, stationary, geometry, error))
        return false;

    GridParcelSeed next;
    next.water = authoredWater;
    std::vector<std::size_t> sources;
    ExactVolume moved;
    for (std::size_t cell = 0; cell < count; ++cell) {
        const double capacity = geometry.startCapacity[cell];
        if (authoredWater[cell] <= capacity)
            continue;
        if (capacity == 0.) {
            error = "positive parcel seed lies in a completely solid cell";
            return false;
        }
        next.water[cell] = capacity;
        moved.Add(authoredWater[cell]);
        moved.Add(-capacity);
        sources.push_back(cell);
    }

    if (!sources.empty()) {
        std::vector<std::size_t> visited(count, count), frontier, neighbors;
        for (const auto source : sources) {
            ExactVolume remaining;
            remaining.Add(authoredWater[source]);
            remaining.Add(-geometry.startCapacity[source]);
            frontier.assign(1, source);
            visited[source] = source;
            std::size_t radius = 0;
            while (!remaining.Empty() && !frontier.empty() &&
                   (maxGraphRadius < 0 || radius < std::size_t(maxGraphRadius))) {
                ++radius;
                neighbors.clear();
                for (const auto cell : frontier) {
                    VisitNeighbors(layout, geometry, cell, [&](std::size_t neighbor) {
                        if (visited[neighbor] == source || geometry.startCapacity[neighbor] == 0.)
                            return;
                        visited[neighbor] = source;
                        neighbors.push_back(neighbor);
                    });
                }
                std::sort(neighbors.begin(), neighbors.end());
                for (const auto cell : neighbors) {
                    if (Deposit(geometry.startCapacity[cell], next.water[cell], remaining))
                        next.maximumGraphRadius = std::max(next.maximumGraphRadius, radius);
                    if (remaining.Empty())
                        break;
                }
                frontier.swap(neighbors);
            }
            if (!remaining.Empty()) {
                error = "parcel seed cannot place its full excess in reachable free capacity";
                return false;
            }
        }
    }

    next.movedVolume = moved.Rounded();
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
