# Pass 5: finite-feature curved-mesh CCD investigation

## Status and scope

**Research only. No production CCD policy, public geometry layout, save format or contact history was changed by this investigation. The original curved-mesh crossing defect remains open.** The final bounded V6 prototype is rejected: ordinary curved cases still become uncertain and the helper retains failed positive controls. No further representation variants were attempted after that result.

Baseline is accepted `4c141e9bb6c68a50c63dec263dcec398200328ad`. [BASELINE_FROZEN.json](BASELINE_FROZEN.json) records the clean Release archive/binary fingerprints. [BASELINE_ORIGINAL_FULL.log](BASELINE_ORIGINAL_FULL.log) is the actual unchanged six-scenario, 60-step run: **372 checks, 4 failures**, with 11/20/15 fast boxes crossing the three curved meshes. The rotated curved case also fails. Flat sheets and initial-arrival checks continue to pass.

The prototype command `mesh 1 trough` selects only the three curved scenarios: **193 checks**, including the original 90-step fast-box tests. It is not the unfiltered 372-check suite. Neither the fixtures, timestep, crossing oracle nor thresholds were weakened. Research exceptions truncate physical trajectories, so `fastBoxesThrough 0` after an exception is **not** a no-crossing proof.

## Established mechanism

The native one-sided cooked-poly path selects a triangle's side from the mean represented body vertices. A finite front contact can remain geometrically intersecting after that mean goes behind the triangle. Stateless side rejection then removes a real continuation contact. An infinite plane or an always-front rule would conceal this failure while producing incorrect rear-entry or finite-edge behaviour.

The causal state needed is a finite, sided contact episode. It must expire after a real finite separation and can hand off only through an actual shared finite edge inside the body. A connected mesh does not imply the body's contact patch is connected. Likewise, a persistent contact elsewhere on the same mesh cannot authorize another patch after that patch separates.

Production paths examined: `src/CollisionGeometry.cpp`, `src/physics/WorldCCDSearch.cpp`, `WorldImpactQueries.h`, `WorldImpactEvents.cpp`, `WorldInternal.h`, `WorldPersistence.cpp`, `WorldBodies.cpp`, `WorldContacts.cpp`, `WorldHotAccessors.h` and `src/PhysicsWorld.cpp`. The cache copies preserve the relevant code and full experimental implementations.

### First exact uncertainty: genuine finite departure

The original coarse-trough box 5 acquires feature 44 at `t=0.012716979254868147`. Its next ordinary query is `t=0.012721030217207668`. The exact native SAT trace finds actual whole-pair finite separation by approximately `t=0.01271904`. This was **not** merely a triangle handoff, float-pose flicker or the fixed witness moving out of the intersection. Across 4,097 sampled poses, all 1,980 native finite misses also failed the point certificate. Feature IDs, neighbours, poses and native/owned/geometric sets are retained in `research/native-sat-v2/` and `research/union-v3/`.

The first chronological prototype subdivided toward that real departure indefinitely because it failed to check actual endpoint absence first. Endpoint finite-expiry is a justified generic correction to the research algorithm, not evidence that the production defect is solved.

### Second exact uncertainty: irrelevant history

In the reduced original fixture at `t=0.0045425842682004589`, the only actual endpoint face is 16 and its native side is **+0.0114695321 m**. Historical front selection cannot change that ordinary front response. Requiring continuity certification there was unnecessary. A per-endpoint native-side equivalence gate removes that unnecessary work; it does not erase the causal anchor needed by later genuinely behind-centre queries.

The review also found that an earlier prototype had changed native response in the tiny negative side band `[-1e-10,0)`. V5 restores the original null/unowned reversal exactly. Ordinary two-sided meshes remain on their existing production policy; the new experimental ownership policy is explicitly restricted to one-sided static meshes against boxes/hulls.

### Third exact uncertainty: genuinely influential ownership

For original coarse-trough box 5, step 6, query `t=0.013023191524169427`:

- Actual physical anchor: `t=0.010362875706079655`.
- Owned seed faces: 76 and 81; these are actual welded neighbours.
- Only geometric endpoint face: 81.
- Native side: **−0.00035343429942181342 m**.
- Native manifold: 0 contacts. Existing selected-front manifold: 4 contacts.

Thus the remaining case cannot be dismissed as an unrelated face or an irrelevant-history request. Its exact anchored motion, represented poses, feature IDs and side values are retained in `research/union-v5/first-influence/`.

A standalone diagnostic establishes a later independent **ordinary native-front finite witness** at `t=0.012135176570080453`, with side `+1.5098949469188883e-9 m`. The suffix then certifies finite continuation to the behind-centre endpoint. Six diagnostic controls pass. The original anchor remains uncertified at the unchanged 1,024-interval budget; the fresh suffix uses 509 visited nodes/255 positive certificates. This proves a possible causal re-acquisition for this exact state; it does **not** establish a practical whole-world policy or justify larger budgets. See `research/native-acquisition-v1/`.

## Preserved experiment disposition

| Stage | Result | Disposition |
|---|---|---|
| Continuity v1 | Fixture failed to compile due to wrong `Shape::Cooked` arguments | Harness failure, preserved |
| Continuity v2 | 7/7 small controls | Research helper only |
| Continuity v3 | 11/11 controls, including hidden finite exit/re-entry, compound offset and rotated charts | Uses an inverse-rotation witness approximation; superseded by native SAT certificate for production investigation |
| Union v1 | Private-world publisher inserted into the wrong context; compile failure | Wiring failure, preserved |
| Union v2 | Coherent compilation; canonical pair lookup throws `map::at` | Wiring failure, preserved |
| Union v3 | 6/6 small union controls; reduced 16-triangle and full-104 three-step cases no longer cross; original curved193 and entry193 fail via bounded uncertainty | Not usable; no physical crossing claim after throws |
| Union v4 | Native SAT certificate + actual endpoint expiry; curved193 still fails, entry regresses, reduced case becomes uncertain | Rejected; curved diagnostic 42.55 s, excessive common uncertainty |
| Union v5 | Native relevance gate + restored null semantics; entry193 and reduced16/full104 pass; curved193 still has 3 failed checks through uncertainty | Research only; curved diagnostic 5.43 s, still unusable |
| Native re-acquisition diagnostic | 6/6 exact captured-state controls; suffix certified without increasing budget | Local mechanism demonstrated, cost and world-policy proof outstanding |
| Union v6 | Curved193 still 3 failed checks through uncertainty; entry193 and reduced16/full104 pass; target-directed helper 8 checks / 3 failed positive controls | Rejected; curved diagnostic 7.681 s, no production adoption |

Times above are diagnostic command durations, not controlled before/after performance claims. V5's reduced16 and full104 trajectories agree for the first two steps and differ by roughly 0.29 mm on step three; both remain on the correct side. The two representations therefore do not have identical physical trajectories.

### Final bounded V6 outcome

The final candidate independently attempts ordinary front re-acquisition for each influential endpoint face, without assuming monotonic high-spin side values. It then traces ancestry **backward** through the target's actual welded finite component: a certificate for one member across an interval seeds only that member's component at the earlier boundary. This prevents a persistent disconnected patch from automatically authorizing the target after its own gap. Native response/Boolean queries and the original 1,024-interval and depth-40 bounds remain unchanged. The re-acquisition search has 32 bounded witness-search iterations; it only finds actual native finite witnesses, and never supplies a continuity proof by itself.

This was the final representation trial. It compiles and links coherently, but the original curved193 still has **3 failed checks**: four ordinary coarse-box runs, two rotated runs and one middle-resolution run terminate with bounded uncertainty. Dense curved-box runs do not throw. The initial-entry193 and reduced16/full104 three-step fixtures pass. Zero counted crossings in truncated cases are not physical acceptance.

The eight additional helper controls retain **three failures**: actual welded-edge handoff is uncertified (53 visited nodes, no positive certificate), and the behind-centre high-spin separation returns `Uncertain` rather than proving expiry. Disconnected topology and absence of invented unrelated ancestry are retained, but those negative results do not repair the failed positives. The proposed old-forward comparison also returns `Uncertain`; it **does not** dynamically prove the suspected false-positive bridging mechanism. The path-dependency problem is a source/design finding, not a reproduced old-helper `Connected` result.

V6's curved-only command takes 7.681 s in this diagnostic window, versus 5.425 s for V5 under different timing conditions. Neither is a matched performance gate; the high certificate counts and remaining ordinary exceptions are independently sufficient to reject production adoption. Its raw source, compilation, original fixtures and exact failed helper output are retained in `research/union-v6/`. There were no additional budget increases, global floating-point changes, state-serialization additions or post-V6 solver variants.

`RoundedInterval.h` and `RoundedSATContinuity.h` are **unexecuted research proposals**, explicitly not adopted: supported-compiler operation/contraction guarantees were not established. The `union-v6-pre-component-review` source is also an unexecuted proposal superseded before compilation after a disconnected-contact ownership hazard was identified. Neither counts as a successful or failed physical experiment.

## ABI, reproduction and retained failures

`PrimitivePose` and existing public contact-preparation layouts are unchanged. The private world layout differs in each prototype, so every private-world implementation unit is compiled together into `UnifiedWorld.cpp`; all old private world objects are removed from the frozen archive before linking. This avoids mixing incompatible `PhysicsWorld::Impl` definitions. The archive contains **two** members named `WorldPersistence.cpp.o`; only the second (physics-private) is replaced, preserving the first public world-save orchestration member. The failed initial archive assertion and failed duplicate-main diagnostic link are preserved.

Each stage retains full source snapshots, compiler/link arguments, working directory, exit code, source hashes and raw logs. `research/build_union.py` documents the coherent build and original reproduction commands. Run against the baseline checkout/dependencies and the frozen archive or rebuild the exact baseline. The research does not require or modify Solbert's independent fluid directory.

Full original six-scenario reproduction:

```sh
build/judas_impact_mesh_cost_tests 60 '' 1.2
```

Curved-only research and initial-entry control:

```sh
.cache/prealpha-pass5/ccd/union-v5/mesh 1 trough
.cache/prealpha-pass5/ccd/union-v5/mesh 1 trough 1.2 entry
```

The preserved reduced-16 fixture comes from `docs/evidence/prealpha-pass4/ccd`; its original source SHA is recorded in [PROVENANCE.json](PROVENANCE.json). Prior evidence was not edited. Objects, executables and large archives remain in ignored cache; only source/log/receipt material is included here.

## Production-readiness gaps

No stateful policy should be adopted solely because its original crossing tests become green. Remaining requirements include:

1. Target-directed finite ancestry: persistent patch A cannot authorize patch B after its independent gap; no arbitrary same-mesh graph inheritance.
2. Bounded, practical ordinary-case continuation cost and an explicit safe uncertainty path. Throwing midway through the current nontransactional `Step` can leave an advanced world and is **research instrumentation only**.
3. Sleeping physical episodes must not be erased merely because `CanRespond` suppresses scheduling. The research `Finish` retirement error was identified before production adoption.
4. Geometry/filter/reset/destroy/part identity and resource-topology changes must invalidate stale ownership appropriately, while continuous velocity changes preserve causal side and invalidate only derived trajectory certificates.
5. Ownership publication must reflect the actual completed physical endpoint, including position repair; initial worker/Boolean/full-response paths must share immutable policy.
6. Proven-causal state must survive fresh-process saves/streaming and origin shifts; missing historical state cannot be invented. Existing primitive-floor cold-save passes do not exercise this new mesh history.
7. Existing same-pose query memoization needs policy identity if the same represented pose can occur with different contact ancestry.

No persistence extension, universal Step rollback, global precision rewrite, simulation-quality reduction or new public capability was introduced. CCD remains an Alpha blocker independently of the production dense scheduling work.
