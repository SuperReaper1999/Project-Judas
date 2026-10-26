#!/bin/sh
# Run from repository root with existing Release tools and a working Linux GL desktop.
# Existing saves are never intentionally reused: choose new proof and user-data roots.
set -eu
project=projects/lunar_island/lunar_island.judasproj
proof=${1:-/tmp/lunar-stage5-reproduction}
userdata=${2:-/tmp/lunar-stage5-reproduction-data}
[ ! -e "$proof" ] || { echo 'Choose a new proof directory'; exit 2; }
mkdir -p "$proof"
for rules in inventory encounters vehicles; do
    node "projects/lunar_island/tests/$rules.test.js" > "$proof/$rules.log"
done
./build/judas_script_check "$project" > "$proof/scripts.log"
python3 projects/lunar_island/tests/build_observer.py --output "$proof/observer"
run() {
    SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata-$3" "$proof/observer" "$project" "$1" "$proof/$2" > "$proof/$1.log" 2>&1
}
run worldwrite world world
run worldread world world
run harvestwrite harvest harvest
run harvestread harvest harvest
run roadtrip road road
run islandtour tour tour
run harbourskiff harbour harbour
run fieldmap map map
run claimhome home home
run cargo cargo cargo
run cargoread cargo cargo
run garagestream garage garage
run garageread garage garage
run encounter encounter encounter
run encounterread encounter encounter
run write housing housing
run read housing housing
run performance performance performance
for kind in car truck motorcycle boat helicopter plane; do run "drive-$kind" "$kind" "$kind"; done
run land-helicopter heli-land heli-land
run land-plane plane-land plane-land
# Do not edit runtime content between a writer and its fresh-process reader.
# The observer moves/aims the player to reach distant fixtures. It does not inject
# inventory, NPC damage, vehicle movement, fuel or successful physics results.
# roadtrip constructs an ordinary car prefab, then uses only physical input.
# driving-timings.csv contains that active phase; whole-run CSV includes loading.
