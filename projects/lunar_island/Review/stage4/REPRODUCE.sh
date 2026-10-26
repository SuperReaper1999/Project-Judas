#!/bin/sh
# From repository root; an existing Release engine and working Linux GL desktop.
# Never reuse proof/data paths containing an expedition you want to preserve.
set -eu
project=projects/lunar_island/lunar_island.judasproj
proof=${1:-/tmp/lunar-stage4-reproduction}
userdata=${2:-/tmp/lunar-stage4-reproduction-data}
[ ! -e "$proof" ] || { echo 'Choose a new proof directory'; exit 2; }
mkdir -p "$proof"
for test in inventory encounters vehicles; do
    node "projects/lunar_island/tests/$test.test.js" > "$proof/$test.log"
done
./build/judas_script_check "$project" > "$proof/scripts.log"
python3 projects/lunar_island/tests/build_observer.py --output "$proof/observer"
run() {
    SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata" "$proof/observer" "$project" "$1" "$proof/$2" > "$proof/$1.log" 2>&1
}
run cargo cargo
run cargoread cargo
run garagestream regional
run garageread regional
for kind in car truck motorcycle boat helicopter plane; do run "drive-$kind" "$kind"; done
run land-helicopter land-helicopter
run land-plane land-plane
run vehicleprefab prefab
run encounter encounter
run encounterread encounter
run performance performance
# Do not edit runtime content between a writer and its fresh-process reader.
# Inventory/damage/vehicle motion come from ordinary project controls and physics;
# only player placement/aim and the explicit prefab construction are fixtures.
