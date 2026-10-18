# Pass 3: one-sided curved-mesh continuation

## Status

**Still open.** No experimental CCD policy was applied to production. Two new,
mechanistically motivated experiments were rejected because neither closes the
unchanged contract and both add work. These are genuine failures, not a retuned
acceptance fixture. The separate bounded source-format cleanup does not claim a
collision correction.

## Baseline and unchanged oracle

The frozen source is `f647d7fd95f60d3a37dba70a51f4e45dc41c2922`.
`BASELINE_FROZEN.json` identifies the exact clean Release executable/archive,
compiler, configuration and source fingerprint. `BASELINE_RESULTS.json` records
commands, exit codes and observer provenance. The original test is
`tests/ImpactMeshCostTests.cpp` without edits: 24 thin boxes per resolution,
`1.f/60.f`, initial velocity `(0,-40,grazing)`, gravity `(0,-9.81,0)`, original
cooked trough and a strict 5 cm actual-sheet crossing gate over 90 fixed steps.
The ragdoll prefix count is independently set to 1 to keep reproduction small;
that does **not** shorten the 90-step fast-box contract.

- `judas_impact_mesh_cost_tests 1 trough`: 193 checks, **4 failed assertions**;
  crossings 11 / 20 / 15 for coarse / medium / dense; no exceptions.
- `judas_impact_mesh_cost_tests 1 trough 1.2 entry`: **193 checks pass**; zero
  two-step arrival crossings. Entry remains a separate, narrower contract.
- The original test includes meaningful negative controls: removing the collider
  must expose all 24 crossings; legal upper flight is not counted; finite-edge
  misses and one-sided rear entry must remain unblocked; rotated coarse geometry
  must satisfy the same physical contract. These controls remain unchanged.

## Established first physical boundary

`probes/BaselineObserver.cpp` observes actual authoritative motion segments, velocities,
poses and endpoint narrowphase. It does not alter solver state. The expanded
`probes/ContinuationObserver.cpp` additionally evaluates the last resolved segment with
`PhysicsWorld::EvaluateBodyMotionSegment` and the public native zero-margin
cooked manifold, against the independent tessellated-sheet oracle.

For coarse box index 0:

1. Step 0 is ordinary separated flight.
2. Step 1 has three real TOIs. Its final segment starts at
   `t=0.008051287353743386`, position approximately
   `(-1.1998311281, 0.3203688264, -0.9477891326)`, and continues to the normal
   fixed boundary at `0.01666666753590107`.
3. The post-impact linear velocity is approximately
   `(0.032964509, -9.862264633, 12.690654755)`; angular speed is only about
   0.24 rad/s. This is a translated glancing contact, not a missing initial TOI.
4. Early in that final segment the sheet has four actual contacts and about
   5 mm penetration. As the body travels into the next curved patch, actual
   penetration rises to 10.98 mm while its center is still 0.343 mm in front.
5. Between sampled times 0.01451282249 and 0.01487179666, its center crosses the
   triangle-front boundary. The manifold changes from four contacts to zero
   although the finite body is still straddling the sheet. The observed center
   depth becomes 0.167 mm, then 2.719357 mm at the fixed boundary.
6. Step 2 has no impact event and ends 55.951319 mm behind the actual sheet,
   violating the unchanged 50 mm gate. Some other boxes subsequently have no CCD
   query at all because the thin-sheet/body broadphase no longer overlaps.

Relevant implementation mechanisms:

- `PhysicsWorld::Impl::AdvanceImpacts` treats a body/primitive pair in
  `supportPairs` as support for the entire mesh, unless `SpinsThroughSupport`
  predicts enough **angular** travel. That is not a certificate for translating
  through a union of curved finite patches.
- `CollisionGeometry.cpp::visitCookedPolyContacts` rejects a one-sided triangle
  when the body vertex-mean is behind its front plane. Contact truth therefore
  changes discontinuously at the center-side boundary, before full body/sheet
  separation. `MeshSeparationAdvance` uses the same center-side rejection.
- An existing penetration-target query measures a global maximum depth relative
  to its value at query start. That does not create persistent physical feature
  ownership when individual one-sided contact features disappear.

This establishes an actual support-continuation / sidedness ownership problem.
It does not prove a unique replacement algorithm or justify accepting arbitrary
backfaces, changing mesh fidelity or adding impulses across a gap.

For a triangle front plane `(n, d)` and body center `c`, current culling tests
`s = n·c - d`. If the body's projected support radius is `h_n > 0`, then
`-h_n < s < 0` still straddles that plane: its interval is
`[s-h_n, s+h_n]`. Where finite-footprint SAT also overlaps, as witnessed
here, a center-side predicate discards that physical contact window as soon as
`s < -eps`. The same instantaneous pose can arise after a
front-arrived contact or during deliberate rear entry; pose alone cannot encode
that causal distinction. A repair therefore needs a coherent episode or a
proven guard preventing the missed front boundary, not a larger backface epsilon.

## Minimal unchanged-input reduction

`probes/MinimalContinuationRepro.cpp` reduces the collision surface to 16 actual
triangles taken from the original coarse source rows, covering the whole
three-step observed path. It does not alter the body, launch, gravity, cadence
or 5 cm gate. Both the original 104-face world and the 16-face reduction produce
**bit-identical positions and orientations at all three fixed endpoints**, with
three actual impacts on step 1 and the same 55.951319 mm crossing on step 2.
The reduced diagnostic returns **failure (exit 1)**, preserving the defect rather
than manufacturing a new green test. `minimal/RESULTS.json`, `run.log` and build
receipt identify the exact source, frozen archive and compiled probe.

## Rejected experiments

All source copies, native compilation/link commands, archive/binary hashes and
raw outputs are retained in the two experiment directories.

| Experiment | Coarse / medium / dense crossings | Failed assertions | Disposition |
|---|---:|---:|---|
| Frozen production | 11 / 20 / 15 | 4 | Original failure retained |
| `concave-target-v1` | 4 / 20 / 14 | 4 | Reject |
| `finite-current-v2` | 5 / 5 / 0 | 3 | Reject |

Both experiments retain the 193-check entry pass and produce no impact-bracket
exceptions. Neither is a passing full continuation solution.

### Existing target for every concave support

The first cache-only variant routes mesh supports through the **existing**
relative-depth target query irrespective of angular speed. This confirms that
translation omitted by the support policy contributes to the defect, but does
not solve all feature disappearance. The medium box index 0 still ends step 2
2.983887 mm behind the sheet after actual TOIs. Its next contact is lost.
No iteration counts, sampling cap, timestep, material, shape or threshold changed.

### Current-sided finite-feature query

The second cache-only variant excludes already touching features when searching
for a newly encountered mesh patch; its selective Boolean predicate is a strict
subset of the ordinary, same-time, current-sided full manifold. It deliberately
**does not** use Pass 2's rejected fixed `frontReference`, which could admit a
Boolean witness excluded by the event-response query and lead to `impact bracket
lost contact geometry`. This variant avoids those exceptions and materially
changes crossings, but still fails coarse/medium/rotated continuation.

It also dramatically increases actual event work. In this diagnostic 60-step
ragdoll prefix, coarse / medium / dense totals are about 888 / 1568 / 3461 ms,
with worst steps 52.55 / 99.78 / 227.57 ms and 395 / 543 / 536 impact events.
The first target-only variant costs about 314 / 478 / 700 ms. These are changed
physical trajectories, **not arithmetic-equivalent performance optimizations**.
Raw measurements are diagnostic; historical Pass 2 timings are not relabelled as
fresh matched baselines. More contact events and lower measured penetration do
not justify shipping an expensive partial correction.

## Recommended next correction boundary — not implemented

A coherent next design is a generation-safe, finite-feature front-contact
**episode** shared by physical contact truth, event response and publication. An
alternative front-boundary guard must prove the same invariants independently.
This is not permission to treat a mesh as one infinite plane or accept all rear
entries.

1. **Acquire causally:** record only an actual front-side witness / front-arrived
   TOI, keyed by body generations, primitive identity, mesh resource identity and
   triangle/connected patch. Public stateless rays and overlap queries retain
   their existing sidedness contract.
2. **Use one immutable query policy per boundary:** zero-margin Boolean truth,
   penetration truth and full response must use identical episode-side rules at
   the identical represented poses. Queries during search/bisection must not
   mutate the episode. A selected witness must imply a nonempty full response.
3. **Continue physically:** a center passing behind a previously contacted front
   is not by itself a new rear entry. Finite SAT separation and finite triangle
   footprint remain authoritative; history must never create geometry or impulse
   across a positive gap. Newly contacted neighbors require actual finite
   topology / traversal, not a blanket body-to-mesh front override.
4. **Bound drift before loss:** a continuously straddling support must remain in
   the CCD ownership set even after angular speed becomes small. Merely retaining
   a cache after the whole body has crossed cannot repair the earlier miss.
   Prove rejection/acceptance intervals before the body leaves the physical
   contact window, using the existing quality/budget policy.
5. **Publish deliberately:** commit new feature ownership only after successful
   actual solver/event boundaries; handle unsuccessful solves and teardown
   without partially published state. Broadphase discovery must keep a real
   straddling episode eligible without making unrelated pairs global candidates.
6. **Invalidate correctly:** body destruction/reuse, shape or mesh replacement,
   explicit reset/teleport, filtering changes and disabled bodies must not alias
   old feature history. Sleeping/wake-up must preserve valid physical ownership.
7. **Preserve cold continuation:** any cross-step history required for physical
   correctness is real save state, not a disposable optimization cache. Define
   bounded resource/feature identity, schema/version and explicit old-save
   limitations rather than inventing missing data. Store mesh-local data so
   origin shifts do not degrade reference precision.
8. **Coalesce only with proof:** coplanar patch grouping may bound work, but must
   preserve finite holes, disconnected components, front/back behavior, creases
   and actual accepted witnesses. Prior feature enumeration's cost is evidence
   against blindly serializing/storing every triangle contact.

Required discriminating validation: unchanged 90-step / rotated fixture;
independent initial-rear-entry and finite-edge misses; contact acquisition then
translation across a curved seam; exiting/re-entering a finite patch; high-spin
and low-spin continuation; two-sided winding; deterministic repeats; cold saves
on either side of the boundary; filtered/destroyed/reused/replaced bodies;
origin shifts; and matched articulated/dense performance. A type inventory or
passing two-step arrival test cannot close this contract.

## Bounded readability work

`formatting/PhysicsWorld.before.cpp`, `formatting/PhysicsWorld.after.cpp` and
`formatting/format-only.patch` isolate the mechanical formatting of
`ImpactTime` and `AdvanceImpacts`. They include, but do not change, the parent's
origin guard and the dense agent's independent exact-membership change.
`formatting/TOKEN_PARITY.json` verifies all 53,686 C++ tokens of the entire file are
unchanged; negative controls detect altered CCD iteration comparison and response
failure guard. The two functions change from 529 to 785 lines; 24 lines over
120 columns (maximum 202) become zero (maximum 100). No experimental algorithm
is part of that output.

### Final combined native preservation check

After the parent's final combined build/provenance refresh, the frozen and final
`judas_impact_mesh_cost_tests 60 trough` ran sequentially in one exclusive CPU
window with the Windows VM shut off and no other owned compile/benchmark running.
All 20 non-timing fields per scenario match, including the three actual
60-step articulated-contact trajectory digests:

| Scene | Identical digest | Frozen / final total ms | Frozen / final worst ms |
|---|---|---:|---:|
| Coarse 104 triangles | `3fdde56f1ea9312b` | 219.27 / 220.58 | 12.77 / 12.11 |
| Medium 1800 triangles | `ebd86a04dabbad71` | 297.98 / 302.50 | 21.69 / 20.60 |
| Dense 7200 triangles | `eece82a1f1f3aee5` | 609.09 / 605.82 | 29.98 / 29.78 |

Both complete processes finish at `checks 193 failures 4`, preserving the exact
four failed assertions and 11 / 20 / 15 fast-box crossings. This is a **passing
behavior-preservation comparison**, not a passing CCD-continuation contract or
speedup claim. It observes actual articulated contacts, not just a free-flight
prefix. `final/RESULTS.json` identifies all source/binary fingerprints and exact
compared fields; the raw paired logs, native build identity and final engine
archive identity are retained alongside it. The final binary's Git revision is
its dirty parent f647, and its source fingerprint identifies the uncommitted
combined candidate; it is not falsely labelled a clean post-commit build.

## Boundaries

No fluid or independent research files were accessed or modified by this
workstream. No new gameplay policy, public binding, timestep reduction, solver
budget increase, collision simplification or main promotion was made. The open
CCD correctness finding remains an Alpha blocker. No human validation is inferred.
