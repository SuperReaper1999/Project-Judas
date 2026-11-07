#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
binary="$repo/build/judas_game_water_lab"
if [[ ! -x "$binary" ]]; then
    printf '%s\n' 'Build the source preview first:' \
        'cmake -S . -B build -DJUDAS_BUILD_GAME_WATER_LAB=ON' \
        'cmake --build build --target judas_game_water_lab -j2' >&2
    exit 1
fi
export JUDAS_ENGINE_ROOT="$repo"
export JUDAS_GPU_DIAGNOSTICS=${JUDAS_GPU_DIAGNOSTICS:-1}
exec "$binary" "$@"
