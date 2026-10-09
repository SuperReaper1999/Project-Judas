# M71 final candidate review

**Preserved pre-polish snapshot.** The operator subsequently requested editor
polish and current JudasJS guidance before human testing. The current combined
candidate and source fingerprints are in [editor-polish](editor-polish/README.md).
The original receipts/hashes below are retained as evidence of this earlier state.

Accepted base and unchanged HEAD: `fb31f0244c9f41f74c439afd99855e3707937edd`.
This is an **uncommitted Linux candidate**, not operator acceptance or Windows
validation. No later milestone, Claude workspace integration, commit, push or
tag is included.

## Frozen implementation and evidence identity

The authority/phase table, API contract, measured tolerances and limitations are
in [M71.md](../../M71.md) and the [focused evidence index](README.md).
The exact candidate files are in [changed-files.txt](changed-files.txt).
[final-source.sha256](final-source.sha256) hashes every changed implementation,
project, script, test, type and current documentation file outside this evidence
folder. Evidence output is excluded from that list to avoid self-referential
hashes. [final-verification.json](final-verification.json) records the read-only
final hash and Git checks, including the individual retained receipts.

Final runtime SHA-256:
`ae42790e00cf28b1aed4f5dd9b07153c623fc6dfd85a8c045359bf33fb42adf8`.
This matches `build/judas`, the final editor's export and the moved package.
Build binaries, exports and save fixtures remain outside the changed-file list.
The project font is a licensed source asset, not a generated executable.

## Build and check command map

The existing build tree was configured for Release before the planned affected
build. `configure.log`, `build-first.log` and the necessary incremental correction
logs retain the actual output. The first build printed seven compiler warnings;
final affected build logs print zero. Configure retains the dependency
HarfBuzz/FreeType notices. No clean/full production-suite run was performed.

Equivalent configuration/build reproduction commands, **not an additional run**:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas judas_editor judas_export judas_scene_author \
  judas_kinematic_tests judas_kinematic_lab_tests judas_kinematic_performance \
  judas_editor_navigation_tests -j2
```

The last application/editor/export relink is `build-streaming-final.log`.
The retained-motion guard's exact lab build/run commands and exit codes are in
[streaming-after-guard.json](streaming-after-guard.json). API commands, installed
TypeScript path, live VM scope and source hashes are in
[api-types-live-final.json](api/api-types-live-final.json). Editor commands and
backend limitations are in [editor/README.md](editor/README.md). The export's
exact argument array and every package file hash are in
[package/receipt.json](package/receipt.json).

| Executable / scope | Retained result |
| --- | --- |
| `build/judas_kinematic_tests` | `kinematic-final.log`: 96 checks, zero failures |
| `build/judas_kinematic_lab_tests` | `lab-after-stream-guard.log`: 36 / 0 |
| `build/judas_kinematic_lab_tests --stream` | `streaming-after-guard.log`: 34 / 0 |
| Lab `--write` then separate `--read` | 4 + 8 / 0; exact arguments in streaming receipt |
| `build/judas_kinematic_performance docs/evidence/m71/fixed-cost.csv` | `performance.log`: two 360-step workloads, all raw samples retained |
| `build/judas_editor_navigation_tests` | `editor_navigation.log`: 31 / 0 |
| `build/judas_rigid_motion_tests` | `rigid_motion.log`: 8,398 / 0 |
| `build/judas_impact_timing_tests` | `impact_timing.log`: 3,197 / 0; existing bounded fallback observations retained |
| `build/judas_contact_lifecycle_tests` | `contact_lifecycle.log`: 1,050 / 0 |
| CharacterMotor / joint / touch / physical animation executables | 32 / 29 / 26 / 64 checks, zero failures; corresponding named logs |
| `build/judas_scene_fingerprint_tests` | `scene_fingerprint.log`: 140 / 0 |
| Existing M70 Character Lab in offscreen GL | `m70-regression.log`: 98 / 0 |
| Material test in offscreen GL | `rendering/material-final.log`: 40 / 0 |
| Selected actual VM `surface` case, types and drift checker | 9 VM checks; 318 symbols, 27 exports, 221 native operations, 36 typed examples, three negative controls; pass |

Offscreen GL application execution used `SDL_VIDEODRIVER=offscreen`,
`LIBGL_ALWAYS_SOFTWARE=1`, `SDL_AUDIODRIVER=dummy` and a private writable
`XDG_DATA_HOME`. This does not establish hardware/compositor input behavior.

## Final standalone and fresh-process continuation

Package: `/tmp/judas-m71-kinematic-moved-final` — 5 assets, 1 scene, 23 files,
52,662,061 bytes; exporter reports 0.043 seconds. The owned export source was
renamed; its original path and original export location are absent. The
repository itself remains present. No engine-root/library override was used.

Both final commands were run with working directory `/tmp`, timeout 120 seconds,
the offscreen environment above, `MESA_DEBUG=context`, and
`XDG_DATA_HOME=/tmp/judas-m71-package-data`:

```sh
JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m71/package/moved.input \
  /tmp/judas-m71-kinematic-moved-final/judas
JUDAS_TEST_SCRIPT=/home/conner/Documents/GitHub/Project-Judas/docs/evidence/m71/package/fresh-load.input \
  /tmp/judas-m71-kinematic-moved-final/judas
```

The 440-scripted-frame save/load/reload/removal run and separate 90-frame saved
slot load both exited zero; service waits add zero-clock frames. Normal project
startup and filled geometry are visible in the retained logs/snapshots.

[package/verification.json](package/verification.json) compares the retained
logs without rerunning the application. Fourteen of fifteen project/package
samples match in all 69 printed simulation fields. Frame 360 follows different
asynchronous load-completion waits (zero versus two), so equal frame numbers
there are not an identical resumed simulation instant; that comparison is
recorded as a timing-dependent difference, not a bit-exact equivalence claim.
The fresh process's frame 30 matches the moved run's frame 360 exactly across
all 69 printed fields, including the restored reversed pusher velocity. Separate
native cold-process checks cover exact durable intent and no movement replay;
the VM presentation-cadence test covers identical fixed input independently.

## Failure preservation and final rerun scope

Original failures remain alongside corrections: example typing, fixture sidecar
reuse, invalid test save metadata, unwritable application save root, abrupt
project-policy catch-up, missing outer motion participant, missing retained
stream intent, and cold first-unskinned GL draws. The final two generic guards
and small sampler initialization fix are described in M71.md; their failures
were not overwritten. Physics/performance results precede the narrow
save/stream/render corrections; no prescribed solver code changed afterward.
Affected VM/cold/stream, GL, app/editor and final package paths were rerun.

Windows execution, physical middle-drag capture, actual typing/gizmo feel and
Conner's visual/input acceptance remain pending. Moving concave geometry,
automatic obstacle stopping, arbitrary speed/thin-feature guarantees and
Claude's game adoption are not claimed.

No existing protected evidence, research/prototype, earlier milestone document
or existing project appears in the changed-file list. Accepted fluid sources
remain unchanged; the shared rigid-motion contract gains prescribed segments
without a fluid-solver rewrite. ROADMAP.md remains absent.
