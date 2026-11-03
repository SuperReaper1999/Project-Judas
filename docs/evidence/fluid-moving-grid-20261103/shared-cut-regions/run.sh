#!/usr/bin/env bash
set -euo pipefail

probe=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=$(mktemp -d /tmp/judas-cut-basin-repro.XXXXXX)
sources=()
for source in CutRegionGeometry CutRegionStep CutRegionPressure CutRegionPhase CutRegionMomentum MovingVolumeGrid GridPhaseFluxRepair GridConservativeRemap GridHalfCellGeometry GridGeometricMomentum HalfDualVolumeMomentum MovingVolumeMomentum; do
    sources+=(--source "$repo/tools/prototypes/fluid_volume_grid/$source.cpp")
done

git -C "$repo" rev-parse HEAD > "$out/source-head.txt"
sha256sum "$repo"/tools/prototypes/fluid_volume_grid/{CutRegion*,MovingVolumeGrid.cpp,MovingVolumePressureInternal.h} > "$out/solver-source-sha256.txt"
python3 "$probe/build.py" --repo "$repo" --output "$out/native-controls" "$@"
python3 "$probe/build.py" --repo "$repo" --output "$out/native-wall" \
    --source "$probe/NativeWallGeometry.cpp" "$@"
python3 "$probe/build.py" --repo "$repo" --output "$out/shared-step" \
    --source "$probe/BasinSharedStep.cpp" "${sources[@]}" "$@"
sha256sum "$out/native-controls" "$out/native-wall" "$out/shared-step" > "$out/binary-sha256.txt"
printf 'probe\texit\n' > "$out/results.tsv"
run() {
    local name=$1 executable=$2 result=0
    shift 2
    "$out/$executable" "$@" > "$out/$name.log" 2>&1 || result=$?
    printf '%s\t%s\n' "$name" "$result" >> "$out/results.tsv"
    cat "$out/$name.log"
}
run native native-controls
run native-wall-geometry native-wall
run authored-025-warmup shared-step
run authored-025-direct shared-step --warmup 0
run authored-025-timestep-control shared-step --timestep-sweep
run authored-020-rotated-literal shared-step --rotated
run authored-020-rotated-conservative shared-step --rotated --conservative-seed
run authored-020-rotated-conservative-direct shared-step --rotated --conservative-seed --warmup 0
run empty-020-native shared-step --rotated --dry-control --warmup 0
run empty-025-native shared-step --dry-control --warmup 0
run equilibrium-025-static shared-step --equilibrium-control --warmup 1 --steps 0
run equilibrium-025-native shared-step --equilibrium-control --warmup 0 --trace-proposals
run equilibrium-025-bracket shared-step --equilibrium-control --warmup 0 --trace-proposals --bracket-control
run equilibrium-020-static shared-step --rotated --equilibrium-control --warmup 1 --steps 0
run equilibrium-020-native shared-step --rotated --equilibrium-control --warmup 0
run rounded-wall-025-native shared-step --rounded-boundary-control --warmup 0
run exact-native-wall-020 shared-step --rotated --conservative-seed --native-wall-pose-control --warmup 0
printf 'Reproduction preserved in %s\n' "$out"
