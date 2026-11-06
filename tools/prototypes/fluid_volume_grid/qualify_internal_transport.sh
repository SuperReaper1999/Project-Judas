#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output_dir=${1:?Usage: qualify_internal_transport.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output_dir"
output_dir=$(cd -- "$output_dir" && pwd)
cxx=${CXX:-c++}
optimization=-O3
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then optimization=-O1; fi
flags=(-std=c++17 "$optimization" -Wall -Wextra -Wpedantic -Werror)
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer); fi
objects=()
for source in CutRegionGeometry CutRegionGasTopology CutHydroacousticGraph CutConnectedOwners CutOwnedFlux CutOwnedProjection CutPhaseFronts CutSpatialTransport CutCondensedPressure CutDirectedTransport CutExactResidual CutFastResidual CutFastDirectedTransport CutLocalPressure CutDynamicTransport CutRectangularOwners; do
    object="$output_dir/$source.o"
    "$cxx" "${flags[@]}" -c "$source_dir/$source.cpp" -o "$object"
    objects+=("$object")
done
for suite in CutLocalContinuationTests CutFastTransportTests CutCondensedPressureTests CutDirectedTransportTests CutExactResidualTests CutLocalPressureTests CutSpatialTransportTests CutPhaseFrontsTests CutConnectedOwnersTests CutDynamicTransportTests; do
    "$cxx" "${flags[@]}" "$source_dir/$suite.cpp" "${objects[@]}" -o "$output_dir/$suite"
    "$output_dir/$suite" | tee "$output_dir/$suite.log"
done
