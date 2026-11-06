#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
output=${1:?Usage: qualify_fast_pressure.sh OUTPUT_DIRECTORY [SAMPLES]}
samples=${2:-3}
mkdir -p -- "$output/reference-source"
output=$(cd -- "$output" && pwd)
tar -xzf "$repo/docs/evidence/fluid-fast-transport-20261106/slow-reference-source.tar.gz" -C "$output/reference-source"
(
 cd -- "$output/reference-source"
 sha256sum -c docs/evidence/fluid-local-pressure-20261106/SOURCE_SHA256SUMS.txt > "$output/reference-source-checks.log"
)
reference="$output/reference-source/tools/prototypes/fluid_volume_grid"
fixture="$output/reference-source/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions"
bash "$reference/qualify_internal_transport.sh" "$output/reference-controls"
bash "$source_dir/qualify_internal_transport.sh" "$output/candidate-controls"
# A source-local quoted include takes precedence over -I. Copy the common
# driver away from current headers so the reference uses only archived headers.
cp -- "$source_dir/FastAuthoredPressureProbe.cpp" "$output/ReferenceBasinProbe.cpp"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
"${CXX:-c++}" "${flags[@]}" -DJUDAS_SLOW_REFERENCE -I"$reference" -I"$fixture" \
 "$output/ReferenceBasinProbe.cpp" "$output/reference-controls/"*.o -o "$output/reference-basins"
"${CXX:-c++}" "${flags[@]}" -I"$source_dir" -I"$fixture" \
 "$source_dir/FastAuthoredPressureProbe.cpp" "$output/candidate-controls/"*.o -o "$output/candidate-basins"
"${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
 "$source_dir/CutFastResidualOracle.cpp" "$output/candidate-controls/"*.o -o "$output/residual-oracle"
python3 "$source_dir/qualify_fast_residual.py" "$output/residual-oracle"
python3 "$source_dir/benchmark_fast_pressure.py" "$output/reference-basins" "$output/candidate-basins" "$output/matched" --samples "$samples"
# Build the separate, deliberately non-green sustained-basin reproduction.
"${CXX:-c++}" "${flags[@]}" -I"$source_dir" -I"$fixture" \
 "$repo/docs/evidence/fluid-fast-transport-20261106/ContinuationProbe.cpp" \
 "$output/candidate-controls/"*.o -o "$output/continuation-basins"
