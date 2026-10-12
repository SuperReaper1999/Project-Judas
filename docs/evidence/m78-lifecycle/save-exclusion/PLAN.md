# LIF-002 — sleep-record ownership correction

Starting accepted checkpoint: `0695433b949df0c1d05ac6798bf118121cc53e2b`.

HISTORICAL — pre-execution plan. Later same-driver checkpoint/corrected receipts
are preserved in `before/` and `after/`; current results are in
[the lifecycle handoff](../../../M78_LIFECYCLE_CLOSURE.md).

Original status: source and focused fixture prepared; build/runtime validation has not
been run at the time this plan was written. Later receipts are separate files.

## Preserved original failure

The original Release discriminator recorded **22 checks / 1 failure**. The
ordinary retained actor restored; the legally excluded simple dynamic actor
was absent from the entity participant but still present in the sleep
participant, so private restoration returned
`saved sleep body unavailable/duplicate`.

- [Original measured result](../../m78-closure/audit-lifecycle/save-exclusion/MEASURED_RESULT.md)
- [Original raw receipts and participants](../../m78-closure/audit-lifecycle/save-exclusion/linux-current-01/)

Those files remain unchanged. This is a pre-existing capture ownership defect,
not a reason to weaken private restoration's malformed-record checks.

## Exact mechanism and bounded correction

`WorldPersistence::Entities` omits explicitly transient ordinary actors. It
retains mapped articulation bodies because they are required by their durable
articulation owner even though their internal entity records are transient.

The touch and completed-contact writers already use that same distinction,
matching mapped bodies by the complete generation-bearing `BodyHandle.id`.
The sleep writer instead included every owned dynamic body. Its record for an
excluded actor consequently could not resolve in the reconstructed scene.

The correction factors the existing contact predicate into one local
`includeBody` callable and uses it for both contact writers and sleep capture.
The sleep writer still requires an owned dynamic body. The touch writers keep
their original behavior for unowned bodies; their strict ownership validation
is not bypassed by silently filtering them.

No participant schema, public API, handle layout, simulation order, body state,
sleep policy, joint continuation or contact publication semantics change.
Private restore's `saved sleep body unavailable/duplicate` rejection remains
unchanged. Previously malformed captures are not retrospectively repaired.

An active articulation owner itself cannot be publicly excluded:
`RuntimeWorld::SetTransientEntity` rejects required `ragdoll`/`animation`
components. The focused fixture checks this normal public policy. It does not
modify private exclusion collections or synthesize an unsupported omitted
articulation owner.

## Focused fixture

Source: `tests/SaveExclusionTests.cpp`.

Entry point:

```text
judas_save_exclusion_tests FRESH-output [repository-root]
```

Run from the repository root or provide its absolute path. The output directory
must not exist. The fixture writes raw captured participants, hashes and a
`RESULT.json`; no executables or build archives are written to evidence.

Cases:

1. Retained ordinary dynamic actor: normal physics updates put it to sleep;
   full capture/private restore retains both its owner and actual sleep state.
2. Publicly excluded simple dynamic actor: scene and sleep records omit it;
   full private restore succeeds and preserves the other body's velocity.
3. Excluded then publicly reincluded actor: scene, sleep and restoration retain
   it again.
4. Real three-body articulation: the original `ragdoll_demo` model is decoded
   headlessly into a real skeleton, its ordinary owner enters ragdoll through
   the public native API, and every mapped transient dynamic body must be in
   both the scene and sleep participant. Full version-4 articulation /
   version-2 contact continuation must restore. A blanket `!transient` sleep
   filter therefore cannot satisfy this test.
5. Malformed orphan, duplicate and truncated sleep payloads: ordinary public
   `PersistSleep` supplies the state bytes, while deliberately invalid record
   identities/lengths must reject the private world. The live source remains
   unchanged.

The original M70/M77 fixtures, thresholds, timestep and evidence are untouched.
Cold continuation is consumed through its normal existing participants; the
fixture does not invent solver seeds, contacts or support loads.

## Required resources and compilation

The test links `judas_engine`; the parent task owns CMake integration and build
scheduling. It does not need a display, audio service or standalone package.
`ResourceManager::SetHeadlessResidency(true)` is the existing test seam for CPU
resource residency; no fake skeleton or GPU geometry is substituted.

Read-only resources:

- `projects/ragdoll_demo/Scenes/main.judas`, owner entity 10;
- `projects/ragdoll_demo/Assets`, normal asset database scan;
- mesh AssetId `46464646464646464646464646464601` and its existing model/sidecar.

Source ownership for this follow-up is limited to `src/WorldPersistence.cpp`,
new `tests/SaveExclusionTests.cpp`, and this new evidence directory. Root owns
CMake and combined validation. No builds or simulations were performed while
preparing the correction.
