# Duplicate-metric source review

This review traces the observer's input equivalence independently of its intended
performance hypothesis. It does not establish observed duplicate counts or a
profitable reuse implementation. No physics runs or builds were performed here.

## Pair-query equivalence

Production `src/physics/WorldImpactQueries.h:87–139` obtains two geometry poses,
applies a geometric bounds rejection, and dispatches geometry contacts using
primitive identities, shapes, poses, margin, and (for penetration) target. It does
not read velocity to determine the result. `src/Narrowphase.cpp:14–24` derives a
primitive from fixed parent shape data, represented pose, and primitive index;
`src/Narrowphase.cpp:140–158` dispatches using these geometric inputs.

The observer's `CCDAudit.h:100–114` key contains:

| Potential false match | Treatment |
|---|---|
| Body slot reused | Generation of each endpoint included. |
| Different compound child | Both primitive indices included. |
| Reversed pair order | Canonical identities and poses plus original directed-order bit; reverse clipping/normal output cannot alias. |
| Slightly different represented pose | All 14 position/quaternion float bit patterns compared. |
| Different speculative margin | Raw float margin bits included. |
| Different penetration target | Both halves of raw double target bits included. |
| Boolean versus manifold answer | Query kind included; no cross-kind equivalence is claimed. |
| Hash collision on the key | `unordered_map` also compares the full word arrays (`CCDAudit.h:62–75`); key hash collision changes performance, not equality. |

The key deliberately does not include query time or velocities: once the same
immutable shape and exact represented pose were obtained, those parameters are
not inputs of the pure geometry dispatcher. Derived orientation/bound caches
should be deterministic functions of the same geometric inputs. If a stale
cache breaks that condition, a repeated-key output mismatch is evidence against
reuse and must be investigated.

**Preconditions:** exactly one world is observed between `Reset()` calls; all
referenced shape/terrain/cooked geometry remains immutable during that Step.
World token, shape contents and mutable terrain revisions are not stored. Mixing
worlds or changing shape contents inside one observation window could create
false matches. The current intended driver has one world and step-local resets;
these metrics are not a general multi-world cache design.

Conservative false negatives remain: equivalent quaternion signs, signed-zero
representations, reversed query direction and different margins/targets are
counted separately. This is intentional exact-input observation.

## Output consistency and its limit

Every observed native call still runs. `CCDAudit.h:133–154` explicitly hashes
every current `Contact` field for every valid manifold point, plus manifold
count/uncertainty. It does not hash padding or unused slots. Two 64-bit digests
are retained and compared, avoiding full-manifold map storage.

Boolean encoding is injective over `{false,true}`, so Boolean result comparison
is exact. Manifold consistency is a two-digest check, not a retained byte-for-byte
proof. An adversarial or accidental joint digest collision is possible. These
small non-cryptographic hashes are diagnostic and are not security evidence.
Zero mismatches is properly described as **no observed digest disagreement**,
not an absolute proof of equivalent arbitrary outputs. Any mismatch invalidates
the claimed observed reuse equivalence until its cause is established.

## Impact request equivalence

Current poses/rates do not completely identify a future binary32 trajectory:
`src/RigidMotion.h:28–52` evaluates elapsed time relative to its segment anchor,
then rounds/integrates. Re-anchoring can change future arithmetic even when an
instantaneous pose and rate match.

`CCDAudit.h:355–383,409–450` therefore captures exact start/finish/target, start
geometry, rates and active segment anchors. Segment fields include begin/end,
anchored pose, linear/drift rates, prescribed state and inverse mass. Shape data
and query implementation are constant under the stated step-local assumptions.
Returned double time bits are compared only for this stricter `same_input` case.

This is **previous completed captured request per pair**, not an exhaustive map
of every prior request. Nested recursive requests can be followed by their
outer request in completion order. Old nonadjacent identical requests may be
missed; that causes undercounting rather than a false same-input claim.

Early bound rejections and recursive wrappers without already-computed poses
are explicitly uncaptured. Their total/result/time counters still apply, but
there is no same-input claim for them. `changed_velocity_same_geometry` and
changed-interval/anchor categories describe changed inputs; they do not justify
reusing a previous no-impact result.

## Attribution and count semantics

- The first observed key owns `unique`; all origins share that step's map.
  Per-origin `unique` is not the number of distinct keys seen within that origin.
- `cross_anchor` compares with first observation's raw anchor time. It does not
  count unique events or necessarily a change from the immediately prior call.
  Multiple events can share the same time.
- Overflow increments `untracked`. Duplicate counts are then lower bounds;
  untracked observations cannot be assumed unique.
- Existing local `ActualTruth` hits/misses include mesh predicates and the
  analytically known false start-budget case. They are not all native Boolean
  geometry calls, and must not be added to pair-query counts as such.

## Timing interpretation

Native time ends before output hashing/map work; measured nested observer
bookkeeping is subtracted from enclosing scopes. Clock overhead, scope checks,
allocation side effects, memory/cache pressure, compiler changes and scheduler
noise remain. Inclusive penetration/manifold/request scopes overlap; their times
are not additive CPU accounting. Compare unobserved runs for authoritative
physics timings and trajectories. Repeated-input wall time cannot be reported
as an achieved or guaranteed speedup.

## Analysis script

`analyze_ccd.py RUN_FOLDER` discovers `ccd.csv` files recursively and prints a
machine-readable summary; an explicit CSV is also accepted. Optional
`--output summary.json --markdown summary.md` writes both formats. The script:

- Summarizes per-kind/per-origin duplicate fractions and raw result checks.
- Marks overflow fractions as lower bounds.
- Reports request/anchor changes, local truth hits, and native wall timings.
- Checks per-row counter partitions, origin sums, geometry-key accounting and
  retained-key bounds; mismatches/aborts/accounting errors return exit code 2.
- Rejects malformed/truncated rows rather than manufacturing missing zeroes.
- Does not run an engine, alter source, introduce a cache, or infer profitability.

The child subsequently ran this analysis on the parent's completed `observed900`
output under a follow-up authorization. All accounting checks passed. The child
did not build or run physics. Findings and scope are recorded separately in
`OBSERVED900_FINDINGS.md`.
