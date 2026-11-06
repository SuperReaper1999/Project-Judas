#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
evidence="$repo/docs/evidence/fluid-sustained-pressure-20261106"
output=${1:?Usage: qualify_sustained_pressure.sh OUTPUT_DIRECTORY [INTERVALS]}
intervals=${2:-120}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$source_dir/qualify_internal_transport.sh" "$output/controls"
mapfile -t objects < "$output/controls/objects.list"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then
 flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
fi
fixture="$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions"
for name in SustainedPressureReplay SustainedPressureProbe CutFastResidualOracle FastAuthoredPressureProbe; do
 "${CXX:-c++}" "${flags[@]}" -I"$source_dir" -I"$fixture" \
  "$source_dir/$name.cpp" "${objects[@]}" -o "$output/$name"
done
python3 "$source_dir/qualify_fast_residual.py" "$output/CutFastResidualOracle"
gzip -dc "$evidence/third-state.bin.gz" > "$output/third-state.bin"
# Verify the input fixture without requiring a Git checkout or external research.
python3 - "$evidence/STATE_SHA256SUMS.txt" "$output/third-state.bin" <<'PY'
import hashlib, pathlib, sys
expected=pathlib.Path(sys.argv[1]).read_text().splitlines()[0].split()[0]
if hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest()!=expected:
 raise SystemExit("retained-state checksum mismatch")
PY
run() {
 local name=$1 budget=$2
 shift 2
 set +e
 /usr/bin/time -v -o "$output/$name.resource" timeout "$budget" "$@" > "$output/$name.log" 2>&1
 local status=$?
 set -e
 printf '%s\n' "$status" > "$output/$name.exit"
 return "$status"
}
run replay 180s "$output/SustainedPressureReplay" replay "$output/third-state.bin"
run sustained "${JUDAS_FLUID_QUALIFICATION_SECONDS:-720}s" "$output/SustainedPressureProbe" "$intervals"
