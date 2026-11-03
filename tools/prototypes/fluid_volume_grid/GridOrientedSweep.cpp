#include "GridOrientedSweep.h"

#include "GridOrientedGeometryInternal.h"

namespace judas::volume_grid {
namespace {

using Field = std::vector<double>;

constexpr double quadratureTolerance = 2e-9;
constexpr double maximumQuadratureEstimate = 2e-7;
constexpr double conservationTolerance = 1e-7;
constexpr std::size_t maximumCells = 1u << 15;
constexpr std::size_t maximumEvaluations = 2048;

double Dot(const GridVec& a, const GridVec& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}
GridVec Scale(const GridVec& a, double s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}
GridVec Difference(const GridVec& a, const GridVec& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
bool Finite(const GridVec& a) {
    return std::all_of(a.begin(), a.end(), [](double x) { return std::isfinite(x); });
}

bool Stationary(const GridOrientedMotion& motion) {
    return motion.angle == 0. && motion.translation == GridVec{};
}

GridOrientedBox PoseAt(const GridOrientedMotion& motion, double time) {
    auto box = motion.start;
    for (int a = 0; a < 3; ++a)
        box.center[a] = std::fma(time, motion.translation[a], box.center[a]);
    if (motion.angle != 0. && time != 0.) {
        const auto& axis = motion.rotationAxis;
        const double angle = time * motion.angle, sine = std::sin(angle), cosine = std::cos(angle);
        for (auto& direction : box.axes) {
            const auto tangent = Cross(axis, direction);
            const double normal = Dot(axis, direction);
            for (int a = 0; a < 3; ++a)
                direction[a] = cosine * direction[a] + sine * tangent[a] +
                               (1. - cosine) * normal * axis[a];
        }
    }
    return box;
}

bool AdmitMotion(const GridLayout& layout, const GridOrientedMotion& motion, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || count > maximumCells || !Finite(motion.translation) ||
        !Finite(motion.rotationAxis) || !std::isfinite(motion.angle)) {
        error = "invalid or oversized oriented sweep";
        return false;
    }
    if (motion.angle != 0. &&
        std::abs(Dot(motion.rotationAxis, motion.rotationAxis) - 1.) >
            128. * std::numeric_limits<double>::epsilon()) {
        error = "oriented sweep rotation axis must be unit length";
        return false;
    }

    const double radius = std::sqrt(Dot(motion.start.half, motion.start.half));
    const double angularReach = std::abs(motion.angle) * radius;
    const double travel = std::sqrt(Dot(motion.translation, motion.translation)) + angularReach;
    if (!(travel <= layout.h) || std::abs(motion.angle) > std::acos(-1.) / 4.) {
        error = "oriented sweep exceeds the admitted per-step travel or rotation";
        return false;
    }

    const auto end = PoseAt(motion, 1.);
    for (int a = 0; a < 3; ++a) {
        double extent = angularReach;
        for (int local = 0; local < 3; ++local)
            extent += std::abs(motion.start.axes[local][a]) * motion.start.half[local];
        if (!(std::min(motion.start.center[a], end.center[a]) - extent > 0. &&
              std::max(motion.start.center[a], end.center[a]) + extent < layout.cells[a] * layout.h)) {
            error = "oriented sweep cannot certify the entire solid path inside the grid";
            return false;
        }
    }
    return true;
}

std::vector<double> SeedIntervals(const GridLayout& layout, const GridOrientedMotion& motion) {
    std::vector<double> times{0., .25, .5, .75, 1.};
    if (motion.angle != 0.)
        return times;

    // Vertex plane crossings include the aperture jumps when an axis-aligned
    // wall becomes coplanar with a shared face. The open quadrature never
    // samples the discontinuity itself.
    for (const int x : {-1, 1}) {
        for (const int y : {-1, 1}) {
            for (const int z : {-1, 1}) {
                const int sign[3]{x, y, z};
                for (int a = 0; a < 3; ++a) {
                    const double displacement = motion.translation[a];
                    if (displacement == 0.)
                        continue;
                    double point = motion.start.center[a];
                    for (int local = 0; local < 3; ++local)
                        point += sign[local] * motion.start.axes[local][a] * motion.start.half[local];
                    const int first = int(std::floor(std::min(point, point + displacement) / layout.h));
                    const int stop = int(std::ceil(std::max(point, point + displacement) / layout.h));
                    for (int i = first; i <= stop; ++i) {
                        const double time = (i * layout.h - point) / displacement;
                        if (time > 0. && time < 1.)
                            times.push_back(time);
                    }
                }
            }
        }
    }
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    return times;
}

struct FieldLayout {
    std::array<std::size_t, 3> offset{};
    std::size_t faces = 0, size = 0;

    explicit FieldLayout(const GridLayout& layout) {
        for (int a = 0; a < 3; ++a) {
            offset[a] = faces;
            faces += layout.FaceCount(a);
        }
        size = 2 * faces + 6 * layout.CellCount();
    }

    std::size_t Aperture(int a, std::size_t face) const { return offset[a] + face; }
    std::size_t Sweep(int a, std::size_t face) const { return faces + offset[a] + face; }
    std::size_t Wall(std::size_t cell) const { return 2 * faces + 6 * cell; }
};

class Integrator {
public:
    Integrator(const GridLayout& grid, const GridOrientedMotion& path, std::string& error)
        : grid_(grid), path_(path), error_(error), field_(grid), integral_(field_.size) {}

    bool Run() {
        if (Stationary(path_))
            return Sample(0., 1., integral_);
        const auto times = SeedIntervals(grid_, path_);
        for (std::size_t i = 1; i < times.size(); ++i) {
            const double lower = times[i - 1], upper = times[i];
            Field coarse;
            if (!Gauss(lower, upper, coarse) || !Refine(lower, upper, coarse, 0))
                return false;
        }
        if (estimate_ > maximumQuadratureEstimate) {
            error_ = "oriented sweep accumulated too much quadrature uncertainty";
            return false;
        }
        return true;
    }

    const Field& Integral() const { return integral_; }
    const FieldLayout& Indices() const { return field_; }
    double Estimate() const { return estimate_; }
    std::size_t Evaluations() const { return evaluations_; }

private:
    bool Sample(double time, double weight, Field& result) {
        if (evaluations_ >= maximumEvaluations) {
            error_ = "oriented sweep quadrature work limit reached";
            return false;
        }
        ++evaluations_;
        const auto box = PoseAt(path_, time);
        GridOrientedGeometry geometry;
        oriented_geometry_detail::FaceMoments blockedArm;
        if (!oriented_geometry_detail::BuildSweepSample(grid_, box, geometry,
                path_.angle != 0. ? &blockedArm : nullptr, error_))
            return false;

        const double area = grid_.h * grid_.h, volume = area * grid_.h;
        const auto angular = Scale(path_.rotationAxis, path_.angle);
        for (int a = 0; a < 3; ++a) {
            for (std::size_t face = 0; face < grid_.FaceCount(a); ++face) {
                const double blocked = (area - geometry.aperture[a][face]) / area;
                double swept = blocked * (path_.translation[a] / grid_.h);
                if (path_.angle != 0.)
                    swept += Cross(angular, blockedArm[a][face])[a];
                result[field_.Aperture(a, face)] += weight * blocked;
                result[field_.Sweep(a, face)] += weight * swept;
            }
        }
        for (const auto& patch : geometry.patches) {
            const auto torque = Cross(Difference(patch.centroid, box.center), patch.normal);
            const auto base = field_.Wall(patch.cell);
            for (int a = 0; a < 3; ++a) {
                result[base + a] += weight * (patch.area / area) * patch.normal[a];
                result[base + 3 + a] += weight * (patch.area / volume) * torque[a];
            }
        }
        return true;
    }

    bool Gauss(double lower, double upper, Field& result) {
        const double half = .5 * (upper - lower), center = lower + half;
        const double offset = half / std::sqrt(3.);
        result.resize(field_.size);
        return Sample(center - offset, half, result) && Sample(center + offset, half, result);
    }

    bool Refine(double lower, double upper, const Field& coarse, int depth) {
        const double middle = lower + .5 * (upper - lower);
        Field left, right;
        if (!Gauss(lower, middle, left) || !Gauss(middle, upper, right))
            return false;
        double error = 0.;
        for (std::size_t i = 0; i < coarse.size(); ++i)
            error = std::max(error, std::abs(coarse[i] - (left[i] + right[i])));
        if (error <= quadratureTolerance) {
            estimate_ += error;
            for (std::size_t i = 0; i < integral_.size(); ++i)
                integral_[i] += left[i] + right[i];
            return true;
        }
        if (depth == 16 || middle == lower || middle == upper) {
            error_ = "oriented sweep quadrature cannot resolve a geometry event";
            return false;
        }
        return Refine(lower, middle, left, depth + 1) && Refine(middle, upper, right, depth + 1);
    }

    const GridLayout& grid_;
    const GridOrientedMotion& path_;
    std::string& error_;
    FieldLayout field_;
    Field integral_;
    double estimate_ = 0.;
    std::size_t evaluations_ = 0;
};

bool PublishAverages(const GridLayout& layout, const GridOrientedMotion& motion,
                     const Integrator& integral, GridOrientedSweep& result, std::string& error) {
    const double area = layout.h * layout.h, volume = area * layout.h;
    const auto& values = integral.Integral();
    const auto& field = integral.Indices();
    for (int a = 0; a < 3; ++a) {
        result.averageAperture[a].resize(layout.FaceCount(a));
        result.solidSweep[a].resize(layout.FaceCount(a));
        for (std::size_t face = 0; face < layout.FaceCount(a); ++face) {
            double aperture = (1. - values[field.Aperture(a, face)]) * area;
            const double swept = values[field.Sweep(a, face)] * volume;
            if (!moving_geometry_detail::AdmitRoundoff(aperture, area) || !std::isfinite(swept)) {
                error = "oriented sweep produced invalid face measures";
                return false;
            }
            result.averageAperture[a][face] = aperture;
            result.solidSweep[a][face] = swept;
        }
    }
    const auto angular = Scale(motion.rotationAxis, motion.angle);
    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = 0; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                const auto cell = layout.Cell(i, j, k), base = field.Wall(cell);
                GridVec normal{}, torque{};
                for (int a = 0; a < 3; ++a) {
                    normal[a] = values[base + a] * area;
                    torque[a] = values[base + 3 + a] * volume;
                }
                if (!Finite(normal) || !Finite(torque)) {
                    error = "oriented sweep produced invalid integrated wall moments";
                    return false;
                }
                if (normal != GridVec{} || torque != GridVec{})
                    result.patches.push_back({motion.start.owner, cell, normal, torque});

                const double faceSweep =
                    result.solidSweep[0][layout.Face(0, i + 1, j, k)] -
                    result.solidSweep[0][layout.Face(0, i, j, k)] +
                    result.solidSweep[1][layout.Face(1, i, j + 1, k)] -
                    result.solidSweep[1][layout.Face(1, i, j, k)] +
                    result.solidSweep[2][layout.Face(2, i, j, k + 1)] -
                    result.solidSweep[2][layout.Face(2, i, j, k)];
                const double wallSweep = Dot(motion.translation, normal) + Dot(angular, torque);
                const double change = result.end.capacity[cell] - result.start.capacity[cell];
                result.maxFaceResidual = std::max(result.maxFaceResidual, std::abs(faceSweep - change));
                result.maxPatchResidual = std::max(result.maxPatchResidual, std::abs(wallSweep + change));
            }
        }
    }
    if (result.maxFaceResidual > conservationTolerance * volume ||
        result.maxPatchResidual > conservationTolerance * volume) {
        error = "oriented sweep fails independent face or pressure-work conservation";
        return false;
    }
    return true;
}

} // namespace

bool BuildGridOrientedSweep(const GridLayout& layout, const GridOrientedMotion& motion,
                            GridOrientedSweep& output, std::string& error) {
    GridOrientedSweep result;
    if (!AdmitMotion(layout, motion, error) ||
        !BuildGridOrientedGeometry(layout, {motion.start}, result.start, error))
        return false;
    if (Stationary(motion))
        result.end = result.start;
    else if (!BuildGridOrientedGeometry(layout, {PoseAt(motion, 1.)}, result.end, error))
        return false;
    Integrator integral(layout, motion, error);
    if (!integral.Run() || !PublishAverages(layout, motion, integral, result, error))
        return false;
    result.evaluations = integral.Evaluations();
    result.estimatedQuadratureError = integral.Estimate();
    if (Stationary(motion))
        result.averageAperture = result.start.aperture;
    output = std::move(result);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
