#!/usr/bin/env bash
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
cd "$repo"
b=$(realpath "${1:?build directory}")
out=$(realpath -m "${2:?new output directory}")
[[ ! -e "$out" ]]
mkdir -p "$out/user"/{data,config,cache}
export XDG_DATA_HOME="$out/user/data" XDG_CONFIG_HOME="$out/user/config" XDG_CACHE_HOME="$out/user/cache"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 JUDAS_ENGINE_ROOT="$repo"
unset DISPLAY WAYLAND_DISPLAY
failed=0
run() {
    local label=$1
    shift
    if timeout 90s "$@" > "$out/$label.log" 2>&1; then
        printf 'PASS %s\n' "$label"
    else
        failed=$((failed+1))
        printf 'FAIL %s\n' "$label"
        tail -20 "$out/$label.log"
    fi
}
run hierarchy "$b/judas_editor_creation_menu_tests"
run asset-actions "$b/judas_editor_asset_actions_tests"
run project-settings "$b/judas_project_settings_editor_tests"
run file-browser "$b/judas_editor_file_browser_tests"
run script-workflow "$b/judas_editor_script_workflow_tests"
run preferences "$b/judas_editor_preferences_tests" "$out/prefs"
run terrain "$b/judas_terrain_authoring_tests" "$out/terrain-fixtures"
run volume-terrain "$b/judas_volume_terrain_tests" "$out/volume-fixtures"
run model-reimport "$b/judas_model_reimport_tests" "$out/reimport"
run fresh-2d "$b/judas_2d_game_workflow_tests" projects/tilemap_workshop/Assets/tiles.png "$out/fresh-2d-project"
run fluid-policy "$b/judas_project_fluid_policy_tests"
run gate-registration ctest --test-dir "$b" -R '^prealpha\.(editor-hierarchy|editor-asset-actions|project-settings-editor|gate-registration)$' --output-on-failure
exit "$failed"
