# Isolated future-trajectory CCD broadphase experiment

Prepared from the exact source identified in `provenance.json`. Only this directory's `PhysicsWorld.cpp` was compiled. Shared source, engine archive and public broadphase remain unchanged.

## Rationale and enclosure contract

The public broadphase deliberately retains previous poses, full-step reach and historical motion segments, because public character/query/presentation users may need those paths. An event-island search at time `now` needs current geometry and possible future contacts only. Reusing historical fat bounds can therefore produce many irrelevant candidates and expensive pose/contact preparations.

This experiment constructs a separate `DynamicAabbTree` lazily at the first actual CCD event. Zero-event steps never build the tree or allocate its proxy list.

Its leaves enclose:

- Static bodies: existing outward `CurrentBound`.
- Centered primitive boxes/spheres: existing conservative `RemainingImpactBound(slot, now, dt)`, including the proved interval quaternion bounds and original sphere fallback.
- Other shapes: the original public tree's fat bound, retaining all established unsupported-shape conservativeness.

Every live body receives a local proxy. Leaves are replaced only after a real trajectory change has appended its new motion segment and updated its original reach bound. Unchanged leaves may retain an older, larger future enclosure; they remain conservative for the remaining interval. Prescribed motion/stop boundaries are covered by the existing sphere fallback when a tighter single-segment proof is unavailable.

For BFS, the existing actual outward shape AABB is queried against the separate tree. Any body actually touching that shape must have geometry inside its leaf and therefore cannot be rejected. Results remain sorted by ascending slot. Dropping noncontact candidates does not reorder discovered actual contacts or native solver rows. The public tree, CCD search algorithm, TOI ordering, impulse calculation, contact fidelity and physical cadence are unchanged.

The optional dirty-pair variant queries a changed body's remaining enclosure against the future tree. Existing scheduled pairs incident on changed bodies are still rescheduled as before; only discovery of additional pairs uses the tighter query.

All local proxies are step-local slot identities, with no callbacks or body creation/destruction during the synchronous impact pass. The tree and its vectors are destroyed on return or exception. Invalid enclosure ranges fall back to the original broadphase for the remainder of that impact pass.

## Controls for this isolated experiment

- Default: future tree for event-island BFS only.
- Presence of `JUDAS_CCD_LOCAL_DIRTY_QUERY`: additionally test tighter dirty-pair discovery.
- Presence of `JUDAS_CCD_LOCAL_ORIGINAL`: use the original tree for same-binary control.
- Presence of `JUDAS_CCD_LOCAL_COMPARE`: also query the original tree and count added/removed candidate differences. This intentionally adds diagnostic cost and should be disabled for timing acceptance.

These temporary controls are not public API or demo-specific physics branches.

## Private counters

Profiler counters report future-tree builds, trajectory updates, bound builds, BFS queries/candidates, optional dirty queries/candidates, compared original candidates, added/removed candidates and fallback count. The comparison totals cover all enabled local query types. An added candidate is not automatically a physical change: a conservative enclosure may include a noncontact body absent from the old fat query.

## Build and current evidence

`python3 /tmp/m87-future-tree/build.py` compiles one isolated TU and links it before the existing archive into `live`. `build.log` records the commands. Compilation/linking passed without warnings. Parent-controlled identical155-step physical-digest and performance comparisons remain authoritative; this file does not claim a runtime pass or speedup.
