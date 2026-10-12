# Final combined failure-path source review

## Boundary and conclusion

Read-only inspection of the current combined source against `docs/M78_FINAL.md`, after the lifecycle audit. No compilation, simulation, test invocation, production edit or earlier-evidence overwrite occurred in this review. The inspected checkout has HEAD `9e77b2226edcf217ab789fe4f0fea42a3415a72d` and an uncommitted candidate. Source hashes below identify this inspection boundary; they are not new validation results or a promise about files edited afterward by other agents.

**No additional severe defect in the retained M78 continuation/static/motor repairs was established by this review.** One previously undocumented **pre-existing save-exclusion mismatch** is established by source control flow, with runtime reproduction still outstanding. The existing LIF-001, legacy `a106...` continuation gap, FLU-001 and incomplete capability coverage remain unresolved as stated in the final report. Review does not turn any capability row into a pass.

## Retained mechanisms inspected

- **Root versus body authority:** `WorldPersistence.cpp:47–60` captures actual pose/velocities only for a Full-fidelity ordinary entity body without CharacterMotor. Coarse/Dormant cached state and motor root state remain separate. The read-side Full/ordinary-body guard mirrors capture. This covers the prescribed slotless-static correction without treating a motor query capsule as its root.
- **Private derived motor reconstruction:** `WorldPersistence.cpp:64–75` restores motor state and then resets the capsule at `position + orientation * offset`, with saved/authored enabled/layer/mask and current entity tags. Support/ignored bodies are remapped through durable entity identity. No fixed step or callback is performed there. `RuntimeBody` prioritizes a motor capsule (`WorldScripts.cpp:16–20`), which is why the ordinary-body exclusion is necessary.
- **Continuation order and mapping:** articulation format 4 defers the command/numerical payload (`WorldPersistence.cpp:138–156`). It is consumed after physical-animation/balance and deformable/fracture reconstruction, before sleep and completed contacts (`:246–257`). Counts, duplicate mappings, owner/joint ordinal and actual body-entity pairs are checked. Articulation 4 must pair with contacts 2; older formats retain their prior behavior.
- **Numerical context:** `PhysicsWorld.cpp:283–321,2832–2855` compares physical/settings bytes and validates finite fields through SaveArchive, the bounded interval and near-unit target. The next actual Step discards saved numerical impulses if its interval differs (`:2074–2080`). Latched actuator commands are separate from numerical guesses. These are bounded private reconstruction records, not raw native pointer/slot serialization.
- **Actual completed publication:** `PhysicsWorld.cpp:2864–2929` restores only eligible positive solved contacts, remaps current generations/revisions, validates physical contexts/filter/primitive identity, and distinguishes unavailable from completed empty. Excluding an otherwise eligible transient contact demotes completeness instead of inventing zero contact. These records are distinct from touch events and solver warm-start caches.
- **Combined ownership isolation:** `PhysicsWorld.cpp:265–281` excludes an independent query-only proxy from physical-publication invalidation while conservatively treating an attached proxy as physical. Matching joint guesses still clear. `DestroyBody` invalidates before attached joints are erased (`:1445–1450`). The first-restored balance sample uses this token without changing standalone History-only waiting (`PhysicalBalance.cpp:281–288`).
- **Load rejection:** the public save service builds and prepares a separate RuntimeWorld, restores all participants, session/streaming, locale and audio preparation, then validates a normal GameSession before swapping (`SaveService.cpp:120–155`). Early rejection destroys only staged ownership (`Finish`, `:83`). `WorldPersistence::Restore` itself mutates its supplied world and is **not** an independent in-place rollback transaction; the service's private-world discipline provides the advertised live-world isolation.
- **Resources:** installation checks task ownership and generation (`ResourceManager.cpp:207–224`); released demand cancels, drops staging and bumps generation (`:367–387`). Pump discards stale prepared GPU work instead of publishing it (`:418–473`). Shared caches can legitimately change while a private load prepares; this is not a claim that every global cache byte is rolled back.
- **Bake publication:** current geometry is reread after real baking before `PublishAuthoringOutputs` (`NavigationDependencies.cpp:55–58`), whose destination expectations are checked before and during the journaled multi-file publish (`AuthoringDocument.cpp:18`). Failed rollback preserves external edits rather than overwriting them. The recent 34/0 receipt is execution evidence for the tested race, not evidence of every filesystem failure mode.
- **Resolved mod policy:** `ModRuntime.cpp:57–74` inspects actual resolved prefab script slots for data-only packages before any RuntimeWorld/project script starts. The 17/0 lifecycle receipt establishes that specific positive/negative contract; this source review adds no security-sandbox claim.

## Newly identified source defect — LIF-002 (proposed identifier), P2

**Legal transient dynamic-body exclusion leaves an unrestorable sleep record.** This was not exercised during this review.

1. `SetTransientEntity` permits a simple dynamic body without a required component (`WorldPersistence.cpp:278–281`); JudasJS `saveExclude` dispatches to it (`ScriptSystem.cpp:506`).
2. Entity capture omits that non-articulated transient object (`WorldPersistence.cpp:29`).
3. Sleep capture independently includes **every owned dynamic body**, with no transient exclusion (`:209`).
4. On restore, that durable entity was not reconstructed, but the sleep reader requires it to resolve to a dynamic body (`:253`), and rejects with `saved sleep body unavailable/duplicate`.

The same entity exclusion and sleep collection loop exist at current HEAD. Thus this is not a demonstrated regression introduced by retained M78 work. `tests/WorldPersistenceTests.cpp:32–33` marks its runtime dynamic actor transient and verifies capture/scene omission, but does not restore that excluded snapshot; its passing result does not cover the contradiction above.

A minimal future reproduction is an ordinary modern world with one persisted body plus one simple dynamic runtime actor marked transient, followed by normal capture/private construction/prepare/restore. Assert capture succeeds, the transient scene object is absent, and restore succeeds. At current source the last assertion is expected to fail from the unmatched sleep record. The corresponding generic correction would align sleep-record eligibility with the entity participant while retaining articulated transient bodies; neither a test nor correction was added here. Do not label this source-established finding as a measured runtime failure.

## Important qualifications and remaining limits

- No old snapshot can retroactively recover numerical history, commands, publication or static velocities that were never saved. The original legacy failing fixture remains evidence rather than a repaired compatibility promise.
- The body context includes immutable collision source/settings fingerprints, compound geometry and terrain radius bounds; it is not a complete independent fingerprint of arbitrary procedural terrain. Normal disk saves additionally require exact project/content/mod identity. Direct native use of these helpers alone has a narrower context contract.
- Exact source/read sample parity and current-standing witnesses come from earlier execution receipts. This inspection did not re-execute them or claim cross-platform bitwise physics equivalence.
- Normal lazy/streamed motor metadata publication remains LIF-001. The private disk-save reconstruction path does not repair the separate region activation/revisit route (`WorldCharacter.cpp:6–14`, `WorldRegions.cpp:46–68`). Ordinary explicit motor transform writes also retain their documented pre-update proxy window.
- The save-service final swap calls `InteractivePlay::Begin` after validation. In the ordinary modern path GameSession has no data-dependent failure after the same private validation (`GameSession.cpp:15–63`); allocation failure/unexpected future startup failures are not transactionally rolled back after the swap. This review establishes no reproduced current blocker from that exceptional boundary.
- `PublishAuthoringOutputs` is a journaled best-effort multi-file transaction, not process-crash-atomic publication of the whole directory. Rollback can fail under filesystem errors or preserve concurrent external edits. Navigation then reopens the asset database **after** successful output publication (`NavigationDependencies.cpp:58`); a failure during that reopen may return false with files already published. Prior-output retention should only be claimed for pre-publication rejection and successful rollback, not every conceivable false return.
- Finite-pass continuation is justified for this existing solver and guarded context; it is not a universal stability guarantee for arbitrary nonlinear controllers. Actual contact queries remain bounded and can fail on candidate/contact overflow.

## Inspected byte identities

| File | SHA-256 |
|---|---|
| `src/WorldPersistence.cpp` | `14a5ce3a78eebb2db801f91ddd91dc35808f23ff637a797c7cbb599d2e756ad4` |
| `src/WorldPersistence.h` | `045ba6e7a103eeb0781196d726ede4572fd88fb1f329de0da045ab54a1a5910c` |
| `src/PhysicsWorld.cpp` | `6a60747e4bb970821e2ebb68adbe78ea89f6fdc3db9137b5160290ca1da2f193` |
| `src/PhysicsWorld.h` | `0f8510bce0a3741ecdec7e94973f4d861b5e04ebe5f0a908aab8ba0a80b9bca9` |
| `src/PhysicalBalance.cpp` | `e7a41780fcd5a1196e3d57066b38fa8f2735714f7cb3a16675ace47379e24e32` |
| `src/SaveService.cpp` | `4daecfe95c7854e3c19f64fdb202620736214dbb54588c5084c204313684ca60` |
| `src/SaveArchive.h` | `9f7a2cb6b75eaf77623f76142168f57b9fa917607e7d4441948eee4fbc0d347b` |
| `src/SaveStorage.cpp` | `cc190c27fa3d01aa45f1dcdde8ef89c0685aa531e1550e9bcfa7979bc6d7ad07` |
| `src/RuntimeWorld.cpp` | `1e9400f8276ba4e86a1f030610fe50a87c9e76d9777e628dd609ea58c4f6fc02` |
| `src/WorldCharacter.cpp` | `381194ab8c9440e588d54a66a3594e77856924dc661ce77937174602e804817a` |
| `src/WorldScripts.cpp` | `3dbb2075270ef0700ce831c08a1e50109a173586651b7e97618e17c971d4688a` |
| `src/WorldRegions.cpp` | `84a9025c0e99a4f76646f36c45b4f56e2af3a3472782c254f564619f9660236f` |
| `src/MotorPersistence.cpp` | `7707e2a9ccda11c168e06066a68b0a263b810886805c1630241d47c90a540ace` |
| `src/GameSession.cpp` | `3c5f002c231309dcf283417c05f2d77e5c197b0e253ee6f02cf6665de13e12f6` |
| `src/InteractivePlay.cpp` | `e870bd3149bd62c7038b8336af5498278e87accdec573c12092c7d8e40264fe4` |
| `src/WorldAudio.cpp` | `81980db4afe5f8d293c83b3c6afaaa979d59b8f384efc3b018bb6756ad8615ab` |
| `src/ResourceManager.cpp` | `408db1ec47556d4e239bb396d0348c13cff9815012feda5c7237865fa83a1b8f` |
| `src/authoring/NavigationDependencies.cpp` | `5666758985926e221ee87f0120698cd6ee1a4e0b2bd2600d4f776224ee949120` |
| `src/authoring/AuthoringDocument.cpp` | `271ce62241a0a07b446d84ea636e2ea74c641359578e99d0d6ec71003f4acead` |
| `src/authoring/ModAuthoring.cpp` | `a93370fb2fb7de4af9b456f96a352841000a1404453973a7e32640b05ba73856` |
| `src/modding/ModRuntime.cpp` | `81e4b8bf77a56cfd91722a23a7126fbacb41e2761c6fde13260785b8f482b06c` |
| `tests/WorldPersistenceTests.cpp` | `b51c36f46c6bb69aa9bb84ffbaa87c222ab5e3b709a354974b0d84c2ce0a6853` |
| `docs/M78_FINAL.md` | `3caeae7a7aca3c823d798df1b236a5c426e432d1e1116ce749963549c20ffae0` |
| `docs/judasjs/saves.md` | `9bb2d911e92f10e8d37ebe930a332397060ae983434bb05fcb40e7e86bd7dc38` |

The only new file written by this review is this receipt. Production, original tests, old evidence, protected research, builds, staging and commits remain untouched by this reviewer.
