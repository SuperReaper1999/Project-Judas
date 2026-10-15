# Optional exact zero-margin primitive manifold reuse

Proposed source only, not integrated, compiled or measured by this subagent.
`/tmp/m87-manifold-cache.cpp` and `.patch`; captured live base is preserved in
`/tmp/m87-manifold-cache-base.cpp`. No shared repository file edited.
Do not combine with the older Boolean truth-cache proposal without new evidence.

## Exact key and ordered rows

Only ordinary Box/Sphere primitives (including such compound children) and zero
margin are cached. Key: directed(a,b,pa,pb), all14 parent position/quaternion float
bit patterns and invocation epoch. Quaternion signs are not canonicalized; float
values are not rounded. Hash collisions require full key equality. The full
ContactManifold is copied, including all point rows in original order and its
uncertain flag. A reversed query has a distinct key and runs its original ordered
geometry. No normal flipping, contact sorting, target tolerance or depth repair.

PairTouches' ordinary primitive path already constructs the same zero-margin
manifold. Routing it through PairAt therefore returns exactly the same count;
subsequent island/penetration/response queries can reuse the original ordered
manifold. Cooked/asset Boolean paths retain their existing implementation. Margin
queries, terrain, capsules and cooked manifolds retain original computation.

The cache stores geometry, NOT penetration truth. PairPenetration still computes
maximum depth from that geometry and compares each requested target independently;
therefore no target belongs in this cache key. The prior Boolean proposal needed
absolute target bits and must not be confused with this design.

## Ownership/lifetime

Parent pose plus primitive identity is every changing input to ordinary primitive
ComputeContacts. Radius/extents, child rotation/offset and pivot are immutable
throughout AdvanceImpacts. Dynamic velocity, gravity, time, sleep and contact/joint
impulses do not enter this geometry. A velocity change can invalidate impact time
while leaving its represented geometry exactly equal. Every AdvanceImpacts call
(one per physical Step/substep) advances the epoch, invalidating shape/generation
changes from prior steps. An RAII active guard disables reuse on return or throw.
There are no external callbacks/geometry publications within that guarded phase.
No data is serialized, no cached geometry becomes an integration/solver anchor.

## Bounded storage/diagnostics

Power-of-two direct-mapped table, minimum64/maximum8192 entries, sized from current
candidate count after initial-impact candidate refresh. Storage allocated lazily
on first eligible zero-margin query; capacity retained but never logically grows
beyond the cap. Current ABI: Contact=168B, ContactManifold=680B, entry=760B;8192
entries use6225920B (~5.94MiB). This is real additional CPU cache/storage overhead,
not free, and must be reported. Existing cache bytes/allocation accounting includes
it. M56 counters expose hits, misses, replacements and retained bytes.

## Integrating parent's required proof

- Exact per-step physical/event digests on matched baseline/candidate workloads;
  include reversed discovery, compound children, contact uncertainty, deep spins,
  rotated gravity and ordinary cooked Boolean collision.
- Save/lifecycle state must remain unchanged; the cache is derived and disposable.
- Measure root's original rotated-pile stress step, hit rate, cache bytes, geometry
  time and allocation overhead. Small cheap-query workloads may lose to hashing/
  copying; no improvement claimed without measurements. Do not weaken thresholds.
