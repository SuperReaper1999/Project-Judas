#!/usr/bin/env bash
# The whole-advance gate deliberately remains RED while the original basins reject.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
output=${1:?Usage: qualify_water_volume.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$source_dir/qualify_internal_transport.sh" "$output/controls"
mapfile -t objects < "$output/controls/objects.list"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then
    flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
fi
"${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
    -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
    "$source_dir/WaterVolumeBasinProbe.cpp" "${objects[@]}" -o "$output/basins"
"${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
    "$source_dir/WaterVolumeFlowProbe.cpp" "${objects[@]}" -o "$output/flow"
failed=0
for mode in pressure advance flow; do
    command=("$output/basins" "$mode")
    if [[ $mode == flow ]]; then command=("$output/flow"); fi
    set +e
    /usr/bin/time -v -o "$output/$mode.resource" \
        timeout "${JUDAS_FLUID_PROBE_SECONDS:-180}s" "${command[@]}" \
        > "$output/$mode.log" 2>&1
    result=$?
    set -e
    printf '%s\n' "$result" > "$output/$mode.exit"
    if [[ $result != 0 ]]; then failed=1; fi
done
exit "$failed"
