#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${1:?Usage: reproduce.sh OUTPUT_DIRECTORY}
mkdir -p -- "$out"
out=$(cd -- "$out" && pwd)
s="$root/tools/prototypes/fluid_volume_grid"
e="$root/docs/evidence/fluid-connected-owners-20261105"
cxx=${CXX:-c++}
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$s")
objects=()
for name in CutConnectedOwners CutOwnedProjection CutHydroacousticGraph CutRegionGeometry CutRegionGasTopology; do
 "$cxx" "${flags[@]}" -c "$s/$name.cpp" -o "$out/$name.o"
 objects+=("$out/$name.o")
done
"$cxx" "${flags[@]}" "$s/CutConnectedOwnersTests.cpp" "${objects[@]}" -o "$out/controls"
"$out/controls" > "$out/controls.log"
"$cxx" "${flags[@]}" -I"$root/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" "$e/AuthoredOwnedProbe.cpp" "${objects[@]}" -o "$out/authored"
# Keep rejection as a failed basin-admission gate. Never count it as an advance.
set +e
"$out/authored" > "$out/authored.log" 2>&1
result=$?
set -e
printf '%s\n' "$result" > "$out/authored.exit"
cat "$out/controls.log" "$out/authored.log"
exit "$result"
