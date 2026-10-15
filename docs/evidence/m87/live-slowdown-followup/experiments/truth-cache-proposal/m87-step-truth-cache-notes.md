# Optional step-wide exact CCD contact-truth cache

Proposed source only, not integrated, compiled or measured by this subagent.
Files `/tmp/m87-step-truth-cache.cpp` and `.patch`, based on committed M87.
No shared repository file was edited. Use only if measured savings justify it.

## Authority/key

The table stores ONLY Boolean actual-contact/absolute-penetration truth. It does
not store/reuse an impact time, conservative plane, reach, solver result, velocity,
contact manifold, margin query or integration anchor.

Each key contains:
- directed body slots and primitive indices, preserving argument order;
- all fourteen exact binary32 parent position/quaternion bit patterns;
- exact binary64 absolute penetration target, AFTER starting depth is added;
- an AdvanceImpacts epoch, invalidating all prior body generations/shapes/assets.

Local three-entry per-query reuse is checked first. Step-table hits update that
local cache exactly as a newly evaluated truth would. Hash collisions require
full key equality and otherwise only replace a derived cache entry. No tolerance,
canonical quaternion sign, float equality, rounding or reduced key is introduced.

## Time and hidden inputs traced

`PairTouches` and `PairPenetrationExceeds` consume `ImpactPoseAt` parent poses,
immutable primitive shape/child/pivot data and immutable cooked/terrain geometry.
`Narrowphase.cpp`'s TerrainVsPrimitive/SampleTerrainAtWorld sample position and
orientation with no clock, velocity, frame or gravity argument. Contacts.cpp
primitive evaluation and CollisionGeometry.cpp cooked queries likewise have no
clock. Deferred gravity and Wake change velocity/sleep, not the current geometry.
No user callback, geometry rebuild or scene/stream mutation runs inside
AdvanceImpacts. Consequently equal key geometry/target means equal truth even
when another event changes velocity or advances absolute search time.

The epoch must remain per AdvanceImpacts invocation. Do not retain truth over
separate steps or hoist it over mutable geometry publication without adding the
appropriate revision. Existing early CanRespond checks still own filtering.

## Bounded storage and visibility

Direct mapped, retained power-of-two table sized from initial candidate count,
minimum64 / maximum8192 entries (96 bytes each on current ABI, maximum0.75MiB).
No allocation on a newly constructed world's steps with no searchable pairs;
old allocation is retained if a workload shrinks, within the same cap. Epoch
invalidation is O(1); uint64 wrap explicitly invalidates entry epochs. Allocation
and storage participate in the existing geometry-cache diagnostics.

Three M56 counters report step-table hits, misses and replacements; a fourth
reports retained bytes. The existing actual-contact reuse counter includes both
local and step-table hits. Other profiling geometry-call counts should fall.

## Evidence status and next decision

Original live core:332460 truth requests,57359 local reuses, therefore275101
truth evaluations before step completion. Cross-query key overlap is NOT yet
measured. Hashing/comparing80 key bytes may lose for cheap sphere misses. Keep
this optional pending profile capture of actual hit rate and wall time. Endpoint,
contact ordering, impact times/event counts, physical impulses and final digests
must remain unchanged on exact matched workloads, including penetration-target
spins and compound/cooked/terrain cases. No performance result is claimed.
