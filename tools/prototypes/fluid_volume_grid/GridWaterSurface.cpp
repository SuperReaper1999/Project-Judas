#include "GridWaterSurface.h"

#include "GridWaterQuery.h"

namespace judas::volume_grid {
namespace {

using grid_water_query_detail::Rectangle;

constexpr std::size_t kTriangleLimit = 1u << 20;

class SurfaceBuilder {
public:
    explicit SurfaceBuilder(const std::vector<BoxMotion>& stationary) : m_solids(stationary) {}

    bool Add(const Rectangle& region, int normalAxis, int sign) {
        m_fragments.clear();
        m_fragments.push_back(region);
        const double plane = region.lo[normalAxis];
        for (const auto& solid : m_solids) {
            if (plane < solid.lo[normalAxis] || plane > solid.hi[normalAxis])
                continue;
            Rectangle cut = region;
            if (!grid_water_query_detail::Intersect(cut, {solid.lo, solid.hi}, normalAxis))
                continue;
            m_remaining.clear();
            for (const auto& fragment : m_fragments)
                grid_water_query_detail::Subtract(fragment, cut, m_remaining, normalAxis);
            m_fragments.swap(m_remaining);
            if (m_fragments.empty())
                return true;
            if (m_fragments.size() > kTriangleLimit / 2)
                return false;
        }

        if (m_fragments.size() > (kTriangleLimit - surface.triangles.size()) / 2)
            return false;
        const int first = (normalAxis + 1) % 3, second = (normalAxis + 2) % 3;
        GridVec normal{};
        normal[normalAxis] = sign;
        for (const auto& fragment : m_fragments) {
            const double area = (fragment.hi[first] - fragment.lo[first]) *
                                (fragment.hi[second] - fragment.lo[second]);
            if (!(area > 0.) || !std::isfinite(area))
                return false;
            GridVec a = fragment.lo, b = fragment.hi;
            a[first] = fragment.hi[first];
            b[first] = fragment.lo[first];
            if (sign < 0)
                std::swap(a, b);
            surface.triangles.push_back({{fragment.lo, a, fragment.hi}, normal});
            surface.triangles.push_back({{fragment.lo, fragment.hi, b}, normal});
        }
        return true;
    }

    GridWaterSurface surface;

private:
    const std::vector<BoxMotion>& m_solids;
    std::vector<Rectangle> m_fragments, m_remaining;
};

} // namespace

bool BuildGridWaterSurface(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                           const GridCellPhase& phase, GridWaterSurface& output,
                           std::string& error) {
    const int up = phase.axis;
    std::vector<BoxMotion> validated;
    if (!grid_phase_detail::Prepare(layout, stationary, up, validated, error))
        return false;
    const auto count = layout.CellCount();
    if (phase.waterPlane.size() != count || phase.capacity.size() != count) {
        error = "water surface phase does not match the grid";
        return false;
    }
    const double cellVolume = (layout.h * layout.h) * layout.h;
    const auto nx = std::size_t(layout.cells[0]), ny = std::size_t(layout.cells[1]);
    for (std::size_t index = 0; index < count; ++index) {
        const std::array<std::size_t, 3> cell{index % nx, index / nx % ny, index / (nx * ny)};
        const double lo = cell[up] * layout.h, hi = (cell[up] + 1) * layout.h;
        const double top = phase.waterPlane[index], capacity = phase.capacity[index];
        if (!(top >= lo && top <= hi) || !(capacity >= 0. && capacity <= cellVolume) ||
            (capacity == 0. && top != lo)) {
            error = "invalid water surface cell plane or capacity";
            return false;
        }
    }
    SurfaceBuilder builder(stationary);
    const auto add = [&](const Rectangle& region, int normalAxis, int sign) {
        if (builder.Add(region, normalAxis, sign))
            return true;
        error = "water surface triangles exceed the mesh bound or numerical range";
        return false;
    };
    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = 0; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                const Rectangle cell{{i * layout.h, j * layout.h, k * layout.h},
                                     {(i + 1) * layout.h, (j + 1) * layout.h, (k + 1) * layout.h}};
                const auto index = layout.Cell(i, j, k);
                const double top = phase.waterPlane[index];
                if (top > cell.lo[up] && top < cell.hi[up]) {
                    Rectangle topFace = cell;
                    topFace.lo[up] = topFace.hi[up] = top;
                    if (!add(topFace, up, 1))
                        return false;
                }

                // Visit each interior shared face exactly once. Only a
                // disagreement of the two material traces exposes a surface.
                for (int normal = 0; normal < 3; ++normal) {
                    std::array<int, 3> neighbor{i, j, k};
                    if (++neighbor[normal] == layout.cells[normal])
                        continue;
                    const auto next = layout.Cell(neighbor[0], neighbor[1], neighbor[2]);
                    const double nextTop = phase.waterPlane[next];
                    const double face = cell.hi[normal];
                    Rectangle shared = cell;
                    shared.lo[normal] = face;
                    if (normal == up) {
                        const bool leftWet = top == face, rightWet = nextTop > face;
                        if (leftWet != rightWet && !add(shared, normal, leftWet ? 1 : -1))
                            return false;
                    } else if (top != nextTop) {
                        shared.lo[up] = std::min(top, nextTop);
                        shared.hi[up] = std::max(top, nextTop);
                        if (!add(shared, normal, top > nextTop ? 1 : -1))
                            return false;
                    }
                }
            }
        }
    }
    output = std::move(builder.surface);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
