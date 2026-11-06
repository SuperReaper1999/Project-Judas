#!/usr/bin/env bash
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
output=${1:?Usage: reproduce.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$repo/tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh" "$output/controls"
"${CXX:-c++}" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
 -I"$repo/tools/prototypes/fluid_volume_grid" \
 -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
 "$repo/docs/evidence/fluid-spatial-transport-20261106/AuthoredSpatialProbe.cpp" \
 "$output/controls/"*.o -o "$output/authored"
set +e
/usr/bin/time -v -o "$output/resource.log" "$output/authored" > "$output/authored.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$output/authored.exit"
# Original pressure proposals still reject. Passing the isolated spatial
# controls must not hide that the basin qualification remains unsuccessful.
exit "$status"
