#!/usr/bin/env bash
set -euo pipefail

root=$(git rev-parse --show-toplevel)
evidence=$(cd "$(dirname "$0")" && pwd)
output=$(mktemp -d)
trap 'rm -rf -- "$output"' EXIT
cd "$root"

source=tools/prototypes/fluid_volume_grid
snapshot="$evidence/../remap-native-continuation/failed/native-root-input.bin"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
modules=(MovingVolumeMomentum HalfDualVolumeMomentum GridHalfCellGeometry
         GridGeometricMomentum GridConservativeRemap GridPhaseFluxRepair)
objects=()
for module in "${modules[@]}"; do
    object="$output/$module.o"
    "${CXX:-c++}" "${flags[@]}" -c "$source/$module.cpp" -o "$object"
    objects+=("$object")
done

"${CXX:-c++}" "${flags[@]}" -I"$source" "$evidence/pressure/LocalRectCutPressure.cpp" \
    "${objects[@]}" -o "$output/local-pressure"
"$output/local-pressure" "$snapshot"
python3 "$evidence/pressure/SliverOracle.py"
python3 "$evidence/pressure/MovingOwnershipOracle.py"

static="$evidence/static"
phase="$static/phase-geometry"
gate="$static/pressure-gate"
"${CXX:-c++}" "${flags[@]}" -I"$source" "$static/GridStaticUnion.cpp" \
    "$static/GridStaticUnionTests.cpp" -o "$output/static-geometry"
"$output/static-geometry"
"${CXX:-c++}" "${flags[@]}" -I"$source" "$static/GridStaticUnion.cpp" \
    "$phase/GridStaticMovingPhase.cpp" "$phase/GridStaticMovingPhaseTests.cpp" \
    -o "$output/static-phase"
"$output/static-phase"

# The authored-wall adapter is pinned to the pressure kernel used in the
# original run. The larger release driver and original engine tests stay intact.
git archive 8fd9a2de1156b262782d8e252f9426d4320a6e91 "$source" | tar -x -C "$output"
pinned="$output/$source"
pinned_objects=()
for module in "${modules[@]}"; do
    object="$output/pinned-$module.o"
    "${CXX:-c++}" "${flags[@]}" -c "$pinned/$module.cpp" -o "$object"
    pinned_objects+=("$object")
done
"${CXX:-c++}" "${flags[@]}" -I"$pinned" "$static/GridStaticUnion.cpp" \
    "$phase/GridStaticMovingPhase.cpp" "$gate/StaticPoolPressureGate.cpp" \
    "${pinned_objects[@]}" -o "$output/static-pressure"
status=0
"$output/static-pressure" > "$output/static-pressure.log" || status=$?
cat "$output/static-pressure.log"
test "$status" -eq 1
test "$(awk '/^FAIL projection: moving-volume transport exceeds donor capacity/ {count++} END {print count+0}' "$output/static-pressure.log")" -eq 4
test "$(awk '/^FAIL / {count++} END {print count+0}' "$output/static-pressure.log")" -eq 4

"${CXX:-c++}" "${flags[@]}" "$evidence/atmosphere/AtmosphericColumn.cpp" -o "$output/atmosphere"
"$output/atmosphere"
"${CXX:-c++}" "${flags[@]}" "$evidence/atmosphere/reservoir/AtmosphericReservoir.cpp" \
    -o "$output/reservoir"
"$output/reservoir"
