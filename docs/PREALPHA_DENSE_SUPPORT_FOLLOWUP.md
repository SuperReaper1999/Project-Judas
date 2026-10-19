# Dense physics: support continuation follow-up

## Scope and release status

This follows the [certified contact fast-path checkpoint](PREALPHA_DENSE_SCALABILITY.md),
`8f6bd18b876158c131bee61fa0dc71625d912c30`, on Experimental. The preceding
human-accepted checkpoint, `e30d5f9bb72a1a341d3eff7485b8d6b3eb6f8ea9`, was promoted
unchanged to main. This follow-up requires separate human acceptance for promotion.

**The 24–30 FPS target remains unmet.** The operator explicitly accepted continued
optimization with a measured remaining performance limit, to be revisited on the
replacement Ryzen CPU. This is not a claim that hardware alone explains the cost.
Repeated engine collision work remains a demonstrated bottleneck.

The project, 1,024 loose pieces, 1,285 total bodies, 96 joints, 60 Hz timestep,
solver iterations, materials and CCD settings are unchanged. No game-specific
physics path, population cut, new sleeping policy or relaxed tolerance is used.

## Corrected mechanism

`PhysicsWorld::Impl::SupportContinuationSafe`, in
[WorldCCDSearch.cpp](../src/physics/WorldCCDSearch.cpp), can retain an existing
inelastic primitive contact episode when conservative bounds certify the entire
remaining interval within the existing penetration allowance. Unsupported or
uncertifiable cases continue through the existing collision search.

For interval `T`, its calculation encloses the lower-clearance proxy

```text
L_T(t) = relative_center_projection(t)
         - maximum_extent_A(T) - maximum_extent_B(T)
         + existing_allowance * length(normal).
```

The extent bounds are independently taken over the whole interval. The upper
endpoint of this proxy's interval is therefore **not** an upper bound on actual
clearance. A nonpositive upper proxy cannot establish that smaller intervals will
also fail: subdivision can reduce both maximum extents.

The old recursion nevertheless stopped on `bound->hi <= 0`. The correction removes
that pruning condition. The depth-three budget (at most 15 nodes), 5 mm allowance
and smaller-shape restriction, endpoint check, native Float trajectories, geometry
predicates, contact forces and event ownership remain intact. No additional solver
iterations or impulses across a positive gap are introduced.

This repairs an unnecessarily weak sufficient certificate, not a universal new
continuous-collision algorithm. Contact chronology and resulting pile trajectories
can change because already-supported contacts need fewer repeated impact solves.

## Mechanism evidence and detecting regression

The exact preserved pair 595/851 trace contains 78 recorded states. Actual engine
calls and a separate branch observer agree for every state:

| Result | Original | Corrected |
|---|---:|---:|
| States certified within the unchanged budget | 0 / 78 | 78 / 78 |
| Subdivision nodes per accepted proof | None; stopped at root | 3–9 |

[SupportContinuationTests.cpp](../tests/SupportContinuationTests.cpp) calls the
actual internal engine method. Its two positive cases rotate touching boxes about
their separating normal, with and without common translation. Their projected
extent remains constant; interval refinement must be allowed to establish this.
The corrected implementation passes all 11 checks. The same test source linked
against the original implementation exits nonzero with four failures.

Two physical negative cases remain rejected: 16.67 mm endpoint penetration and
9.90 mm interior penetration with a completely clear endpoint. The latter catches
an endpoint-only shortcut. Native depth sampling confirms the negative fixtures;
sampling does not replace the positive continuous interval certificate.

## Matched Linux measurements

Intel Core i7-4790, four physical cores/eight threads, approximately 16 GB RAM,
GTX 970, NVIDIA 580.178.04; GCC 15.2 Release. Same authored room, native renderer,
profiling explicitly disabled, 270 physics steps (4.5 simulated seconds).
These synthetic one-step-per-frame captures are **not displayed FPS**.

| Fixed-step cost | Accepted pre-optimization baseline | Safe fast paths | Final support build |
|---|---:|---:|---:|
| Mean | 92.742 ms | 63.033 ms | 46.827 ms |
| Median | 28.424 ms | 23.004 ms | 22.230 ms |
| p95 | 338.077 ms | 213.605 ms | 158.741 ms |
| Worst | 1,658.782 ms | 942.047 ms | 387.280 ms |

These are preserved single observations, not statistical confidence intervals or
universal bounds. The support change alters chronology, so its physical trajectories
are qualified separately; bitwise baseline trajectory equality is not claimed.
Its mean reduction relative to the safe fast paths is approximately 25.7%, with
58.9% lower observed worst-step cost. Relative to the initial baseline, mean cost
is approximately halved. The earlier isolated correction run measured 46.271 ms
mean and 262.562 ms worst. All physical/work rows and endpoint geometry match the
final build exactly; the timing difference is retained as run-to-run variability.

Total impact events decrease from 3,520 to 3,071 and collision searches from
6,608,132 to 4,136,546. Step 270 decreases from 51 events to five. This explains the
reduced late tail; the engine still performs costly coupled contact work.

### Real application cadence remains slow

The final build's separate 35-second requested capture completes 367 fixed steps
over 36.032 observed application seconds, including the last catch-up frame. It
advances only 6.117 simulated seconds: approximately 0.170 simulated seconds per
wall second. Mean output is 1.582 FPS; the rate from the median interval is 5.215
FPS. Forty-three frames reach the unchanged eight-step catch-up branch. The worst
fixed step over this later horizon is 948.381 ms. Process maximum RSS is 176,648 KiB.

This later horizon is more expensive than the 270-step capture. It must not be
hidden behind the shorter matched result. Its different physical horizon also
prevents an aggregate FPS speedup claim against the earlier baseline captures.
The result remains unsuitable for responsive gameplay in this extreme scenario.
The replacement CPU may help; these measurements do not establish that it will
meet the requested frame rate without further algorithmic work.

## Physical observations and limits

The unchanged temporal observer produces exactly the same 270 physical rows as
the ordinary corrected benchmark, so the observer does not change the trajectory.
All 1,024 pieces remain present, finite and within the actual finite floor footprint
at every observed step; their total mass remains 2,048 kg. Endpoint queries report
zero uncertain/unsupported pairs and 7.187 mm deepest penetration, versus 7.217 mm
for the preceding fast-path checkpoint.

The all-step floor support oracle improves its worst clearance from -63.582 mm to
-45.075 mm. However, the maximum native pair penetration sampled every ten steps
changes from 57.799 mm to **61.963 mm**, both at step 190. These are mixed numerical
results, not a blanket claim of improved accuracy. No new tolerance is invented to
turn those observations into a pass. The existing physical regression contracts
are the acceptance checks; dense-pile penetration remains a limitation.

The tagged population's mechanical energy never exceeds its initial 32,346.318 J;
its final value is 26,263.336 J. This is not a whole-world conservation theorem.
The pair-depth observations are sampled, not an all-time continuous-depth bound.

## Rejected experiments retained

| Experiment | Actual result and disposition |
|---|---|
| Coherent step-local Double collision geometry | 96.684 ms mean / 416.014 ms p95 / 1,299.078 ms worst: slower than the safe Float checkpoint. Rejected. Also loses exact reconstruction of some public post-impact motion segments; maximum measured position discrepancy 0.481 micrometres. |
| Reuse cached remaining bounds during island discovery | Exact 270-row/endpoint parity; 62.037 ms mean, but mixed tails versus 63.033 ms. No reliable meaningful gain established. Not adopted. |
| Event-local body lookup / lazy rotation memo | Exact captured event/body/constraint/result parity and 22 control checks, but only a small mean gain with worse p95/max. Not adopted. |
| Persistent raw broadphase pair cache | 24,025 standalone checks and detecting invalidation mutants. No engine integration or whole-world timing; no speedup claimed. |

The Double experiment's independently found support-projection error, its corrected
follow-up, original failure, archive dependency closure and public-history failure
are preserved. That research is not installed into production. Existing historical
evidence and independent fluid research remain unchanged.

## Final qualification and reproduction

Final build, regression, cold-save, export, cadence and fingerprint receipts are
indexed in [the follow-up evidence directory](evidence/prealpha-dense-support-20261019/README.md).
They distinguish completed validation from retained experimental failures. Windows
and Android have not been requalified for this Linux-focused follow-up.

The normal Release runtime/editor/exporter and focused targets build with zero
warnings. All 31 focused invocations pass their expected exits, including three
deliberately broken negative controls. The first 18 validation wrappers correctly
refused stale source provenance after tracked documentation changed during a build;
they executed no tests. Identity was refreshed and those 18 checks passed. The
original refusals are preserved and are not counted as runtime failures or passes.

Fresh-process M70 and M77 continuation adds 61 passing checks across four processes.
M77 compares the actual restored physical endpoint exactly against uninterrupted
continuation, with both physically standing, and both saved files remain byte
unchanged. Those functional GL checks used llvmpipe; they are not GPU performance
evidence. Two existing warnings in the unchanged protected diagnostic observer
are preserved separately from the warning-free normal production build.

The ordinary project exports six assets and four scenes. Its final runtime launches
from a relocated, read-only package under `/tmp`, uses the NVIDIA renderer and
closes its own window normally with exit zero. This checks the default 128-piece
room's startup and teardown; the separate measured application runs exercise the
actual 1,024-piece room. No human play-test is inferred from either.

The permanent focused regression can be run through the existing validation tool:

```sh
cmake --build build --target judas_support_continuation_tests
python3 scripts/prealpha_validate.py --build-dir build \
  --test support-continuation --output /tmp/judas-support-check
```

Use [physics_dense_measure.py](../scripts/physics_dense_measure.py) to freeze and
run the same room on the replacement CPU. Preserve resolution, fixture hash,
60 Hz cadence, profiling setting and simulation horizon. Synthetic frame counts
and slowed simulation must not be presented as real-time FPS.

## Human checklist

1. Open Physics Stress Lab and press **3**. Watch the initial fall and developing
   pile; remaining hitches are expected.
2. Check **1 / 2 / 4**, motor toggle **M**, impulses **J / K** and dropped pieces
   **P**. Verify contact, natural sleeping and waking.
3. Check **F6 / F7** save/load, pause/resume and reload.
4. On the new CPU, repeat the recorded benchmark and compare actual simulation
   progress as well as output cadence.

Main is not updated by this follow-up. No new human acceptance, Alpha readiness,
M100 work or release tag is claimed.
