# Separate finding — runtime motor tags and query tags disagree

Status: **LIF-003, current Linux measured failure: 20 checks / 5 failures,
exit 1**. The ordinary rigid-body controls pass; the CharacterMotor's logical
runtime tags disagree with its actual physics-query tags. Pre-existingness is
source-traced against checkpoint `0695433b949df0c1d05ac6798bf118121cc53e2b`,
not a newly executed checkpoint comparison. No correction was made for this
separate finding. Native Windows and actual JS-VM mutation routes remain unexecuted.

The initial prepared-only status and earlier fixture/source hashes remain
unchanged in `PREPARED_IDENTITY.json`. They describe preparation, not the later
executed fixture. Current executed identities and raw results are in
[linux-final/IDENTITY.json](linux-final/IDENTITY.json),
[RECEIPTS.json](linux-final/RECEIPTS.json) and
[fixture/RESULT.json](linux-final/fixture/RESULT.json).

## Concrete source path

- `ScriptSystem.cpp` dispatches `entity.addTag/removeTag/hasTag` through normal
  `RuntimeWorld::AddTag`, `RemoveTag`, and `HasTag`.
- `RuntimeWorld.cpp` updates `m_entityCategories[id].tags`, then calls
  `PhysicsWorld::SetBodyTags` on `m_entityCategories[id].body`.
- A motor's actual physics query capsule is instead
  `m_characters[id].motor.observationBody`; `RuntimeBody` returns that proxy.
  The motor constructor does not install that proxy in the ordinary rigid-body
  category `body` field.
- Existing `UpdateCharacters` republishes `definition.tags`, not
  `TagsOf(id)`. An added runtime tag therefore is not applied to the capsule;
  an authored tag removed at runtime can be written back into the capsule on
  the next metadata update.

The LIF-001 lazy-construction/configuration correction publishes the existing
authored tags earlier. It does not correct this separate mutation ownership
path. The original M78 checkpoint's AddTag/RemoveTag and fixed-update tag
source have the same mismatch. Prepared-stage and checkpoint file hashes are in
`PREPARED_IDENTITY.json`; the final compiled fixture and linked archive identity
are separately recorded in `linux-final/IDENTITY.json`.

## Minimal discriminator

`DynamicMotorTags.cpp` links `judas_engine` and takes a fresh output path. It
creates one ordinary motor with nonzero offset/oblique orientation and two
project tags. It verifies an authored-tag hit and an absent-runtime-tag miss,
uses the real public native operations underlying JudasJS, then queries the
actual capsule by the added/removed tags before and after a zero-time normal
metadata update. A second lazy actor tests adding a tag before construction.

The required expected behavior is actual logical/query agreement. Any nonzero
exit is a failure, not an expected-error pass. No fixture changes a private
collection, advances elapsed physics or relies on another collidable object.

A separate ordinary dynamic rigid-body world uses the same tag classification
and oblique frame. Its authored-tag/absent-tag controls and real AddTag/RemoveTag
queries must pass; it cannot interfere with or accidentally satisfy the motor
world's query. This distinguishes the shared public tag/query API from a
motor-specific physical representation mismatch.

## Measured result and limits

The extended fixture compiled and linked successfully, then exited 1 with five
failed assertions. All eight ordinary rigid-body classification/mutation/query
controls passed. Motor construction, authored-tag positive/absent-tag negative
controls and logical AddTag/RemoveTag changes also passed. The failures are:

1. Added runtime tag does not immediately select the actual motor capsule.
2. A normal zero-time metadata update still does not publish that added tag.
3. Removed authored tag still selects the actual capsule immediately.
4. The removed authored tag still selects the capsule after metadata update.
5. Lazy capsule construction does not use a tag added to current logical state.

All observations occurred without advancing elapsed physics time. These are
failed required positive/negative query outcomes, not deliberate rejection
controls counted as passes. The executed source hash is
`07d43fdc41ac54d5a75680aa90abdb70c04789f31be7cb77b07490ff7d197cbb`;
the linked current Linux archive hash is
`e2e4aec5ed13c6a6e5eb4db0f528682671ad879ae27080665699d8b61681618f`.
The probe uses normal public C++ RuntimeWorld operations underlying JudasJS.
The JS dispatch path is source-traced, not independently executed by this probe.
The receipts show persistent absence/presence before and after the update, not
an observed tag disappearing or reappearing during that update. Republishing
authored tags explains the stale state by source inspection. Required-tag
raycasts are exercised; excluded-tag queries, movement/steps/support, replacement,
revisit and full tag/filter lifetime combinations are not.

No runtime patch was made for LIF-003. This finding is separate from the
authorized LIF-001/LIF-002 fixes and is deferred for a separate generic correction;
it does not invalidate their narrower authored-publication/ownership assertions.
