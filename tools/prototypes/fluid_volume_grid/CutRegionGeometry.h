#pragma once

#include "MovingGridGeometry.h"

#include <optional>

namespace judas::volume_grid {

struct CutRegion {
    GridVec lo, hi;
    std::size_t parent;
};

struct CutRegionFace {
    std::size_t left, right;
    int axis;
    GridVec lo, hi;
    double startArea, endArea, meanArea, sweptVolume, length;
};

struct CutRegionChart {
    GridLayout layout;
    std::vector<CutRegion> regions;
    std::vector<std::size_t> offsets;
    std::vector<CutRegionFace> faces;
    std::vector<std::size_t> movingRegions, movingFaces;
    std::optional<BoxMotion> moving;
    GridVec minTravel{}, maxTravel{};
};

struct CutRegionGeometry {
    std::vector<double> startCapacity, endCapacity;
    std::vector<CutRegionFace> faces;
    std::vector<SolidPatch> patches;
    std::optional<BoxMotion> motion;
    double maxSweepError = 0., maxPatchError = 0.;
};

// The static union is carved out of local rectilinear pieces. One translating
// box may use the retained region indices while its endpoints stay in the
// precompiled envelope. The envelope must remain disjoint from the static
// union. A sweep that splits one retained region into disjoint fluid pockets
// is rejected. The chart boundary is sealed.
bool PrepareCutRegionChart(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                           const std::optional<BoxMotion>& moving, const GridVec& minTravel,
                           const GridVec& maxTravel, CutRegionChart& output, std::string& error);

bool BuildCutRegionGeometry(const CutRegionChart& chart, const GridVec& startOffset,
                            const GridVec& displacement, CutRegionGeometry& output,
                            std::string& error);

inline bool BuildCutRegionGeometry(const CutRegionChart& chart, const GridVec& displacement,
                                   CutRegionGeometry& output, std::string& error) {
    return BuildCutRegionGeometry(chart, {}, displacement, output, error);
}

// All bounds and returned measures use chart metres. The moving pose time is
// normalized to the interval and the chart already excludes stationary solids.
double CutRegionClippedVolume(const CutRegion& region, const std::optional<BoxMotion>& motion,
                              const GridVec& clipLo, const GridVec& clipHi, double time = 0.);

bool ReconstructCutRegionWaterPlanes(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                                    int phaseAxis, const std::vector<double>& startWater,
                                    std::vector<double>& waterPlanes, std::string& error);

struct CutRegionAperture {
    double start = 0., mean = 0.;
};

struct CutRegionFacePhase {
    std::array<CutRegionAperture, 2> wet, dry;
};

// Average over [begin,end] in normalized interval time. The normal coordinate
// in clipLo and clipHi is ignored.
CutRegionAperture MeasureCutRegionAperture(const CutRegionFace& face,
                                         const std::optional<BoxMotion>& motion,
                                         const GridVec& clipLo, const GridVec& clipHi,
                                         double begin = 0., double end = 1.);

// Each side uses its own phase trace. A horizontal face exactly at a plane
// reads the phase from the donor side. Wet and dry use independent area clips.
CutRegionFacePhase MeasureCutRegionFacePhase(const CutRegionFace& face,
                                           const std::optional<BoxMotion>& motion, int phaseAxis,
                                           double leftPlane, double rightPlane);

} // namespace judas::volume_grid
