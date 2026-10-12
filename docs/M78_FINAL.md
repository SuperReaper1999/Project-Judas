# M78 final combined corrective candidate and audit handoff

**Uncommitted / unstaged. Human acceptance pending. M78 is not comprehensively validated.**
Starting HEAD remains `9e77b2226edcf217ab789fe4f0fea42a3415a72d` (M77, not human validated).
This report supersedes *current candidate status* in earlier M78 handoffs; their original source identities, failures, experiments and measurements remain unchanged.

Evidence: [closure directory](evidence/m78-closure/) · [current capability overlay](evidence/m78-closure/coverage/README.md) · [remaining coverage](evidence/m78-closure/coverage/REMAINING_COVERAGE.md).

## Correction summary and ownership review

| Mechanism | Established cause and retained correction | Exact implementation / boundary |
| --- | --- | --- |
| Angular momentum / energy | Explicit orientation drift kept angular velocity unchanged as world inertia rotated. Bounded implicit midpoint/Cayley body drift and world momentum transport correct the free-body contract. Anchored CCD certificates retain their anchored drift generator while physical angular velocity is transported at event boundaries. | `RigidBody.cpp/.h`, `RigidMotion.h`, `PhysicsWorld.cpp`. Positive-definite free-body contract; powered/constrained multibody dynamics remain approximate. No M77 gain, input, rig, cadence or threshold retuning. |
| Contact / CCD | Mesh-wide advancement needed its complete enclosure; clipped contact witnesses incorrectly shared one depth. Complete enclosure and per-witness separations fix these mechanisms. | `CollisionGeometry.cpp/.h`, `PhysicsWorld.cpp`. Actual physical response still constructs full manifolds and retains original contact/CCD requirements. |
| Completed observations | Actual solved start/TOI contacts shared invalidation with warm-start history. Separately completed observations preserve real solved loads without replaying an impulse. | `PhysicsWorld.cpp/.h`. Generation, revision, filtering and bounded contact/candidate limits remain. Warm cache is not a synthetic observation. |
| Boolean geometry query cost | Existence queries constructed/sorted full manifolds during repeated cooked-geometry searches. A shared visitor returns after a real accepted witness for Boolean callers only. | `CollisionGeometry.cpp/.h`, `PhysicsWorld.cpp`. Full response path unchanged; no distance skip, forced sleeping, mesh replacement or quality reduction. |
| Prefab construction | Sort comparisons repeatedly scanned the full authored scene. Precomputed first-occurrence ranks preserve stable order and new-child relative order. | `Prefab.cpp`. Existing authored gravity/reference precedence and identity machinery retained. |
| Streaming retention | Oversized retained state must pin without loss, then retry after legitimate consumer-owned shrink. The original multi-region test also needed an explicit capacity matching its real snapshots. | Existing `WorldStreaming.cpp` retry mechanism retained; stronger real-script shrink/revisit proof in `WorldStreamingTests.cpp`; explicit scalability fixture capacity, engine default unchanged. |
| Navigation dependencies | Terrain cook did not provide a complete owned-scene dependent-rebuild workflow. Explicit inspection/rebuild traces registered owned and explicitly published kit scenes through normal resolution/baking/publication. | `authoring/NavigationDependencies.cpp/.h`, `editor/TerrainTools.cpp/.h`, `tools/NavigationBake.cpp`. Saved inputs only; kit stays read-only; private unpublished content cannot be inferred. |
| Cold joint continuation | Latched commands, finite-pass joint guesses and completed publication were absent from the slot. Each influences the first resumed numerical boundary. | `PhysicsWorld.cpp/.h`, `WorldPersistence.cpp/.h`, `PhysicalBalance.cpp`. Articulation 4/contact 2, existing durable mappings and participant limit; guarded private reconstruction, no blind cache replay or settling delay. |
| Moving static anchor / motor cold query | The cached slotless static state overwrote actual prescribed body pose and omitted its motion. Motor capsule state must not replace its root authority. | `WorldPersistence.cpp`. Capture actual Full ordinary body pose/velocities; preserve reduced/root authority; reconstruct offset/filter/enabled/tag query representation before disk-save callbacks. |
| **Combined publication isolation, added in closure** | Refresh of an independent query-only capsule unconditionally cleared the restored physical publication flag before balance preparation. The real rows remained, but the task lost its first resumed step. | `PhysicsWorld::Impl::InvalidateRestoredContinuation`. Independent query-only mutations preserve publication; any attached joint or ordinary physical body still invalidates. Matching per-joint guesses still clear. No consumer/scene exception. |
| Windows warning | Inner motor ignored-body variable shadowed an outer body variable. | `WorldPersistence.cpp`: identifier-only rename. Linux object bytes unchanged; native compile verifies requested warning removal. Unrelated warning cleanup not attempted. |

[Detailed source review](evidence/m78-closure/review-physics/PUBLICATION_ISOLATION_FOLLOWUP.md) covers slot validity, destruction order, connected-proxy negatives, conservative ordinary-body invalidation and remaining bounds. Malformed/newer/missing paired continuation data reject a private candidate; partial parsing is not published to the live world. No new owner/controller/renderer architecture was introduced.

[Final failure-path review](evidence/m78-closure/review-physics/FINAL_FAILURE_PATH_REVIEW.md) additionally checks participant ordering, root/body authority, resources, resolved mods and publication. Live-world load isolation comes from SaveService's private reconstruction; calling `WorldPersistence::Restore` on an existing world is not an in-place rollback transaction. Navigation output publication is journaled best effort, not directory-wide crash atomicity. Pre-publication rejection preserves last-good output; failed filesystem rollback or an asset-database reopen after publication has a narrower guarantee. Shared resource-cache changes during private preparation are not promised to roll back.

The historical tests which assumed constant world angular velocity were aligned to the actual momentum-preserving drift contract. Their earlier bytes/results remain preserved. The original M77 controller/application fixtures and M70 CharacterLab inputs are unchanged.

## First resumed boundary and actual physical endpoint

Before the closure guard, the independent-motor fixture passed capture **25/0** but failed fresh-process read **13/1**. Its first task clock stayed at **0.35 s**. The direct physical-contact isolation probe failed **8/1**. Both failures and the rejected first observer setup are preserved.

After the guard, the same probes pass **8/0**, **25/0** and **13/0**, with the clock advancing **0.35 → 0.366667 s**. No timestep, original control, iteration budget or success threshold changed.

The separate read-only endpoint observer compares actual mapped rigid bodies and current completed foot contacts, not just cached controller history. Linux restored and uninterrupted endpoints match for all 13 reported bodies' physical fields and contact rows when keyed by durable entity identity. Fresh native body-slot handles and traversal order intentionally differ. Raw whole-document inequality and the corrected identity-based comparison both remain recorded.

- Actual upright: **0.9985843897**.
- Root height above current support: **1.3615871668 m**.
- Actual relative COM speed: **0.0000021378 m/s**.
- Two feet carry real completed support contacts; derived current physical standing is true.

[Before reproduction](evidence/m78-closure/linux-mechanism-before-v2/RESULTS.json) · [after execution](evidence/m78-closure/mixed-final/RESULTS.json) · [physical comparison](evidence/m78-closure/mixed-final/ACTUAL_ENDPOINT_BY_ENTITY.json).

The ordinary standing and recovery cold saves were also reread with the same final read-only body/contact observer, using copied exact existing snapshots and no new writer or controller change. Both reach actual supported physical standing with two real foot contacts. Linux standing upright/height/relative speed are **0.999934 / 1.418283 m / 1.996e-6 m/s**; recovery is **0.999990 / 1.249519 m / 3.629e-7 m/s**. Original and copied snapshot bytes remain unchanged. [Actual endpoints](evidence/m78-closure/ordinary-actual-endpoints/RESULTS.json).

## Final-source Linux verification

This is focused incremental Release compilation/relink using hashed unchanged objects and pinned dependencies, **not another clean/full-suite run**. Changed PhysicsWorld and the expanded continuation driver were compiled once. The identifier-only WorldPersistence compile is byte-identical to its previous Linux object. Additional existing Application test drivers were compiled only because their objects were absent.

| Exact route | Result |
| --- | ---: |
| Continuation positive/negative/context/static/motor/query ownership | 112 / 0 |
| M77 controller / foundation | 119 / 0; 34 / 0 |
| Rigid motion / momentum / energy | 10,239 checks pass |
| Physical animation / joints / ragdoll | 64 / 0; 29 / 0; 36 / 0 |
| Contacts / impact CCD | 1,076 / 0; 89 cases / 3,197 checks / 0 failures |
| Core persistence / prefab / retention | 27 / 0; 37 / 0; 82 / 0 |
| Gravity selection | 70 / 0 |
| M77 actual JS step / lifecycle / radial | 15 / 0; 26 / 0; 18 / 0 |
| M77 fresh step / stand / recovery cold pairs | 24/13; 23/13; 21/13, all zero failures |
| Ordinary standing/recovery actual-body readouts | Both supported; two real foot contacts each |
| M70 application / fresh cold pair | 98 / 0; 10/14, zero failures |
| Actual Application save + streaming, separate processes | write 11 / 0; read 19 / 0; read-b 19 / 0 |
| Actual Application signal lanes/lifetime + cold load | 17 / 0; 6 / 0 |
| Original navigation dependency fixture, proper owned input | 38 / 0 |
| Editor Edit/Play/Stop; export; moved standalone from `/tmp` | Pass; exact final runtime identity |
| Original `a106…` legacy cold read | **13 / 1 — preserved failure** |

Counts are assertions in their exact fixtures, not independent engine-capability counts. [Core matrix](evidence/m78-closure/final-build/LINUX_MATRIX.json), [application matrix](evidence/m78-closure/final-applications/LINUX_MATRIX.json), [selected Application routes](evidence/m78-closure/selected-applications-v2/RESULTS.json), [navigation](evidence/m78-closure/navigation-unit/RESULTS.json), [smokes](evidence/m78-closure/final-smokes/RESULTS.json).

The first navigation unit invocation omitted required arguments (exit 2); its log remains and the correct owned-project follow-up passes. The initial selected-Application link discovered missing driver/support objects; its failure remains before the narrow driver compile and reuse of existing runtime support objects. These are runner setup errors, not silently counted runtime passes.

## Native Windows functional validation

Final-source native Windows functional closure is complete within the following routes. [Native report](evidence/m78-closure/native/REPORT.md) · [case receipts](evidence/m78-closure/native/SUMMARY.json) · [source/object/binary identity](evidence/m78-closure/native/FINAL_BINARY_IDENTITY.json).

| Exact native route | Result |
| --- | ---: |
| Continuation / publication isolation | 112/0; 8/0 |
| Controller / rigid motion / physical animation / joints | 119/0; 10,239 pass; 64/0; 29/0 |
| Contacts / impact CCD | 1,076/0; 89 cases / 3,197 checks / 0 failures |
| Gravity / core persistence / retention | 70/0; 27/0; 82/0 |
| M77 step / radial / lifecycle | 15/0; 18/0; 26/0 |
| M77 fresh step / standing / recovery cold pairs | 24/13; 23/13; 21/13, all zero failures |
| Mixed motor/balance cold pair and actual physical endpoint | 25/13, zero failures; 13 body records and 16 completed contacts match within native pair |
| Ordinary standing/recovery actual physical reads | Both supported; two real foot contacts each |
| M70 application / new cold pair | 98/0; 10/14, zero failures |
| Current signals Application / fresh cold load | 17/0; 6/0 |
| Editor Edit/duplicate/undo/Play-Stop/save | Exit 0 |
| Final runtime in moved existing ordinary export | Exit 0; unchanged 1,720-frame four-scene/save/load/pause/reload harness |
| Exact legacy `a106…` read | **13/1 — preserved failure** |

The native mixed current physical endpoint has upright **0.998602**, support-relative root height **1.361870 m** and relative COM speed **7.136e-7 m/s**. Ordinary standing and recovery body/contact reads also demonstrate supported standing. Within-platform physical comparisons exclude only fresh native handles and traversal order; no cross-platform bitwise trajectory promise is made. CharacterLab's cold pair establishes authority and finite pose, not full M70 trajectory equality. The 112-check continuation fixture separately measures exact actual static-anchor pose/linear/angular velocities and pre-callback motor root/query reconstruction.

The final native links place freshly compiled PW/WP objects exactly once before the unchanged frozen archive (`71a722dd…`), with extraction proof that old members were not used. This is explicit object replacement, not a claim that the frozen archive itself contains the latest source. The requested shadow warning is absent; two pre-existing WP conversion warnings remain. Linux rename-only object bytes and native `.text` sections are unchanged; native raw COFF metadata differs.

Storage/setup link and preflight failures remain preserved. Only named generated duplicates were retired after lossless host hash/size verification; VM configuration and historical inputs remain unchanged. A stale copied signal-demo script caused **10/1 + 4/3** failures; the exact current copied project passed **17/0 + 6/0** using the same binary. This was a verification-content correction, not an engine change. The moved smoke replaced only the runtime in a new copy of an existing export and used an unrelated working directory with unavailable engine root; it was not a fresh native export-generator run. Its 4→2 MiB change was a diagnostic disk-reserve preflight only, not a simulation or acceptance threshold change.

The final-source native SaveStreaming Application route, the four new Linux-only audit observers, a fresh native export-generator run and full M70 trajectory equality remain explicit gaps. Earlier unaffected native receipts remain prior-source evidence rather than fresh executions.

The unchanged Windows 10 VM is a native functional environment, not a performance gate or physical GPU/controller/audio validation platform. Its preserved 56.376 ms historical maximum remains diagnostic. No effort was made to meet a native-host timing target inside the VM.

## Linux performance and remaining tails

Unchanged workload: four original 13-body articulations, 48 constraints, 8,192-triangle floor, 900 steps at 60 Hz, full contact/CCD requirements. No new performance engine change was made during closure.

| Measurement | Median ms | p95 ms | Max ms | Steps over 16.67 ms |
| --- | ---: | ---: | ---: | ---: |
| Preserved failed corrective candidate | 1.040693 | 18.875046 | 33.793832 | 78/900 |
| Prior cold/static repair | 1.129538 | 6.184333 | 28.697686 | 12/900 |
| First final closure run; external compilation observed afterward | 2.413731 | **17.700774** | **95.307776** | 50/900 |
| Serial CPU-observed prior archive | 1.26542 | 6.31734 | 30.5498 | 13/900 |
| Serial CPU-observed final archive | 1.29902 | **7.06230** | **33.3996** | **18/900** |

The later final **wall-time p95** passes the unchanged 16.67 ms host gate; thread CPU p95 is **6.91117 ms**. The thread clock is observational and identical in both serial variants. Eight unrelated compiler processes and approximately 46% CPU pressure were observed after the earlier raw miss. No process was stopped/reprioritized and no affinity/resource setting changed. The original run lacks per-step CPU clocks, so exact attribution of each old stall is unproved; its failed raw measurement is retained, not averaged away. Serial ordering and uncontrolled external load also preclude a causal optimization claim for the small prior/final timing difference.

All runs retain **56,576 contact rows, 19,163 CCD iterations, seven bounded fallbacks and 26/52 sleeping bodies**. Collision/CCD tails and incomplete mesh articulation sleeping remain real limits. Earlier bounded profiler analysis identifies continuous impact search/event solving as the dominant expensive scope, not a claim that its entire wall duration is isolated linear constraint CPU.

[Original raw final measurement](evidence/m78-closure/final-performance/RESULTS.json) · [contention observation](evidence/m78-closure/final-performance/HOST_CONTENTION.json) · [serial CPU/wall diagnostic](evidence/m78-closure/cpu-observer/RESULTS.json). Linux and native Windows build/simulation workloads were scheduled serially.

## Resumed original capability audit

The corrective work is not the comprehensive audit. The overlay retains **485 overlapping discovery IDs**: 345 public JS rows, 89 mechanism groups, 34 components and 17 asset families. It preserves original platform states and attaches narrowly scoped actual case proofs. Symbol enumeration, source locations and related-suite links are not per-member validation.

Four additional high-risk contracts were traced from public/brief expectation through implementation, discriminating positive/negative oracle and actual observation:

1. **Streamed motor publication: 46 checks / 8 failures.** Newly reproduced **pre-existing LIF-001**. Initial capsules publish default layer/mask; an authored-disabled motor can be enabled; retained active capsules revisit disabled. Correct rotated offset, root/velocity retention and stale-generation rejection pass. Zero-dt ordinary character update repairs the observed metadata. Relevant lifecycle sources are exactly HEAD. Combined tag-filtered casts fail, but isolated tag-state diagnosis is source-derived because the wrong layer/enabled state already blocks them. No unrelated production repair was attempted.
2. **Resolved data-only prefab/mod policy: 17 / 0.** A linked published prefab reintroduces a slot absent from cached scene data. Valid published behavior/schema composes; absent schema and wrong typed property reject actual resolved content without evaluating a project constructor. This is not a security-sandbox or all-mod-payload certification.
3. **Navigation source revision after real bake: 34 / 0.** An ordinary linked prefab changes at the post-bake boundary while cancellation returns false. Current inputs are revalidated, stale publication rejected, all other published bytes preserved, staging removed; fresh explicit rebuild and normal export validation pass. This is distinct from a standalone launch or every kit/dependency race.
4. **Legal simple dynamic save exclusion: 22 / 1.** Newly reproduced **pre-existing LIF-002, medium/P2**. The ordinary control restores all participants and actual velocity (**12/0**). The excluded actor is legally accepted and absent from the captured scene, but restoration rejects its orphaned sleep record (**10/1**, `saved sleep body unavailable/duplicate`). JudasJS dispatch was source-traced; this probe executes the native public mechanism and full capture/private prepare/restore, not a VM call or asynchronous save-service UI test. The same entity exclusion and sleep-collection control flow exists at HEAD. No unrelated production correction was made.

[First three audit results, mechanism and reproduction](evidence/m78-closure/audit-lifecycle/linux-current-01/SUMMARY.md) · [measured save-exclusion defect and exact reproduction](evidence/m78-closure/audit-lifecycle/save-exclusion/MEASURED_RESULT.md). Actual save/streaming and signals Application routes add their own cross-system evidence above. Fixture include/incomplete-type/metadata-order development errors and corrected follow-ups remain preserved.

Original requirements: the supplied M77 brief is verified at SHA-256 `e2ba1ffe8d9b8db4019ec120d425cbcda41ce03746003bb0943286e53802622b`. All 77 identities remain in the requirement overlay; **58 original briefs remain unavailable**. Availability of 19 originals does not imply every claim reconciled. [M77 reconciliation](evidence/m78-closure/coverage/M77_RECONCILIATION.md) and [explicit gaps](evidence/m78-closure/coverage/REMAINING_COVERAGE.md) preserve this distinction.

## Unresolved defects and limitations

1. **LIF-002, medium/P2:** a legally excluded simple dynamic actor leaves an unrestorable required sleep record. Aligning sleep eligibility with entity/articulation ownership is a proposed separate correction boundary; do not discard required internal articulation bodies. It is pre-existing and remains unfixed.
2. **LIF-001, medium/P2:** streamed/lazy motor query metadata and enabled-state publication gap. Reproduction and scoped proposed boundary above; not repaired, not a demonstrated M78 regression.
3. **FLU-001, deferred:** preserved unchanged Linux legacy-PBF four-second settling check fails at 0.558701396 m/s against `<0.5 m/s`; a prior native result passes. The independent proposed criterion is for later review only. No fluid source/test/threshold, M54/M55, JSGT, FTFT or Solbert research changed.
4. **Legacy `a106…`, compatibility limitation:** exact 52,557 bytes and SHA `a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b` remain. The old slot lacks latched/numerical/publication history. New-format success cannot supply that missing historical state; the unchanged failing read remains. Older static records cannot recover omitted velocities either.
5. **Physics tails:** p95 gate passes in the later host observation, but maxima, bounded CCD fallbacks and incomplete mesh sleeping persist. No unconditional responsive-under-contention guarantee.
6. **Coverage / hardware:** per-member JS errors/results/lifetimes; every authored field/edit/override; malformed asset/cancellation/cook/mod/export combinations; original brief gaps; fresh human review and physical Windows hardware remain incomplete. Fixed origin is still fixed, not live rebasing; curated mod payload and anatomy/controller bounds remain. Several older milestone-guide status headings still describe their original candidate stage; the current architecture/handoff takes precedence rather than silently reinterpreting those records.

## Consolidated human acceptance checklist

1. In `projects/balance_lab/balance_lab.judasproj`, use the documented F1–F4 scenes and ordinary controls: active physical step, moving/rotating support, fallen recovery and radial gravity. Confirm real physical standing and sensible negative cases.
2. Save with F6 while a corrective step is in progress, then F7/load in a fresh run. Confirm bodies/feet physically reach supported standing, with no skipped clock, stale success flag, pose teleport or controller retuning.
3. In CharacterLab, exercise active/passive articulation, impulses and the prescribed moving anchor; save/load while active. Confirm no floor-impact lockup or cold-restored anchor discontinuity.
4. In the current streamed experience, walk out/back, exercise real save/load and signals, and inspect query-sensitive callbacks around activation. Treat LIF-001 immediate filters and LIF-002 excluded-dynamic-body restoration as unresolved. Review their preserved negative fixtures and decide their separate follow-up boundaries; they are not working-feature claims.
5. In the terrain/dependent-navigation workflow, save/cook a collider edit, inspect both dependent scenes, rebuild explicitly, then test cancellation/stale rejection and retained last-good content. Published kit inputs must remain read-only.
6. Inspect the mod negative fixtures and missing/typed behavior diagnostics; valid published linked content should work through ordinary public architecture.
7. Open the editor, Edit/Play/Stop/reload, then run the moved final package from an unrelated directory. Recheck capture/focus, rendered physics and usability manually.
8. Later validate physical Windows GPU/input/audio/controller behaviour. Native VM results establish the recorded functional paths only.

## Fingerprints, exact files and checkpoint readiness

[Final source/binary manifest](evidence/m78-closure/FINAL_FINGERPRINTS.json), [verification receipt](evidence/m78-closure/FINAL_VERIFICATION.json) and [exact checkpoint file inventory](evidence/m78-closure/CHANGED_FILES.txt) define the final review boundary. The verification receipt is separately generated metadata outside the self-hash graph. Executables, archives, caches and the new diagnostic linker MAP outputs remain hash-only local artifacts, outside the checkpoint inventory. No files are staged; no commit/push/tag or M79 work occurred. Final verification checks the frozen repository, all older M78 evidence layers, original fixtures/local inputs and unrelated `POST_M73_REPORT.txt`; it permits only the explicitly reviewed current changes. Independent fluid research is untouched.

The operator's late documentation request additionally updates README current status/navigation and the copyright owner line in README and LICENSE to **Conner/SuperReaper1999**. MIT permission terms are unchanged. These documentation-only changes do not change any compiled runtime or test semantics.

**Corrective candidate: ready for operator checkpoint authorization independently of the remaining audit coverage, subject to the linked final verification receipt passing.** The corrected numerical/save/publication contracts have focused Linux and native Windows functional evidence and actual physical witnesses. No newly demonstrated severe blocker in the retained corrections remains. The two pre-existing lifecycle/save defects, deferred fluid contract issue, missing legacy history, measured physics tails and remaining coverage/human/hardware gaps are explicitly retained. This is not comprehensive M78 validation, acceptance of every engine capability, or human validation of M77/M78.
