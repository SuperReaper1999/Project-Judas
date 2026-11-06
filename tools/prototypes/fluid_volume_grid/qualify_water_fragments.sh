#!/usr/bin/env bash
# A RED material-advance gate is preserved until complete authored intervals pass.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
output=${1:?Usage: qualify_water_fragments.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$source_dir/qualify_internal_transport.sh" "$output/controls"
mapfile -t objects < "$output/controls/objects.list"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then
    flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
fi
for probe in WaterFragmentTests WaterFragmentBasinProbe; do
    "${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
        -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
        "$source_dir/$probe.cpp" "${objects[@]}" -o "$output/$probe"
done
failed=0
for mode in flow level rotated; do
    command=("$output/WaterFragmentBasinProbe" "$mode")
    if [[ $mode == flow ]]; then command=("$output/WaterFragmentTests"); fi
    set +e
    /usr/bin/time -v -o "$output/$mode.resource" \
        timeout "${JUDAS_FLUID_PROBE_SECONDS:-120}s" "${command[@]}" \
        > "$output/$mode.log" 2>&1
    result=$?
    set -e
    printf '%s\n' "$result" > "$output/$mode.exit"
    if [[ $result != 0 ]]; then failed=1; fi
done
exit "$failed"
