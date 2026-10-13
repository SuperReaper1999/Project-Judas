# M81 integration review

Scope: wheel component ownership, fixed-step/presentation separation, persistence,
prefab/entity references, streamed suspension/republication, and no-wheel paths.
This is source review plus the identified focused reproduction; it is not human
acceptance or comprehensive certification of every vehicle configuration.

## Established defect and correction

Per-object streamed registration exposed an ordering defect. Under the ordinary
distance fidelity policy, a managed chassis 1000 m from the focus entered Dormant
before a subsequently registered wheel introduced its Full-fidelity requirement.
The requirement then prevented policy evaluation, without reconstructing the
missing native body. Wheel-before-chassis registration worked; the lab's original
unmanaged garage did not exercise the failing order.

The original independent reproduction is preserved in
`managed-stream-original-failure.log`, its exact test source in
`managed-stream-original-test.cpp.txt`, and original source in
`managed-stream-original-WorldRegions.cpp.txt`. Hashes and unchanged inputs are in
`managed-stream-original-failure.json`. The pre-correction Release engine produced
**50 checks / one failure**; the reversed registration-order and unreferenced-body
negative controls passed.

`RuntimeWorld::StageRegionObject` now promotes an already registered reduced
chassis through the existing `SetEntityFidelity(Full)` reconstruction when its
wheel is registered. Staged native bodies remain disabled and unpublished. A
reconstruction error follows the ordinary staged-registration failure/retirement
path. If the wheel registers first, its existing derived requirement already
retains Full at chassis creation. This changes neither distance policy nor demo
configuration. The final lifecycle test additionally checks publication isolation.

Final combined validation receipts are recorded separately by the candidate's
validation runner; the preserved original failure is not overwritten.

## Ownership and lifecycle conclusions

- Wheel visual owners reject concurrent rigid-body, CharacterMotor, ragdoll,
  deformable and socket root ownership, and reject non-unit owner scale.
  The referenced chassis is an ordinary rigid body. Dynamic authority is needed
  for force response; static/kinematic handoffs remain safe and unavailable.
- `UpdateWheels` resolves the authored chassis EntityId to a native handle each
  step. Destroyed/reused body slots cannot replace that identity. Contact body
  references likewise retain generation-aware native handles.
- Wheel-owner destruction, explicit hierarchy destruction, region retirement and
  world Stop clear instance state. Independent native chassis destruction leaves
  its wheel handles safe but unavailable; a strict durable save rejects their
  missing required chassis rather than discarding state.
- Full fidelity is derived from current wheel references. Removing the last
  wheel or changing references releases only that derived requirement; unrelated
  `requiresFull` reasons remain intact. Authored reset discards commands, spin and
  runtime configuration. Destroyed entities intentionally remain destroyed under
  the pre-existing reset contract.
- Fixed evaluation changes native chassis/ground momentum; post-step publication
  supplies wheel centre/orientation. Renderer uses previous/current published
  wheel poses through `PresentedTransform`, ahead of ordinary hierarchy traversal,
  preventing double application of a parented chassis transform.

## Save, prefab and streaming conclusions

- The optional `wheels` save participant preserves commands, axle spin, phase,
  suspension travel and preceding load needed by rolling resistance. Raw native
  handles and cast scratch are not serialized. Missing/duplicate/invalid payloads
  fail the staged restore. Derived placement is reconstructed before resumed
  presentation.
- Wheel references use existing EntityIds and ordinary prefab/editor duplicate
  remapping. Whole-assembly adoption prevents separating the required wheel and
  chassis ownership. Cross-region chassis and current ground contacts pin the
  appropriate region; retirement removes derived requirements.
- Bounded retained wheel payloads use streaming archive version 5. Older
  no-wheel archives retain versions 1–4 and unchanged encoding. New save envelopes
  admit one additional optional participant, without replacing existing schemas.
- No-wheel scenes add no wheel fingerprint marker, durable participant or
  streaming wheel payload. Their policy, gravity and transform paths remain the
  original paths; the new independent negative control verifies that a distant
  unreferenced managed actor remains Dormant after publication.

## Explicit limits

The physical wheel is a swept-sphere suspension/tyre approximation with one spin
degree of freedom, not a separate colliding rigid wheel or full gyroscopic model.
Virtual wheel-only support does not itself form a rigid sleep island. Initial
overlap reports no penetration depth and does not fabricate positional repair.
Live orphan wheels are safe but are intentionally not valid durable assemblies.
These limits are documented in `docs/M81_WHEELS.md`; this review does not expand
the milestone to solve them.
