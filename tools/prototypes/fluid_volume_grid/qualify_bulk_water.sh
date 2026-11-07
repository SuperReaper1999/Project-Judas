#!/usr/bin/env bash
# The authored-basin gate remains RED until complete sustained advances pass.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
output=${1:?Usage: qualify_bulk_water.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$source_dir/qualify_internal_transport.sh" "$output/controls"
mapfile -t objects < "$output/controls/objects.list"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then
    flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
fi
for source in WaterBulkStorage WaterBulkReconcile WaterBulkCondensation WaterBulkProjection; do
    object="$output/$source.o"
    "${CXX:-c++}" "${flags[@]}" -c "$source_dir/$source.cpp" -o "$object"
    objects+=("$object")
done
printf '%s\n' "${objects[@]}" > "$output/objects.list"
for probe in WaterBulkTests WaterBulkCondensationTests WaterBulkBasinProbe; do
    "${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
        -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
        "$source_dir/$probe.cpp" "${objects[@]}" -o "$output/$probe"
done
set +e
"$output/WaterBulkTests" > "$output/controls.log" 2>&1
result=$?
printf '%s\n' "$result" > "$output/controls.exit"
"$output/WaterBulkCondensationTests" > "$output/condensation.log" 2>&1
condensation=$?
printf '%s\n' "$condensation" > "$output/condensation.exit"
"$output/WaterBulkBasinProbe" 0 level > "$output/invalid-cli.log" 2>&1
negative=$?
printf '%s\n' "$negative" > "$output/invalid-cli.exit"
set -e
failed=0
if [[ $result != 0 || $negative != 2 || $condensation != 0 ]]; then failed=1; fi
if [[ ${JUDAS_FLUID_BULK_BASINS:-1} == 1 ]]; then
    for mode in level rotated; do
        set +e
        /usr/bin/time -v -o "$output/$mode.resource" \
            timeout "${JUDAS_FLUID_PROBE_SECONDS:-120}s" \
            "$output/WaterBulkBasinProbe" "${JUDAS_FLUID_BASIN_INTERVALS:-10}" "$mode" \
            > "$output/$mode.log" 2>&1
        result=$?
        printf '%s\n' "$result" > "$output/$mode.exit"
        set -e
        if [[ $result != 0 ]]; then failed=1; fi
    done
else
    printf '%s\n' 'Authored basins not run; controls-only qualification.' > "$output/basins-not-run.txt"
fi
exit "$failed"
