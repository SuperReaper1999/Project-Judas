#include "LiquidGeometry.h"
#include "LiquidSurface.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Vector = glm::dvec3;
unsigned checks = 0;

struct Errors {
    double volume = 0;
    double firstMoment = 0;
    double aperture = 0;
    double normal = 0;
    double storage = 0;
    double inverse = 0;
    double partition = 0;
} worst;

void Check(bool condition, const std::string& label) {
    ++checks;
    if (!condition) throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}

GravityEquilibrium Plane(Vector up) {
    GravityEquilibrium result;
    result.kind = GravityEquilibrium::Kind::Plane;
    result.up = glm::normalize(up);
    result.magnitude = 9.81;
    return result;
}

LiquidGeometry Transform(const LiquidGeometry& geometry, const glm::dquat& rotation,
                         Vector translation) {
    auto result = geometry;
    for (auto& tetrahedron : result.cells)
        for (auto& vertex : tetrahedron) vertex = rotation * vertex + translation;
    return result;
}

std::vector<LiquidPolygon> Transform(const std::vector<LiquidPolygon>& polygons,
                                   const glm::dquat& rotation, Vector translation) {
    auto result = polygons;
    for (auto& polygon : result)
        for (auto& vertex : polygon) vertex = rotation * vertex + translation;
    return result;
}

LiquidMoment Clip(const LiquidGeometry& geometry, const GravityEquilibrium& equilibrium,
                  double head, MeshData* caps = nullptr) {
    LiquidMoment result;
    for (const auto& tetrahedron : geometry.cells) {
        std::array<double, 4> coordinates;
        for (unsigned i = 0; i < coordinates.size(); ++i)
            coordinates[i] = equilibrium.Coordinate(tetrahedron[i]);
        auto part = LiquidClip(tetrahedron, coordinates, head, caps);
        result.volume += part.volume;
        result.first += part.first;
    }
    return result;
}

double StoredCapacity(const LiquidBasinData& storage, double head) {
    if (head <= storage.curve.front().q) return 0;
    if (head >= storage.curve.back().q) return storage.capacity;
    const auto end = std::upper_bound(storage.curve.begin(), storage.curve.end(), head,
        [](double value, const LiquidCurvePoint& point) { return value < point.q; });
    const auto index = static_cast<std::size_t>(end - storage.curve.begin() - 1);
    const auto& a = storage.curve[index];
    const auto& b = storage.curve[index + 1];
    const long double t = (static_cast<long double>(head) - a.q) /
                          (static_cast<long double>(b.q) - a.q);
    if (storage.capacityPolynomials.empty()) return a.volume + t * (b.volume - a.volume);
    const auto& c = storage.capacityPolynomials[index];
    return c.Volume(t);
}

bool MonotoneKnots(const LiquidBasinData& storage) {
    if (storage.curve.size() < 2 || storage.curve.front().volume != 0 ||
        storage.curve.back().volume != storage.capacity) return false;
    for (std::size_t i = 1; i < storage.curve.size(); ++i) {
        if (!(storage.curve[i].q > storage.curve[i - 1].q) ||
            !(storage.curve[i].volume >= storage.curve[i - 1].volume)) return false;
    }
    return true;
}

void CoefficientContracts() {
    LiquidCapacityPolynomial cubic;
    cubic.volume = {0, 0, 0, 1};
    cubic.area = {0, 0, 3};
    bool correct = true;
    for (long double t : {0.L, .125L, .5L, .875L, 1.L}) {
        correct &= std::abs(cubic.Volume(t) - t * t * t) < 2e-15L;
        correct &= std::abs(cubic.Area(t) - 3 * t * t) < 2e-15L;
        correct &= std::abs(cubic.Integral(t) - t * t * t * t / 4) < 2e-15L;
    }
    Check(correct, "analytic nonlinear storage, physical derivative and energy primitive agree");
    LiquidCapacityPolynomial tiny;
    tiny.volume = {.5, .5, .5, .5};
    tiny.area = {1e-20, 1e-20, 1e-20};
    Check(tiny.Volume(.5L) == .5 && tiny.Area(.5L) > 0,
          "physical small area is retained when cumulative volume increments round below one ULP");
}

void AnalyticObliqueCuts() {
    const auto box = LiquidBox({0, 0, 0}, {1, 1, 1});
    const auto equilibrium = Plane({1, 1, 0});
    const auto face = LiquidBoundary(box, {0, 0, -1}, 0);
    Check(!face.empty(), "analytic oblique cut uses actual original box boundary");
    bool correct = true;
    for (double s : {0., .125, .5, 1., 1.25, 1.875, 2.}) {
        // Integrate x+y<=s over [0,1]^2, then integrate the independent z axis.
        const double tail = 2 - s;
        const double volume = s <= 1 ? s * s / 2 : 1 - tail * tail / 2;
        const double xyMoment = s <= 1 ? s * s * s / 6 :
            .5 - tail * tail / 2 + tail * tail * tail / 6;
        const auto actual = Clip(box, equilibrium, s / std::sqrt(2.));
        const double area = LiquidFaceArea(face, equilibrium, s / std::sqrt(2.));
        const double volumeError = std::abs(actual.volume - volume);
        const double momentError = glm::length(actual.first - Vector(xyMoment, xyMoment, volume / 2));
        const double areaError = std::abs(area - volume);
        worst.volume = std::max(worst.volume, volumeError);
        worst.firstMoment = std::max(worst.firstMoment, momentError);
        worst.aperture = std::max(worst.aperture, areaError);
        correct &= volumeError < 2e-11 && momentError < 2e-11 && areaError < 2e-11;
    }
    Check(correct, "oblique analytic volume, first moments and wet-face area agree");

    const auto ceiling = LiquidBoundary(box, {0, 0, 1}, 1);
    const auto vertical = Plane({0, 0, 1});
    Check(LiquidFaceArea(ceiling, vertical, std::nextafter(1., 0.)) == 0 &&
          std::abs(LiquidFaceArea(ceiling, vertical, 1) - 1) < 2e-15 &&
          std::abs(LiquidFaceArea(ceiling, vertical, std::nextafter(1., 2.)) - 1) < 2e-15,
          "collapsed face heights are dry below, wet exactly at and above full height");
}

void CutPlaneContracts() {
    // The two positive distances lie on opposite sides of the old inside
    // epsilon, but neither is inside the actual plane. Intersecting their edge
    // against zero used to extrapolate and create volume outside the tetrahedron.
    LiquidGeometry crossing;
    crossing.cells.push_back({Vector(-1, -1, 0), Vector(1, 1e-16, 0),
                              Vector(0, 2e-11, 1), Vector(0, -1, 1)});
    const auto totalVolume = [](const LiquidGeometry& geometry) {
        double total = 0;
        for (const auto& t : geometry.cells)
            total += std::abs(glm::dot(t[1] - t[0],
                glm::cross(t[2] - t[0], t[3] - t[0]))) / 6;
        return total;
    };
    const auto equilibrium = Plane({0, 1, 0});
    const double whole = totalVolume(crossing);
    const auto clipped = Clip(crossing, equilibrium, 0);
    Check(clipped.volume >= 0 && clipped.volume <= whole + 2e-15,
          "near-plane clipping never extrapolates beyond the original physical volume");
    const auto below = LiquidCutGeometry(crossing, {{0, 1, 0, 0}});
    const auto above = LiquidCutGeometry(crossing, {{0, -1, 0, 0}});
    Check(std::abs(totalVolume(below) + totalVolume(above) - whole) < 2e-13,
          "complementary physical half-spaces partition volume without tolerance-band overlap");

    // Every vertex is strictly outside, yet the old all-inside fast path kept
    // the entire tetrahedron because all distances were less than 1e-12.
    LiquidGeometry outside;
    outside.cells.push_back({Vector(0, 1e-13, 0), Vector(1000, 2e-13, 0),
                             Vector(0, 2e-13, 1000), Vector(0, 9e-13, 0)});
    Check(totalVolume(outside) > 1e-12 &&
          LiquidCutGeometry(outside, {{0, 1, 0, 0}}).cells.empty(),
          "all-inside fast path does not retain a strictly outside thin tetrahedron");
    const glm::dquat rotation(.5, .5, .5, .5);
    const auto rotated = Transform(outside, rotation, {});
    const Vector normal = rotation * Vector(0, 1, 0);
    Check(LiquidCutGeometry(rotated, {{normal, 0}}).cells.empty(),
          "exact physical half-space classification is independent of world up");
}

void NegativeFrameControls() {
    const auto geometry = LiquidBox({0, 0, 0}, {2, 1, .5});
    const auto equilibrium = Plane({0, 1, 0});
    const auto rotation = glm::angleAxis(glm::radians(90.), Vector(0, 0, 1));
    auto rotatedGravity = equilibrium;
    rotatedGravity.up = rotation * equilibrium.up;
    const auto rotated = Transform(geometry, rotation, {});
    const double expected = LiquidCapacity(geometry, equilibrium, .4);
    Check(std::abs(LiquidCapacity(rotated, rotatedGravity, .4) - expected) < 1e-11 &&
          std::abs(LiquidCapacity(rotated, equilibrium, .4) - expected) > .1,
          "negative control detects rotating geometry without its gravity frame");

    const auto shifted = Transform(geometry, glm::dquat(1, 0, 0, 0), {0, 3, 0});
    Check(std::abs(LiquidCapacity(shifted, equilibrium, 3.4) - expected) < 1e-11 &&
          std::abs(LiquidCapacity(shifted, equilibrium, .4) - expected) > .1,
          "negative control detects translating geometry without shifting plane head");

    MeshData cap;
    Clip(rotated, rotatedGravity, .4, &cap);
    bool correct = !cap.vertices.empty(), catchesReversal = !cap.vertices.empty();
    for (const auto& vertex : cap.vertices) {
        correct &= glm::dot(Vector(vertex.normal), rotatedGravity.up) > 1 - 1e-6;
        catchesReversal &= glm::dot(-Vector(vertex.normal), rotatedGravity.up) < -.99;
    }
    Check(correct && catchesReversal, "physical cap normal detects a reversed surface orientation");
}

void Covariance(const glm::dquat& rotation, Vector translation, unsigned index) {
    const auto geometry = LiquidBox({-.8, 0, -.35}, {1.2, 1.4, .85});
    const auto equilibrium = Plane({1, 2, -.7});
    const auto rotated = Transform(geometry, rotation, translation);
    auto selected = equilibrium;
    selected.up = rotation * equilibrium.up;
    const double offset = glm::dot(selected.up, translation);
    const auto originalFace = LiquidBoundary(geometry, {0, 0, 1}, .85);
    const auto rotatedFace = Transform(originalFace, rotation, translation);
    double lower = std::numeric_limits<double>::infinity(), upper = -lower;
    for (const auto& tetrahedron : geometry.cells)
        for (Vector vertex : tetrahedron) {
            lower = std::min(lower, equilibrium.Coordinate(vertex));
            upper = std::max(upper, equilibrium.Coordinate(vertex));
        }
    bool correct = true, normals = true;
    for (double fraction : {.07, .2, .37, .5, .73, .9}) {
        const double head = lower + (upper - lower) * fraction;
        const auto expected = Clip(geometry, equilibrium, head);
        MeshData cap;
        const auto actual = Clip(rotated, selected, head + offset, &cap);
        const double volumeError = std::abs(actual.volume - expected.volume);
        const double momentError = glm::length(actual.first -
            (rotation * expected.first + translation * expected.volume));
        const double areaError = std::abs(LiquidFaceArea(rotatedFace, selected, head + offset) -
                                         LiquidFaceArea(originalFace, equilibrium, head));
        worst.volume = std::max(worst.volume, volumeError);
        worst.firstMoment = std::max(worst.firstMoment, momentError);
        worst.aperture = std::max(worst.aperture, areaError);
        correct &= volumeError < 2e-10 && momentError < 2e-9 && areaError < 2e-10;
        normals &= !cap.vertices.empty();
        for (const auto& vertex : cap.vertices) {
            const double error = glm::length(Vector(vertex.normal) - selected.up);
            worst.normal = std::max(worst.normal, error);
            normals &= error < 2e-6;
        }
    }
    Check(correct, "rigid-frame volume, moments and clipped aperture covariance case " + std::to_string(index));
    Check(normals, "rotated cap normals follow gravity independently of world axes case " + std::to_string(index));

    bool planes = true;
    for (std::size_t i = 0; i < geometry.cells.size(); ++i) {
        const auto before = LiquidTetPlanes(geometry.cells[i]);
        const auto after = LiquidTetPlanes(rotated.cells[i]);
        for (std::size_t j = 0; j < before.size(); ++j) {
            const Vector normal = rotation * Vector(before[j]);
            const double distance = before[j].w + glm::dot(normal, translation);
            planes &= glm::length(Vector(after[j]) - normal) < 2e-11 &&
                      std::abs(after[j].w - distance) < 2e-10;
        }
    }
    Check(planes, "original physical tetrahedron face planes transform covariantly case " + std::to_string(index));
}

void SurfaceStorage(const glm::dquat& rotation, unsigned index) {
    const auto box = LiquidBox({-1, 0, -1}, {1, 1, 1});
    const auto geometry = Transform(box, rotation, {});
    auto equilibrium = Plane(rotation * Vector(0, 1, 0));
    LiquidBasinData basin;
    std::string error;
    Check(BakeLiquidBasin(geometry, equilibrium, 1e-6, .02, "rotation-regression", basin, error),
          "physical basin bake case " + std::to_string(index) + ": " + error);
    LiquidSurfaceSettings settings;
    settings.enabled = true;
    settings.columns = settings.rows = 4;
    std::shared_ptr<const LiquidSurfaceData> surface;
    Check(BakeLiquidSurface(basin, settings, surface, error, &geometry) && surface,
          "physical dynamic-surface bake case " + std::to_string(index) + ": " + error);

    bool monotone = true, exact = true, inverse = true;
    for (const auto& cell : surface->cells) {
        const auto& storage = cell.storage;
        monotone &= MonotoneKnots(storage);
        for (std::size_t i = 1; i < storage.curve.size(); ++i) {
            const double a = storage.curve[i - 1].q, b = storage.curve[i].q;
            if (!storage.capacityPolynomials.empty()) {
                const auto& polynomial = storage.capacityPolynomials[i - 1];
                monotone &= polynomial.volume.front() == storage.curve[i - 1].volume &&
                            polynomial.volume.back() == storage.curve[i].volume;
            }
            double previous = StoredCapacity(storage, a);
            // Adjacent representable heads have no interior double: their
            // rounded midpoint/nextafter sequence need not already be ordered.
            std::array<double, 5> samples{a, std::nextafter(a, b), a + (b - a) / 2,
                                          std::nextafter(b, a), b};
            std::sort(samples.begin(), samples.end());
            for (double q : samples) {
                const double cached = StoredCapacity(storage, q);
                const double direct = LiquidCapacity(storage.geometry, equilibrium, q);
                const double discrepancy = std::abs(cached - direct);
                worst.storage = std::max(worst.storage, discrepancy);
                exact &= std::isfinite(cached) && discrepancy < 2e-10 &&
                         cached >= 0 && cached <= storage.capacity;
                monotone &= cached >= previous;
                if (!storage.capacityPolynomials.empty()) {
                    const long double fraction = (static_cast<long double>(q) - a) /
                        (static_cast<long double>(b) - a);
                    const double area = storage.capacityPolynomials[i - 1].Area(fraction);
                    exact &= std::isfinite(area) && area >= 0;
                }
                previous = cached;
            }
        }
        for (double fraction : {.05, .3, .5, .8, .95}) {
            const double volume = storage.capacity * fraction;
            const double q = LiquidInverse(storage, volume);
            const double discrepancy = std::abs(LiquidCapacity(storage.geometry, equilibrium, q) - volume);
            worst.inverse = std::max(worst.inverse, discrepancy);
            inverse &= discrepancy < 2e-10;
        }
    }
    Check(monotone, "strict serialized knots and sampled physical storage remain monotone case " + std::to_string(index));
    Check(exact && inverse, "cubic storage and inverse match independent geometric clipping case " + std::to_string(index));

    bool partition = true;
    for (double head : {.1, .25, .5, .75, .9, 1.1}) {
        double total = 0;
        for (const auto& cell : surface->cells) total += StoredCapacity(cell.storage, head);
        const double expected = 4 * std::clamp(head, 0., 1.);
        const double discrepancy = std::abs(total - expected);
        worst.partition = std::max(worst.partition, discrepancy);
        partition &= discrepancy < 2e-9;
    }
    // The chart chooses a reference axis, so individual partitions need not be
    // identical after rotation. Only aggregate physical storage is invariant.
    Check(partition, "aggregate physical storage is rotation invariant despite chart repartitioning case " + std::to_string(index));

    std::ostringstream encoded;
    encoded << std::setprecision(17);
    Check(SaveLiquidSurface(encoded, *surface), "normal Surface1 serialization case " + std::to_string(index));
    std::istringstream input(encoded.str());
    std::shared_ptr<const LiquidSurfaceData> restored;
    Check(ReadLiquidSurface(input, equilibrium, restored, error) && restored &&
          restored->cells.size() == surface->cells.size() && restored->faces.size() == surface->faces.size(),
          "strict Surface1 decoder accepts generated rotated storage case " + std::to_string(index) + ": " + error);

    if (index == 0) {
        auto malformed = *surface;
        malformed.cells.front().storage.curve[1].volume = -1;
        std::ostringstream invalid;
        invalid << std::setprecision(17);
        Check(SaveLiquidSurface(invalid, malformed), "negative decreasing capacity fixture serializes");
        std::istringstream rejected(invalid.str());
        std::shared_ptr<const LiquidSurfaceData> destination;
        Check(!ReadLiquidSurface(rejected, equilibrium, destination, error) && !destination &&
              error.find("nonmonotonic capacity knots") != std::string::npos,
              "strict decoder rejects decreasing capacity without publishing a surface");
        malformed = *surface;
        malformed.cells.front().storage.curve[1].q = malformed.cells.front().storage.curve.front().q;
        std::ostringstream duplicate;
        duplicate << std::setprecision(17);
        Check(SaveLiquidSurface(duplicate, malformed), "negative duplicate head fixture serializes");
        std::istringstream duplicateInput(duplicate.str());
        Check(!ReadLiquidSurface(duplicateInput, equilibrium, destination, error) && !destination &&
              error.find("nonmonotonic capacity knots") != std::string::npos,
              "strict decoder rejects duplicate heads without publishing a surface");
    }
}

// Separate polygon integration is the aperture oracle. It does not call the
// production face-subtraction helper, use cell volume as area, or read a cache.
LiquidPolygon ExactFaceClip(const LiquidPolygon& polygon, glm::dvec4 plane, bool inside) {
    LiquidPolygon result;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        const auto distance = [&](Vector point) {
            return static_cast<long double>(plane.x) * point.x +
                   static_cast<long double>(plane.y) * point.y +
                   static_cast<long double>(plane.z) * point.z - plane.w;
        };
        const long double x = distance(a), y = distance(b);
        const bool first = inside ? x <= 0 : x > 0;
        const bool second = inside ? y <= 0 : y > 0;
        if (first) result.push_back(a);
        if (first != second) result.push_back(a + (b - a) * double(x / (x - y)));
    }
    return result;
}

double PolygonArea(const LiquidPolygon& polygon) {
    double result = 0;
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
        result += glm::length(glm::cross(polygon[i] - polygon[0], polygon[i + 1] - polygon[0])) * .5;
    return result;
}

double ExactOpenArea(const std::vector<LiquidPolygon>& original,
                     const std::vector<glm::dvec4>& solid,
                     const GravityEquilibrium& equilibrium, double head) {
    double result = 0;
    for (auto polygon : original) {
        for (const auto& plane : solid) {
            const auto outside = ExactFaceClip(polygon, plane, false);
            result += PolygonArea(ExactFaceClip(outside, {equilibrium.up, head}, true));
            polygon = ExactFaceClip(polygon, plane, true);
            if (polygon.empty()) break;
        }
    }
    return result;
}

std::shared_ptr<const LiquidSurfaceData> RotateSurface(const LiquidSurfaceData& source,
                                                     const glm::dquat& rotation) {
    auto data = std::make_shared<LiquidSurfaceData>(source);
    data->axis = rotation * data->axis;
    data->x = rotation * data->x;
    data->y = rotation * data->y;
    data->equilibrium.up = rotation * data->equilibrium.up;
    data->equilibrium.center = rotation * data->equilibrium.center;
    for (auto& cell : data->cells) {
        cell.centre = rotation * cell.centre;
        cell.storage.equilibrium = data->equilibrium;
        cell.storage.geometry = Transform(cell.storage.geometry, rotation, {});
        cell.loadingGeometry.clear();
        for (const auto& tet : cell.storage.geometry.cells)
            cell.loadingGeometry.push_back(CompileLiquidTetQuery(tet, data->equilibrium));
    }
    for (auto& face : data->faces) {
        face.normal = rotation * face.normal;
        face.polygons = Transform(face.polygons, rotation, {});
    }
    return data;
}

void SolidApertureContracts() {
    const auto geometry = LiquidBox({-1, 0, -1}, {1, 1, 1});
    const auto equilibrium = Plane({0, 1, 0});
    LiquidBasinData basin;
    std::string error;
    Check(BakeLiquidBasin(geometry, equilibrium, 1e-6, .02, "aperture-contract", basin, error),
          "near-plane aperture fixture bakes actual physical storage");
    LiquidSurfaceSettings settings;
    settings.enabled = true;
    settings.columns = settings.rows = 2;
    std::shared_ptr<const LiquidSurfaceData> source;
    Check(BakeLiquidSurface(basin, settings, source, error, &geometry) && source,
          "near-plane aperture fixture uses normal physical surface faces");
    std::size_t selected = 0;
    while (selected < source->faces.size() && std::abs(source->faces[selected].normal.x) < .9) ++selected;
    Check(selected < source->faces.size(), "near-plane fixture has a real shared face");
    const auto& original = source->faces[selected];
    const Vector tangent = glm::normalize(glm::cross(original.normal, equilibrium.up));
    double lower = 1e30, upper = -1e30;
    for (const auto& polygon : original.polygons) for (Vector point : polygon) {
        lower = std::min(lower, glm::dot(tangent, point));
        upper = std::max(upper, glm::dot(tangent, point));
    }
    const double tilt = 2e-11 / (upper - lower);
    const std::array<glm::dquat, 3> rotations = {
        glm::dquat(1, 0, 0, 0),
        glm::angleAxis(.8, glm::normalize(Vector(1, 2, 3))),
        glm::angleAxis(1.37, glm::normalize(Vector(1, 2, 3)))
    };
    for (unsigned i = 0; i < rotations.size(); ++i) {
        const auto data = RotateSurface(*source, rotations[i]);
        for (bool intersects : {false, true}) {
            // The negative solid ends just outside the whole shared face.
            // The positive solid reaches halfway across that exact same face.
            const double offset = original.offset + tilt *
                (intersects ? (lower + upper) / 2 : lower) - (intersects ? 0 : 1e-16);
            std::vector<glm::dvec4> solid = {
                {original.normal + tilt * tangent, offset},
                {-original.normal, -original.offset + .01},
                {equilibrium.up, 10}, {-equilibrium.up, 10},
                {tangent, 10}, {-tangent, 10}
            };
            for (auto& plane : solid) plane = {rotations[i] * Vector(plane), plane.w};
            LiquidSurface surface(data, 2);
            const double overflow = surface.SetSolids({solid}, error);
            Check(error.empty() && overflow == 0,
                  "bounded physical solid publishes through SetSolids orientation " + std::to_string(i));
            std::fill(surface.discharge.begin(), surface.discharge.end(), 0);
            surface.discharge[selected] = 1;
            const auto& face = data->faces[selected];
            const double head = std::max(surface.cells[face.a].q, surface.cells[face.b].q);
            // Public energy of exactly one flow is .5*distance*Q^2/area.
            // This observes the aperture actually used by surface physics.
            const double energy = surface.KineticEnergy(face.a);
            const double actual = .5 * face.distance / energy;
            const double expected = ExactOpenArea(face.polygons, solid, data->equilibrium, head);
            double unblocked = 0;
            for (const auto& polygon : face.polygons)
                unblocked += PolygonArea(ExactFaceClip(polygon, {data->equilibrium.up, head}, true));
            const double discrepancy = std::abs(actual - expected);
            worst.aperture = std::max(worst.aperture, discrepancy);
            std::cout << std::setprecision(17) << "SOLID_APERTURE orientation=" << i
                      << " intersecting=" << intersects << " unblocked=" << unblocked
                      << " expected=" << expected << " actual=" << actual
                      << " error=" << discrepancy << '\n';
            Check(std::isfinite(energy) && energy > 0 && discrepancy < 2e-10,
                  "actual SetSolids aperture agrees with independent polygon intersection");
            Check(intersects ? expected > unblocked * .25 && expected < unblocked * .75 :
                  std::abs(expected - unblocked) < 2e-10 && actual <= unblocked + 2e-10,
                  intersects ? "independent intersecting solid genuinely blocks wet shared-face area" :
                               "independent nonintersecting solid cannot enlarge the wet aperture");
            Check(surface.SetSolids({}, error) == 0 && error.empty(),
                  "removing the solid republishes the ordinary open face");
            std::fill(surface.discharge.begin(), surface.discharge.end(), 0);
            surface.discharge[selected] = 1;
            const double restoredHead = std::max(surface.cells[face.a].q, surface.cells[face.b].q);
            const double restored = .5 * face.distance / surface.KineticEnergy(face.a);
            double originalArea = 0;
            for (const auto& polygon : face.polygons)
                originalArea += PolygonArea(ExactFaceClip(polygon, {data->equilibrium.up, restoredHead}, true));
            Check(std::abs(restored - originalArea) < 2e-10,
                  "solid removal invalidates derived aperture cache without changing physical faces");
        }
    }
}

// A fixed integer generator avoids implementation-defined distribution mappings.
std::uint64_t randomState = 0x5b3d84a729c160e7ULL;
double RandomSigned() {
    randomState ^= randomState >> 12;
    randomState ^= randomState << 25;
    randomState ^= randomState >> 27;
    const std::uint64_t bits = (randomState * 2685821657736338717ULL) >> 11;
    return 2 * (static_cast<double>(bits) / 9007199254740992.) - 1;
}

Vector RandomVector() {
    const double x = RandomSigned();
    const double y = RandomSigned();
    const double z = RandomSigned();
    return {x, y, z};
}
} // namespace

int main() {
    try {
        CoefficientContracts();
        AnalyticObliqueCuts();
        CutPlaneContracts();
        NegativeFrameControls();
        SolidApertureContracts();
        std::vector<glm::dquat> rotations = {
            glm::dquat(1, 0, 0, 0),
            glm::angleAxis(.8, glm::normalize(Vector(1, 2, 3))),
            glm::normalize(glm::dquat(glm::quat(0, .6f, .8f, 0))),
            glm::angleAxis(1e-15, glm::normalize(Vector(1, 0, 2))),
            glm::angleAxis(1e-10, glm::normalize(Vector(1, 3, 2)))
        };
        for (unsigned i = 0; i < 32; ++i) {
            Vector axis = RandomVector();
            if (glm::length(axis) < .1) axis += Vector(.7, -.4, .2);
            rotations.push_back(glm::angleAxis(RandomSigned() * 3., glm::normalize(axis)));
        }
        for (unsigned i = 0; i < rotations.size(); ++i) {
            Covariance(rotations[i], RandomVector() * 7., i);
            SurfaceStorage(rotations[i], i);
        }
        std::cout << std::setprecision(17)
                  << "ROTATION_ERRORS volume=" << worst.volume
                  << " first_moment=" << worst.firstMoment
                  << " aperture=" << worst.aperture
                  << " cap_normal=" << worst.normal
                  << " storage=" << worst.storage
                  << " inverse=" << worst.inverse
                  << " partition=" << worst.partition << '\n';
        std::cout << "Liquid rotation " << checks << " checks, " << rotations.size()
                  << " orientations, 0 failures; fixed seed 0x5b3d84a729c160e7\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL liquid rotation after " << checks << " checks: " << exception.what() << '\n';
        std::cerr << std::setprecision(17)
                  << "ROTATION_ERRORS_SO_FAR volume=" << worst.volume
                  << " first_moment=" << worst.firstMoment
                  << " aperture=" << worst.aperture
                  << " cap_normal=" << worst.normal
                  << " storage=" << worst.storage
                  << " inverse=" << worst.inverse
                  << " partition=" << worst.partition << '\n';
        return 1;
    }
}
