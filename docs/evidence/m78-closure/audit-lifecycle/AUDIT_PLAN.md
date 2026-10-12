# M78 lifecycle closure audit — prepared probes

Status: **prepared, not compiled or executed**. This is a current-source review and three deliberately bounded standalone fixtures. It does not certify whole subsystems, and it does not rewrite the frozen discovery register or original evidence.

## Authority and original brief gaps

The frozen `CAPABILITIES.json` explicitly has 485 overlapping discovery rows and zero claims that every declared input is independently proved. A related suite link is not a semantic oracle. `REQUIREMENT_CROSSWALK.json` still reports missing original briefs for M43, M53 and M68 (also M38). The available M59, M67 and M76 briefs support the specific contracts below. This review does not reconstruct absent requirements from modern documentation.

Relevant frozen discovery rows: A.04 identity, A.05 prefab remapping, A.06 component authority, I.01 modern save participants, G.04 navigation staleness, J.06 mod policy, J.08 moved exports, plus `ASSET.Prefab`. Relevant interaction routes are `identity-lifetime`, `cold-active-state`, `author-cook-package` and `jobs-replacement`. Existing corrected cold-save tests are useful evidence for their exact world-persistence route; they do not prove streamed region reconstruction uses the same mechanism.

## Selected contracts

| Priority / bounded contract | Actual expectation and source path | Existing evidence gap | Positive and negative oracle |
|---|---|---|---|
| 1. Streamed motor query representation at publication | M59 lines 116–122 require privately staged content to publish a complete validated activation unit before queries/callbacks. Authoritative motor root is distinct from its capsule at `root + orientation * offset`. `WorldStreaming::Advance` calls `StageRegionObject`, optional `RestoreRegionObject`, then `PublishRegion`. `WorldCharacter::RuntimeCharacter` creates geometry; `UpdateCharacters` normally sets filter/tags/enabled and offset pose. | Existing streaming tests assert retained root velocity and safe retirement, but not the actual filtered query geometry immediately after initial activation or revisit, before a fixed update. New cold-save reconstruction proof is a different path. | Cast through the real rotated offset capsule using its authored layer and required tag; verify disabled motor is absent. Repeat after root/velocity edit and unload/revisit, require fresh generation and unchanged simulation time. Then run ordinary zero-dt `UpdateCharacters` as a diagnostic comparison, without making initial assertions depend on it. Missing publication synchronization produces wrong/absent casts, default layer/tag or wrong enabled state. |
| 2. Data-only linked prefab policy after resolution | M76 lines 100–104 require indirect script dependencies and published behavior/schema enforcement. `ModContent::ValidateReferences` checks authored content; `ModRuntime::ComposeContent` subsequently resolves the actual prefab and checks every resulting slot. A published prefab is not permission to conceal private executable behavior. | `ModContentTests` covers manifest trust, packages, leases and publication failure. `ModApplicationTests` covers a real permitted composed game. Neither is a focused negative oracle for slots reintroduced by prefab resolution when the requesting scene's cached slots are absent. | Valid published prefab/behavior/schema composes without constructing a world or evaluating its throwing constructor. Remove the schema while retaining file publication: must fail at the resolved behavior check. Change the source prefab's property to the wrong type: must fail the actual schema. Restore valid property: normal composition succeeds. Deleting resolved-slot enforcement would make the two negatives accept. |
| 3. Navigation input revision changes after bake, before publication | M67's source-revision/transaction contracts plus the current `NavigationDependencies.h` contract require explicit owned rebuild and last-good preservation. `Uses` traces prefab asset dependencies; `RebuildNavigationDependencies` bakes privately, re-reads current resolved geometry, then calls `PublishAuthoringOutputs`. | Existing focused test meaningfully covers malformed late surface, real post-bake cancellation, two direct terrain dependents and appearance-only edits. It does not independently exercise a supported linked prefab input changing after bake while the callback returns **false** (not cancellation), nor byte equality of every untouched published output on this race. | Two owned scenes share a linked physical prefab and initially fresh bakes. Change collider source: both become stale. Change it again at the third existing cancel poll (after actual bounded bake), returning false. Require `inputs changed` rejection, exact all-other-published-file bytes, no published orphan and no scratch. Explicitly rebuild the current source and require both fresh plus normal export scene validation success. Without revalidation the stale intermediate bake is published and the negative fails. |

These are supported linked prefabs. `ValidatePrefab` explicitly rejects nested prefab references with `nested prefab references are unsupported in M36`. No probe invents nested-prefab support, and that limitation remains visible.

## Source concerns to discriminate, not yet runtime findings

1. `WorldRegions.cpp:61–70` restores a staged motor via `SetRuntimeTransform` / `SetEntityState`, then disables its body. `WorldScripts.cpp:61` writes an observation capsule at the root; `WorldCharacter.cpp:12,35` uses the rotated shape offset. `PublishRegion` (`WorldRegions.cpp:43–59`) currently enables authored `SceneBody` registrations, not motor-only capsules. `UpdateCharacters` normally fixes pose/filter/tags/enabled later. This is a concrete candidate for a callback/query window, not a measured failure yet.
2. The mod resolved-policy block (`ModRuntime.cpp:58–75`) is present and expected to protect the indirect behavior seam. Failed composition uses a private candidate session; the fixture checks the prior effective view's base ownership, not a claim that a partially rejected output is reusable.
3. Navigation input revalidation (`NavigationDependencies.cpp:56–58`) is present and expected to reject the race. The callback is an existing deterministic owner-thread boundary, not a sleep/race simulator and not a cancellation injection. The source edit itself is excluded from last-good byte equality because it is the legitimate external change; scenes, navigation assets/meta, project and all other files are included.

## Retained corrections and remaining scope

- Prefab resolution's first-occurrence rank table replaces an expensive comparator scan while preserving stable authored order and new-child expansion order. Existing new assertions already exercise those exact order semantics. The linked policy and navigation probes add consumers of that resolution result; they do not manufacture a performance before/after claim.
- Streaming retention pressure clears/retries its suspension error each boundary and atomically replaces a snapshot only after the new bounded capture succeeds. Existing real JS payload shrink/revisit test is already meaningful; it is not repeated ceremonially here. The new motor probe examines a different publication boundary.
- Navigation rebuild already excludes read-only kit rows and refuses inherited prefab surfaces without owned override. Mixed owned/read-only kit byte preservation and all future dependency formats remain unexercised by these three probes.
- Current `ProjectExporter` validates transitive asset closure, all included scenes, stale navigation and region references before private staging/promotion. Existing export tests and final moved/source-hidden packages already exercise actual launch and last-good package replacement. The navigation probe's `ValidateSceneContent` assertions only cover the normal export **validation gate**; they are not another standalone launch claim.
- The current mod v1 rejects world/liquid/deformable/environment/audio-effect/catalog payload types explicitly. These are documented limits, not omissions to complete in this audit.
- Save/new-contact/joint continuation semantics have their separate Linux/native positive/negative and actual-endpoint evidence. No warm-history, solver, save format or controller changes are proposed by these probes.

## Files / scheduled execution

- `StreamingMotorPublication.cpp`
- `ResolvedPrefabModPolicy.cpp`
- `NavigationInputRace.cpp`
- `FixtureSupport.h`
- `run_prepared.py`
- `PREPARATION_IDENTITY.json` (read-only input/probe hashes)

No CMake additions are required. The runner reuses the actual target's compile/link flags, compiles only the standalone source, replaces only its test object and requested current engine archive in the link command, and writes unique owned outputs. It strips the generated link dependency-file option to avoid modifying the original build.

Root must schedule the runs sequentially with native Windows work. Example Linux recipe (not executed during preparation):

```sh
python3 docs/evidence/m78-closure/audit-lifecycle/run_prepared.py \
  --execute \
  --engine-archive .cache/m78-cold-boundary/final-after-static-build/libjudas_engine.a \
  --output docs/evidence/m78-closure/audit-lifecycle/runs/linux-current-01
```

Omit `--execute` to inspect recipes only. `--only streaming`, `--only mod-policy` or `--only navigation` selects a single bounded case. Existing output directories are refused rather than deleted. The runner retains exact commands, source/archive hashes, compile/link/run receipts, stdout/stderr and each fixture's `RESULT.json` plus its ordinary project/package inputs. Any failure is preserved; do not adjust expected values merely to pass.

Native recipe: compile each same source with the original corresponding MSVC target's definitions/includes, link against the freshly verified final archive and existing unchanged support libraries (navigation additionally needs `judas_world_authoring` and `judas_navigation_baker`). Run a unique fresh output directory. There are no Linux-only APIs in these fixtures and no graphical window is required. Record actual native source/archive/executable hashes and results independently; do not infer native correctness from Linux compilation.
