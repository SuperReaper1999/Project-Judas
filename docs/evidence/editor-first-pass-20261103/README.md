# Editor workflow and performance checkpoint

The runtime baseline was built from Experimental
`84b94833df172c9962c61bff6e5a9d8597c95cec`. The editor candidate kept the same
rendering and simulation settings. The versioned evidence carries the exact
compiled input hashes and both build receipts.

## What changed

- File → Build & Export uses the existing native package, destination,
  startup scene and unsaved-document policies. The browser build remains a
  separately gated option. Advanced asset and scene inclusion settings are
  still under Project Settings.
- File → Import assets and the Asset Browser share the source picker.
  FBX, glTF, GLB and OBJ can proceed directly to the existing model cooker,
  preview and placement. Ordinary asset registration and explicit reimport
  recipes remain available.
- Hierarchy row membership, depth and order are reused until the document
  or search changes. Only visible rows are normally drawn. Full-list
  interactions continue to use authored order. Inline rename and open
  context menus retain their UI owners.
- Play checks each distinct model archive once per request. The terrain
  receipt checks still inspect the current source and product bytes on
  each Play. Ordinary imports can still use their last good cook in Play.
  Export retains its independent freshness gate.
- The unchanged Experimental baseline failed the JudasJS inventory gate
  because five existing device input entries were absent. Regenerating the
  two documentation inventories fixed that gate without a binding change.
  Both new editor regression suites are registered in the focused gate.

## Runtime baseline

Linux Release, Ryzen 5 5600, SDL offscreen, Mesa llvmpipe with two driver
workers. Frame intervals include software drawing. These figures are a
reproducible backend-specific baseline. Read
[RUNTIME_BASELINE.md](RUNTIME_BASELINE.md) and the archived raw results for
conditions, provenance, run limits and workload distinctions.

| Workload | Median | p95 | Worst | Peak RSS |
|---|---:|---:|---:|---:|
| Fresh 2D frame interval | 4.53 ms | 5.48 ms | 7.49 ms | 241.5 MiB |
| Lunar Island frame interval | 111.17 ms | 142.27 ms | 150.81 ms | 1,494.4 MiB |
| Rendering Lab short frame interval capture | 727.66 ms | 817.98 ms | 1,421.89 ms | 266.8 MiB |
| Scenario 3 live frame interval with moving camera | 59.17 ms | 99.99 ms | 108.43 ms | 432.7 MiB |
| Scenario 3 fixed-clock control, fixed step | 3.85 ms | 12.60 ms | 22.37 ms | 399.3 MiB |

The full Rendering Lab observer hit its 120-second deadline. Its 221 partial
rows were retained and a separate 50-frame route completed. Lunar Island
exercised actual movement but profiler truncation limits scope attribution.
The live stress route used the authored all-shelf impulse. The fixed-clock
control is a separate 240-step motor tour with physical endpoint validation,
including 0.671 mm peak measured penetration and no nonfinite fields.

Hardware GPU timings and Windows/Android qualification are pending. Baseline
measurements should not be used as estimates of native visible FPS.

## Reproduction

Run from the repository root. Each capture owns its temporary project and
user-data roots. Use Release binaries with matching inputs and flags. The
runtime archive includes its own driver, route inputs, exact small 2D fixture
and per-file hashes.

```bash
p=.cache/editor-first-pass-20261103
e=docs/evidence/editor-first-pass-20261103
python3 "$e/generate_editor_fixtures.py" "$p/editor-fixtures"
bash "$e/run_editor_measurement.sh" "$p/baseline-bin/judas_editor" "$p/replay-before" repeated-model
bash "$e/run_editor_measurement.sh" "$p/candidate-bin/judas_editor" "$p/replay-after" repeated-model
python3 "$e/summarize_editor.py" "$p"/replay-{before,after}/profile.json
bash "$e/run_qualification.sh" .cache/2d-stage1-build "$p/replay-qualification"
bash "$e/run_workflow.sh" "$p/candidate-bin/judas_editor" "$p/replay-workflow"
```

`run_editor_measurement.sh` uses the existing profiler exercise to hold early
frames in the bounded capture. Its offscreen backend cannot capture a real
pointer. The benchmark checks the actual loaded scene, Play/Stop and save
markers. See the separate interaction controls for pointer and keyboard checks.

## Matched editor results

The standalone editor comparison used the same fresh-process fixtures on both
builds and no concurrent compiler workload. Each of the two repeated-model
samples checks 64 hidden placements of the same archive, plus the original
Tiny Game scene. Hidden placements isolate startup validation from their
drawing cost. Startup is measured as the outer frame containing world
construction, excluding the following Play frame. The candidate's explicit
preflight scope checked one archive per request.

| Editor measurement | Before | After |
|---|---:|---:|
| Transition frame with 64 placements, two samples | 531.72–544.02 ms | 15.96–19.68 ms |
| Transition frame for Tiny Game alone, one control | 10.98 ms | 10.25 ms |
| First rendered Play frame with 64 placements | 539.01–544.59 ms | 535.51–538.26 ms |

The first software-rendered frame still costs about 0.54 s on this path.
The editor improvement addresses the startup validation stall. The previous
fixture error, a missing required legacy mesh field, is retained separately
and excluded from the measured comparison.

The [Hierarchy panel comparison](hierarchy/README.md) isolates actual ImGui
CPU work without a renderer. The steady median for the synthetic 8,192-object
list dropped from 203.42 ms to 0.0065 ms at 100%. A first tagged query still
cost 23.39 ms in its single cold sample. Keep cold filtering and the steady
cached path separate. The fractional-row failure at 150% and corrected
interaction trace are both retained.

## Qualification

- Eleven native suites passed 575 assertions. Four focused CTest registrations
  passed, including the two new editor registrations and the gate registration
  controls. The API and current-document checks passed.
- Actual ImGui controls covered valid, invalid and duplicate model imports,
  browsing, export availability, disabled Play-mode commands, browser opt-in
  and Hierarchy interaction across 100% and 150%. The first Play-mode button
  failure and the first scaled-scroll failure are preserved.
- The editor created four independent 2D/3D projects across dark and light
  themes at 100% and 150%. Both export save choices were exercised. Each
  package copied the candidate runtime byte-for-byte, moved to a different
  directory, and ran from an unrelated working directory with the engine-root
  environment override unset. Play/Stop preserved authored state. The 3D
  light/dark workflow also includes a fresh editor-process reopen control.
- A separate editor application run cooked an animated glTF fixture,
  rendered a preview, placed it, and verified that one Undo restored the
  original scene. The import-review unit test used the new button and the
  same worker, including source failures and duplicate rejection.
- The lock, Basic Water and release-policy sources are unchanged. The 26
  fluid-policy assertions passed on the Experimental candidate.

The application workflows use editor automation and simulated ImGui input.
Desktop pointer capture and hardware rendering remain for the human pass.

## Human check

1. Open an editable copy of a 2D or 3D project. Use **File → Import assets**
   with one of your model sources and try preview, placement and Undo.
2. Scroll a large Hierarchy. Try a name or tag filter, Shift-select a range,
   and create a child from a row near the bottom.
3. Play and Stop a scene with many instances of one model. Open the Profiler
   to see how many distinct archives the Play preflight checked.
4. Open **File → Build & Export**, choose a folder outside the project,
   select the intended saved-content choice and launch the moved package.
