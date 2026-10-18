# Independent derived impact-candidate cache review

## Conclusion

No concrete candidate omission, ordering change or reference-lifetime defect was
found in the reviewed production callgraph. This is a bounded source review, not
a replacement for exact candidate-list observers, trajectories or performance
measurements. Reviewed source hashes are in SOURCES.json.

## Exact-set argument

Let `F_a` be the querying body's current fat tree box, `Q` its actual query and
`F_b` another leaf's current fat box. The original sorted tree result is
`S(Q)={b | F_b overlaps Q}`. If `F_a.Contains(Q)`, then
`S(Q)` is exactly the ascending-order subsequence of `S(F_a)` selected by the
same inclusive `F_b.Overlaps(Q)` predicate. The helper uses precisely these
existing predicates. It does not retain contact, sleeping, filter, trajectory or
TOI truth. `CanRespond` still runs after candidate retrieval.

If `Q` escapes `F_a`, the helper performs the original query. This matters for
an event-time or unusual bound not certified by the current proxy enclosure.
An unchanged own fat box alone is insufficient: another proxy can enter it.
The global reinsertions revision clears all retained lists whenever any leaf fat
box changes during this guarded event pass.

## Actual mutation and callback boundaries

- Within `AdvanceImpacts`, pre-loop proxy growth and later velocity-changing
  bodies use `CoverStepReach`; actual `MoveProxy` increments
  `reinsertionsSinceStep`. Initial cache construction follows the pre-loop growth.
- `RefreshProxy` also increments that counter, but ordinary endpoint refresh is
  after `AdvanceImpacts` returns. Public reset/body-shape/create/destroy and origin
  operations are not invoked by an in-pass callback.
- Coupling begin/solve callbacks finish before `AdvanceImpacts`; coupling finish
  occurs afterward. Gameplay contact publication is afterward.
- `Wake` changes sleeping/island state and scans joints/contact observations; it
  neither mutates proxy boxes nor queries the tree.
- Event geometry, bound/pair helpers, JointInputs/PrepareJoints and impulse solver
  do not invoke QuerySlots or the cache recursively.
- Body vectors/slots and live tree membership therefore remain stable throughout
  each use of the returned candidate reference. Revision clearing and cache calls
  occur only after that iteration has completed.

The helper copies tree scratch before retaining an entry. Its filtered vector is
consumed before another Get. Oversized-cache fallback references ordinary
QuerySlots scratch under the same non-recursive callgraph as the baseline.
Full-key unordered lookup is never iterated for discovery order; candidate
vectors retain original sorted order.

## Budget and fallback

The retained-ID capacity and slot-count limits are enforced before insertion.
Oversized superset queries fall back to an actual original query unless the query
already equals that fat box. Revision clearing resets all admission bookkeeping.
The filtered scratch and unordered-map metadata are additional bounded memory;
the retained-ID limit alone must not be described as the entire scratch footprint.
Allocation failures remain ordinary exceptions, not a silently truncated list.

## Specific test gaps raised to the implementation owner

1. The first 14-unit-control version has no leaf strictly inside the cached fat
   superset but outside its actual query. Existing leaf 3 merely touches the
   actual bound at x=1. Thus disabling the subset filter can evade those helper
   controls. Add a strict fat-only leaf, assert its exclusion and run a deliberate
   filter-disabled negative.
2. Add an oversized cached-fat superset with an actual subset query, so fallback
   must issue the actual query rather than returning the overly broad fat result.

The owner confirmed these controls and corresponding test-only mutations are
being added. They are test coverage deficiencies, not observed production cache
bugs. The separate live candidate-list observer remains essential for validating
all real event queries against the unchanged tree oracle.

## Remaining scope

No compile, benchmark or tests were run by this independent reviewer. Future
changes introducing proxy mutation, body lifecycle callbacks, nested tree/cache
queries or concurrent authoritative mutation inside this pass must re-establish
these invariants. The helper is not a cross-step or persisted spatial truth cache.
