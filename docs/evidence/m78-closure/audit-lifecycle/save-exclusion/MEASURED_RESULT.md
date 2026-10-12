# LIF-002 — excluded simple dynamic body leaves an orphan sleep record

## Measured scope

Linux Release, one scheduled compilation/link/execution against the final combined engine archive. **22 checks / 1 failure, exit 1.** No assertion treats the rejected legal save as a pass. No Windows run, full-suite rerun, engine correction, original-fixture change or acceptance-threshold change was made.

- Ordinary dynamic case: **12 / 0**. Normal runtime entity ownership, capture, scene decode, private build, prepare and full restore succeed. Retained real velocity and entity identity remain correct.
- Publicly excluded dynamic case: **10 / 1**. `SetTransientEntity(runtimeId, true)` succeeds. Capture succeeds and its saved scene correctly omits the simple actor. Private build/preparation succeed, but full participant restoration returns **false** with `saved sleep body unavailable/duplicate`.

The actor is created with normal `RuntimeWorld::CreateEntity` and marked transient through the public native ownership method used by JudasJS `saves.exclude`. Private collections are not changed. There are no controllers, joints, scripts, fluids, streamed regions or simulation steps in this discriminating fixture.

## Root cause and classification

**Pre-existing generic persistence defect, P2; not a demonstrated M78 regression.** Exact unchanged capture omission, sleep-selection and restore-guard strings are in `linux-current-01/SOURCE_MECHANISM_COMPARE.json`, comparing current source with HEAD. The relevant current WorldPersistence file includes accepted M78 corrections, but these three mechanisms are byte-identical to their HEAD equivalents.

`WorldPersistence::Entities` omits non-articulated transient entities. The separate sleep collector still writes every owned dynamic body, including the omitted actor. During private restoration that actor has no body mapping, so the sleep participant rejects the slot. This is a mismatch between participant ownership predicates; no physics-quality or numerical-solver adjustment is relevant.

The original `tests/WorldPersistenceTests.cpp:32–33` marks a runtime dynamic actor transient, but verifies capture/scene omission only. It does not read that excluded snapshot. Passing that test therefore did not cover this failure.

The save-exclusion contract can make a slot successfully written but later unreadable while this actor exists. This probe establishes the full native participant failure, not UI/service callback behavior or loss of a live world: restoration was deliberately performed on a separate private RuntimeWorld.

## Exact identities

- HEAD: `9e77b2226edcf217ab789fe4f0fea42a3415a72d`.
- Linked final engine archive: `a5ec91426ce0f8476db3bbdbda8ad52f3094d38c37b64a0052c7ef4bd37c2e1f`.
- Current WorldPersistence source: `14a5ce3a78eebb2db801f91ddd91dc35808f23ff637a797c7cbb599d2e756ad4`.
- HEAD WorldPersistence source: `3a0f0b2ef9de4b0c40289e44562d52b62975028a5dad3d50fa03353038e24440`.
- Probe source: `9ac8c60aa46143371e4015c5e4bf316fc528c68ae32bdb0229ca4c220fca74d9`.
- Executed binary: `8060b7136aa83ec115b28d3e41e87471dc37dc03218370158743ce005c37ad69`.
- Compiler stderr is empty; compile and link succeeded. The nonzero exit is the measured runtime assertion failure.

## Reproduction

The exact scheduled command was:

```sh
python3 docs/evidence/m78-closure/audit-lifecycle/save-exclusion/run_probe.py --execute --output .cache/m78-closure/save-exclusion-01
```

The runner rejects a different archive hash and existing output directory. A fresh owned output name is required for a deliberate future rerun. `PREPARED_RECIPE.json` and `linux-current-01/IDENTITY.json` contain the exact Release flags/link command, unchanged library hashes and source identity. The single driver replaces only the original world-persistence driver object, without changing CMake.

## Preserved evidence

- `linux-current-01/run.stdout.log` and `run.stderr.log`.
- `linux-current-01/fixture/RESULT.json` contains all positive/negative assertions and the actual error.
- `linux-current-01/fixture/{ordinary,excluded}/captured-scene.judas` preserves the exact scene decoded from each captured slot.
- Every raw captured participant is preserved as `*.participant`, with versions/sizes/hashes in each case's `PARTICIPANTS.json`.
- Exact compiled source/helper, compile/link/run receipts, source comparison and new manifest are preserved alongside these inputs.
- Executable/object stay only in owned cache and are represented by hashes; no build output is added to evidence.

## Remaining work

No correction was attempted. A future generic correction should align sleep-record eligibility with the scene participant's retained ownership, including the intentional articulated transient-body exception. It should prove both legal cases above and not disable sleeping or weaken missing-body rejection. The failure remains part of the current audit result, separately from already repaired continuation/static/motor mechanisms.
