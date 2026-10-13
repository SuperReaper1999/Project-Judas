# Independent cold-save archive comparison

This is read-only analysis of the original failed fresh-process test. No native
application, physics step, new impulse or engine change was run for this report.
The original failure remains in `../../linux/final-read.txt`.

## Exact inputs

- Original snapshot: `/tmp/judas-m83-vehicle-final-user/judas/games/15afb54227c136f6e3dca996d266e8bf2b204f950e090a571a3cb7bb528b6876/Saves/Slots/damage-current.save`.
- Snapshot SHA-256: `c669804c9b102072b8cd3ede1954a98ffaef3173d6da26887949da965120a3e9`.
- Exact 66,335-byte copy retained as `original-damage-current.save` here.
- Original restored participant: `../../linux/final-read/restored-entities.bin`.
- Decode/compare script: `compare_entities.py`; full observations and its own
  fingerprint are in `COMPARISON.json`.

## Observed difference

The entities participant contains regenerated scene text, authoritative binary
entity records, then a presentation view. Comparing the entire participant
mistakes changes in the first/last sections for a change of physical state.

| Section | Result |
|---|---|
| Allocation counter, simulation time, all 84 entity records | **7,328 bytes exactly identical** |
| Body records and force/torque accumulators | Exactly identical for bodies 3, 900, 910, 1100, 1110, 1500, 1501 |
| Scene non-transform lines | Exactly identical |
| Scene transform records | 38 differ in text; maximum position difference 1.0e-7 m, orientation difference 5.83e-8 rad; scales unchanged |
| Camera view rotation, scale and FOV | Exactly identical |
| Camera view position | Difference 0.000114942979 m (0.115 mm) |

Expected/restored total entities sizes are 50,061 / 50,057 bytes. Their embedded
scene text is 42,684 / 42,680 bytes. Consequently the same authority section
starts at offsets 42,688 / 42,684; the view starts at 50,016 / 50,012. There are
**no differing authoritative byte offsets or entity records**. The JSON records
the four differing aligned view bytes and both absolute offsets.

Scene quaternion sign changes `q -> -q` preserve the same orientation. Small
normalization/child-transform roundoff remains in regenerated scene text. This
is distinct from the larger view-position difference: it must not be attributed
solely to quaternion roundoff. The project can recompute its view through
`presentationUpdate` from presented body transforms even with no fixed physics
step; restoration also snaps presentation history. This report establishes the
unchanged authoritative endpoint, not byte-identical rendering history or a
universal exact text/camera round-trip promise.

Relevant source: `WorldPersistence::Entities` separately serializes scene text,
physical records and view; `RuntimeWorld::SetEntityState` restores body poses and
velocities; `PhysicsWorld::ResetBody` does not normalize the supplied physical
quaternion; wheel/presentation reconstruction can normalize visual rotations.

## Impulse-test limitation

The separate original 20 N·s test remains failed. A 40 kg nose receives an
initial 0.5 m/s velocity increment before other interactions; one free 60 Hz
interval covers only 8.33 mm. Its real tyre contacts, ground friction and attached
material can change that response. Therefore greater than 1 cm net movement
after 45 intervals is not established by the impulse magnitude alone.

This is a potential confound, **not an assertion about the diagnostic run's
actual outcome**. Immediate velocity, awake state, resolved motion and contact
impulses must distinguish real physical stopping from a restoration defect.

## Reproduce without running Judas

From repository root:

```sh
python3 docs/evidence/m83-vehicle-followup/development/cold-analysis/compare_entities.py \
  docs/evidence/m83-vehicle-followup/development/cold-analysis/original-damage-current.save \
  docs/evidence/m83-vehicle-followup/linux/final-read/restored-entities.bin
```

The script reads both inputs and emits JSON to stdout. It does not edit them,
invoke native code or relax an engine/test acceptance threshold.
