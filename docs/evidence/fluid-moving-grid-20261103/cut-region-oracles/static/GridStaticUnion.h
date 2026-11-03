#pragma once

#include "MovingGridGeometry.h"

namespace judas::volume_grid {

struct GridStaticPartition {
    std::array<std::vector<double>, 3> edges; // Chart grid units.
    std::vector<unsigned char> open;
};

struct GridStaticUnion {
    static constexpr std::size_t uncut = std::numeric_limits<std::size_t>::max();
    GridLayout layout;
    std::vector<BoxMotion> stationary;
    std::vector<double> capacity;
    std::array<std::vector<double>, 3> aperture;
    std::vector<GridStaticPartition> partitions;
    std::vector<std::size_t> cellPartition;
    std::array<std::vector<std::size_t>, 3> facePartition;
    std::size_t cachedPieces = 0;
    std::size_t maximumLocalPieces = 0;
    std::size_t occupancyChecksBound = 0;
};

// Stationary chart-contained boxes may overlap, touch, or share owner IDs.
// This is union measure and connectivity, with no per-owner traction data.
// Disconnected free pockets in one coarse cell are not representable by one
// material volume. Reject those and over-budget cuts transactionally.
bool BuildGridStaticUnion(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                          GridStaticUnion& output, std::string& error);

// Compose a frozen static union and a moving AABB set. Dynamic paths must
// remain strictly separated from each other and all static boxes. Output
// contains dynamic wall patches and sweeps only. This pressure-geometry
// handoff does not certify combined pocket topology or phase storage.
bool BuildGridStaticMovingUnion(const GridStaticUnion& stationary,
                                const std::vector<BoxMotion>& moving,
                                MovingGridGeometry& output, std::string& error);

// Intersect arbitrary chart-metre bounds with one coarse cell's free volume.
// The same query serves a half-cell and a local phase-plane clip.
bool ClipGridStaticCellVolume(const GridStaticUnion& stationary, const std::array<int, 3>& cell,
                              const GridVec& clipLo, const GridVec& clipHi,
                              double& volume, std::string& error);

// Query an open rectangle in a shared coarse face or on any interior plane
// of the given cell. Use chart metres. Ignore the normal component of the
// clipping bounds. A coincident solid surface is closed.
bool ClipGridStaticFaceArea(const GridStaticUnion& stationary, const std::array<int, 3>& cell,
                            int normal, double chartPlane, const GridVec& clipLo,
                            const GridVec& clipHi, double& area, std::string& error);

} // namespace judas::volume_grid
