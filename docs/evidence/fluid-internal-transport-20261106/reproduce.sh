#!/usr/bin/env bash
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
evidence="$repo/docs/evidence/fluid-internal-transport-20261106"
output=${1:?Usage: reproduce.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$repo/tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh" "$output/controls"
cxx=${CXX:-c++}
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
includes=(-I"$repo/tools/prototypes/fluid_volume_grid" -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions")
"$cxx" "${flags[@]}" "${includes[@]}" "$evidence/AuthoredTransportProbe.cpp" "$output/controls/"*.o -o "$output/authored"
objects=()
for object in "$output/controls/"*.o; do
    [[ "$object" == */CutPhaseFronts.o ]] || objects+=("$object")
done
# The white-box diagnostic includes the prototype implementation to measure
# its private integrator directly. Do not link a second copy of that module.
"$cxx" "${flags[@]}" "${includes[@]}" "$evidence/CofaceFrontDiagnostic.cpp" "${objects[@]}" -o "$output/coface"
set +e
/usr/bin/time -v -o "$output/authored-resource.log" "$output/authored" > "$output/authored.log" 2>&1
basin_status=$?
"$output/coface" > "$output/coface.log" 2>&1
coface_status=$?
set -e
printf '%s\n' "$basin_status" > "$output/authored.exit"
printf '%s\n' "$coface_status" > "$output/coface.exit"
printf 'Original basin status: %s; coface status: %s (both currently fail; see raw logs).\n' "$basin_status" "$coface_status"
# A full basin qualification remains a failure. The passing scoped controls
# must not turn that result into a green fluid-backend qualification.
exit "$basin_status"
