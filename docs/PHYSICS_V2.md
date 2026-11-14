# Physics V2 — implementation and human handover

**14 November 2026. Experimental; human acceptance pending.** Functional and
integration gates pass. Severe-crash frame budgets remain unmet: this is not
universal real-time performance qualification or a release promotion.

The current implementation code is **`b2810116602119668732c72c90c8f7556ab508b5`**.
The focused update below qualifies that code; the original `0a85558e` handover
and measurements are retained in the subsequent sections as historical evidence.
The matching SDK identifies its exact documentation-handover checkpoint in
`build/linux-sdk/BUILD_INFO.json`. Main remains
`1094d64515604fe9d03fdab479efc7f880b112a8`. Consumer SDK selections are unchanged.

## Focused panel/contact performance update

This pass fixes profiler counter overflow and removes repeated preparation without
changing simulation resolution, material strengths, substeps, iterations, damage
thresholds, contact coverage or control inputs. No quality reduction was used.

- Counter observations now reduce in bounded per-thread mailboxes, independently
  of raw scope-event queues. Frame snapshots retain Sum/Latest/Maximum semantics,
  including mixed observations, worker scopes spanning frames, clear/freeze and
  the original explicit saturation diagnostics. Hot recording allocates no heap
  memory. **35 profiler checks** include 300,000 observations without lost values.
  Raw scope timelines remain capacity-bounded; complete gameplay counter coverage
  is not a promise to retain arbitrarily many raw startup scope events.
- Material contacts reuse immutable rigid shape/primitive preparation within one
  frozen material substep. The working set is bounded to **64 candidates and 32
  primitives per cached body**. Larger cases retain ordinary full preparation;
  no contacts are discarded. The next substep invalidates the scratch, and every
  exit releases paged collision references. Attachment rows prepare unchanged
  mass/inertia once while continuing to read live velocities for every reaction.
- Detailed exterior skin topology follows actual connected/removed sections.
  Breaking a redundant bond no longer rebuilds/uploads identical skin data.
  Palette motion, real separation/removal and cold restoration still publish
  correctly. Legitimate GL topology submissions can still take roughly **45 ms**;
  this pass does not claim to have eliminated that peak.

The same full-quality four-Ural scene was replayed three times per build,
interleaved, at 20 m/s, 1280x720, VSync/cap off and shadow budget eight on the
reference Ryzen 5 5600 / GTX 970 / 8 GiB host. Headline values are medians of
per-trial statistics from **profiler-disabled** runs:

| Metric | Before | After |
|---|---:|---:|
| Average FPS | 35.48 | 59.75 |
| p50 frame ms | 18.78 | 16.19 |
| p95 frame ms | 48.63 | 31.11 |
| p99 frame ms | 117.41 | 48.68 |
| Median trial maximum frame ms | 267.71 | 186.61 |
| Frames >100 ms, median count | 17 | 4 |

**All-run absolute maxima were 2,041.42 / 2,014.95 ms.** Both occur at the first
measured frame of the first trial; their cause is unconfirmed. They are retained
in the raw data and trial statistics. Nearby samples show 3.42/3.66 process CPU
seconds across threads and zero process swap; this does not establish an OS,
driver, loading or physics cause. Do not present the median trial maximum as an
exception-free worst-case guarantee. The longest sequence above 16.67 ms falls
from about 19.97 to 5.70 seconds, including sustained frames below 60 FPS rather
than implying the application stopped for that whole period.

Complete profiled captures show **zero gameplay record drops/truncation**. Per
completed fixed step, rigid/terrain material contact time changes from roughly
4.36 to 3.00 ms and attachment solving from 0.216 to 0.148 ms. Internal constraints
remain about 3.12 ms; self contacts about 0.94 ms; wake propagation about 0.79 ms.
Those are separate scopes, not values to add to inclusive CCD or GPU durations.
CPU-side GL timings can include driver waits. The final profiled capture still
reports **127 ms of discarded simulation backlog** over the capture; unprofiled
backlog totals are unavailable. Heavy four-Ural crashes remain outside the
minimum-60-FPS and aggressive 5 ms combined-engine budgets.

Peak process RSS was **2,921–2,924 MiB before** and **2,923–2,980 MiB after**;
there is no demonstrated RAM reduction. Whole-device VRAM reached **3,978–3,984
MiB before** and **3,979–3,987 MiB after**, close to the card's 4 GiB capacity.
`DrawDeformables` creates a mesh per instance; `BeginMeshUpload` creates private
material handles and `UploadMeshMap` uploads their images independently. Those
textures are properly destroyed with their owning mesh, but identical deformable
instances do not share their immutable material textures. This is a confirmed
resource-sharing limitation, not proof that VRAM pressure caused a particular
stall. Correct resource-sharing/lifetime work is a separate follow-up; this pass
retains the measured memory footprint and the unresolved delays explicitly.

A separate real combined terrain/animation/particles control remains essentially
unchanged: p99 **6.797 -> 6.809 ms**, with actual workload-state checks, no dropped
diagnostics and no reported simulation loss. One paired control is not a general
performance guarantee.

Qualification completed for the exact implementation checkpoint:

- Local **181/181 routine tests**, **158 deformable checks**, **184 structural
  checks** and **35 profiler checks** pass. New checks cover dense/large compound
  fallbacks, stale geometry and redundant-bond skin equivalence.
- **2,040 full recorded physical-state hashes match exactly** against the prior
  qualified implementation: two 900-step crash writers and separate 120-step cold
  readers. Both durable cold boundaries match. All three Ural pairs also retain
  exact reported vehicle-state observations (17/19/19 post-launch observations).
- The clean SDK passes all eight creation/Play-Stop/tool/integrity/moved-export
  operations. An actual **portable OpenGL 3.3 NVIDIA** four-Ural run launches,
  collides, destroys connections and renders the resulting wrecks. This fixed
  clock check establishes functionality, not portable FPS qualification.
- [Completed Linux code/SDK CI](https://github.com/SuperReaper1999/Project-Judas/actions/runs/43591266261)
  and [completed Windows native SDK/selected contracts](https://github.com/SuperReaper1999/Project-Judas/actions/runs/43591390116)
  both pass at exact `b2810116`. Windows hardware graphics, Android and browser
  execution were not performed.

Raw inputs, executable hashes, paired trials, complete counter breakdowns, memory
samples, cold hashes and retained negative experiments are in
`.cache/physics-v2-panel-optimisation-20261114/PERFORMANCE_REPORT.json` and adjacent
receipts. The earlier ablation cases are diagnostic feature removals, not these
optimisations. Human acceptance and main promotion remain separate decisions.

## Architecture and corrected causes

- **One fixed-step owner, four fresh native contact intervals.** The previous
  solver reused one manifold preparation through four evolving intervals.
  Contacts now follow current geometry without increasing interval count.
  Forces, impulses, coupling rounds, contact publication, sleep clocks and motion
  history retain explicit macro/interval ownership. Outer `StepCoupled` composes
  complete steps. Warm contact impulses use their actual native duration.
- **Successful collisions continue.** The fourth-distinct-arrival per-body
  motion freeze is removed. Its former pose/velocity disagreement was a real
  truncation defect. The bounded global guard and failed-response holds remain
  explicit, with conservative certified tangential continuation where supported.
- **Joint guesses warm once per public step.** Fresh kernels had inadvertently
  replayed drive, limit and finite-material warm guesses at every native interval.
  Later intervals retain rigid bilateral estimates while solving every physical
  row with unchanged forces, torque caps and iteration budgets. No body velocity
  reset, rotation lock, artificial damping or fidelity reduction was added.
- **Immediate arrivals precede unnecessary future searches.** Certain native
  box/identity-child compound contacts establish current-time events first.
  Strictly future roots can wait while those events run; every required future
  query completes before time advances. Revision invalidation, original event
  ordering and conservative ordinary fallbacks remain authoritative.
- **Bounded independent geometry work uses the existing pool.** Default four
  lanes, tunable with `JUDAS_MK2_QUERY_LANES=1|2|4`; small/unsupported batches remain
  serial. Workers own private scratch, join before mutation, and publish in stable
  pending order. Borrowing owner anchors no longer bypasses private bound-storage
  allocation. The representative four-lane query scratch was about 588 KiB.
- **Support and material preparation stay local.** Spherical normal anchors use
  sphere geometry rather than a rotating material point. Native box separation
  can avoid constructing unnecessary witnesses. Material bounds, body mass/inertia
  frames and edge-tree refits avoid repeated preparation when inputs are unchanged.
  These small changes preserve their original solve order and physical work.
- **Fast triggers use actual completed travel.** Rigid/kinematic sensors use the
  piecewise motion ledger; CharacterMotor contributes accepted fixed-orientation
  translations through static sensors, including successful steps from the true
  capsule origin. Rejected probes and teleports are not inferred as swept travel.
  Transient Enter/Exit, filters, lifetimes, coalescing and origin shifts are covered.

Ubuntu qualification also exposed physical-balance integration defects. A brief
upright sample could permanently suppress a later get-up; the recovery controller
now uses current posture until standing is measured. Corrective swing velocity
lookahead uses the same accelerated swing clock as the actual target. Adjacent
static colliders share one support frame, retaining a real carrier and all measured
reactions; separate dynamic/kinematic frames, sensors and query-only bodies do not.

## Original correctness and integration evidence (0a85558e)

- **181/181 routine CTests pass locally and in exact-code Ubuntu CI.** The balance
  suite has **148 physical checks**, including real two-collider reactions and
  rigid-origin slope controls. Old code fails the new controls; corrected code
  passes without changing their gains, impulses, deadlines or standing thresholds.
- Fourteen permanent CCD scheduling scenarios compare full per-step state,
  impulses, touch history and motion paths across serial/two/four lanes, with
  actual helper/deferred work. Equivalent scheduling changes remain exact.
  Architecture changes are not represented as preserving historical wreck hashes.
- Spinning-sphere support, angular momentum/energy, thin/rotating/offset geometry,
  grazing contacts, kinematics, wheels, articulation, sleeping/waking, terrain,
  deformables, sensors and modern persistence retain independent controls.
  Character/controller coverage has **77 checks**; separation has **3,992**.
- Elastic continuation controls follow the analytic 16.6667/33.3333 m paths at
  1,000/2,000 m/s, versus the old roughly 15.6 m truncation. Endpoint errors were
  approximately 0.90/1.80 micrometres, with zero reported relative energy error.
- **23 additional native programs**, vehicle cold write/read, sustained Terrain V2
  residency and Basic Water cold restoration pass. Eight explicit portable
  renderer gates were exercised; renderer portability code is unchanged.
- The clean SDK passes eight authoring/tool/export operations, including Editor
  creation, Play/Stop, SDK integrity and relocated editor/CLI standalone exports.
- Private Challenger frontal and four-car debris writers each advance 900 steps;
  separate readers restore the exact durable shape/state hash before advancing
  120 further steps. All 900 writer rows contain zero path holds and no truncated
  records. This proves the cold boundary; it is not a claim of identical warm/cold
  continuation when project-owned repeat/post-drive clocks restart.
- Native front-wall fixtures at **2 and 4 m/s** retain all three bonnet attachments
  (`000`), with no broken structural links. At 2 m/s bonnet plastic rotation is
  zero; at 4 m/s it is about 0.304 rad. The severe 30 m/s fixture still deforms and
  fully detaches the bonnet (`111`), preserving physical damage capability.

Actual completed CI, not queued work:

- [Ubuntu engine/SDK qualification](https://github.com/SuperReaper1999/Project-Judas/actions/runs/43576866228):
  181 routine gates, compiler/tooling checks and Ubuntu 24.04/glibc 2.39 SDK baseline.
- [Windows/MSVC qualification](https://github.com/SuperReaper1999/Project-Judas/actions/runs/43576971800):
  complete native SDK and selected native contracts. This is not the full Linux
  matrix, Windows hardware GL, or Windows editor/export gameplay qualification.
- [Code-checkpoint documentation](https://github.com/SuperReaper1999/Project-Judas/actions/runs/43576866270).

The earlier `83738806` Ubuntu run failed five balance checks; its failed logs and
controlled reproductions are retained. They are not counted as passing evidence.

## Original whole-game performance (0a85558e)

Ryzen 5 5600, GTX 970/NVIDIA 580.178.04, 8 GiB RAM, Kubuntu; 1280x720,
VSync/cap disabled, identical authored inputs and eight-shadow budget. Three
NEW captures per case use the clean code checkpoint above. Twelve unchanged OLD
captures are explicitly reused, with their original identities and a separate
fresh OLD four-way stability control (99.06 ms worst frame).

**OLD is the preserved candidate foundation with old scheduling selected, not
pristine `d1ce11c7`.** The separate Afterglow comparison below uses the actual
pristine executable. Removing motion holds changes physical outcomes; these
are matched authored workloads, not identical-state speedup claims.

Values are medians of three per-trial statistics; ranges expose worst-frame
variation. Full p50/p95/p99, FPS/lows, hitches, counters and identities remain in
`final-macro-owner-frame-qualification/AGGREGATED.json` beneath the evidence root.

| Workload | OLD p99 ms | NEW p99 ms | OLD worst-frame ms | NEW worst-frame ms | OLD / NEW maximum fixed-step ms |
|---|---:|---:|---:|---:|---:|
| One driven vehicle | 2.46 | 2.63 | 3.47 | 3.90 | 2.25 / 2.58 |
| Active vehicle fleet | 3.63 | 3.81 | 5.76 | 5.52 | 4.44 / 3.97 |
| Combined terrain/animation/particles | 6.80 | 6.86 | 7.76 | 8.69 | 4.70 / 5.13 |
| Four-way private-model crash | 11.63 | 11.77 | 113.20 | 72.19 | 25.79 / 31.31 |

The four-way NEW worst-frame range is **72.18–75.86 ms**. Median >16.67 ms frames
fall from 10 to 9, >100 ms frames from 2 to 0, and the longest hitch sequence from
474.09 to 374.43 ms. No simulation time is reported discarded. Unprofiled controls
show **96.85 to 69.32 ms** worst frames, supporting a real gameplay improvement.
The maximum individual physics step increases because valid collision motion
continues; ordinary vehicle p99 also has a small cost. Neither result is hidden.

All normal-workload p99 values meet 8.33 ms, but one fleet repetition contains an
isolated **17.96 ms** frame. Its physics step is 3.28 ms and recorded Play work
4.04 ms; approximately 13.5 ms is unattributed. Its cause is unconfirmed. It is
retained, not discarded as noise. Four-way crashes still fail 16.67 ms and the
aggressive 5 ms combined-engine goal. The final captures have zero diagnostic
drops/truncation; initial overflowed pilots remain historical evidence.

Four-way content remains 99 bodies, 18 wheels and about 1.746 million submitted
triangles. Median peak RSS is about 184 MiB for normal vehicles, 224 MiB combined,
and 3,214 MiB for the private yard. Whole-device VRAM peaks near 2,416 MiB, including
the desktop/startup. Whole-capture process CPU is about one core; short query
worker bursts do not demonstrate full-core scaling. Owner mutation remains ordered.
Measured same-frame CPU/GPU completion p99 is about 9.82 ms combined and 17.32 ms
four-way. CPU/GPU intervals overlap and are **not added** to obtain frame time;
GPU command spans may contain gaps and device telemetry is not per-process use.

### Original Afterglow crash

The frozen 218-file workload hash is
`c88e58cc0a687b47e239f6c50927352da52b7172e4ba44dccd95ad5af61ace25`.
Two fixed-clock and one rendered replay per build pass the real three-car cluster,
intact approach and irreversible-damage checks. Exact runtime/input identities
are retained. Both builds use the same unchanged engine-asset root. Raw captures
are preserved; 119 actual game frames are analysed separately from trailing host
shutdown. Fixed-clock costs are not presented as gameplay FPS.

Rendered outer-work p50/p95/p99/max changes from **3.32/13.07/16.68/23.42 ms** to
**3.32/13.37/15.47/23.28 ms**. Maximum rendered physics steps are 20.19/19.95 ms.
This is **no convincing peak-cost improvement for Afterglow**; one rendered trial
per build cannot establish a general p99 speedup. It still has a >16.67 ms frame.
The collision-continuation correction is independently established by physical
controls, not inferred from those timings or a similar final wreck.

## Remaining limits and rejected shortcuts

- The global **256 frontier-event guard per native interval**, plus bounded
  terminal retirement, can still hold motion under extreme overload. The retained
  200,000 m/s stress travels 822 m rather than 3,333 m and can starve an unrelated
  later collision. This is an explicit unsupported limit, not a solved guarantee.
- Query-only controller sweeps cover accepted fixed-orientation translations
  through static sensors. Relative moving/rotating sensor timing and rotating
  controller capsules are not supplied by that API; native rigid/kinematic sensor
  ledger coverage is separate.
- Severe crash performance is not yet minimum-60-FPS qualified. Further work must
  target the measured event/response critical path, not reduce coverage, structural
  fidelity or supported behaviour. Normal p99 success is not an exception-free
  frame-time guarantee.
- Windows hardware graphics, Android and browser execution were not performed.
  A local glibc 2.43 SDK is for this host; Ubuntu baseline evidence belongs to the
  CI-built SDK, not a claim that this host binary runs on older libc.
- Tiny-speed collision suppression was rejected. Blanket convex refresh failed
  angular-momentum controls; horizon-prefix rejection failed a certificate;
  oriented-plane/discovery/batch experiments gave no net frame benefit. Failed
  experiments remain in the evidence folder. No giant cache, GPU physics system,
  reduced sampling coverage or lower solver quality was introduced.

## Human testing

From the repository, launch the matching local SDK against the existing verified
private project; downloaded models remain private and absent from SDK/Git:

```sh
./build/linux-sdk/judas .cache/mk2-collision-architecture/private-current-yard/Crash_Yard.judasproj
```

For the four-way layout use `Four_Way_Architecture.judasproj` in the same directory
and F4. The frozen benchmark project is a qualification fixture with its own save
seed; it is not the ordinary interactive launch target.

1. Select all four vehicles with 1–4. Drive/reverse W/S, steer A/D, brake Space.
2. Hold RMB to orbit; wheel zooms, C returns behind the car. Confirm wheel/material
   presentation and steering/suspension remain correct.
3. Cycle launch speed with X and launch with Enter. Try minor frontal bumps, then
   the barrier, pole, thin wall and corner; use R between independent trials.
4. Try repeated/severe impacts and F4 in the four-way layout. Check continuing
   motion, permanent damage, partial/complete detachment and independent debris.
5. K/L save/load; quit completely and relaunch before loading a damaged save.
6. Open a current project in `build/linux-sdk/judas_editor`; check Play/Stop,
   terrain support/streaming and fast static trigger crossings in normal gameplay.

**Human acceptance is the operator's decision.** No main promotion or publication
is authorised by these automated results.

All local raw evidence, executable/input hashes and negative controls are under
`.cache/physics-v2-audit-20261113/`. Current receipts include
`macro-owner-routine-ctest.log`, `extra-native-qualification-macro-owner/`,
`macro-owner-sdk-work/`, `macro-owner-native-cold-proof/`,
`macro-owner-minor-bumps/`, `final-macro-owner-frame-qualification/`,
`final-macro-owner-unprofiled-control/`, `final-afterglow-macro-owner-analysis/`,
and the saved completed platform-run JSON/artifacts. Final SDK reconciliation
receipts are recorded separately with its exact handover revision.
