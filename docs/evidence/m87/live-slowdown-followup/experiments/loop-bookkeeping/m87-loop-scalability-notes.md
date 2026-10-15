# M87 exact CCD-loop bookkeeping proposal

Prepared against committed `cee55a898aa0e4938665903f3b207d828acdb1df` source.
No shared repository file was edited. Files: `/tmp/m87-loop-base.cpp`,
`/tmp/m87-loop-scalability.cpp`, `/tmp/m87-loop-scalability.patch`.
This patch is proposed source, not compiled or performance-validated here.

## Order and semantics argument

1. Island seed order, island FIFO traversal, joint traversal, sorted broadphase
   candidates, primitive traversal and first-hit short circuit remain unchanged.
2. The existing `touching` vector remains the authoritative first-hit order.
   Hash membership is never iterated. It replaces its former linear hit lookup.
3. Negative pair membership is recorded only if every tested primitive is an
   ordinary box/sphere and every zero-margin manifold reports `uncertain=false`.
   These are resolved separations. Arithmetic-uncertain misses and specialized
   cooked/terrain/capsule Boolean misses retain their original reverse retry.
4. A discovered simple manifold is reused for response only when its original
   argument order already equals the ascending canonical response order. Reverse
   discovery manifolds are not flipped or substituted. Compound pairs cache only
   the first primitive that actually ran and hit; all other response primitives
   execute exactly the original canonical geometry calls.
5. Discovery and response use the same fixed event time, anchored ledger poses,
   immutable shapes and zero margin. Intervening Wake and deferred acceleration
   affect sleep/velocity, not sampled geometry. There is no cross-event cache.
6. Immediately after collecting a body's genuinely changed velocity, event-local
   membership becomes 2 rather than 1. It remains nonzero. No later operation
   needs the distinction between included=1 and included=2 except the scheduled
   pair scan, where equality to2 is precisely membership in the old `changed`
   vector. Dirty keys still receive the same final sort/unique.
7. No event time, minimum-time ordering, geometry predicate, target, tolerance,
   physical row, joint/contact iteration, impulse, restitution, timestep, event
   count or sleep policy changes. Existing uncertainty retry remains explicit.

## Validation the integrating parent should perform

- First compare original and patched exact per-step physical digests plus event
  counts over identical reproducible dense-pile inputs, including the live1024
  scene beyond step187. Profiler geometry-query counts will intentionally fall.
- Check compound multi-primitive interactions, reversed first discoveries,
  uncertain geometry and cooked contacts with unchanged physical endpoints.
- Retain actual wall-clock/CCD phase data; local hash/manifold storage may have
  overhead in tiny islands and must earn its place in representative captures.
- This patch does not bound legitimate sequential dense-pile event cascades; it
  reduces exact repeated bookkeeping and duplicate primitive geometry work.
