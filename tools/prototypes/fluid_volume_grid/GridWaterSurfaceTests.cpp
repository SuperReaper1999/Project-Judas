#include "GridWaterSurface.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace {

using namespace judas::volume_grid;

constexpr GridLayout kLayout{{4,4,4},.5};

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double measurement = 0) {
        ++total;
        if (!pass) ++failed;
        std::printf("%s %s %.17g\n",pass ? "PASS" : "FAIL",label,measurement);
    }
};

GridVec Subtract(const GridVec& a, const GridVec& b) {
    return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};
}

GridVec Cross(const GridVec& a, const GridVec& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}

double Dot(const GridVec& a, const GridVec& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

struct GeometryMeasures {
    double totalArea = 0;
    GridVec positiveArea{}, negativeArea{};
    bool valid = true, unique = true, excludesSolids = true, internal = true;
};

GeometryMeasures Measure(const GridWaterSurface& surface, const std::vector<BoxMotion>& solids) {
    GeometryMeasures measures;
    std::set<std::array<GridVec,3>> seen;
    for (const auto& triangle : surface.triangles) {
        const auto& points = triangle.vertices;
        const GridVec winding = Cross(Subtract(points[1],points[0]),Subtract(points[2],points[0]));
        const double area = .5*Dot(winding,triangle.normal);
        measures.valid = measures.valid && std::isfinite(area) && area > 0 &&
            Dot(triangle.normal,triangle.normal) == 1;
        measures.totalArea += area;
        auto key = points;
        std::sort(key.begin(),key.end());
        measures.unique = measures.unique && seen.insert(key).second;
        for (int axis = 0; axis < 3; ++axis) {
            if (triangle.normal[axis] > 0) measures.positiveArea[axis] += area;
            if (triangle.normal[axis] < 0) measures.negativeArea[axis] += area;
            const double lo = std::min({points[0][axis],points[1][axis],points[2][axis]});
            const double hi = std::max({points[0][axis],points[1][axis],points[2][axis]});
            measures.valid = measures.valid && lo >= 0 && hi <= kLayout.cells[axis]*kLayout.h;
            if (triangle.normal[axis] != 0)
                measures.internal = measures.internal && lo == hi && lo > 0 && lo < kLayout.cells[axis]*kLayout.h;
        }
        for (const auto& solid : solids) {
            bool overlap = true;
            for (int axis = 0; axis < 3; ++axis) {
                const double lo = std::min({points[0][axis],points[1][axis],points[2][axis]});
                const double hi = std::max({points[0][axis],points[1][axis],points[2][axis]});
                if (triangle.normal[axis] != 0)
                    overlap = overlap && lo >= solid.lo[axis] && hi <= solid.hi[axis];
                else
                    overlap = overlap && hi > solid.lo[axis] && lo < solid.hi[axis];
            }
            measures.excludesSolids = measures.excludesSolids && !overlap;
        }
    }
    return measures;
}

bool FlatSurface(Checks& checks, int axis, double height, const std::vector<BoxMotion>& solids,
                 GridWaterSurface& surface) {
    GridPhaseGeometry flat;
    GridCellPhase cells;
    MovingGridGeometry solid;
    std::string error;
    bool ok = BuildGridPhaseGeometry(kLayout,solids,axis,height,flat,error) &&
        BuildMovingGridGeometry(kLayout,solids,solid,error);
    if (ok) {
        // The two exact capacity expressions can round differently at a
        // full cell. Publish the canonical available endpoint volume.
        for (std::size_t i = 0; i < flat.water.size(); ++i)
            flat.water[i] = std::min(flat.water[i],solid.startCapacity[i]);
        ok = ReconstructGridCellPhase(kLayout,solids,axis,flat.water,cells,error) &&
            BuildGridWaterSurface(kLayout,solids,cells,surface,error);
    }
    if (!ok) std::printf("flat surface error %s\n",error.c_str());
    checks.Expect(ok,"authoritative flat volume extracts");
    return ok;
}

void ClosedAndFlat(Checks& checks) {
    GridWaterSurface surface;
    for (const double height : {0.,2.}) {
        if (!FlatSurface(checks,1,height,{},surface)) continue;
        checks.Expect(surface.triangles.empty(),"sealed completely dry or full grid has no free surface");
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!FlatSurface(checks,axis,1,{},surface)) continue;
        const auto m = Measure(surface,{});
        checks.Expect(m.totalArea == 4 && m.positiveArea[axis] == 4 && surface.triangles.size() == 32,
                      "grid-aligned waterline emits one interface in each column",m.totalArea);
        checks.Expect(m.valid && m.unique && m.internal,
                      "chart rotation retains positive winding and unique interior triangles");
    }
}

void SolidClips(Checks& checks) {
    for (const double bottom : {.625,.75,.754}) {
        const BoxMotion box{1,{.25,bottom,.25},{1.25,1.25,1},{}};
        const std::vector<BoxMotion> solids{box};
        GridWaterSurface surface;
        if (!FlatSurface(checks,1,.75,solids,surface)) continue;
        const auto m = Measure(surface,solids);
        const double expectedArea = bottom <= .75 ? 3.25 : 4;
        checks.Expect(std::abs(m.totalArea-expectedArea) < 1e-12 &&
                      std::abs(m.positiveArea[1]-expectedArea) < 1e-12,
                      bottom <= .75 ? "solid crossing or touching removes its exact top rectangle"
                                    : "4 mm gap above the surface leaves its full area",m.totalArea);
        checks.Expect(m.valid && m.unique && m.internal && m.excludesSolids,
                      "clipped water triangles never enter the box or duplicate a face");
    }
}

void StepsAndDroplets(Checks& checks) {
    const BoxMotion box{2,{.875,.796875,.25},{1.125,.84375,1.25},{}};
    const std::vector<BoxMotion> solids{box};
    GridPhaseGeometry low,high;
    MovingGridGeometry geometry;
    GridCellPhase phase;
    GridWaterSurface surface;
    std::string error;
    if (!BuildGridPhaseGeometry(kLayout,solids,1,.75,low,error) ||
        !BuildGridPhaseGeometry(kLayout,solids,1,.875,high,error) ||
        !BuildMovingGridGeometry(kLayout,solids,geometry,error)) {
        checks.Expect(false,"stepped volume seeds");
        return;
    }
    for (int k = 0; k < kLayout.cells[2]; ++k)
        for (int j = 0; j < kLayout.cells[1]; ++j)
            for (int i = 0; i < kLayout.cells[0]; ++i) {
                const auto cell = kLayout.Cell(i,j,k);
                const auto volume = i < 2 ? high.water[cell] : low.water[cell];
                high.water[cell] = std::min(volume,geometry.startCapacity[cell]);
            }
    const bool stepped = ReconstructGridCellPhase(kLayout,solids,1,high.water,phase,error) &&
        BuildGridWaterSurface(kLayout,solids,phase,surface,error);
    if (!stepped) std::printf("stepped surface error %s\n",error.c_str());
    checks.Expect(stepped,"height step with solid crossing extracts");
    if (stepped) {
        const auto m = Measure(surface,solids);
        constexpr double exposedSide = .125*2-.046875;
        checks.Expect(std::abs(m.positiveArea[0]-exposedSide) < 1e-12 &&
                      std::abs(m.positiveArea[1]-4) < 1e-12 &&
                      std::abs(m.totalArea-(4+exposedSide)) < 1e-12,
                      "vertical step faces point into low water side and clip solid exactly",m.positiveArea[0]);
        checks.Expect(m.valid && m.unique && m.internal && m.excludesSolids,
                      "vertical clipping preserves unique finite positive-area faces");
    }

    std::vector<double> water(kLayout.CellCount());
    water[kLayout.Cell(1,1,1)] = .125;
    const bool drop = ReconstructGridCellPhase(kLayout,{},1,water,phase,error) &&
        BuildGridWaterSurface(kLayout,{},phase,surface,error);
    checks.Expect(drop,"isolated interior full cell extracts");
    if (drop) {
        const auto m = Measure(surface,{});
        checks.Expect(surface.triangles.size() == 12 && m.totalArea == 1.5 &&
                      m.positiveArea == GridVec{.25,.25,.25} && m.negativeArea == m.positiveArea &&
                      m.valid && m.unique && m.internal,
                      "isolated water has six oriented sides and a downward free face",m.totalArea);
        const auto prior = surface.triangles;
        phase.waterPlane[kLayout.Cell(2,2,2)] = std::numeric_limits<double>::quiet_NaN();
        checks.Expect(!BuildGridWaterSurface(kLayout,{},phase,surface,error) && !error.empty() &&
                      surface.triangles.size() == prior.size() &&
                      surface.triangles[0].vertices == prior[0].vertices,
                      "nonfinite phase plane rejects without replacing mesh");
    }
}

} // namespace

int main() {
    Checks checks;
    ClosedAndFlat(checks);
    SolidClips(checks);
    StepsAndDroplets(checks);
    std::printf("SUMMARY grid water surface %d checks %d failures\n",checks.total,checks.failed);
    return checks.failed ? 1 : 0;
}
