# Pass 4 curved mesh contact continuation research

## Disposition

**The production CCD finding remains open.** None of these policy experiments is
part of the production engine. The final prototype closes the original crossing
fixture and preserves three measured articulated trajectories, but needs durable
contact-history semantics and substantially broader lifecycle proof. Its matched
articulated cost is about 40% higher. Shipping its incomplete ownership state
would replace one correctness problem with unverified persistence and reentry
behaviour.

The accepted baseline is `7da56c2cc6ce7acda827a6e4019d610622b237e2`.
`BASELINE_FROZEN.json` identifies the clean Release executable and engine archive.
The unchanged original fixture is preserved in `baseline-source/ImpactMeshCostTests.cpp`.
Its timestep, launch velocities, 24 boxes, 90-step continuation, actual tessellated
sheet, 5 cm crossing gate and rear-entry/finite-edge controls were retained.

## Causal mechanisms

The original initial-arrival test already passes. Continued motion loses contact
because a whole concave mesh is treated as one supported primitive while its
front-side predicate discards a triangle as soon as the body's vertex-mean
passes behind the triangle plane. A finite body can still straddle that plane.
The original 16-triangle reduction reproduces this same loss on the baseline.

Three mechanisms were investigated together:

1. **Finite front-contact ownership.** Acquire actual front witnesses, retain the
   corresponding native triangle indices, and use one immutable policy for
   accepted-contact Boolean truth, full response and penetration searches.
   Reversed island queries normalize the pair identity before looking up ownership.
2. **Finite adjacency.** An owned triangle may convey ownership to a neighbor only
   while both finite triangle/body SAT tests overlap and their actual shared edge
   intersects the convex body. Coincident but unwelded geometry is not connected.
   Neither an infinite plane nor a whole connected component is an ownership grant.
3. **Per-feature penetration budgets.** A global deepest-penetration maximum can
   hide an individual triangle's disappearance. Already touching features use
   their own initial depth plus the existing quarter-feature growth allowance.
   A new feature uses its reference front-plane depth with a cap before center-side
   loss. Actual finite accepted geometry is still required before an impulse.

For a new feature with reference plane penetration `d`, projected center-to-support
radius `h`, and existing allowance `a`, the experimental threshold is
`min(max(0,d)+a, h/2)`. An already accepted feature uses its actual initial
penetration plus `a`. This is an experimental response policy, not a continuum
contact theorem or permission to change the original acceptance fixture.

### Preserving existing safe trajectories

The final prototype proves a narrower condition before adding continuation work:
a centered box's represented center stays strictly in front of **every** triangle
whose finite geometry can overlap the remaining swept body. It uses the existing
outward interval model for anchored binary32 center drift, including rounding,
and the existing swept-body enclosure. If certified, the original support/spin
policy remains unchanged. Convex hulls, compound bodies and uncertain intervals
fail closed to the experimental continuation path.

The final derived optimization omits the adjacency traversal when every current
candidate center is already in front. Ownership cannot admit an additional
behind-side feature in that case. A 15,288-state represented-pose comparison
produces identical episode indices, response fields and Boolean truth before and
after this optimization: digest `c2616c38ce580694`.

## Experiments and unchanged controls

| Experiment | Coarse / medium / dense crossings | Original full result | Decision |
|---|---:|---:|---|
| Accepted baseline | 11 / 20 / 15 | 4 failed assertions | Original failure |
| Canonical episode ownership without finite propagation | 4 / 4 / 0 | 3 failed assertions | Reject |
| Finite shared-edge propagation with feature arrival events | 0 / 0 / 0 | 193 pass | Too expensive and incomplete |
| Same, retaining the original angular growth guard | 0 / 0 / 0 | 193 pass | Too expensive and incomplete |
| Global mesh growth budget with ownership | 2 / 10 / 11 | 4 failed assertions | Reject |
| Per-feature growth budget with ownership | 0 / 0 / 0 | 193 pass | Expensive and incomplete |
| Certified front-safe admission plus per-feature budget | 0 / 0 / 0 | 193 pass | Research only |
| Same with derived front-only traversal shortcut | 0 / 0 / 0 | 193 pass | Research only |
| Identical final budgets with ownership disabled | 3 / 0 / 0 | 2 failed assertions | History is causally necessary |

Every listed ordinary entry check separately passes the original 193 assertions.
The history ablation changes no budget, timestep, fixture or threshold. Removing
ownership brings back coarse and rotated continuation failures. Therefore the
passing result does **not** justify omitting continuation state from persistence.

The first development attempt also exposed a reversed-query lookup error. Its
four coarse geometry exceptions disappeared after canonicalizing the pair lookup.
The failed outputs remain under `episode-v1/uncanonical-failed`; that early source
was not separately snapshotted, so those outputs are not falsely paired with the
later corrected source. The later stage sources are preserved separately.

### Independent focused controls

- Eleven geometry assertions cover acquisition, behind-center continuation,
  front normal retention, actual Boolean/full-response agreement, reversed
  arguments, full separation, finite edges, initial rear entry, actual shared-edge
  transfer, and the coincident-unwelded negative control.
- Ten physical-world assertions cover actual front contact followed by a
  50 rad/s finite departure, returning below the sheet and passing upward from
  behind without a contact event, explicit reset, and destroyed-slot reuse.
  This proves release **across completed fixed boundaries**. It does not prove
  every within-step high-spin exit/reentry sequence.
- The 104- and 16-triangle reduced worlds both avoid the original three-step
  crossing with the final prototype. Their first two endpoints match exactly;
  their third endpoints differ by about 0.24 mm. The old reduction was
  trajectory-equivalent for the old motion; the changed trajectory/proximity
  selection must not be presented as a new three-endpoint identity proof.

## Matched articulated performance

The strict pair ran sequentially while the Windows VM was suspended and other
owned builds/benchmarks were held. Same fixture, 60 fixed steps, identical engine
settings and body/mesh inputs. Raw logs and commands are in
`strict-v6-front-fast/`.

| Mesh | Baseline total / worst ms | Prototype total / worst ms | Identical actual digest |
|---|---:|---:|---|
| 104 triangles | 222.80 / 11.97 | 313.13 / 15.66 | `3fdde56f1ea9312b` |
| 1800 triangles | 297.91 / 20.61 | 421.09 / 27.88 | `ebd86a04dabbad71` |
| 7200 triangles | 620.85 / 29.82 | 864.40 / 40.90 | `eece82a1f1f3aee5` |

All three actual articulated trajectory digests, penetration measurements, event
counts, query counts and temporal sampling counts match the baseline. Additional
ownership/admission geometry is derived work. Totals rise 39.2–41.3%; worst steps
rise 30.8–37.2%. These are not speedups. This fixture exposes total/worst cost;
no per-step median or p95 is invented from those values. The separate dense-pile
workstream records its own median/p95/worst measurements.

Earlier first-touch-per-triangle variants produced 404–543 events in 60 steps and
up to 206 ms worst steps. Those costs and failed alternatives are preserved rather
than hidden behind the final successful crossing count.

## Production readiness gaps

Before considering a production implementation:

- Define and prove finite exit/reentry ownership **within** an anchored interval,
  particularly substantial rotation and temporary separation. Do not assume
  boundary-only expiration is sufficient.
- Preserve the selected side of two-sided sheets across an owned episode; the
  prototype follows current center-side selection there and has no complete proof.
- Centralize invalidation for filters, body authority, geometry replacement,
  destruction and slot reuse. Current generations/asset pointers/reset revisions
  cover only part of this requirement; filter reenable is not established.
- Bound and prune history without silently discarding physically necessary state.
- Persist causal history with durable owner identity, actual topology/context
  validation, atomic failure handling, explicit old-save limits and cold-process
  endpoint comparison. None of that plumbing was added speculatively.
- Establish regional suspension and excluded/unowned/planet collider policy.
- Prove behind-center resting/sliding friction and support through the ordinary
  initial-contact path. The prototype routes sequential current-pose geometry
  through the policy; the parallel preparation path is explicitly restricted to
  asset-free boxes/spheres and never reads or mutates episode state.
- Review publication after unsuccessful solver boundaries and resource replacement.
- Remove or justify the measured general cost before making it a default path.

These are explicit uncovered contracts, not claims of detected production defects
in systems that have not adopted this prototype.

## ABI and reproduction

No existing `Contact`, `PrimitivePose`, `Shape`, `ContactPreparedOrientation`,
`GeometryDistance`, `CollisionGeometryStats` or public `PhysicsWorld` layout changed.
`review/ABI_HEADERS.json` verifies the relevant baseline headers remain byte
identical. Only new internal structs/functions and an extended
`MeshSeparationAdvance` signature appear in experimental headers. Native symbol
receipts show the old archive's only caller was the fully replaced
`PhysicsWorld.cpp.o`; both affected implementation objects were recompiled.
There is no mixed old/new struct layout proof masquerading as a runtime pass.

The experiment builds link the two replacement objects with the exact frozen
accepted engine archive and the original test objects. `probes/` contains the
actual cache-oriented build scripts and observation sources. They refer to
ignored local build/archive paths; adapt these to a fresh owned checkout and
output directory rather than overwriting historical evidence. Binary/archive
fingerprints are provided; executables, objects and libraries are intentionally
excluded from this evidence tree.

No fluid implementation, independent research, game content, public JudasJS API,
production CCD policy, solver iteration budget or acceptance threshold changed in
this workstream. Protected evidence remains unchanged. This is neither human
acceptance nor Alpha readiness.
