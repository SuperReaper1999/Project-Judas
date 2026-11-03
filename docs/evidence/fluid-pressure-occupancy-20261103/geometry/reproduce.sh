#!/bin/bash
set -euo pipefail
stage=.cache/fluid-static-balanced-20261103
analysis="$stage/deep-geometry"
for directory in .cache/fluid-zero-settling-kernel/clean/player-baseline-strict "$stage/player-discover-0.20"; do
    echo "$directory"
    read -ra center <<< "$(jq -r '.fixtures[]|select(.name=="density-20-1000")|.initial_position|@tsv' "$directory/results.json")"
    for spacing in .02 .01; do
        echo "spacing=$spacing"
        "$analysis/measure" "$directory/density-20-1000-warmup-particles.csv" "${center[@]}" "$spacing"
    done
done
