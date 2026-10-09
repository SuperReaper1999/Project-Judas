# M71 human-review follow-up: wheel zoom and viewport selection

Uncommitted candidate based on `fb31f0244c9f41f74c439afd99855e3707937edd`.
The operator reported missing wheel zoom and unreliable small/nested object
selection in the newly polished editor. `human-failure.json` preserves the
report and a source-backed numerical reproduction. Earlier M71/editor receipts
remain intact snapshots; this folder identifies the later candidate.

## Cause and correction

Wheel navigation had no implementation. The camera now dollies along its view
direction, preserving orientation, lens and focus point. One positive wheel
step leaves 80% of the previous focus distance; fractional wheel steps work.
Distance is bounded at 0.1–5000m for ordinary framing. Wheel input is accepted
only in the Edit viewport with no panel/text ownership or active drag. Scrolling
the hierarchy/inspector/assets does not move the camera. Help and the current
editor guide expose the control.

Selection already flattened the hierarchy, but every object was represented by
an enlarged sphere. A large floor's sphere extended above the floor and won
before the small child's actual visible surface. The retained fixture records
the wrong floor entry at 14.3015m versus the small object's proxy at 27.9031m.

`EditorScenePicker` now tests rendered boxes, spheres/ellipsoids, compound
children, terrain triangles and mesh triangles after hierarchy resolution.
World-distance comparison survives nonuniform scale, and hits retain the child
ID. Offset conventions match authored rendering. Hidden mesh parts, available
material sidedness and mirrored rest-skin winding follow the renderer. Empty
objects have secondary 0.25m pivot proxies that cannot steal visible hits.

Mesh/cooked geometry is decoded on demand through existing loaders, with a
64-entry cache keyed by stable asset ID, path, file size and modification time.
Terrain meshes have an 8-entry cache. Project changes clear both. No per-frame
decoding, GPU readback, new asset pipeline or runtime physics path was added.
Selection, gizmos and placement use the same ImGui pointer coordinates as the
central viewport. Physics, scripts, accepted lab content and standalone remain
unchanged.

## Focused evidence

- `navigation.log`: 59 camera/input checks, zero failures (28 new wheel checks).
- `picking.log`: original 32 geometry checks, zero failures.
- `picking-final.log`: 36 final geometry/material/rest-skin checks, zero failures.
- `session-final.log`: eight actual SDL → ImGui → editor wheel/selection checks,
  zero failures, exit 0;
  private tiny nested box/sphere fixture, ordinary Play/Stop and exact authored
  scene preservation. `final.nested.png` shows the selected child inspector.
- `configure.log`, `build-first.log`, `build-final.log`, `build-guard-final.log`
  and `build-session-final.log`: affected Release builds, zero compiler warnings.
  Later builds cover only the sidedness/bounds and test-timing corrections.
  No production, fluid or unrelated physics suites were rerun.

The editor application uses a private copy of the normal registered kinematic
project under `/tmp/judas-m71-wheel-pick-owned`. The fixture adds only ordinary
authored objects; it does not modify `projects/kinematic_lab`. Reproduce it by
copying that project, appending `fixture-objects.json` to its named scene objects,
setting `next-id` to 9010, then running the editor with
`JUDAS_EDITOR_AUTOTEST_PICKING=1` and `JUDAS_EDITOR_AUTOTEST=<output prefix>`.
The captured run uses `SDL_VIDEODRIVER=offscreen`, `LIBGL_ALWAYS_SOFTWARE=1`,
`SDL_AUDIODRIVER=dummy` and a private `XDG_DATA_HOME`.

`session-first.log` retains a test-fixture failure: required named render fields
were omitted, so the fixture scene did not load. The corrected fixture uses the
normal complete render fields. No engine change was needed for that failure.
`session-fixture-corrected.log` retains the first passing integration run before
the final material/rest-skin checks and an explicit real-body Play assertion.
`session-reviewed.log` preserves one early real-body assertion failure: it ran
at Play entry before the first fixed publication. The final assertion runs
after normal simulation has advanced and verifies all 14 lab bodies. This was
an automation timing correction, with no runtime lifecycle change.

## Current identity and limits

`final-source.sha256`, `changed-files.txt` and `verification.json` identify the
combined current candidate and verify unchanged runtime/API/project sources
against preceding receipts. Previous hash lists are not overwritten.
There are 94 current source/asset fingerprints and 103 passing focused checks.
The desktop editor was reopened as `judas-m71-editor-wheel-pick-20261009.service`
(PID 1559077); native window and running service were confirmed. Runtime SHA-256
remains `ae42790e00cf28b1aed4f5dd9b07153c623fc6dfd85a8c045359bf33fb42adf8`.

Picking is geometry-based, not a pixel-perfect selection buffer: texture alpha
cutouts are not tested, and Edit meshes use the authoring rest pose. First-click
CPU decoding and linear triangle scans can delay clicks on very large meshes.
Obscured objects remain accessible through the hierarchy. Offscreen input and
screenshots do not establish physical desktop feel or Windows behaviour; the
reopened desktop editor is handed back for operator review. No commit/push/tag.
