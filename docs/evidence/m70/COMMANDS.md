# M70 executed commands and artifact receipts

Working directory unless specified: repository root. Linux x86-64, GCC 15.2.0,
CMake 4.2.3, Release / Unix Makefiles. The accepted base is
`934c5d3f0556c920cc7cae8b80dc4677d8cbf87b`.

## Build

Generated new target rules before building:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

The initial affected target list and elapsed/exit receipts are in
`build/release-build.json`. Ordinary narrow corrections required incremental
builds; their original logs are retained. The final frozen implementation was
built with:

```sh
cmake --build build --parallel 4 --target \
  judas_multi_target_ik_tests judas_physical_animation_tests \
  judas_character_lab_tests judas_character_lab_performance \
  judas judas_editor judas_export judas_scene_author
```

`build/mapping-final-build.log` exited zero with **zero compiler warnings**.
Configure emitted the existing HarfBuzz build-system notice and FreeType CMake
deprecation warning. Earlier compiler warnings and their corrected rebuilds
are preserved; zero final compiler warnings does not mean every earlier build
was warning-free. No clean rebuild or full production-suite run was performed.

## Focused mathematics and real-VM lab

```sh
build/judas_multi_target_ik_tests
build/judas_physical_animation_tests
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_character_lab_tests .cache/m70-review/lab-source-final
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_character_lab_tests .cache/m70-review/cold-source-final cold-write
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_character_lab_tests .cache/m70-review/cold-source-final cold-read
```

Each final invocation exited zero: IK 38 checks, physical 64, lab 66, cold writer
10 and fresh-process reader 14. Output directories were fresh for writers; the
reader intentionally used the writer's actual M61 slot. No VM/module default
initialization replay was substituted for restoration.

`focused/regression-results.json` records exact commands, exits and elapsed
times for the affected existing targets. The authoring pixel check initially
failed inside restricted graphics permissions. The **unchanged binary** passed
34/34 with normal desktop graphics permissions:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_world_authoring_runtime_tests \
  .cache/m70-review/authoring-original-desktop-permissions
```

The final new checks and application smokes used the final implementation. Old
regressions were not repeated after the narrow opted-in tensor/mapping-cache
corrections, which do not alter their disabled legacy path. No Renderer change
was retained from the graphics-permission investigation.

## API, types and unchanged cookbook scripts

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judasjs_examples_tests .cache/m70-review/cookbook-final
node scripts/check_judasjs_api.mjs \
  --typescript /tmp/judas-win-linux-ts/node_modules/typescript/lib/typescript.js \
  --runtime .cache/m70-review/cookbook-final/surface.json
```

Cookbook: 278 checks / zero failures. TypeScript 6.0.3, live enumeration, 310
symbols / 27 exports / 213 native operations / 15 callbacks, 35 example files
and three negative drift controls passed. The lab executes both new example
files unchanged through the actual VM. TypeScript remains developer tooling;
no runtime or permanent build dependency was introduced.

## Normal application and editor export

Each normal application invocation used its corresponding absolute test-script
path, without private dispatch or scene-name physics:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/wall.steps \
  build/judas projects/character_lab/character_lab.judasproj
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/board.steps \
  build/judas projects/character_lab/character_lab.judasproj
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/physical.steps \
  build/judas projects/character_lab/character_lab.judasproj
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  JUDAS_EDITOR_AUTOTEST=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/editor-final \
  JUDAS_EDITOR_AUTOTEST_EXPORT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/package-final \
  build/judas_editor \
  .cache/m70-review/owned-export-source/character_lab/character_lab.judasproj
```

Final application logs: `focused/app-wall-corrected.log`, `app-board.log`,
`app-physical-final.log`. Editor log: `focused/editor-export-final.log`.
All exited zero. Duplicate/undo and authored scene after Play/Stop were
IDENTICAL. Editor export included eight assets. Offscreen SDL cannot perform
relative mouse capture: its repeated capture messages are not a human input
pass or compiler warnings. Screenshot inspection verifies visible content,
not gameplay feel. Real pointer/quality acceptance remains pending.

The committed `application/*.steps` contain the same actions, with repository-
relative screenshot destinations. The executed local copies used absolute
destinations. All use a 1/60-second frame interval; no cadence/fidelity reduction.

## Moved, source-hidden package

The editor-generated package was moved from `.cache/m70-review/package-final`
to `/tmp/judas-m70-character-lab-20261008`. Its input was an owned project copy;
that copy's `Assets` and `Sources` were renamed `.hidden-after-export` afterward.
The reviewed repository project and Claude's content were not modified.

For each of `wall`, `board`, `physical`, the following command form ran from
`/tmp/judas-m70-unrelated-working-directory`, using the matching name in both
environment paths. The executable received **no project/scene arguments**:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  XDG_DATA_HOME=/tmp/judas-m70-package-wall-state \
  JUDAS_TEST_SCRIPT=/tmp/judas-m70-steps/wall.steps \
  /tmp/judas-m70-character-lab-20261008/judas
```

All three exited zero. Package logs resolve `game.judasproj` and registered
scenes inside the moved package. Normal scene switches, saves/loads, reload,
spawn and modes were exercised. The package has 31 files / 53,029,199 bytes
(50.57 MiB), three registered scenes and eight packaged assets. Existing Linux
system requirements remain: compatible SDL/system libraries, C++ runtime and
OpenGL drivers. No new platform abstraction or Windows validation is implied.

## Isolated M56 measurements

After all builds and other owned workloads finished:

```sh
build/judas_character_lab_performance .cache/m70-review/performance-cached-final
```

Same 72-node figure, 60 warm steps then 120 measured 60 Hz steps, one and ten
instances in disabled / IK / partial / passive modes. Final summary and all
eight raw M56 captures are in `performance/`. The isolated pre-cache workload
is preserved in `development/performance-uncached/`; the earlier overlapping
build run is explicitly labeled preliminary. These measure CPU simulation,
not rendered FPS, GPU cost or worst-budget IK convergence.

## Final identity

### Final HUD-only follow-up

The final review added a compact per-actor summary of existing drive error,
mapped joint key, torque and saturation to project `lab.js`. This changes no
targets, motion, engine bytes or numeric/performance workloads. No rebuild was
needed. Narrow follow-up commands:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_character_lab_tests .cache/m70-review/lab-hud-final
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/physical.steps \
  build/judas projects/character_lab/character_lab.judasproj
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  JUDAS_EDITOR_AUTOTEST=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/editor-hud-final \
  JUDAS_EDITOR_AUTOTEST_EXPORT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/package-hud-final \
  build/judas_editor \
  .cache/m70-review/owned-export-hud-source/character_lab/character_lab.judasproj
```

The lab passed 66/66 and both normal application/editor processes exited zero.
The editor again exported eight assets and verified IDENTICAL undo/Play-Stop
authored state. Logs use `*-hud-final` suffixes. The newly owned export source
copy's Assets/Sources were hidden after export. Its package replaced the old
local package at the same final `/tmp` path; the preceding package was preserved
as `/tmp/judas-m70-character-lab-before-hud-20261008`.
All three moved-package cases were repeated with the exact executable/environment/
working-directory forms above and exited zero; argument vectors and timings are
in `focused/package-hud-final-results.json`. The final package hashes/size in
the receipts refer to this HUD version. Raw math, API and M56 results remain
valid for the unchanged final executable bytes.

`ARTIFACT_RECEIPTS.json` records final executable hashes without committing
executables, platform details, compiler-warning counts and package file hashes.
`CHANGED_FILES.txt` is the exact non-ignored candidate file list.
`FINAL_FINGERPRINTS.sha256` hashes every listed candidate file except itself
and the checksum-verification receipt, avoiding recursive self-hashes. The
fingerprints bind source, authored/cooked project content, tooling, docs and
evidence; they are not a claim of Windows or human visual acceptance.


## Passive human-performance follow-up

The ordinary two-rig public-control reproduction preserves the actual project:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_character_lab_performance .cache/m70-review/passive-frozen-final passive-repro
```

The preceding uncached PhysicsWorld was separately built with the same driver,
run into `passive-original-all-bodies`, then the frozen candidate was restored
byte-for-byte and rebuilt. No commit/reset was used. Build/source receipts,
compact binary32 state comparison and selected M56 frames are under
`development/passive-follow-up/`; large raw captures remain receipted in `.cache`.

Only affected targets were relinked, with zero warnings, then these executed:

```sh
build/judas_impact_timing_tests
build/judas_joint_tests
build/judas_touch_event_tests --output .cache/m70-review/passive-touch-final
build/judas_contact_lifecycle_tests
build/judas_physical_animation_tests
build/judas_character_lab_tests .cache/m70-review/passive-lab-final
```

Hidden-GL environment above applies to the lab test. All exited zero. Exact
argument vectors and results are retained in `affected-check-results.json`.
No full production or unrelated fluid/research suite was repeated.

The normal exporter used a fresh owned project copy and the relinked runtime;
`export-command.json` records its exact arguments. The package was moved to the
same final `/tmp/judas-m70-character-lab-20261008` path, preserving the preceding
package as `judas-m70-character-lab-before-passive-20261008`. The owned source
Assets/Sources were hidden afterward. `package-application-results.json` records
the absolute no-argument launches from an unrelated working directory: actual
F3/P startup and the existing physical/lifetime action script, both exit zero.
This package has 31 files / 53,040,986 bytes. Human re-test is separate.

## Consumer and G/floor landing-freeze correction (current cohort)

Earlier commands/results above remain historical records for their own bytes.
The operator's force-closed G/floor freeze, core diagnosis, rejected hashes and
compact replay are under `development/passive-crash-follow-up/`.
The diagnostic recipe generates only temporary copied-source objects/binaries:

```sh
python3 docs/evidence/m70/development/passive-crash-follow-up/replay/reproduce.py \
  --output .cache/m70-review/crash-replay-recipe-corrected
```

Default is the corrected sampler. Its executed verification completed in
29.937 ms / 18 events / 1,684 queries, no caps or sampling fallback. The separately
retained baseline timed out beyond 35 seconds and was not repeated. It uses the
same captured post-velocity-solve bodies, joints, geometry, filters and cadence;
ordinary public Step alone was not sufficient to reproduce the captured state.

The affected targets were rebuilt/relinked with zero compiler warnings. The
first build mistakenly named a nonexistent API target after completing the valid
targets; that original command failure is preserved. Final tests:

```sh
build/judas_rigid_motion_tests
build/judas_impact_timing_tests
build/judas_contact_lifecycle_tests
build/judas_joint_tests
build/judas_touch_event_tests --output .cache/m70-review/crash-touch-final
build/judas_physical_animation_tests
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  build/judas_character_lab_tests .cache/m70-review/owner-contact-final
build/judasjs_examples_tests .cache/m70-review/js-surface-contact-final surface
node scripts/check_judasjs_api.mjs \
  --typescript /tmp/judas-win-linux-ts/node_modules/typescript/lib/typescript.js \
  --runtime .cache/m70-review/js-surface-contact-final/surface.json
```

All pass: motion 8,398, impact timing 3,197, contact lifecycle 1,050, joints 29,
touch events 26, physical animation 64, actual lab/owner-contact 98, live surface
9. Type/source/reference/live drift checks pass 311 symbols / 27 exports / 214
native operations / 15 callbacks / 35 examples and three negative controls.
Logs and precise result qualification are retained in that follow-up directory.

Current ordinary-project M56 runs were sequential, 600 fixed steps each:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  build/judas_character_lab_performance .cache/m70-review/passive-anchor-corrected-repro passive-repro
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_M70_REPRO_STEPS=600 \
  build/judas_character_lab_performance .cache/m70-review/active-wall-corrected-repro active-wall-repro
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_M70_REPRO_STEPS=600 \
  build/judas_character_lab_performance .cache/m70-review/active-physical-corrected-repro active-physical-repro
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_M70_REPRO_STEPS=600 JUDAS_M70_PASSIVE_FIRST=1 \
  build/judas_character_lab_performance .cache/m70-review/passive-to-active-corrected-repro active-physical-repro
```

See
`corrected-runtime-summary.json` and compact per-step counts/body hashes.
All four complete finite poses and no script diagnostics. Fixed CPU timings
include real remaining active/contact spikes; they are not desktop FPS.

The rebuilt editor was checked against a fresh owned source copy:

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  JUDAS_EDITOR_AUTOTEST=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/editor-crash-corrected \
  JUDAS_EDITOR_AUTOTEST_EXPORT=/home/conner/Documents/GitHub/Project-Judas/.cache/m70-review/package-editor-crash-corrected \
  build/judas_editor .cache/m70-review/owned-editor-crash-corrected-source/character_lab/character_lab.judasproj
```

Exit zero; duplicate/undo and Play/Stop authored state IDENTICAL; save succeeded;
eight assets exported. Offscreen relative-pointer warnings do not certify input.
`export-command.json` records the fresh normal export used for the current moved
package `/tmp/judas-m70-character-lab-corrected-20261008`. Only the owned copy's
Assets/Sources were hidden. Wall G/landing and physical lifetime/save/load/reload
package runs from an unrelated working directory both exit zero; their exact
arguments and logical-input files are in `package-application-results.json`.

The corrected package was also reopened through an ordinary desktop user service
without automation/offscreen/software-render settings. The persistent process
and pending human acceptance are recorded in `desktop-reopen-service.json`.
No Windows environment is available; Windows M70 moved-package testing remains
outstanding. No full production or unrelated historical/research run was repeated.
Current `ARTIFACT_RECEIPTS.json` and final fingerprints describe this new cohort;
preceding artifact receipts and rejected fingerprints remain preserved.
