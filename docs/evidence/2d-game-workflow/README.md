# Fresh-project and complete-game evidence

Base: Experimental 54db79cdf9b93dade83ed9d4c6d6bdde10f3ad6e.
Linux Release/GCC 15.2, Ryzen 5 5600. Application tests use offscreen Mesa llvmpipe;
actual editor tests use NVIDIA GTX970/X11. Shared host, not an isolated benchmark.
No main promotion, publication or human/platform acceptance.

## Final results

RESULTS.json contains eight passing focused suites with source/build provenance:
fresh authoring workflow 24 assertions, complete game 22, loss/retry 6, plus
existing tilemap application/editor, sprite animation integration, planar physics
and MK2 physics. Two additional **separate-process** runs pass: partial continuation
17 assertions and completed-save restoration 5. API checks pass; build organization
18/18; registration six passes and two unavailable-generator skips.

Actual editor logs independently cover New 2D Project → save → Play/Stop, and the
complete game → real SDL/ImGui paint → Undo → Play/Stop → save at light 135%.
Authored source is identical after Undo and Play/Stop. Handler-level import, slice,
clip/event editing and script attachment are automation, not manual UI acceptance.

Final export: 11 assets, one scene, 57,677,622 bytes, 0.235 s on this host. The final
runtime was relocated to /tmp/judas-beacon-grove-final, run **with that directory as
its working directory**, a missing engine-root override and a writable isolated
user directory. The harness exercises collection, saving/loading, animation,
physical gate removal, traversal and rendered victory. No script errors appear.
relocated-end.png is the observed completed-game display, not a fabricated mockup.

Mixed software-rendered application durations, excluding swap/wait:
- Win: 254 samples, median 2.973 ms, p95 5.471 ms, worst 261.679 ms.
- Loss: 44 samples, median 2.703 ms, p95 3.473 ms, worst 62.358 ms.
- Cold continuation: 132 samples, median 2.978 ms, p95 7.734 ms, worst 257.321 ms.
These include transition/resource work and software-rendering outliers. They do
not establish a steady-frame GPU budget or a physics performance regression.

FINGERPRINTS.json covers exact source/project bytes, binaries and every exported
file. Binaries retain their actual dirty pre-checkpoint build identities; the
later source checkpoint does not imply a rebuild from its commit hash. Main and
published packages are not involved.

## Reproduce

Use fresh output directories; tests deliberately reject overwriting their fixtures.

```sh
cmake -S . -B build-2d -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build-2d --target judas judas_editor judas_export judas_beacon_grove_application_tests judas_2d_game_workflow_tests judas_tilemap_editor_tests judas_tilemap_application_tests judas_sprite_animation_integration_tests judas_planar_physics_tests judas_mk2_physics_tests --parallel 2
python3 scripts/prealpha_validate.py --build-dir build-2d --test 2d-game-workflow --test beacon-grove-application --test beacon-grove-loss --test tilemap-editor --test tilemap-application --test sprite-animation-integration --test planar-physics --test mk2-physics --output /tmp/beacon-gates-fresh --expected-channel Experimental
build-2d/judas_beacon_grove_application_tests projects/beacon_grove/Beacon_Grove.judasproj /tmp/beacon-gates-fresh/beacon-grove-application cold
build-2d/judas_beacon_grove_application_tests projects/beacon_grove/Beacon_Grove.judasproj /tmp/beacon-gates-fresh/beacon-grove-application cold-win
build-2d/judas_export projects/beacon_grove/Beacon_Grove.judasproj /tmp/beacon-export-fresh build-2d/judas
```

The modern slots from the producer are required by both cold-process commands.
For an actual editor test, copy the project, isolate XDG_DATA_HOME, then use
JUDAS_EDITOR_AUTOTEST=<output-prefix> and JUDAS_EDITOR_AUTOTEST_TILEMAP=1. The test
moves/restores the mouse and exits; do not run over unsaved operator edits.
JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=<fresh-directory> plus
JUDAS_EDITOR_AUTOTEST_NEW_2D=top-down exercises actual New Project handling.
The relocated harness paths in relocated.txt must be adjusted for screenshot
outputs on a different machine. Set JUDAS_TEST_SCRIPT to it and use writable
XDG_DATA_HOME plus JUDAS_ENGINE_ROOT=/missing/engine from the moved package.

## Preserved failed runs

first-win/win-v2: invalidated collectible wrapper queried in enter/exit callback.
win-v3: sandbox denied the home save directory; fixed by isolating test storage.
win-v4: test incorrectly expected startup-only worldReady on restoration.
win-v5: test inspected script capture before its first restored lifecycle boundary.
All are test/game-side errors corrected explicitly, not relaxed engine assertions.
STALE_BINARY_RESULTS.json records a rejected mixed-build gate attempt; its
unavailable entries are not counted as passes. RESULTS.json is the final fully
source-matched focused run. No old failures or protected evidence were edited.
