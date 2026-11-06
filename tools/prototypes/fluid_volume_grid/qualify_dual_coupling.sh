#!/usr/bin/env bash
# This is an integration gate, NOT a claim that the current candidate passes.
# Preserve failed basin receipts and return failure if either basin rejects.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
output=${1:?Usage: qualify_dual_coupling.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$source_dir/qualify_internal_transport.sh" "$output/controls"
mapfile -t objects < "$output/controls/objects.list"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
seconds=${JUDAS_FLUID_PROBE_SECONDS:-180}
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then
 flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
 seconds=${JUDAS_FLUID_PROBE_SECONDS:-600}
fi
"${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
 -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
 "$source_dir/CutCoupledBasinProbe.cpp" "${objects[@]}" -o "$output/basin"
failed=0
for mode in filled original; do
 steps=3
 if [[ $mode == original ]]; then steps=1; fi
 for orientation in level rotated; do
  name="$mode-$orientation"
  set +e
  /usr/bin/time -v -o "$output/$name.resource" timeout "${seconds}s" \
   "$output/basin" "$mode" "$orientation" "$steps" orthogonal > "$output/$name.log" 2>&1
  result=$?
  set -e
  printf '%s\n' "$result" > "$output/$name.exit"
  if [[ $result != 0 ]]; then failed=1; fi
 done
done
exit "$failed"
