#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output_dir=${1:?Usage: qualify_moving_grid.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output_dir"
output_dir=$(cd -- "$output_dir" && pwd)
cxx=${CXX:-c++}
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)

objects=()
for source in MovingVolumeGrid MovingVolumeMomentum HalfDualVolumeMomentum CoupledVolumeBox \
    GridWaterSurface GridOrientedGeometry GridOrientedSweep OrientedMovingVolume GridHalfCellGeometry \
    GridGeometricMomentum GridConservativeRemap GridPhaseFluxRepair GridParcelSeed; do
    object="$output_dir/$source.o"
    "$cxx" "${flags[@]}" -c "$source_dir/$source.cpp" -o "$object"
    objects+=("$object")
done

for suite in GridPhaseGeometryTests GridCellPhaseTests GridWaterQueryTests GridPressureReactionTests \
    PlanarMovingVolumeTests EvolvingVolumeTests MovingVolumeMomentumTests \
    HalfDualVolumeMomentumTests MovingGridTests GridWaterSurfaceTests \
    GridOrientedGeometryTests GridOrientedSweepTests OrientedMovingVolumeTests GridHalfCellGeometryTests \
    ExposureVolumePressureTests GridConservativeRemapTests GridPhaseFluxRepairTests \
    GridGeometricMomentumTests GridParcelSeedTests; do
    "$cxx" "${flags[@]}" "$source_dir/$suite.cpp" "${objects[@]}" -o "$output_dir/$suite"
    arguments=()
    if [[ "$suite" == PlanarMovingVolumeTests ]]; then
        arguments+=(--strict-phase-flux)
    fi
    "$output_dir/$suite" "${arguments[@]}" | tee "$output_dir/$suite.log"
done
