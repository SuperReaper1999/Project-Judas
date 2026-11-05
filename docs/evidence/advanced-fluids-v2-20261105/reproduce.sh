#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${1:?Usage: reproduce.sh OUTPUT_DIRECTORY [SAMPLES]}
samples=${2:-1}
mkdir -p -- "$out"
out=$(cd -- "$out" && pwd)
s="$root/tools/prototypes/fluid_volume_grid"
e="$root/docs/evidence/advanced-fluids-v2-20261105"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$s")
cxx=${CXX:-c++}
for name in CutDynamicTransport CutRectangularOwners CutRegionGeometry CutHydroacousticGraph CutRegionGasTopology; do
    "$cxx" "${flags[@]}" -c "$s/$name.cpp" -o "$out/$name.o"
done
objects=("$out"/{CutDynamicTransport,CutRectangularOwners,CutRegionGeometry,CutHydroacousticGraph,CutRegionGasTopology}.o)
"$cxx" "${flags[@]}" "$s/CutDynamicTransportTests.cpp" "${objects[@]}" -o "$out/unit"
"$out/unit" | tee "$out/unit.log"
"$cxx" "${flags[@]}" -I"$root/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
    "$e/AuthoredDynamicProbe.cpp" "${objects[@]}" -o "$out/authored"
# This is an admission gate, not an expected-failure test. Current candidate
# returns nonzero because the original basins do not pass. Preserve that exit.
set +e
"$out/authored" "$samples" > "$out/authored.log" 2>&1
result=$?
set -e
cat "$out/authored.log"
printf '%s\n' "$result" > "$out/authored.exit"
exit "$result"
