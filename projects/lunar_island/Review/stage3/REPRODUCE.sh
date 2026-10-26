#!/bin/sh
# Run from repository root with an existing Release engine build.
# Choose unused output/user-data paths to preserve earlier evidence.
set -eu
project=projects/lunar_island/lunar_island.judasproj
proof=${1:-/tmp/lunar-stage3-reproduction}
userdata=${2:-/tmp/lunar-stage3-reproduction-data}
[ ! -e "$proof" ] || { echo 'Choose a new proof directory'; exit 2; }
mkdir -p "$proof"
node projects/lunar_island/tests/inventory.test.js > "$proof/inventory.log"
node projects/lunar_island/tests/encounters.test.js > "$proof/encounters.log"
./build/judas_script_check "$project" > "$proof/scripts.log"
python3 projects/lunar_island/tests/build_observer.py --output "$proof/observer"
# These need a working desktop/GL driver; dummy audio is deliberate test isolation.
SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata" "$proof/observer" "$project" encounter "$proof/native" > "$proof/write.log" 2>&1
SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata" "$proof/observer" "$project" encounterread "$proof/native" > "$proof/read.log" 2>&1
SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata-perf" "$proof/observer" "$project" performance "$proof/performance" > "$proof/performance.log" 2>&1
python3 projects/lunar_island/tests/make_barrier_fixture.py "$proof/Barrier Fixture"
SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata-barrier" "$proof/observer" "$proof/Barrier Fixture/lunar_island.judasproj" barrier "$proof/barrier" > "$proof/barrier.log" 2>&1
# Previous systems, on the same combined candidate; saves stay separately scoped.
SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata-housing" "$proof/observer" "$project" write "$proof/housing" > "$proof/housing-write.log" 2>&1
SDL_AUDIODRIVER=dummy XDG_DATA_HOME="$userdata-housing" "$proof/observer" "$project" read "$proof/housing" > "$proof/housing-read.log" 2>&1
