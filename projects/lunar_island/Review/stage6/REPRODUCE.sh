#!/bin/sh
# Run from repository root with matching Release engine/tools and a Linux GL desktop.
set -eu
project=projects/lunar_island/lunar_island.judasproj
proof=${1:-/tmp/lunar-stage6-reproduction}
userdata=${2:-/tmp/lunar-stage6-reproduction-data}
[ ! -e "$proof" ] || { echo 'Choose a fresh proof directory'; exit 2; }
mkdir -p "$proof"
for rules in inventory encounters vehicles; do
    node "projects/lunar_island/tests/$rules.test.js" > "$proof/$rules.log"
done
./build/judas_script_check "$project" > "$proof/scripts.log"
python3 projects/lunar_island/tests/build_observer.py --output "$proof/observer"
run() {
    SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata-$3" "$proof/observer" "$project" "$1" "$proof/$2" > "$proof/$1.log" 2>&1
}
run helicopter-cruise flight flight
run onboarding ux ux
run flightwrite flight-save flight-save
run flightread flight-save flight-save
run land-helicopter heli heli
run land-plane plane plane
run drive-plane plane-drive plane-drive
run drive-car car car
run worldwrite world world
run worldread world world
run write housing housing
run read housing housing
run cargo cargo cargo
run cargoread cargo cargo
run harvestwrite harvest harvest
run harvestread harvest harvest
run performance performance performance
# Actual application/input/UI/physics. Only test placement and aim reach fixtures.
# Do not change runtime source between a writer and fresh-process reader.
# helicopter-cruise uses 90-Hz rendered frames with unchanged 60-Hz physics.
# Its original failure and intermediate failed hover experiment remain preserved.
