#include "GridPressureReaction.h"
#include "GridHydrostaticReaction.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace judas::volume_grid;

GridVec Add(const GridVec& a, const GridVec& b) {
    return {a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}

GridVec Subtract(const GridVec& a, const GridVec& b) {
    return {a[0]-b[0], a[1]-b[1], a[2]-b[2]};
}

GridVec Scale(const GridVec& v, double scale) {
    return {scale*v[0], scale*v[1], scale*v[2]};
}

double Dot(const GridVec& a, const GridVec& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}

double Norm(const GridVec& v) {
    return std::sqrt(Dot(v, v));
}

struct Checks {
    int total = 0;
    int failed = 0;

    void Expect(bool pass, const char* label, double value = 0) {
        ++total;
        if (!pass) ++failed;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

// Independent exact reference for an affine hydrostatic field clipped by a
// planar free surface. This integration does not read grid capacities,
// apertures, numerical pressure or phase reconstruction.
GridPressureLoad HydrostaticOracle(const BoxMotion& box, const GridVec& gravity,
                                   const GridVec& surface, const GridVec& origin,
                                   double density) {
    GridPressureLoad result;
    result.owner = box.owner;
    const auto depth = [&](const GridVec& p) {
        return Dot(gravity, Subtract(p, surface));
    };

    for (int axis = 0; axis < 3; ++axis) {
        const int first = (axis+1)%3;
        const int second = (axis+2)%3;
        for (int sign : {-1,1}) {
            std::vector<GridVec> vertices;
            const bool corners[4][2] = {{false,false}, {true,false}, {true,true}, {false,true}};
            for (const auto& corner : corners) {
                GridVec point{};
                point[axis] = sign < 0 ? box.lo[axis] : box.hi[axis];
                point[first] = corner[0] ? box.hi[first] : box.lo[first];
                point[second] = corner[1] ? box.hi[second] : box.lo[second];
                vertices.push_back(point);
            }

            std::vector<GridVec> wet;
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                const GridVec& a = vertices[i];
                const GridVec& b = vertices[(i+1)%vertices.size()];
                const double da = depth(a);
                const double db = depth(b);
                if (da >= 0) wet.push_back(a);
                if ((da >= 0) != (db >= 0))
                    wet.push_back(Add(a, Scale(Subtract(b, a), da/(da-db))));
            }

            GridVec inward{};
            inward[axis] = -sign;
            for (std::size_t i = 1; i+1 < wet.size(); ++i) {
                const GridVec tri[] = {wet[0], wet[i], wet[i+1]};
                const double area = Norm(Cross(Subtract(tri[1], tri[0]), Subtract(tri[2], tri[0])))/2;
                double sumPressure = 0;
                GridVec sumLever{};
                GridVec sumWeightedLever{};
                for (const auto& vertex : tri) {
                    const double pressure = density*std::max(0.0, depth(vertex));
                    const GridVec lever = Subtract(vertex, origin);
                    sumPressure += pressure;
                    sumLever = Add(sumLever, lever);
                    sumWeightedLever = Add(sumWeightedLever, Scale(lever, pressure));
                }
                result.force = Add(result.force, Scale(inward, area*sumPressure/3));
                // ∫ triangle x p dA, using barycentric products with exact
                // integrals A/6 on the diagonal and A/12 off the diagonal.
                const auto firstMoment = Scale(Add(Scale(sumLever, sumPressure), sumWeightedLever), area/12);
                result.torque = Add(result.torque, Cross(firstMoment, inward));
            }
        }
    }
    return result;
}

void HydrostaticControls(Checks& checks) {
    const BoxMotion box{7, {.43,.52,.61}, {.91,1.08,1.47}, {}};
    const GridVec center = Scale(Add(box.lo, box.hi), .5);
    const GridVec size = Subtract(box.hi, box.lo);
    const GridVec origin = {-.31, .22, .17};
    const GridVec gravity = {1.25, -9.81, .75};
    const auto compare = [&](const GridPressureLoad& measured, double volume,
                             const GridVec& wetCentroid, const GridVec& acceleration,
                             const char* forceLabel, const char* torqueLabel) {
        const GridVec force = Scale(acceleration, -1000*volume);
        const GridVec torque = Cross(Subtract(wetCentroid, origin), force);
        const double forceError = Norm(Subtract(measured.force, force));
        const double torqueError = Norm(Subtract(measured.torque, torque));
        checks.Expect(forceError < 3e-11, forceLabel, forceError);
        checks.Expect(torqueError < 3e-11, torqueLabel, torqueError);
    };
    compare(HydrostaticOracle(box, gravity, {0,3,0}, origin, 1000),
            size[0]*size[1]*size[2], center, gravity,
            "fully submerged affine force", "fully submerged off-centre torque");

    const GridVec down = {0,-9.81,0};
    const double waterline = .82;
    GridVec wetCenter = center;
    wetCenter[1] = (box.lo[1]+waterline)/2;
    compare(HydrostaticOracle(box, down, {0,waterline,0}, origin, 1000),
            size[0]*(waterline-box.lo[1])*size[2], wetCenter, down,
            "partial immersion force", "partial immersion off-centre torque");

    // Plane crosses all four vertical edges. Integrate the column height
    // H(x,z) = meanHeight + sx*(x-center.x) + sz*(z-center.z).
    const double meanHeight = size[1]/2;
    const double sx = gravity[0]/-gravity[1], sz = gravity[2]/-gravity[1];
    GridVec obliqueCenter = center;
    obliqueCenter[0] += sx*size[0]*size[0]/(12*meanHeight);
    obliqueCenter[1] = box.lo[1]+meanHeight/2+
        (sx*sx*size[0]*size[0]+sz*sz*size[2]*size[2])/(24*meanHeight);
    obliqueCenter[2] += sz*size[2]*size[2]/(12*meanHeight);
    compare(HydrostaticOracle(box, gravity, center, origin, 1000),
            size[0]*meanHeight*size[2], obliqueCenter, gravity,
            "oblique partial immersion force", "oblique partial immersion off-centre torque");

    const auto dry = HydrostaticOracle(box, down, {0,box.lo[1]-.004,0}, origin, 1000);
    checks.Expect(Norm(dry.force) == 0 && Norm(dry.torque) == 0,
                  "4 mm dry gap has no water pressure force or torque");
    const auto grazing = HydrostaticOracle(box, down, {0,box.lo[1],0}, origin, 1000);
    checks.Expect(Norm(grazing.force) == 0 && Norm(grazing.torque) == 0,
                  "grazing waterline transfers no hydrostatic load");
}

void PlanarWallPatchControls(Checks& checks) {
    const GridLayout layout{{12,12,12}, .2};
    const BoxMotion box{3, {.43,.52,.61}, {.91,1.08,1.47}, {}};
    const GridVec origin = {-.31,.22,.17};
    constexpr double gravity = -9.81;
    std::string error;
    for (int axis = 0; axis < 3; ++axis) {
        for (const double height : {box.lo[axis]-.004, box.lo[axis]+.004,
                                    (box.lo[axis]+box.hi[axis])/2, box.hi[axis]+.004}) {
            GridPhaseGeometry geometry;
            if (!BuildGridPhaseGeometry(layout, {box}, axis, height, geometry, error)) {
                std::printf("planar geometry error %s\n", error.c_str());
                checks.Expect(false, "planar wet and dry wall patches build");
                return;
            }
            const auto measured = AccumulatePlanarHydrostaticReaction(geometry, gravity, 1000, 0, origin);
            GridVec surface{}, down{};
            surface[axis] = height;
            down[axis] = gravity;
            const auto expected = HydrostaticOracle(box, down, surface, origin, 1000);
            const double forceError = Norm(Subtract(measured.at(0).force, expected.force));
            const double torqueError = Norm(Subtract(measured.at(0).torque, expected.torque));
            checks.Expect(forceError < 3e-11, "clipped planar grid-wall pressure matches water-force oracle", forceError);
            checks.Expect(torqueError < 3e-11, "clipped planar grid-wall pressure matches water-torque oracle", torqueError);
        }
    }
}

void SweptPlanarReactionControl(Checks& checks) {
    const BoxMotion box{5, {.43,.52,.61}, {.91,.64,1.47}, {.084,.255,-.031}};
    const GridVec origin = {-.31,.22,.17};
    const GridVec gravity = {0,-9.81,0};
    const GridVec surface = {0,.72,0};
    const auto load = MeasurePlanarHydrostaticMotion(box, 1, surface[1], gravity[1], 1000, 1, origin);
    const double topExits = (surface[1]-box.hi[1])/box.translation[1];
    const double bottomExits = (surface[1]-box.lo[1])/box.translation[1];
    const double events[] = {0, topExits, bottomExits, 1};
    GridVec force{}, torque{};
    for (int i = 0; i < 3; ++i) {
        const double center = (events[i]+events[i+1])/2;
        const double half = (events[i+1]-events[i])/2;
        // Gauss-Legendre is exact for force and torque between crossings,
        // which are at most linear and quadratic respectively.
        for (const double sign : {-1.,1.}) {
            const auto shift = Scale(box.translation, center+sign*half/std::sqrt(3.));
            const BoxMotion sampled{box.owner, Add(box.lo,shift), Add(box.hi,shift), {}};
            const auto water = HydrostaticOracle(sampled, gravity, surface, origin, 999);
            const auto air = HydrostaticOracle(sampled, gravity, {0,3,0}, origin, 1);
            force = Add(force, Scale(Add(water.force,air.force), half));
            torque = Add(torque, Scale(Add(water.torque,air.torque), half));
        }
    }
    const double forceError = Norm(Subtract(load.force, force));
    const double torqueError = Norm(Subtract(load.torque, torque));
    checks.Expect(forceError < 3e-11, "swept waterline crossing gives exact mean reaction", forceError);
    checks.Expect(torqueError < 3e-11, "swept waterline crossing gives exact mean torque", torqueError);

    const double area = (box.hi[0]-box.lo[0])*(box.hi[2]-box.lo[2]);
    const auto pressureIntegral = [&](double travel) {
        const double lo = box.lo[1]+travel-surface[1], hi = box.hi[1]+travel-surface[1];
        const double wetLo = std::min(lo,0.), wetHi = std::min(hi,0.);
        return .5*area*gravity[1]*(hi*hi-lo*lo+999*(wetHi*wetHi-wetLo*wetLo));
    };
    const double expectedWork = pressureIntegral(0)-pressureIntegral(box.translation[1]);
    const double error = std::abs(Dot(load.force,box.translation)-expectedWork);
    checks.Expect(error < 3e-11, "swept waterline reaction matches pressure potential change", error);
    const BoxMotion reversed{box.owner, Add(box.lo,box.translation), Add(box.hi,box.translation),
                             Scale(box.translation,-1)};
    const auto reverse = MeasurePlanarHydrostaticMotion(reversed, 1, surface[1], gravity[1], 1000, 1, origin);
    const double reversalError = Norm(Subtract(load.force,reverse.force))+
                                 Norm(Subtract(load.torque,reverse.torque));
    checks.Expect(reversalError < 3e-11, "reverse traversal preserves mean load and reverses work", reversalError);
}

GridVec RotateAxes(const GridVec& v) {
    return {v[2], v[0], v[1]};
}

void AffinePressureSplitControl(Checks& checks) {
    const GridLayout layout{{12,12,12}, .2};
    const BoxMotion box{17, {.43,.52,.61}, {.91,1.08,1.47}, {.084,.055,-.031}};
    const GridVec origin = {-.31,.22,.17};
    const GridVec gravity = {1.25,-9.81,.75};
    const GridVec datum = {0,3,0};
    const GridVec center = Scale(Add(box.lo, box.hi), .5);
    const GridVec middle = Add(center, Scale(box.translation,.5));
    const GridVec size = Subtract(box.hi, box.lo);
    const double volume = size[0]*size[1]*size[2];
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {box}, geometry, error)) {
        std::printf("geometry error %s\n", error.c_str());
        checks.Expect(false, "affine pressure split fixture builds");
        return;
    }
    const auto hydro = [&](const GridVec& position) {
        return 1000*Dot(gravity, Subtract(position, datum));
    };
    std::vector<double> totalPressure(layout.CellCount()), dynamicPressure(layout.CellCount());
    for (int k = 0; k < layout.cells[2]; ++k)
        for (int j = 0; j < layout.cells[1]; ++j)
            for (int i = 0; i < layout.cells[0]; ++i) {
                const GridVec p{(i+.5)*layout.h, (j+.5)*layout.h, (k+.5)*layout.h};
                const auto cell = layout.Cell(i,j,k);
                dynamicPressure[cell] = 1.5*p[0]*p[1]-1.4*p[2]*p[2];
                totalPressure[cell] = dynamicPressure[cell]+hydro(p);
            }
    const auto total = AccumulateGridPressureReaction(geometry.averagePatches, totalPressure, origin);
    const auto dynamic = AccumulateGridPressureReaction(geometry.averagePatches, dynamicPressure, origin);
    if (total.size() != 1 || dynamic.size() != 1) {
        checks.Expect(false, "affine pressure split has one solid owner");
        return;
    }
    const GridVec exactHydroForce = Scale(gravity, -1000*volume);
    const GridVec exactHydroTorque = Cross(Subtract(middle, origin), exactHydroForce);
    const double unsplitError = Norm(Subtract(total[0].force, Add(dynamic[0].force, exactHydroForce)));
    checks.Expect(unsplitError > 1, "cell-centre pressure is a failing exact moving hydrostatic-force control", unsplitError);

    const GridVec splitForce = Add(dynamic[0].force, exactHydroForce);
    const GridVec splitTorque = Add(dynamic[0].torque, exactHydroTorque);
    const auto start = HydrostaticOracle(box, gravity, datum, origin, 1000);
    const BoxMotion endBox{box.owner, Add(box.lo, box.translation), Add(box.hi, box.translation), {}};
    const auto end = HydrostaticOracle(endBox, gravity, datum, origin, 1000);
    const double forceError = Norm(Subtract(splitForce,
        Add(dynamic[0].force, Scale(Add(start.force, end.force), .5))));
    const double torqueError = Norm(Subtract(splitTorque,
        Add(dynamic[0].torque, Scale(Add(start.torque, end.torque), .5))));
    checks.Expect(forceError < 3e-11, "affine split matches independent fully wet moving force", forceError);
    checks.Expect(torqueError < 3e-11, "affine split matches independent fully wet moving torque", torqueError);

    // Fully wet affine p integrates exactly at the solid volume centroid.
    // The dynamic term stays adjoint to the original cell capacity change.
    double geometricWork = volume*(hydro(center)-hydro(Add(center, box.translation)));
    for (std::size_t i = 0; i < dynamicPressure.size(); ++i)
        geometricWork += dynamicPressure[i]*(geometry.endCapacity[i]-geometry.startCapacity[i]);
    const double workError = std::abs(Dot(splitForce, box.translation)-geometricWork);
    checks.Expect(workError < 3e-11, "affine split matches exact hydrostatic geometric work", workError);
}

void MovingWorkControl(Checks& checks) {
    const GridLayout layout{{12,12,12}, .2};
    const BoxMotion body{11, {.36,.66,.75}, {.99,1.12,1.38}, {.081,-.078,.096}};
    const GridVec origin = {-.13,.39,.27};
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {body}, geometry, error)) {
        std::printf("geometry error %s\n", error.c_str());
        checks.Expect(false, "moving work fixture builds");
        return;
    }
    checks.Expect(true, "moving work fixture builds");

    const auto pressureField = [](const GridVec& x) {
        return 53+31*x[0]-17*x[1]+12*x[2]+13*x[0]*x[1]-4*x[2]*x[2];
    };
    const auto pressureFor = [&](bool rotated) {
        std::vector<double> pressure(layout.CellCount());
        for (int k = 0; k < layout.cells[2]; ++k)
            for (int j = 0; j < layout.cells[1]; ++j)
                for (int i = 0; i < layout.cells[0]; ++i) {
                    const GridVec x{(i+.5)*layout.h, (j+.5)*layout.h, (k+.5)*layout.h};
                    pressure[layout.Cell(i,j,k)] = pressureField(rotated ? RotateAxes(RotateAxes(x)) : x);
                }
        return pressure;
    };
    const auto pressure = pressureFor(false);
    const auto loads = AccumulateGridPressureReaction(geometry.averagePatches, pressure, origin);
    checks.Expect(loads.size() == 1 && loads[0].owner == body.owner, "moving reaction retains solid owner");
    if (loads.size() != 1) return;
    double expectedWork = 0;
    for (std::size_t i = 0; i < pressure.size(); ++i)
        expectedWork += pressure[i]*(geometry.endCapacity[i]-geometry.startCapacity[i]);
    const double workError = std::abs(Dot(loads[0].force, body.translation)-expectedWork);
    checks.Expect(workError < 2e-12, "average solid force is the swept-volume work adjoint", workError);
    checks.Expect(std::abs(expectedWork) > 1e-3, "moving reaction has nonzero work", expectedWork);
    const auto stale = AccumulateGridPressureReaction(geometry.startPatches, pressure, origin);
    checks.Expect(stale.size() == 1 &&
                  std::abs(Dot(stale[0].force, body.translation)-expectedWork) > 1e-3,
                  "start-only wall traction is a failing moving-work control");

    auto offsetPressure = pressure;
    for (double& value : offsetPressure) value += 500;
    const auto shifted = AccumulateGridPressureReaction(geometry.averagePatches, offsetPressure, origin);
    const double gaugeError = Norm(Subtract(loads[0].force, shifted[0].force))+
                              Norm(Subtract(loads[0].torque, shifted[0].torque));
    checks.Expect(gaugeError < 3e-11, "closed moving box reaction is pressure-gauge invariant", gaugeError);

    const BoxMotion rotatedBody{body.owner, RotateAxes(body.lo), RotateAxes(body.hi), RotateAxes(body.translation)};
    MovingGridGeometry rotatedGeometry;
    if (!BuildMovingGridGeometry(layout, {rotatedBody}, rotatedGeometry, error)) {
        std::printf("rotated geometry error %s\n", error.c_str());
        checks.Expect(false, "rotated moving work fixture builds");
        return;
    }
    const auto rotatedLoads = AccumulateGridPressureReaction(rotatedGeometry.averagePatches,
        pressureFor(true), RotateAxes(origin));
    const double rotationError = Norm(Subtract(RotateAxes(loads[0].force), rotatedLoads[0].force))+
                                 Norm(Subtract(RotateAxes(loads[0].torque), rotatedLoads[0].torque));
    checks.Expect(rotationError < 3e-11, "moving force and torque rotate with the grid chart", rotationError);

    auto invalidPressure = pressure;
    invalidPressure[geometry.averagePatches.front().cell] = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try {
        (void)AccumulateGridPressureReaction(geometry.averagePatches, invalidPressure, origin);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    checks.Expect(rejected, "nonfinite pressure rejects without publishing a reaction");
}

} // namespace

int main() {
    Checks checks;
    HydrostaticControls(checks);
    PlanarWallPatchControls(checks);
    SweptPlanarReactionControl(checks);
    MovingWorkControl(checks);
    AffinePressureSplitControl(checks);
    std::printf("SUMMARY grid pressure reaction %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
