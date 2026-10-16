# M91 — Predictable Collision at Scale

M91 develops on Experimental. Human acceptance and main promotion are separate.
See the [measured evidence, failures and limits](evidence/m91/REPORT.md).

[Architecture](ARCHITECTURE.md) · [Mechanical Arcade lab](../projects/physics_stress_lab/README.md) ·
[M87 investigation](evidence/m87/live-slowdown-followup/REPORT.md)

## What changes

The existing chronological CCD event loop remains authoritative. New separated
pairs still use the existing first-touch search and actual-contact impact solver.
The fixed cadence, materials, gravity integration, contact manifolds, ten velocity
iterations, restitution/energy policy, sleeping rules and sampling budgets remain.
No distance-based freezing, reduced contact fidelity or scene-specific path exists.

M91 changes **persistent-contact continuation**, not the definition of geometric
contact. Numerical trajectories can change; this is not a bit-identical simulation
optimization. In particular, retaining an already-established support is allowed
only by the bounded geometric rule below. A strictly zero-overlap simulator is not
claimed: Judas already has a 5 mm position-solver allowance and approximate CCD
fallbacks. Their limitations must not be confused with exact predicate truth.

### Eligibility and compatibility

The new continuation policy applies only to **free, inelastic native-primitive
contact events**. Hard-jointed islands, external physics couplers, elastic flights
and events containing cooked/compound/displaced geometry retain the original
contact-cache invalidation and CCD event policy. Geometry bounds alone do not
certify a mechanism’s reaction history. An unrestricted prototype regressed the
unchanged M77 corrective-step controller; isolated controls showed both new
identity retention and support certification could alter that coupled response.
The restricted implementation restores the baseline output without changing the
controller, fixtures, iterations or acceptance thresholds. Exact arithmetic and
derived-work optimizations remain applicable to those systems.

### Contact identity is distinct from an impulse guess

An impact invalidates a velocity-changing island's old warm-start impulses. It
need not erase the identity of its established geometric features. For eligible events the cache now
retains zero-load identities and records inelastic TOI features with **zero** warm
impulse. Elastic departures remain new flights, so a short airborne return still
receives its own restitution. A physical impact impulse is never replayed as
next-frame support load.

Next step still performs fresh ordinary narrowphase and the existing generation,
primitive, normal, proximity and one-to-one local-anchor checks. Unmatched awake
features disappear. Destruction, filtering/material changes and slot generations
retain their existing lifetime rules. Completed load/contact observations remain
separate and continue to contain actual solved rows, not certificates.

This is not new save state. The existing limitation around missing warm-contact
continuation for arbitrary unjointed piles remains; a cold save is not promised to
reproduce an uninterrupted pile bit for bit. Articulated M70/M77 contracts are
validated independently.

### A support survives a changed trajectory only with a proof

Previously, changing a body's velocity discarded other support-pair exemptions
unless those pairs happened to be touching at that event. Small positive gaps then
re-entered first-touch CCD repeatedly. M91 tries to certify the complete remaining
motion before discarding an already-established support.

For centred native boxes and spheres, choose a current geometric normal `n` and
outward interval bounds over the actual anchored trajectories. Certify

```text
lower(n · (centreA(t) - centreB(t))) - extentA(n) - extentB(n)
    + allowance * |n| > 0                 for EVERY remaining t

allowance = min(existing 0.005 m position slop, 0.25 * smallest half-feature)
```

This proves bounded overlap along a fixed plane, **not zero overlap**. Other
ordinary contact residuals can exceed 5 mm; this is not a global penetration cap. It cannot
introduce a contact, publish an impact, apply an impulse or exempt an unrelated
new pair. Existing penetration beyond the allowance immediately falls back.
Unsupported compound/cooked/terrain shapes, displaced primitives, prescribed
rotation/discontinuities, uncertain arithmetic and unsuccessful proofs retain the
original CCD invalidation/search path. Other objects and newly encountered pairs
always retain ordinary CCD.

The rotation enclosure follows M87's represented quaternion arithmetic, factored
into a shared helper. If an interval is too wide, at most three subdivisions
(eight terminal intervals) tighten it. Each accepted interval encloses **all**
represented times; this is not collision sampling. Exhaustion means ordinary CCD,
not permission to skip a collision.

### Keep common translation correlated

Two independent position ranges lose shared motion. Instead, for segment anchors
`pA,pB`, velocities `vA,vB`, begins `aA,aB` and query start `s`:

```text
relative(t) = [pA + vA*(s-aA) - pB - vB*(s-aB)]
              + (vA-vB)*(t-s) + [-roundingError, roundingError]
```

The error enclosure covers the double time subtraction, conversion to float,
float multiplication and float addition separately. Full outward ULP bounds cover
binade boundaries and subnormals. Static centres are exact represented inputs.
This accounts for staggered segment anchors and float quantization; assuming an
exact-real affine path alone is demonstrably insufficient. At coordinates too
coarse for the proof, it fails closed. No world-up direction is used.

### Derived work and arithmetic

- Stable cache publication runs only after TOI identities change its order,
  including cleanup on failure. Already ordered event-free steps avoid a sort
  and its temporary allocation.
- An unchanged trajectory's earlier conservative sweep bound may cover a later
  subinterval. Velocity/generation changes invalidate it; geometry remains the
  authority.
- Impact-island traversal visits a step-local joint adjacency list in original
  joint encounter order instead of scanning every unrelated joint per body. It
  allocates/builds only when an actual event needs it; free flight and fully quiet
  worlds avoid this new adjacency scratch allocation.
- Interval scalar multiplication/division avoids duplicated endpoint operations.
  The normal binary64 exponent test uses exact exponent bits instead of two
  library `ilogb` calls; subnormals retain the original calculation. Outward
  endpoints, expression order and exact fallback remain. Reference tests compare
  endpoint bits over ordinary and extreme IEEE inputs.

## Profiling

M56 exposes continuation attempts/acceptances, partition count, over-budget
fallback count, coupled/elastic/geometry fallback events, peak impact-island
bodies/contact rows, and inclusive proof/cache times. CCD geometry/search, island discovery,
impact response and observation-publication categories remain available.
These are overlapping inclusive categories; **do not sum them as disjoint cost**.
There is no per-contact profiler-event flood. Inspect dropped/truncated records.

`tests/PhysicsStressLiveBenchmark.cpp` optionally writes actual endpoint primitive
penetrations for the lab's authored `loose_piece` population. This observation is
outside fixed-step timing. It is not a general collision certificate for arbitrary
projects, and finite fields alone are not physical acceptance.

## Reproduction and demonstration

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target judas judas_editor judas_export \
  judas_collision_continuity_tests judas_contact_interval_tests \
  judas_physics_stress_live_benchmark -j4
build/judas_collision_continuity_tests
build/judas_contact_interval_tests --benchmark
build/judas projects/physics_stress_lab/physics_stress_lab.judasproj
```

The unchanged authored populations exercise 32 mechanisms, 96 joints and
128/512/1024 loose bodies. **F5** runs the watch tour; **1–4** choose the normal
registered scenes. The largest pile remains an extreme workload, not a 60 Hz
certification. Longer runs still expose seconds-long global contact-island
cascades. M91 is a measured improvement, not closure of every dense-contact
performance limit. The evidence states measured tails and censored failures.

For matched timing, link the same observer source against the preserved baseline
and final engine, then use `scripts/m91_measure.py`. It alternates process order,
preserves complete per-step CSVs and endpoint geometry, records native peak RSS,
and kills only its owned process group on timeout. An unfinished step has no
invented duration. Synthetic fixed-cadence runs do not measure displayed FPS.

The evidence supplies the exact baseline, binaries, commands, configurations,
source fingerprints, historical fixture references and rejected alternatives.
