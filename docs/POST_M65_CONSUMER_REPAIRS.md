# Post-M65 consumer corrections

**Historical accepted review/repair record:** the original observations, findings
and pending-review statements below retain their recorded scope. Later engine
changes can supersede them; use [Architecture](ARCHITECTURE.md) for current status.

Current candidate based on `26e6f089fde0f39d68659882f6204da3a453473e`. This is the corrective pass following the [accepted consumer review](POST_M65_CONSUMER_REVIEW.md), not M66. The historical review and original games remain unchanged. Automated results below do not replace human gameplay acceptance.

## Six closures

| Item | Reproduced cause | Correction and proof | Remaining boundary |
|---|---|---|---|
| N2 resident-reference cost | Property JSON inspection constructed a complete QuickJS VM/module repeatedly per region/slot. | Live streaming reads entity references validated during normal script initialization, caches by authored slot data, scans once per outer advance, and resolves target ownership each time. Pending metadata temporarily pins safely. JSON authoring helpers reuse a bounded JSON-only context. Focused metadata tests exercise replacement, destruction, typed defaults, multiple slots, adoption, unload/revisit and zero hot-path VM construction. | JSON authoring helpers may construct one context on their thread's first use. Unsupported arbitrary C++ mutation of script membership is not a public runtime editing API. |
| N1 target capsules | Query dispatch treated target capsules as zero-extent boxes. | Shared segment/capsule distances, exact ray cylinder/hemisphere intersections and capsule witnesses for sphere/capsule/box casts. Independent 1.7 m / 1.1 m fixtures, containment, rotations, filtering and stale handles pass. The actual Void pilot receives 100→93 suit damage through its real capsule after the copied game's approximation is removed. | No motor-to-motor blocking or CCD redesign. Other casts retain their existing bounded conservative advancement. |
| N3 / Void S1 | Full metadata copies repeatedly performed linear definition lookup for each entity. | Indexed entity lookup and maintained ordered script-owner membership. Genuine all-entity enumeration retains its semantics. Slot-data replacement reconstructs the instance through normal teardown/start. The unchanged 0/400/800/1600-box reproduction is measured with/without a script. | Other systems still perform linear whole-world work; mutable entity access currently invalidates the index each fixed step. This is not a claim of constant-cost simulation. |
| N3 / Skate F-01 | The imported head's free ball joint permits ideal spherical rolling/spinning; after damping that motion, the right foot can continue rocking about its free hinge. These are real velocities, not merely a broken sleep timer. Identical joint/pair-policy writes also needlessly woke bodies. | Identical writes now preserve equilibrium. **With explicit operator approval**, optional default-zero passive `rotationalResistance` was added to free ball/hinge axes; the copied rig authors 0.05 N·m·s/rad at its head and right foot only. Original undamped traces are retained. Actual 13-body/12-joint rigs are tested at 1/10/20 instances, impacts, continuously applied torque, cold reconstruction and removed support. | The unchanged idealized free rig is not claimed to sleep. Authored resistance is a disclosed physical content adaptation, not a forced-sleep timeout or rest-pose drive. |
| N4 harness | Harness omitted the real outer scene/save/stream boundary and captured a long run as one profiler frame. No-draw frames also skipped audio publication, preventing save prefill. | Shipping application and harness share `SceneSession::AdvanceOuter`. Each test/wait frame has its own M56 boundary. Bounded `WAIT_SERVICES`, scene assertions, paused captures and restored-input semantics use normal services. Audio updates precede the drawing-only guard. | Scripted clocks measure operations, not desktop FPS. `STEPS` remains the historical application-frame count. Workers progress in bounded wall-time waits. |
| Skate F-02 streaming units | Whole-scene identity allocation, relocation, hierarchy flattening and retained restoration ran in bulk main-thread units. | Per-object progress for allocation, remap, registration/restoration; immutable hierarchy preparation on existing workers. Staged content remains unavailable until the atomic publication commit. Cancellation/retirement retain normal handles and references. Measured out/back units stay under the unchanged 2 ms target. | Native navigation registration, coherent snapshot/script teardown and final publication remain indivisible. The budget is a dispatch target, not an OS latency guarantee; larger native assets/callbacks can exceed it. |

## Comparable performance

Release on this development machine. Matched resident tests use the same accepted park and explicitly request the same street IDs (one: street-0; four: street-0..3); only the temporary benchmark replaces the moving interest policy with those explicit requests. Reference protection is retained. Profiler enabled, last 120 ordinary hardware-rendered frames. No concurrent own build/benchmark during these comparisons. Host process observations are retained; unrelated processes were not stopped.

| Steady streaming outer integration | Before median / p95 / max ms | After median / p95 / max ms |
|---|---|---|
| One resident region | 26.637 / 42.281 / 50.611 | 0.142 / 0.210 / 0.229 |
| Four identical resident regions | 142.156 / 153.259 / 221.874 | 0.210 / 0.310 / 0.459 |

Four-region baseline reached the fixed-step catch-up cap in 119/120 retained frames; corrected run 0/120. Median whole-frame CPU intervals were 148.961→9.921 ms. One-region intervals were 29.630→9.945 ms. These are machine observations, not portable FPS guarantees. An earlier unconstrained four-region attempt loaded different region sets as the skater moved; it is preserved and excluded from the matched comparison.

Original 3,000-step inert-scenery fixture (wall seconds, profiling/rendering off):

| Render-only entities | No script before / after | One empty fixedUpdate before / after |
|---|---|---|
| 0 | 0.189 / 0.187 | 0.212 / 0.200 |
| 400 | 0.251 / 0.251 | 8.786 / 0.947 |
| 800 | 0.290 / 0.293 | 30.904 / 1.928 |
| 1600 | 0.457 / 0.482 | 138.100 / 4.809 |

One-step startup is recorded separately. In the corrected one-script workload, definition copies are 27,000 / 1,227,000 / 2,427,000 / 4,827,000; lookups 42,001 / 3,642,401 / 7,242,801 / 14,443,601. Each run has 3,001 index rebuilds because another fixed-step consumer requests mutable entity access. Structural work no longer approaches four times when inert scenery doubles. No retrospective claim is made about unrecorded competing host activity in the early S1 runs.

Streaming unit trace: seven publications, 69 object registrations/relocations, 29 retained restorations/removals, three retirements. Largest unit 0.584 ms (retirement commit); component registration max 0.041 ms; snapshot/script teardown max 0.408 ms; atomic publication max 0.055 ms. No named unit exceeded 2 ms in that actual-hardware out/back run. The aggregate outer scope and complete per-unit median/p95/max/overrun records are linked in the evidence index.

Imported-rig final fixed-step checks (same 13 bodies / 12 joints per instance, 60 Hz, explicitly authored resistance):

| Instances | Mapped asleep | Sleep observed by | First 30 s average fixed cost | Quiet fixed cost |
|---|---|---|---|---|
| 1 | 13/13 | 9 s | 0.098 ms | 0.026 ms |
| 10 | 130/130 | 13 s | 6.778 ms | 0.490 ms |
| 20 | 260/260 | 30 s | 18.142 ms | 1.170 ms |

Sleep latency is sampled once each simulated second. All three impact/resettle cycles pass at each population, including a sustained-torque control that remains awake, cold reconstruction and removed support. These are operation costs, not renderer FPS; the 20-rig initial settling workload remains considerably more expensive than quiet islands.

Ordinary three-game hardware-rendered observer runs use the actual Application wall-clock accumulator (hidden test window, dummy audio device/offline mixing). Last 120 retained frames:

| Game | Frame CPU median / p95 ms | Fixed step median / p95 / max ms | Bodies / voices | Catch-up caps |
|---|---|---|---|---|
| Skate | 2.497 / 3.364 | 0.534 / 0.652 / 0.714 | 70 / 7 | 0 |
| Rooftop | 2.277 / 2.925 | 0.387 / 0.548 / 0.601 | 67 / 4 | 0 |
| Void | 4.027 / 5.259 | 0.910 / 1.136 / 1.176 | 176 / 9 | 0 |

Recorded frame maxima are 119.542 / 60.450 / 60.921 ms, all in the final screenshot/profile-export frame (601). Those captures are retained, not hidden or labelled fixed-step stalls. Ordinary inter-frame interval maxima are 4.905 / 3.845 / 6.440 ms. Full statistics and scope accounting are in `ordinary-game-performance.json`; these runs are not hardware/audio/visual acceptance.

## Behaviour and compatibility

- No gameplay rules moved into C++. No accepted liquid algorithms or cadence changed.
- `rotationalResistance` is optional, finite, nonnegative and zero by default. It opposes relative angular velocity on a ball joint's three axes or a hinge's free axis, through ordinary implicit solver rows; it neither drives a rest pose nor grants sleep.
- Scene/prefab/editor parsing and joint JS configuration expose the same coefficient. Default-zero authored fingerprints retain existing canonical bytes. Nonzero values add explicit fingerprint extensions; the global schema is unchanged. Articulation save chunks use version 2 only when resistance is present and still read version 1.
- Cached script references come from actual declared property schema, including defaults; arbitrary strings/numbers are not guessed to be references. Replaced slot properties create a fresh instance rather than retaining stale metadata.
- Typed external references continue to pin their current owner. Adoption changes that owner; removed/replaced references release pins. Region tombstones remain tombstones, not newly reused identities.
- Streaming publication is atomic. Worker preparation receives immutable data; live component mutations remain on the runtime thread. Retained entities/scripts are restored while private before becoming observable.
- Existing D-01/D-02/D-03 joint/localization corrections remain intact. Unpublished localization catalogs still return visible fallback instead of throwing.
- The Void game-side bolt approximation is removed only from this current consumer copy. Its original evidence and Claude source remain untouched.

## Verification

Implementation/tooling/consumer bytes were frozen in `docs/evidence/post_m65_repairs/candidate-freeze.json` before the single clean gate. **Runtime source stayed unchanged throughout and after it.** Clean Release: zero warnings. Async: 12 cases / 246 checks. First production invocation: 136/139 targets passed; the original overall failure is retained. Three narrow follow-ups provide complete passing coverage without repeating the suite:

- Consumer authoring invocation omitted its project/output arguments: corrected invocation passes 35 checks.
- Save-storage invocation did not use its required isolated `m61-storage*` directory prefix: corrected invocation passes 28 checks.
- The isolated audio-world fixture advanced streaming/audio but never initialized the region scripts. Conservative unknown-schema pins correctly prevented retirement. Its frame now also advances the normal script lifecycle: rebuilt target passes 65 checks. This is a test-lifecycle correction, not a runtime pin bypass.

`final-coverage.json` records those exact post-gate test/driver byte changes and which affected target was rerun. No result is overwritten or described as a pristine first-pass full-suite success. Capsule: 40 checks; metadata/reference churn: 32; actual imported rig: 93; cookbook: 244; TypeScript/source/type/reference/live VM coverage: 289 public symbols / 27 exports, three negative drift controls.

Final consumer checks pass: Rooftop course checkpoint 4 / completed / zero falls; Skate actual IK (~3.5e-6 m error), socket (~9.1e-8 m) and runtime joints/material observations; Void real capsule damage, boarding/fire, Ruby core acquired and stage 3 ambush begun. The existing mission autopilot is unchanged. Actual out/back residency, authored-reference pin/release/adoption, ordinary save/load, fresh-process Skate save (score 314 restored), editor Play/Stop, repeated paused switches and reload all pass. Launcher teardown returns bodies/voices to 0/0; revisited Skate returns to its prior 70-body/7-voice steady state.

Export contains nine registered scenes and 129 assets, 70,328,724 bytes. Package: `.cache/post-m65-repairs-package/JudasThreeGames`; physically moved copy: `/tmp/JudasThreeGames-post-M65-repairs`. Actual shipped executable starts all four selections and pause panels from `/tmp`; a temporary observer beside it proves ordinary scene switching/reload and is then removed. Package runtime hash matches the final build. See the [evidence index](evidence/post_m65_repairs/README.md) for outputs, original failures, follow-ups and exact fingerprints.

## Project and review controls

Project: `projects/post_m65_consumers/post_m65_consumers.judasproj`.
F1 Skate; F2 Rooftop Run; F3 Void Courier; F4 launcher. These are ordinary registered scenes, including switches from their pause panels. Each game's existing controls/gameplay are retained; see [consumer controls](POST_M65_CONSUMER_REVIEW.md).

Human review:
1. Skate: travel down the streets and back; save/load; bail, let the rig settle and disturb it again.
2. Rooftop: slopes, wall run, every checkpoint, pause and complete the course.
3. Void: board/unboard, fire, receive on-foot capsule hits and inspect local gravity.
4. Switch repeatedly between all games while running and paused; reload and inspect captures/profiler.
5. Repeat in the moved standalone package.

Known consumer limitations from the accepted review remain, including straight-line Void autopilot, importer/content restrictions, partial IK and unsupported composed disk saves. None are reclassified as solved by these corrections.

Proposed commit after human acceptance: `Fix post-M65 consumer regressions and scalability`. No commit, push, tag or M66 work is authorized in this pass.
