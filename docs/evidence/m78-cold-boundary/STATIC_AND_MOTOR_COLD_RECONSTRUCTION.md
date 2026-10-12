# Static anchors and motor query reconstruction after cold restore

This is the narrow follow-up to [the continuation repair](COLD_CONTINUATION_REPAIR.md). It records copied-source diagnosis and proof. Actual production application and final Linux/Windows results belong to their separate source-application and final-combined receipts. Original M70 and M77 fixtures, gains, thresholds, solver budgets and cadence remain unchanged.

## Original M70 rejection

The first actual combined Linux matrix passed M70 cold-write **10/0**, then rejected cold-read **14 checks / 3 failures**. The true restore diagnostic was `joint continuation physical context differs`; the original check's label evaluated its error string before the Restore call. A logging-only copied fixture exposed the diagnostic without changing the test.

[The failure receipt and field comparison](character-restore-error/CONTEXT_DIFFERENCES.json) identify the first joint's static anchor. Joint settings, motion authority, shape, mass/inertia and filtering matched. Its actual position/orientation and prescribed linear/angular velocity did not. The saved body was at `(-1.80292249, 1.09743369, -0.03864399)` with a tilted orientation and nonzero velocity, but reconstruction returned its initial `(-1.8, 1.06727505, 0)`/identity/zero-velocity state.

The exact original 107,787-byte failing snapshot is retained as [m70-static-anchor-original-failed.save](character-restore-error/m70-static-anchor-original-failed.save), SHA-256 `e99338822e8843b91467e535b1e7e85116e12c33be4580f10b34b79e39539fee`. It is failure evidence, not a build artifact.

### Cause

`WorldRagdoll` prescribes static partial-articulation anchors through ordinary PhysicsWorld pose/velocity setters. Those bodies need not have a managed entity slot. `RuntimeWorld::GetEntityState` returns cached entity state for a slotless record, so it can differ from the actual prescribed body.

`WorldPersistence::Entities` captured a correct presented transform into its saved scene, but its separate physical record used that stale cached state. On read, `SetEntityState` overwrote the reconstructed physical pose with the stale physical record. Its static-body path also did not restore prescribed velocities. The strict new continuation guard correctly refused the resulting different physical boundary. Relaxing that guard would conceal an existing persistence omission.

### Correction

Within the existing entity participant and physical record:

- For a **Full-fidelity real SceneBody**, capture actual PhysicsWorld pose and linear/angular velocity, including slotless static bodies.
- Preserve normal cached authority for Coarse/Dormant entities and entities without a real rigid body.
- Exclude CharacterMotor observation capsules: their offset query pose and velocity are not the entity's authoritative root motion.
- On read, restore saved velocities to the corresponding Full-fidelity real body after normal entity-state reconstruction and before force restoration.

The saved motion authority, body settings, entity-reference scheme and participant format are unchanged. Old entity records can be read, but omitted actual state in an old capture cannot be promised retroactively exact.

## Motor query reconstruction

The narrowed static correction exposed a separate existing cold reconstruction window. `SetRuntimeTransform` placed a CharacterMotor observation capsule at the entity root. Ordinary `WorldCharacter::UpdateCharacters` later refreshes it at `motor.position + motor.orientation * settings.offset`. Until that first update, queries could see misplaced geometry. Fresh query capsules also retained default layer/mask/tags/enabled values until normal update; a disabled motor could therefore expose an enabled capsule during that window.

The private `WorldPersistence::Motors` read path now reconstructs that derived capsule immediately after restoring the motor:

1. Use the actual saved motor root and rotated authored shape offset for its pose.
2. Publish the authored enabled state, collision layer/mask and entity tags.
3. Preserve existing root velocity ownership and safe support/ignored-body remapping.

This performs no simulation advance, callbacks, force integration, controller phase change or cache replay. It runs before resumed scripts and before deferred joint/contact continuation. It does not change ordinary live transform or CharacterMotor movement semantics.

## Copied proof

The exact final proposal and append-only permanent test patch are [indexed here](static-anchor-final-proposal/FINAL_PROPOSAL_IDENTITY.json). WP SHA-256 is `39ae0eb2132f7b5f6f43710dd82accf38ef21b04dc1c57bd8af4279da84ad2bb`; the proposed permanent continuation test source is `2c37b796a0a4e78935b1009b500e15063c9730414389e785812f341544117f69`.

| Exact copied-source case | Result |
| --- | --- |
| Original M70 cold-write / fresh cold-read | 10/0 / 14/0 |
| Original M77 in-progress cold-write / fresh cold-read | 24/0 / 13/0 |
| New static/reduced/motor guards, original frozen archive | 49 checks / 10 failures |
| Same guards, static-only copied correction | 49 checks / 8 failures |
| Same guards, final static plus motor reconstruction | 49/0 |
| Instrumentation-only original M70 body observer | 10/0 / 14/0; exact 37-body parity |

[Application receipts](static-anchor-final-proposal/RECEIPT.json), [guard receipts](static-anchor-final-proposal/GUARDS_V2_RECEIPT.json) and [actual body parity](static-anchor-final-proposal/FINAL_ACTUAL_BODY_PARITY.json) retain commands, source/archive identities and raw outputs. The body comparison distinguishes **35 real rigid bodies** from **two derived motor query proxies**. Both groups match immediately after restoration, before callbacks or resumed steps; proxy parity is not used to claim those capsules own root state.

The 49 new guards cover an independently prescribed slotless static body, normal Coarse/Dormant transitions, and enabled/disabled motors with arbitrary orientation and nonzero offset. They check authoritative motor/root motion, actual pre-step query pose, collision layer/mask, tags, enabled state, layer-filtered ray hit geometry and normal first fixed-update behavior. No object-name physics branch or hidden native gameplay path is involved.

### Preserved diagnostic failures

The initial broad RuntimeBody capture was rejected because it could substitute an offset motor query capsule for root authority. An early minimal static probe incorrectly assumed its lazy entity record already existed; the corrected fixture uses normal `WorldPersistence::Prepare`. Another early motor probe omitted the source's normal query refresh. Their original outputs remain in the preceding copied-proof directories.

The expanded arbitrary-axis probe initially reported **49/2** from exact cached entity quaternion comparisons. Ordinary `SetEntityState`/`SetRuntimeTransform` normalization changed cached quaternion components by at most `1.20e-7` (L2 `1.248e-7`), corresponding to approximately `2.04e-8` radians after double-precision normalization. Motor state and physical proxy fields were already exact. [The measured rounding record](static-anchor-final-proposal/CACHED_ENTITY_QUATERNION_ROUNDING.json) preserves that failed probe. The revised **new** cached-only assertion uses an explicit `2e-7` component-L2 tolerance; actual motor/proxy comparisons and all historical acceptance thresholds remain exact/unchanged.

## Preservation and limits

The original M70 rejection and the old `a106ce...` M77 incomplete-format failure remain preserved. No old snapshot is silently rewritten or repaired with invented absent state. New capture/restore correctness is proved separately from legacy compatibility.

No fluid code, research, M70/M77 gameplay fixture, physics solver, motion-authority rule, gains or thresholds were changed by this follow-up. The correction adds no JS API, no participant and no save-format version. Nothing has been staged, committed or pushed.
