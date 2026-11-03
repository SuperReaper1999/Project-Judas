#!/usr/bin/env bash
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
editor=$(realpath "${1:?editor}")
out=$(realpath -m "${2:?new evidence directory}")
[[ ! -e "$out" ]]
mkdir -p "$out"
for item in '2d-dark100 dark 1.0 grid' '2d-light150 light 1.5 side-view' '3d-dark150 dark 1.5 none' '3d-light100 light 1.0 none'; do
    read -r name mode scale template <<< "$item"
    result="$out/$name"
    pref="$result/user/data/Judas/Editor"
    mkdir -p "$pref" "$result/user/config" "$result/user/cache"
    printf '{"version":2,"current":{"name":"Current","panels":["assets"]},"appearance":{"mode":"%s","scale":%s},"layouts":[],"updates":{"automaticAtStartup":false,"channel":""}}\n' "$mode" "$scale" > "$pref/workspace.json"
    envs=()
    if [[ "$template" != none ]]; then envs+=("JUDAS_EDITOR_AUTOTEST_NEW_2D=$template"); fi
    if [[ "$scale" == 1.5 ]]; then envs+=("JUDAS_EDITOR_AUTOTEST_EXPORT_SAVED=1"); fi
    env -u DISPLAY -u WAYLAND_DISPLAY -u JUDAS_PROFILE -u JUDAS_PROFILE_OUTPUT \
        SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 \
        XDG_DATA_HOME="$result/user/data" XDG_CONFIG_HOME="$result/user/config" XDG_CACHE_HOME="$result/user/cache" \
        JUDAS_ENGINE_ROOT="$repo" JUDAS_EDITOR_AUTOTEST="$result/editor" \
        JUDAS_EDITOR_AUTOTEST_NEW_PROJECT="$result/project" JUDAS_EDITOR_AUTOTEST_EXPORT="$result/package" \
        "${envs[@]}" timeout 90s "$editor" > "$result/editor.log" 2>&1
    rg -q 'explicit export choice produces the requested package' "$result/editor.log"
    rg -q 'authored scene after play/stop is IDENTICAL' "$result/editor.log"
    rg -q 'save ok' "$result/editor.log"
    rg -q 'play frame:' "$result/editor.log"
    test -s "$result/editor.edit.png" && test -s "$result/editor.play.png"
    test -s "$result/package/game.judasproj"
    cmp -s "$(dirname "$editor")/judas" "$result/package/judas"
    python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p["appearance"]["mode"]==sys.argv[2] and p["appearance"]["scale"]==float(sys.argv[3])' "$pref/workspace.json" "$mode" "$scale"
    mv -- "$result/package" "$result/relocated-package"
    printf 'FRAMES 30\nFRAME_SECONDS 0.0166666667\nSCREENSHOT 20 %s/relocated.png\n' "$result" > "$result/route.txt"
    (cd /tmp && env -u DISPLAY -u WAYLAND_DISPLAY -u JUDAS_ENGINE_ROOT \
        SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 \
        XDG_DATA_HOME="$result/user/data" XDG_CONFIG_HOME="$result/user/config" XDG_CACHE_HOME="$result/user/cache" \
        JUDAS_TEST_SCRIPT="$result/route.txt" JUDAS_WINDOW_SIZE=640x480 timeout 30s \
        "$result/relocated-package/judas" "$result/relocated-package/game.judasproj" > "$result/relocated.log" 2>&1)
    test -s "$result/relocated.png"
    printf 'PASS %s %s %s %s\n' "$name" "$mode" "$scale" "$template"
done
