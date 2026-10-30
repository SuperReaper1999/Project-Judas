# 2D traversal qualification evidence

Base Experimental ac14d677430bf7fd78d12fc3aa61789afd0208f4. Linux Release,
GCC 15.2, Ryzen 5 5600. Native application/export tests use Mesa llvmpipe;
actual desktop editor checks use NVIDIA GTX970/X11. This is a local host
qualification, not Ubuntu-compatible distribution or Windows/Android acceptance.
Main, released packages and independent research are untouched.

## Final observed results

RESULTS.json: eight source-matched suites passed. One-way:23 native checks;
grid:40 including setup checks; editor scripting:65; MK2 rotation:31,404.
Planar physics retains its pre-existing exact 3D trajectory digest. Existing MK2,
tilemap/prefab and Beacon Grove regressions pass. No solver budgets, damping,
physical cadence or acceptance thresholds changed.

Three additional separate-process runs pass: one-way crossing4 checks, grid
mid-cell7, and grid pending-cell plus buffered-turn7. Participant fixtures use
explicit synthetic project/content identities for physical continuation tests;
the real standalone tour separately uses production save/content validation.

Editor-new-grid records the actual New Project grid template, save and Play/Stop;
authored source remains identical across Play/Stop. Native editor workflow tests
also create the hierarchy preset, attach its source and execute it. These are
automated checks, not human judgment of the new controls.

Final relocated export:5 assets,2 scenes,57,471,949 bytes,0.219 s on this host.
It runs from /tmp/judas-2d-traversal-proof with that as working directory,
JUDAS_ENGINE_ROOT pointing to a missing directory and an isolated writable user
save directory. The production harness traverses both gold platforms, changes
scenes, saves/loads a moving grid actor, tests a blocked cell and returns.

check_traversal_tour.py independently verifies both landing heights, grid endpoint
observations and ENTITY/native-body pose agreement. Final result:175 arithmetic
assertions over55 observation frames. This is one fixture, not175 unique engine
capabilities. Original telemetry logs fail the same oracle at frame0.

API/type/inventory checks pass with59 typed examples. CMake organization18/18;
registration6 passes and2 unavailable-generator skips. No Windows hardware,
Android hardware or independent per-camera parallax claim follows.

## Reproduce

Use fresh output directories. Keep the producer fixture for its cold readers.

```sh
cmake -S . -B build-2d -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental
cmake --build build-2d --target judas judas_editor judas_export judas_one_way_platform_tests judas_grid_movement_tests judas_planar_physics_tests judas_mk2_physics_tests judas_mk2_rotation_tests judas_tilemap_tests judas_beacon_grove_application_tests judas_editor_script_workflow_tests --parallel 2
python3 scripts/prealpha_validate.py --build-dir build-2d --test one-way-platform --test grid-movement --test editor-script-workflow --test planar-physics --test mk2-physics --test mk2-rotation --test tilemap --test beacon-grove-application --output /tmp/traversal-gates-fresh --expected-channel Experimental
build-2d/judas_one_way_platform_tests /tmp/traversal-gates-fresh/one-way-platform --cold
build-2d/judas_grid_movement_tests /tmp/traversal-gates-fresh/grid-movement --cold
build-2d/judas_grid_movement_tests /tmp/traversal-gates-fresh/grid-movement --cold-queue
build-2d/judas_export projects/2d_traversal_workshop/2D_Traversal_Workshop.judasproj /tmp/traversal-package-fresh build-2d/judas
```

Move that complete package. From its new working directory use writable
XDG_DATA_HOME, JUDAS_ENGINE_ROOT=/missing/engine and JUDAS_TEST_SCRIPT=<tour.txt>.
Adjust the tour's absolute screenshot output paths on another machine. Then run
`python3 tools/validation/check_traversal_tour.py <captured-runtime-log>`.
The native obj0 observation in this fixture is actor1 in both scenes. The oracle
checks ENTITY pose against that independently published native body observation.

Editor automation uses isolated project/preferences and
JUDAS_EDITOR_AUTOTEST=<prefix>, JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=<fresh-folder>,
JUDAS_EDITOR_AUTOTEST_NEW_2D=grid. It exits after duplicate/Undo/Play/Stop/save;
do not run over unsaved operator work. Human inspection of one-way direction
arrows and movement feel remains on the report's checklist.

## Preserved failures and boundaries

- grid-interruption.log: a newly inserted overlap originally prevented back-out.
  grid-second-blocker.log demonstrates the correction and second-blocker control.
- grid-axis-control.log: the axis-only prototype loses a released-before-step tap.
  The final driver adds an earlier queued-save assertion; axis-final-control.log
  records its failure with that same preserved source. Reproduce with
  `judas_grid_movement_tests <fresh-dir> --controller <grid-axis-only.js.txt>`.
  The original source is retained byte-for-byte; none of its failures is a pass.
- relocated.log: first scene composition inherited a blocking grid obstacle;
  native player body stayed near x2 instead of reaching the upper platform.
  inherited-obstacle-source.zip retains the original source project without
  binaries, caches or saves. Corrected source removes it from the side scene.
- relocated-corrected.log: physical route succeeds, but ENTITY still reads authored
  pose. telemetry-negative.log preserves the independent oracle rejection.
  relocated-proof.log includes the corrected generic harness observation.

One-way geometry is static and straight. Deeply intersecting spawn recovery,
moving/curved one-way surfaces, drop-through, automatic one-way tile layers,
pathfinding, RPG battles and live-origin grid rebasing remain explicit boundaries.
The code adds one native geometry option and project-owned movement policy, not
an unbounded genre framework. No release or human acceptance was manufactured.

FINGERPRINTS.json records exact source/project bytes, binary hashes and every
exported file. Binaries retain their true dirty pre-checkpoint provenance; a later
checkpoint commit does not imply they were rebuilt from that commit hash.
