#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output_dir=${1:?Usage: qualify_internal_transport.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output_dir"
output_dir=$(cd -- "$output_dir" && pwd)
cxx=${CXX:-c++}
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
objects=()
for source in CutRegionGeometry CutRegionGasTopology CutHydroacousticGraph CutConnectedOwners CutOwnedFlux CutOwnedProjection CutPhaseFronts CutDynamicTransport CutRectangularOwners; do
    object="$output_dir/$source.o"
    "$cxx" "${flags[@]}" -c "$source_dir/$source.cpp" -o "$object"
    objects+=("$object")
done
for suite in CutPhaseFrontsTests CutConnectedOwnersTests CutDynamicTransportTests; do
    "$cxx" "${flags[@]}" "$source_dir/$suite.cpp" "${objects[@]}" -o "$output_dir/$suite"
    "$output_dir/$suite" | tee "$output_dir/$suite.log"
done
