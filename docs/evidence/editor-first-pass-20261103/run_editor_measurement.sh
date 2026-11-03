#!/usr/bin/env bash
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
home="$repo/.cache/editor-first-pass-20261103"
bin=$(realpath "${1:?editor binary}")
out=$(realpath -m "${2:?output directory}")
fixture=${3:?fixture name}
mkdir -p "$out"
case_dir=$(mktemp -d "$home/editor-run.XXXXXX")
trap 'rm -r -- "$case_dir"' EXIT
cp -a "$home/editor-fixtures/$fixture/." "$case_dir/"
mkdir -p "$case_dir/user"/{data,config,cache}
env -u DISPLAY -u WAYLAND_DISPLAY \
    SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 \
    XDG_DATA_HOME="$case_dir/user/data" XDG_CONFIG_HOME="$case_dir/user/config" XDG_CACHE_HOME="$case_dir/user/cache" \
    JUDAS_ENGINE_ROOT="$repo" JUDAS_EDITOR_AUTOTEST="$out/editor" \
    JUDAS_EDITOR_AUTOTEST_PROFILER=1 JUDAS_PROFILE=1 JUDAS_PROFILE_SPIKE_MS=25 JUDAS_PROFILE_OUTPUT="$out/profile.json" \
    /usr/bin/time -v -o "$out/time.txt" timeout 90s "$bin" "$case_dir/sample.judasproj" > "$out/editor.log" 2>&1
rg -q 'authored scene after play/stop is IDENTICAL' "$out/editor.log"
rg -q 'save ok' "$out/editor.log"
rg -q 'play frame:.* [1-9][0-9]* bodies' "$out/editor.log"
