# Dense physics: event dependencies and separation certificates

## Status

Experimental development from `b7db703ca3fc3c0d39ecc31009a1148b5b778762`,
measured on 21 October 2026. Main is unchanged. This is automated Linux
qualification, not human acceptance, Alpha readiness or closure of the dense-pile
performance finding. **The live 1,024-piece case still stalls badly.**

The editable demonstration is the existing
[Physics Stress Lab](../projects/physics_stress_lab/README.md). Its geometry,
materials, 1,024 loose pieces, 1,285 total bodies, 96 joints, 60 Hz cadence and
eight-step catch-up cap are unchanged. No game scripts or collision primitives
were altered. See [portable evidence](evidence/prealpha-event-dependencies-20261021/README.md).

## Investigation and correction

Claude's independent cloud report identifies the same amplification: one local
TOI reaches a large connected contact island; the solve changes many velocities;
then incident collision searches and scheduling repeat across that neighbourhood.
The supplied archive is preserved at its original location and identified by
hash. Twenty-two relevant archived physics inputs independently match this
checkout's development parent byte-for-byte.

The report's 94.7% repeated no-impact answers are observations, not reusable
proofs. Both endpoints may have changed trajectory. Likewise, correlation
between step cost and query count does not make every query equally expensive.
The final ablation below removes most counted queries but saves much less time.

The correction changes the event architecture in two places:

1. **Pending frontier and endpoint dependencies.**
   [ImpactEventSchedule.h](../src/physics/ImpactEventSchedule.h) replaces the
   linear earliest-event scan and global scheduled-event invalidation scan with
   an indexed heap and endpoint adjacency lists. Only pending impacts occupy
   these structures. Retiring an event removes both dependencies; a previously
   absent event can reenter after a motion change. Exact time followed by
   lexicographic primitive identity remains the ordering rule. Dirty identities
   are still processed in the original sorted order.
2. **Whole-interval separation certificates.**
   [WorldImpactEvents.cpp](../src/physics/WorldImpactEvents.cpp) uses the existing
   outward conservative `RemainingImpactBound` before expanding candidate pairs
   into scheduled queries, and during contact-island discovery. The same cached
   enclosure can cover later subintervals of an unchanged motion segment. Every
   actual velocity change invalidates it. Only supported centred box/sphere
   bounds with finite, valid enclosures can reject a pair; other combinations
   retain the original path.

For remaining interval I, with represented geometry G_i(t) contained in its
conservative enclosure E_i(I):

```text
G_i(t) subset E_i(I), G_j(t) subset E_j(I), for every t in I
E_i(I) intersection E_j(I) = empty  =>  no contact anywhere in I
```

This is not a cached sampled Boolean. Enclosures include represented rounding
and rotation through the existing bound implementation. Inclusive tangency is
not separation. Unknown/range-failed enclosures do not reject anything. Current
prescribed motion is one active segment followed by a stationary endpoint;
across that stop the rotational tightening declines and the enclosing sphere
still covers the monotone centre path.

Finite scheduled impacts whose endpoint changes are re-queried, even when their
new broadphase bounds no longer overlap. This retires stale events. Previously
absent events are rediscovered from the changed endpoints' broadphase candidates.
No-impact identities no longer trigger dependency invalidation merely because
they once appeared in a large fat-proxy neighbourhood.

**The actual connected impact solve is retained.** There is no island-size cap,
impulse/velocity cutoff, temporal batching window, dropped object, reduced
cadence, changed solver budget, sleeping override or stale TOI reuse. Near-time
batching was not adopted: a convenient time window would change chronology
without an established physical acceptance argument.

## Matched measurements

Ryzen 5 5600, 8 GB RAM, GTX 970, native Linux/NVIDIA graphics, GNU 15.2 Release
`-O3 -DNDEBUG`. Runs are sequential with no compilation or concurrent benchmark.
Frozen source/binary identities, hardware policy, timestamps, raw rows, RSS and
commands are in the evidence. Profiled measurements are separate from timings.

Fixed-step milliseconds; p95 uses the existing floor-index convention:

| Run | Baseline mean / median / p95 / worst | Candidate mean / median / p95 / worst |
|---|---|---|
| 270 steps A | 30.112 / 14.586 / 104.535 / 177.269 | 27.939 / 14.356 / 93.396 / 150.058 |
| 600 steps A | 75.180 / 54.532 / 198.638 / 614.487 | 67.568 / 49.044 / 177.781 / 535.214 |
| 600 steps B, reverse execution order | 74.807 / 54.652 / 199.988 / 616.140 | 67.280 / 48.270 / 177.132 / 533.405 |

Each 600-step run processes exactly **5,682 impact events and 3,924,868 search
iterations** on both sides. Actual CCD queries fall from **27,321,013 to
4,745,707 (82.63% fewer)**. Every physical digest, contact count, awake/sleeping
count, speed observation, search iteration/peak and endpoint geometry matches.
No numerical trajectory change was accepted or hidden. Mean fixed-step cost
falls about 10%; this is not an order-of-magnitude improvement. Long-run peak
RSS is roughly 176 MiB baseline and 173–174 MiB candidate; this small difference
is not a general memory-efficiency claim.

### Actual live reproduction

Two visible 35-second runs use ordinary wall-clock accumulation and the
unchanged catch-up policy. Whole frames starting after five wall seconds:

| Observation | Baseline | Candidate |
|---|---|---|
| Late visible FPS | 0.926 | 1.120 |
| Late p95 frame | 1,774.628 ms | 1,445.618 ms |
| Worst late frame | 2,086.997 ms | 1,961.262 ms |
| Total completed physics steps | 441 | 491 |

The candidate advances further in the same wall window, so live endpoints are
not equivalent physical times. All 441/491 respective live-prefix digests match
the corresponding exact-cadence capture. Exact runs separately compare equal
physical endpoints. Cheap startup frames inflate all-run FPS; they are not used
to suggest this case is playable. **Neither 24/30 nor 60 FPS is achieved.**

### Where the remaining time goes

Over the same 118 retained late frames containing fixed steps, the existing
profiler reports:

| Inclusive category | Baseline ms | Candidate ms |
|---|---:|---:|
| Continuous impacts | 19,360.76 | 16,515.68 |
| CCD search | 3,940.59 | 2,768.17 |
| Contact-island discovery | 4,706.13 | 4,110.29 |
| Impact solve | 3,230.91 | 3,248.23 |
| CCD geometry, nested across consumers | 6,249.53 | 5,753.13 |

These overlap and must not be added into a cost partition. They are retained
profile history, not all-run totals. Geometry calls fall from 17,928,281 to
9,745,779 in this same subset. Most eliminated queries were cheap bound rejects;
repeated actual geometry, connected response, trajectory/support maintenance
still cost substantially. Rendering and catch-up policy are not the underlying
physics cause.

## Rejected experiments and failures

All prototype sources, build commands/hashes and available raw outcomes are
preserved separately from the final candidate:

- A second dynamic tree of swept enclosures removes many queries but its update
  and query cost cancels the savings. Its 400-step mean is 77.052 ms against
  75.920 ms baseline. It is excluded from production.
- An exact represented-pose manifold cache also preserves trajectories but adds
  allocation, lookup and large-manifold copy overhead. Its 400-step mean is
  77.577 ms. It is excluded from production.
- The intermediate finite-frontier-only variant improves cost but does not prune
  the corresponding island-discovery work. The combined final code retains both
  justified changes.
- The first multi-target Make invocation regenerated the new target, then failed
  to dispatch that newly introduced target from its original makefile. The
  subsequent normal build succeeds. No build failure is counted as a pass.
- LeakSanitizer cannot run under the sandbox's ptrace arrangement. Its original
  errors are preserved; native ASan/UBSan/LeakSanitizer execution succeeds.
- One save/stream invocation omitted required arguments and exited with usage
  status 2. It is retained as a setup failure. Correct native write/read/read-b
  invocations pass separately; the invalid invocation is not an engine failure.

The earlier exact sphere/box replay and rounding-driven contact-episode
investigation remains at `.cache/dense-root-cause-20261020/`; it is not rewritten
or claimed to classify every event. That representation/continuation problem
remains distinct from this dependency/scheduling correction.

## Qualification and boundaries

- Release runtime, editor, exporter and affected tests build with no warnings.
- Independent full-scan queue oracle: 20,000 deterministic updates plus explicit
  tie, retirement, reactivation, empty-queue and endpoint-dependency cases.
  The omission negative control detects exactly the two intended failures.
- Normal and omission-control executions pass under native address/undefined
  behaviour/leak sanitizers. New positive/negative cases are registered in the
  existing validation manifest/CI collision group.
- Focused impacts, timing/energy, native mesh features, original curved-mesh
  arrival and continuation (372 checks each), motion bounds/geometry, rigid
  contacts, joints, kinematics, CharacterMotor, ragdolls, completed contacts,
  physics continuation and origin/stream checks pass.
- Fresh M70/M77 cold writer/readers pass through the unchanged protected observer,
  including actual restored physical endpoints. Save/stream write/read/read-b,
  editor Play/Stop on an owned project copy and relocated read-only standalone
  launch/render/exit pass. The original projects are unchanged.
- Measurement tooling remains strict by default. Its new explicit
  `--allow-query-reduction` mode accepts only fewer queries per step; trajectory,
  other work fields and endpoint differences still fail. Negative tooling
  controls verify all those boundaries.

Public APIs, save formats, collision primitives and fluid implementation are
unchanged. Windows and Android were not rerun on this 8 GB machine. Protected
history, independent fluid research and existing projects remain untouched.

## Remaining work and human checklist

B-01 remains **improved, not resolved**. The pile still forms large real contact
islands, and the current event response repeatedly solves their constraints.
The rounding/contact-episode mechanism can also provoke repeated events. A
further contact-graph/continuation redesign needs independent physical proof;
a numerical cutoff or graph-depth limit is not a substitute. The failed caches
show that adding another index is not automatically useful.

For human review:

1. Open the unchanged Physics Stress Lab, or its new exported package recorded in
   `application/PACKAGE.json` in the evidence.
2. Press F5 for the normal watch tour; toggle motors with M and impulse with J/K.
3. Press 3 for the unchanged 1,024-piece case. Expect remaining severe stalls.
4. Check actual collisions, motor wake-up, then F6/F7 save/load and R reload.
5. Check 4 for the rotated room. Escape releases the pointer; WASD/mouse inspect.

No new human acceptance, main promotion, M100 work or release tag is implied.
