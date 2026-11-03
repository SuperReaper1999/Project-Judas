#include "GridOrientedGeometryInternal.h"

#include "GridPhaseGeometry.h"

namespace judas::volume_grid {
namespace {

using moving_geometry_detail::Index;
using Vertices = std::vector<GridVec>;

GridVec operator+(const GridVec& a, const GridVec& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
GridVec operator-(const GridVec& a, const GridVec& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
GridVec operator*(const GridVec& a, double s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}
double Dot(const GridVec& a, const GridVec& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}
GridVec Unit(int axis, double sign = 1.) {
    GridVec result{};
    result[axis] = sign;
    return result;
}
bool Finite(const GridVec& a) {
    return std::all_of(a.begin(), a.end(), [](double x) { return std::isfinite(x); });
}
GridVec Average(const Vertices& points) {
    GridVec average{};
    for (const auto& p : points)
        average = average + p;
    return average * (1. / points.size());
}

struct Plane {
    GridVec normal;
    double offset;
    int axis = -1; // Snap intersections to exact coordinate planes.
};

Plane AxisPlane(int axis, double coordinate, double sign = 1.) {
    return {Unit(axis, sign), sign * coordinate, axis};
}

struct Polygon {
    Vertices vertices;
    GridVec normal;
    bool solidWall = false;
};
using Polyhedron = std::vector<Polygon>;

struct Box {
    GridOrientedBox body; // Center and half extents in grid units.
    GridVec extent;
};

double RadiusOn(const GridOrientedBox& box, const GridVec& axis) {
    double radius = 0.;
    for (int a = 0; a < 3; ++a)
        radius += box.half[a] * std::abs(Dot(box.axes[a], axis));
    return radius;
}

bool Disjoint(const GridOrientedBox& a, const GridOrientedBox& b) {
    const GridVec delta = b.center - a.center;
    const auto separates = [&](GridVec direction) {
        const double norm = std::sqrt(Dot(direction, direction));
        if (norm < 32. * std::numeric_limits<double>::epsilon())
            return false;
        direction = direction * (1. / norm);
        const double distance = std::abs(Dot(delta, direction));
        const double extent = RadiusOn(a, direction) + RadiusOn(b, direction);
        const double rounding = 64. * std::numeric_limits<double>::epsilon() *
                                (1. + distance + extent);
        return distance - extent > rounding;
    };
    for (const auto& axis : a.axes)
        if (separates(axis))
            return true;
    for (const auto& axis : b.axes)
        if (separates(axis))
            return true;
    for (const auto& first : a.axes)
        for (const auto& second : b.axes)
            if (separates(Cross(first, second)))
                return true;
    return false;
}

bool Prepare(const GridLayout& layout, const std::vector<GridOrientedBox>& input,
             std::vector<Box>& boxes, std::string& error) {
    if (input.size() > 256) {
        error = "too many static oriented boxes";
        return false;
    }
    constexpr double frameTolerance = 128. * std::numeric_limits<double>::epsilon();
    boxes.reserve(input.size());
    for (auto box : input) {
        if (!Finite(box.center) || !Finite(box.half)) {
            error = "nonfinite oriented box center or extent";
            return false;
        }
        for (int a = 0; a < 3; ++a) {
            if (!Finite(box.axes[a]) || !(box.half[a] > 0.) ||
                std::abs(Dot(box.axes[a], box.axes[a]) - 1.) > frameTolerance) {
                error = "oriented box requires positive extents and unit axes";
                return false;
            }
            for (int b = 0; b < a; ++b) {
                if (std::abs(Dot(box.axes[a], box.axes[b])) > frameTolerance) {
                    error = "oriented box requires perpendicular axes";
                    return false;
                }
            }
        }
        if (Dot(Cross(box.axes[0], box.axes[1]), box.axes[2]) <= 0.) {
            error = "oriented box requires a right-handed frame";
            return false;
        }
        box.center = box.center * (1. / layout.h);
        box.half = box.half * (1. / layout.h);
        GridVec extent{};
        for (int a = 0; a < 3; ++a) {
            extent[a] = RadiusOn(box, Unit(a));
            if (!(box.half[a] > 0. && box.center[a] - extent[a] > 0. &&
                  box.center[a] + extent[a] < layout.cells[a])) {
                error = "oriented box must have representable extent inside the grid";
                return false;
            }
        }
        for (const auto& existing : boxes) {
            if (box.owner == existing.body.owner || !Disjoint(box, existing.body)) {
                error = "oriented boxes must have unique owners and provably disjoint geometry";
                return false;
            }
        }
        boxes.push_back({box, extent});
    }
    return true;
}

template <typename Visit>
void VisitBounds(const GridLayout& layout, const Box& box, int faceAxis, Visit visit) {
    Index first{}, stop{};
    GridVec lower{}, upper{};
    for (int axis = 0; axis < 3; ++axis) {
        const double margin = 16. * std::numeric_limits<double>::epsilon() *
                              (1. + box.body.center[axis] + box.extent[axis]);
        lower[axis] = box.body.center[axis] - box.extent[axis] - margin;
        upper[axis] = box.body.center[axis] + box.extent[axis] + margin;
        first[axis] = std::max(0, int(std::ceil(lower[axis])) - (axis != faceAxis));
        stop[axis] = std::min(layout.cells[axis] + (axis == faceAxis), int(std::floor(upper[axis])) + 1);
    }
    for (int k = first[2]; k < stop[2]; ++k)
        for (int j = first[1]; j < stop[1]; ++j)
            for (int i = first[0]; i < stop[0]; ++i)
                visit(Index{i, j, k});
}

Vertices Rectangle(const GridVec& center, const GridVec& a, const GridVec& b) {
    return {center - a - b, center + a - b, center + a + b, center - a + b};
}

Polyhedron OriginalBox(const GridOrientedBox& box) {
    Polyhedron poly;
    poly.reserve(6);
    for (int axis = 0; axis < 3; ++axis) {
        const int a = (axis + 1) % 3, b = (axis + 2) % 3;
        for (double sign : {-1., 1.}) {
            const auto normal = box.axes[axis] * sign;
            poly.push_back({Rectangle(normal * box.half[axis], box.axes[a] * box.half[a],
                                      box.axes[b] * (sign * box.half[b])), normal, true});
        }
    }
    return poly;
}

Vertices ClipPolygon(const Vertices& polygon, const Plane& plane, Vertices* crossings = nullptr) {
    Vertices clipped;
    if (polygon.empty())
        return clipped;
    clipped.reserve(polygon.size() + 1);
    auto previous = polygon.back();
    double before = Dot(plane.normal, previous) - plane.offset;
    for (const auto& point : polygon) {
        const double after = Dot(plane.normal, point) - plane.offset;
        if ((before < 0. && after > 0.) || (before > 0. && after < 0.)) {
            auto intersection = previous + (point - previous) * (before / (before - after));
            if (plane.axis >= 0)
                intersection[plane.axis] = plane.offset * plane.normal[plane.axis];
            clipped.push_back(intersection);
            if (crossings)
                crossings->push_back(intersection);
        }
        if (after <= 0.) {
            clipped.push_back(point);
            if (after == 0. && crossings)
                crossings->push_back(point);
        }
        previous = point;
        before = after;
    }
    return clipped;
}

void ClipPolyhedron(Polyhedron& poly, const Plane& plane) {
    bool inner = false, outer = false;
    for (const auto& face : poly) {
        for (const auto& p : face.vertices) {
            const double distance = Dot(plane.normal, p) - plane.offset;
            inner |= distance < 0.;
            outer |= distance > 0.;
        }
    }
    if (!outer)
        return;
    Vertices intersections;
    for (auto& face : poly)
        face.vertices = ClipPolygon(face.vertices, plane, inner ? &intersections : nullptr);
    poly.erase(std::remove_if(poly.begin(), poly.end(), [](const auto& face) {
        return face.vertices.size() < 3;
    }), poly.end());
    if (!inner || intersections.size() < 3)
        return;

    // Clipping shared edges in opposite directions can produce slightly
    // different intersections. Merge only within coordinate roundoff.
    Vertices cap;
    for (const auto& p : intersections) {
        const double scale = std::max({1., std::abs(p[0]), std::abs(p[1]), std::abs(p[2])});
        const double tolerance = 32. * std::numeric_limits<double>::epsilon() * scale;
        if (std::none_of(cap.begin(), cap.end(), [&](const auto& q) {
                const auto d = p - q;
                return Dot(d, d) <= tolerance * tolerance;
            }))
            cap.push_back(p);
    }
    if (cap.size() < 3)
        return;
    const auto center = Average(cap);
    const auto tangent = Unit((plane.axis + 1) % 3);
    const auto bitangent = Cross(plane.normal, tangent);
    const auto angle = [&](const auto& p) {
        const auto relative = p - center;
        return std::atan2(Dot(relative, bitangent), Dot(relative, tangent));
    };
    std::sort(cap.begin(), cap.end(), [&](const auto& a, const auto& b) { return angle(a) < angle(b); });
    poly.push_back({std::move(cap), plane.normal, false});
}

void ClipWallPolygons(Polyhedron& poly, const Plane& plane) {
    for (auto& face : poly) {
        if (std::any_of(face.vertices.begin(), face.vertices.end(), [&](const auto& point) {
                return Dot(plane.normal, point) > plane.offset;
            }))
            face.vertices = ClipPolygon(face.vertices, plane);
    }
    poly.erase(std::remove_if(poly.begin(), poly.end(), [](const auto& face) {
        return face.vertices.size() < 3;
    }), poly.end());
}

double Volume(const Polyhedron& poly) {
    GridVec center{};
    std::size_t vertices = 0;
    for (const auto& face : poly) {
        for (const auto& p : face.vertices) {
            center = center + p;
            ++vertices;
        }
    }
    if (vertices == 0)
        return 0.;
    center = center * (1. / vertices);
    double volume = 0.;
    for (const auto& face : poly) {
        const auto& p = face.vertices;
        const auto a = p[0] - center;
        for (std::size_t i = 2; i < p.size(); ++i)
            volume += Dot(a, Cross(p[i - 1] - center, p[i] - center)) / 6.;
    }
    return std::max(0., volume);
}

double TriangleArea(const GridVec& a, const GridVec& b, const GridVec& c, const GridVec& normal) {
    return .5 * Dot(Cross(b - a, c - a), normal);
}

double Area(const Vertices& points, const GridVec& normal) {
    double area = 0.;
    for (std::size_t i = 2; i < points.size(); ++i)
        area += TriangleArea(points[0], points[i - 1], points[i], normal);
    return std::max(0., area);
}

GridVec FaceFirstMoment(const Vertices& points, const GridVec& normal, const GridVec& origin) {
    GridVec moment{};
    for (std::size_t i = 2; i < points.size(); ++i) {
        const double area = TriangleArea(points[0], points[i - 1], points[i], normal);
        const auto centroid = (points[0] + points[i - 1] + points[i]) * (1. / 3.);
        moment = moment + (origin + centroid) * area;
    }
    return moment;
}

bool OwnedByCell(const Polygon& face, const GridOrientedBox& box, const Index& cell) {
    for (int axis = 0; axis < 3; ++axis) {
        if (face.normal[axis] == 0.)
            continue;
        const double outwardNeighbor = cell[axis] + double(face.normal[axis] > 0.);
        const double shared = outwardNeighbor - box.center[axis];
        if (std::all_of(face.vertices.begin(), face.vertices.end(),
                        [&](const auto& p) { return p[axis] == shared; }))
            return false;
    }
    return true;
}

bool AppendPatches(const Polyhedron& poly, const GridLayout& layout, const GridOrientedBox& box,
                   const Index& cell, std::vector<GridOrientedPatch>& output, bool secondMoment) {
    const double scale = layout.h * layout.h;
    for (const auto& face : poly) {
        if (!face.solidWall || !OwnedByCell(face, box, cell))
            continue;
        const auto& p = face.vertices;
        double area = 0.;
        GridVec center{};
        for (std::size_t i = 2; i < p.size(); ++i) {
            const double piece = TriangleArea(p[0], p[i - 1], p[i], face.normal);
            center = center + (p[0] + p[i - 1] + p[i]) * (piece / 3.);
            area += piece;
        }
        if (!(area > 0.))
            continue;
        center = center * (1. / area);
        GridOrientedPatch patch{box.owner, layout.Cell(cell[0], cell[1], cell[2]), face.normal,
                                (box.center + center) * layout.h, area * scale, {}};
        for (std::size_t i = 2; secondMoment && i < p.size(); ++i) {
            const double piece = TriangleArea(p[0], p[i - 1], p[i], face.normal);
            const std::array<GridVec, 3> relative{
                (p[0] - center) * layout.h, (p[i - 1] - center) * layout.h,
                (p[i] - center) * layout.h};
            const auto sum = relative[0] + relative[1] + relative[2];
            for (int a = 0; a < 3; ++a) {
                for (int b = 0; b < 3; ++b) {
                    double product = sum[a] * sum[b];
                    for (const auto& vertex : relative)
                        product += vertex[a] * vertex[b];
                    patch.centeredMoment[a][b] += ((piece * scale) / 12.) * product;
                }
            }
        }
        if (!std::isfinite(patch.area) || !Finite(patch.centroid) ||
            !std::all_of(patch.centeredMoment.begin(), patch.centeredMoment.end(), Finite))
            return false;
        output.push_back(patch);
    }
    return true;
}

bool AdmitMeasures(std::vector<double>& values, double maximum) {
    for (auto& value : values)
        if (!moving_geometry_detail::AdmitRoundoff(value, maximum))
            return false;
    return true;
}

} // namespace

static bool BuildGeometry(const GridLayout& layout, const std::vector<GridOrientedBox>& input,
                          GridOrientedGeometry& output, std::string& error,
                          std::optional<GridOrientedPlane> wet, bool sweepOnly,
                          oriented_geometry_detail::FaceMoments* blockedArm) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || (wet && (wet->axis < 0 || wet->axis > 2))) {
        error = "invalid oriented cut grid or phase axis";
        return false;
    }
    if (wet && !grid_phase_detail::HeightInGridUnits(layout, wet->axis, wet->height,
                                                     wet->height, error))
        return false;
    std::vector<Box> boxes;
    if (!Prepare(layout, input, boxes, error))
        return false;

    const double area = layout.h * layout.h, volume = area * layout.h;
    GridOrientedGeometry next;
    if (!sweepOnly)
        next.capacity.assign(count, volume);
    if (wet) {
        next.wetCapacity.resize(count);
        grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
            next.wetCapacity[layout.Cell(cell[0], cell[1], cell[2])] =
                volume * std::clamp(wet->height - cell[wet->axis], 0., 1.);
        });
    }
    const auto wetFraction = [&](int normal, const Index& face) {
        if (normal == wet->axis)
            return double(grid_phase_detail::PlaneIsWet(face[normal], wet->height,
                                                        layout.cells[normal]));
        return std::clamp(wet->height - face[wet->axis], 0., 1.);
    };
    oriented_geometry_detail::FaceMoments moments;
    for (int normal = 0; normal < 3; ++normal) {
        next.aperture[normal].assign(layout.FaceCount(normal), area);
        if (blockedArm)
            moments[normal].resize(layout.FaceCount(normal));
        if (wet) {
            next.wetAperture[normal].resize(layout.FaceCount(normal));
            grid_phase_detail::VisitGrid(layout, normal, [&](const Index& face) {
                next.wetAperture[normal][layout.Face(normal, face[0], face[1], face[2])] =
                    area * wetFraction(normal, face);
            });
        }
    }

    bool validPatches = true;
    const auto clip = sweepOnly ? ClipWallPolygons : ClipPolyhedron;
    for (const auto& box : boxes) {
        const auto original = OriginalBox(box.body);
        VisitBounds(layout, box, -1, [&](const Index& cell) {
            if (!validPatches)
                return;
            auto clipped = original;
            for (int axis = 0; axis < 3 && !clipped.empty(); ++axis) {
                clip(clipped, AxisPlane(axis, cell[axis] - box.body.center[axis], -1.));
                clip(clipped, AxisPlane(axis, cell[axis] + 1. - box.body.center[axis]));
            }
            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            if (!sweepOnly)
                next.capacity[index] -= volume * Volume(clipped);
            validPatches = AppendPatches(clipped, layout, box.body, cell, next.patches, !sweepOnly);
            if (wet && validPatches) {
                const double plane = wet->height - box.body.center[wet->axis];
                ClipPolyhedron(clipped, AxisPlane(wet->axis, plane));
                next.wetCapacity[index] -= volume * Volume(clipped);
                // A solid facet exactly at the water plane is owned by the
                // dry phase. The artificial water cap is never a solid wall.
                for (auto& face : clipped) {
                    if (std::all_of(face.vertices.begin(), face.vertices.end(),
                                    [&](const auto& p) { return p[wet->axis] == plane; }))
                        face.solidWall = false;
                }
                validPatches = AppendPatches(clipped, layout, box.body, cell, next.wetPatches, true);
            }
        });
        if (!validPatches) {
            error = "oriented pressure patch measures are not representable";
            return false;
        }

        for (int normal = 0; normal < 3; ++normal) {
            VisitBounds(layout, box, normal, [&](const Index& face) {
                const GridVec origin{double(face[0]), double(face[1]), double(face[2])};
                const int a = (normal + 1) % 3, b = (normal + 2) % 3;
                auto polygon = Rectangle((Unit(a) + Unit(b)) * .5, Unit(a, .5), Unit(b, .5));
                const auto relative = box.body.center - origin;
                for (int axis = 0; axis < 3 && polygon.size() >= 3; ++axis) {
                    for (double sign : {-1., 1.}) {
                        const auto n = box.body.axes[axis] * sign;
                        polygon = ClipPolygon(polygon, {n, box.body.half[axis] + Dot(n, relative)});
                    }
                }
                const auto index = layout.Face(normal, face[0], face[1], face[2]);
                next.aperture[normal][index] -= area * Area(polygon, Unit(normal));
                if (blockedArm)
                    moments[normal][index] = FaceFirstMoment(polygon, Unit(normal), origin - box.body.center);
                if (wet && wetFraction(normal, face) > 0.) {
                    polygon = ClipPolygon(polygon, AxisPlane(wet->axis, wet->height - origin[wet->axis]));
                    next.wetAperture[normal][index] -= area * Area(polygon, Unit(normal));
                }
            });
        }
    }

    if (!AdmitMeasures(next.capacity, volume) || !AdmitMeasures(next.wetCapacity, volume)) {
        error = "oriented cut capacity is outside cell volume";
        return false;
    }
    for (int normal = 0; normal < 3; ++normal) {
        if (!AdmitMeasures(next.aperture[normal], area) || !AdmitMeasures(next.wetAperture[normal], area)) {
            error = "oriented cut aperture is outside face area";
            return false;
        }
    }
    output = std::move(next);
    if (blockedArm)
        *blockedArm = std::move(moments);
    error.clear();
    return true;
}

bool BuildGridOrientedGeometry(const GridLayout& layout, const std::vector<GridOrientedBox>& boxes,
                               GridOrientedGeometry& output, std::string& error,
                               std::optional<GridOrientedPlane> wet) {
    return BuildGeometry(layout, boxes, output, error, wet, false, nullptr);
}

bool oriented_geometry_detail::BuildSweepSample(const GridLayout& layout,
                                                const GridOrientedBox& box,
                                                GridOrientedGeometry& output,
                                                FaceMoments* blockedArm,
                                                std::string& error) {
    return BuildGeometry(layout, {box}, output, error, std::nullopt, true, blockedArm);
}

} // namespace judas::volume_grid
