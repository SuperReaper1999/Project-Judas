# Native closure plan — awaiting final source and root clearance

No Windows build, simulation, resource change or cleanup has been started for this follow-up. Linux validation owns the current CPU slot. The sole initially requested source change is the `WorldPersistence.cpp` local rename with SHA-256 `14a5ce3a78eebb2db801f91ddd91dc35808f23ff637a797c7cbb599d2e756ad4`; a subsequently identified mixed-motor/restored-contact interaction is being investigated by the root team. This plan does not presume its outcome.

## Frozen inputs

Prior native archive: `71a722dded64cfcc634d679e55ddbd257058da888ada914f7f7150b8d88c2d62`, 67,019,246 bytes, 138 members. Guest path: `C:\JudasValidation\M78ContinuationBuild\cold-repair-final-v2\judas_engine.lib`. Lossless verified host bytes remain under `.cache/m78-cold-boundary/native-binaries/final2-repair/`.

Prior final source, test fixtures, scripts, 28-execution matrix, actual physical endpoint witness, editor and moved-package receipts remain immutable. Fresh outputs will use unique `M78Closure` guest paths and this evidence directory. A prior pass is not relabeled as a new run.

## Minimal compilation

1. Obtain the final source freeze and root GO; verify exact changed-source hashes and unchanged headers/layouts.
2. Compile only the changed TU(s) with the captured MSVC 14.44.35207 Release flags (`/O2 /Ob2 /MD /fp:precise /std:c++17 /W4 /utf-8 /bigobj`). For a pure local rename, compare raw object bytes and normalized COFF sections with the preserved previous object; report exactly which identity matches. Normalized code equivalence alone is not an identical executable claim.
3. Reuse independently hashed unchanged test/Application/editor objects. If a new archive cannot coexist within the existing guest disk, use the root-approved explicit replacement object before the untouched archive, with `/MAP` and `/VERBOSE:LIB` proving that the replaced archive member was not extracted. Report the exact archive-plus-object identity rather than claiming the old archive itself contains new source.
4. Link and run one generated executable at a time. Preserve its bytes and hash on the host before removing an owned duplicate. Any old-file cleanup needs named-path authorization and fresh guest/host SHA and byte-count verification.

## Focused final-source matrix

- Continuation unit: unchanged 98 checks plus any separately approved new guard test.
- World-save and streaming regression: 27 and 82 checks respectively.
- M77 fresh corrective cold-write/read, with the original timestep and acceptance unchanged; then the existing read-only actual-body/History/current-contact observer on that same new capture. Preserve physical endpoints independently before a read can overwrite output filenames.
- M70 CharacterLab baseline and fresh cold-write/read, covering full partial-anchor body state and CharacterMotor proxy exclusion.
- Exact legacy `a106` cold-read remains a separate preserved expected failure; no snapshot rewrite, replay overlay or threshold change.
- If a mixed-motor regression changes the final source, execute its exact final focused native fixture; keep the prior failure separately.
- Ordinary editor edit/duplicate/undo/Play/Stop/save and moved standalone scene/save/load/pause/reload smoke. Identical executable bytes permit explicit reuse of prior receipts; different bytes require narrow relink/smoke.

## Two additional high-risk registered contracts

1. **I.01 / `cold-active-state`:** `judas_save_streaming_tests <projects/streamed_range/streamed_range.judasproj> write <owned-output>`, followed by `read` and `read-b`. This exercises actual Application/SaveService replacement, authored logical look/fire and prefab spawn, adopted qualified identity, target/joint tombstones, changed suspended-region revisit, locale/session state, independent slots and held-input edge suppression. It adds functional coverage beyond the lower-level 82-check streaming target and direct M77 cold snapshots.
2. **H.08 / `ui-input-signals`:** `judas_signal_application_tests <projects/signals_lab/signals_lab.judasproj>`, followed by the same project with `load`. This exercises fixed/UI signal lanes during pause and a zero-step frame, repeated receiver deletion, live adoption and region revisit, then fresh-process subscription/state restoration. The project is about840KiB. It does not validate physical OS focus or real controller hardware.

These existing source fixtures use the same five Application/runtime TUs already preserved. Only their test driver objects need fresh compilation. Verify their complete fixture/project identity and guest availability first; do not retune a failure.

## Disk and environment bounds

The prior last measured free disk was 27,344,896 bytes before the small final endpoint outputs; obtain a fresh read-only measurement after GO. A second67MB archive cannot fit. Editor inputs plus executable require roughly30MB; a new normal export roughly52MB. Preserve the current moved package and original inputs. If the fresh disk check blocks an operation, request only bounded duplicate archival/retirement; do not enlarge VM resources or delete packages/evidence.

Use the unchanged4vCPU/6GiB Windows10 VM, captured MSVC setup, and established `GALLIUM_DRIVER=llvmpipe` environment. Windows is functional validation only: no FPS gate, timing investigation or hardware-graphics claim. Linux owns performance acceptance. Retain the two preexisting WP conversion warnings; verify the requested shadow warning disappears. Prior FLU-001 Linux failure/native pass discrepancy remains unchanged and outside this repair.
