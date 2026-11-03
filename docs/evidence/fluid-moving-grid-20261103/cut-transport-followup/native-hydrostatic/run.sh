#!/usr/bin/env bash
set -euo pipefail
source_root=$(git rev-parse --show-toplevel)
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
results=$(mktemp -d /tmp/judas-native-cut-root.XXXXXX)
sources=(--source "$here/NativeContinuousEndpoint.cpp")
for name in CutRegionGeometry CutRegionStep CutRegionPressure CutRegionPhase CutRegionMomentum MovingVolumeGrid GridPhaseFluxRepair GridConservativeRemap GridHalfCellGeometry GridGeometricMomentum HalfDualVolumeMomentum MovingVolumeMomentum; do
    sources+=(--source "$source_root/tools/prototypes/fluid_volume_grid/$name.cpp")
done
python3 "$here/build.py" --repo "$source_root" --exact --output "$results/native-continuous" "${sources[@]}" "$@"
sha256sum "$results/native-continuous" > "$results/binary-sha256.txt"
printf 'probe\texit\n' > "$results/results.tsv"
probe() {
    local label=$1 result=0
    shift
    "$results/native-continuous" "$@" > "$results/$label.log" 2>&1 || result=$?
    printf '%s\t%s\n' "$label" "$result" >> "$results/results.tsv"
    tail -1 "$results/$label.log"
}
probe global
probe local --local-remap
probe geometric --geometry-remap
probe hybrid --geometry-after-first
printf 'Logs preserved in %s\n' "$results"
