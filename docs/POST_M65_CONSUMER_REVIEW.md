# Post-M65 real-game consumer review

**Historical accepted review/repair record:** the original observations, findings
and pending-review statements below retain their recorded scope. Later engine
changes can supersede them; use [Architecture](ARCHITECTURE.md) for current status.

## Scope and result

Reviewed accepted engine `51c18c249432e09910dabc41e670bf960db8050c`.
**No runtime implementation, solver, binding or public API was changed.**
Only ordinary consumer content and separate review tools were added.
No M66 work, commit, push or tag. Original consumer workspaces/processes were
not edited or terminated. Protected historical engine evidence remains intact.

The deliverable is [Judas Three Games](../projects/post_m65_consumers/README.md),
an ordinary registered project with a menu, three independent scene experiences,
and Skate's five streamed street regions. It uses the accepted Release engine.
The nine-scene export was copied outside the repository and exercised from `/tmp`.
Both repository project and moved package are ready for **human review**; automated
screenshots, PCM output and intent checks do not certify human feel or listening.

**M65 makes real improvements:** Rooftop incline support and checkpoint events;
Skate resolved-joint lift, usable imported-rig IK/sockets, material identity and
runtime joints; Void's ordinary-script gravity queries; logical harness input,
script-view/paused capture, entity-reference authoring and batch operations.

**It does not resolve all consumer problems.** Two actual M65 regressions were
found: capsule query targets use the wrong primitive intersection, and streamed
resident pin checks repeatedly construct complete JS VMs. The latter substantially
slows Skate. Existing inert-entity scaling, imported-rig sleeping, content tooling
and camera/UI restrictions remain. Nothing was silently redesigned to hide these.

## Sources and port boundary

The immutable comparison inputs are copied under
[evidence/post_m65_consumers](evidence/post_m65_consumers/README.md):

- Skate `SKATE_GAME_ENGINE_AUDIT.md` (original frozen engine) plus its latest-main
  M61/PR #1/M64 follow-up. Port source: `ClaudeJudasSkateGame/Judas/projects/skate_game`.
- Rooftop original engine experiment report and supplied `RooftopRun_Package`.
- Void original engine experiment report and supplied `VoidCourier_Package`.

The latter two began on the pre-M65 engine. Skate's supplied source already has
M64 smooth collision and M61 saves. Those improvements are **not credited to M65**.
[Import provenance](evidence/post_m65_consumers/import-provenance.json) contains
original file hashes, source paths and deterministic asset-ID remaps.

### Coexistence changes

- All assets live under `Assets/skate`, `Assets/rooftop`, `Assets/void`; asset IDs
  and references are deterministically remapped, binary content retained.
- Logical action names are prefixed per game. Their physical bindings are
  unchanged. A small public `input` facade forwards to Judas; shared UI controls
  use the union of the original bindings.
- One JS collection component performs `scenes.load` at the ordinary outer
  boundary: **F1 Skate, F2 Rooftop, F3 Void, F4 menu**, including while paused.
  All games use `legacy-gameplay false`; each retains its script-owned camera.
- Skate's audio groups, classification registry, English/Spanish catalogs and
  world manifest are included. Other scenes do not set Skate's spatial interest;
  package switch observations show its regions unloaded in those experiences.
- Export selects the launcher, three gameplay roots and five street scenes.
  Skate lab scenes remain in the developer project but not the package. Existing
  export policy still includes registered runtime assets, including small lab
  script assets; no build tree, original executables or audit logs are exported.
- Scene replacement resets an unfinished run. Explicit Skate save slots and
  session locale/best values retain their normal lifetimes. No new save framework.

### Small game-side uses of M65

**Skate:** lower resolved toe height replaces per-clip/crossfade lift guesses;
board/rider visual references are typed authored entity properties. Ground/grind
acceleration samples `physics.gravity`. Ground probing/board rotation, offline
foot clips, velocity-snap grinding, bail damping, six sound helpers and mesh
batching remain: replacing them would change gameplay or lacks a better proof.
The real imported Mixamo chain was separately exercised with limb IK and a hand
socket; this does not claim IK alone supplies skateboard balance/foot orientation.

**Rooftop:** five ordinary authored sensors and JS callbacks replace checkpoint
AABB polling. Incline tangent projection now covers uphill and downhill; the
old uphill restriction is removed. Jump/coyote, wall-run, slide, mantle, roll,
timer and hints remain the original game. Seven short geometry queries ignore
self so the new query-only motor body cannot interfere with wall-run detection.

**Void:** ship/raider helpers sample `physics.gravity` instead of duplicating
scene radial fields. A trial removal of the pilot bolt approximation failed on
actual game geometry. It was restored, with a factual comment, because M65's
capsule query result is worse. Existing ray hits still drive body impulses/damage.
Raiders, debris initialization, compact planets, markers and mission rules remain
original project JS. No new AI/navigation work.

## Original findings: complete comparison

`FIXED` means exercised behavior, with attribution below. `IMPROVED` means useful
but deficient. `OBSOLETE` means the old premise is superseded; it does not assign
credit to M65. `NOT EXERCISED` explicitly limits a claim, including human widgets
or controller feel. Source-confirmed limitations are distinguished from tests.

### Skate — defects and missing primitives

| Original | Classification | Current evidence / remaining boundary |
|---|---|---|
| D-01 long-travel slider divergence | OBSOLETE | PR #1 predates M65. Current long-travel joint regressions pass. Actual board can create/reanchor a slider; normal grinds still use snapping, so constraint-grind gameplay is **not** newly certified. |
| D-02 violated-limit elastic rebound | OBSOLETE | PR #1 correction, current joint regressions. No change to Skate's already authored grind rules. |
| D-03 unpublished catalog reported as missing/throwing | OBSOLETE | PR #1 correction. Actual English/Spanish startup/save scenario has no missing-key noise; no attribution to M65. |
| E-01 resolved joint/socket access | FIXED by M65 | Real rider toe transforms now drive deck lift; actual hand socket error below 0.000001 m in the probe. Not a clip-name approximation. |
| E-02 runtime IK | IMPROVED but still deficient | Actual LeftUpLeg/LeftLeg/LeftFoot target error 0.00000352 m. Two-bone positional contribution works; foot orientation/balance/board control are not automatic. Offline clips remain. |
| E-03 box-only static collision | OBSOLETE | M64 cooks already in supplied source; current imported ramps load and course geometry works. This port does not recreate box chains or credit their prior removal to M65. |
| E-04 collider introspection/closest point | OBSOLETE | M64 API and latest Skate rail introspection retained; normal streamed rail cache is rebuilt as regions publish. No new grind-path framework. |
| E-05 no game persistence | OBSOLETE | M61 slot save/load works in this actual streamed Skate project; score/time, position/velocity and rail references restore. A general profile/settings-store wishlist is not solved by that observation. |
| E-06 runtime materials/hit identity | FIXED by M65 | Actual board switched to a test physical material, read back friction/restitution, and a board ray returned the same material ID; restored afterward in the isolated fixture. Existing bail damping retained. |
| E-07 runtime joint creation/reanchor | FIXED by M65 | Actual board slider created/configured/destroyed through public JS; validity retired correctly. Anchors are owner-local at the public boundary. Does not prove a redesigned physical grind is better. |
| E-08 size-aware primitive UVs | NOT EXERCISED | M65 instance UV overrides/source and focused serializer checks exist. Actual Skate keeps its metric-UV batch meshes for draw efficiency; no false claim that this fixes its scene or replaces batching. |
| E-09 non-motor gravity sampling | FIXED by M65 | Ground/grind math now samples the actual field. Void additionally checks public sampling against its motor's acceleration. Skate's deliberately flat `UP` and energy telemetry are content assumptions. |

### Skate — authoring, documentation and performance

| Original | Classification | Current evidence / remaining boundary |
|---|---|---|
| C-01 curved/mesh collider authoring | IMPROVED but still deficient | M64 static cooks retained; M65 batch operations exercised on actual scenes. No arc/profile generator, array along path or artist-topology repair workflow was added. |
| C-02 single selection/search/component paste | IMPROVED but still deficient | Actual scene multi-select, batch edit/undo, body copy/paste and duplicate pass. Search/filter UI exists in source; folders and human widget ergonomics not certified here. |
| C-03 typed ragdoll mapping/no skeleton picker | IMPROVED but still deficient | Real imported joint names usable; current picker and axes are source-confirmed. Human picker interaction is NOT EXERCISED. Manual mapping remains, no auto-fit/humanoid generator. |
| C-04 import preprocessing/FBX/48 joints/single mesh | UNCHANGED | Same reduced 22-joint GLB used. Current source still requires glTF preparation; no FBX, reduction, unit correction, clip-merge or root-motion tool added. |
| C-05 one sound emitter / ambiguous failure | IMPROVED but still deficient | Six game helpers retained. M65 group/clip/settings/capacity diagnostics pass focused checks. Actual voices produce nonzero mixed PCM; no multi-emitter component or listening certification. |
| C-06 positional UI format | IMPROVED but still deficient | Collection menu built through named author JSON and normal UI serializer. Existing game layouts stay untouched. Runtime wire format remains positional; no structural merge/editor rewrite. |
| C-07 unwanted lab scene export | FIXED by M65 | Actual selected export has exactly nine scenes; source Skate labs excluded. Registered-asset inclusion policy remains broader than scene dependency closure. |
| C-08 paused/multiple screenshots | FIXED by M65 | Shipped executable captures each paused game; ordinary loop and harness respect script views. Separate harness outer-boundary/profiler gaps remain below. |
| C-09 typed entity references | IMPROVED but still deficient | Board/deck/rider references authored; actual batch duplication remaps both visual references. **New resident pin-check overhead regresses streaming**, see N2. Semantic tags remain appropriate for rails/sounds. |
| B-01 uphill departure wording | FIXED by M65 | Actual Rooftop uphill support and full course, not just API availability. Skate's selected rigid-board controller remains intentionally different. |
| B-02 real-rig joint-frame guidance | IMPROVED but still deficient | Current axis view/docs and real skeleton identity available. Imported rig mapping preserved; frame fitting still manual, widget usability awaits human check. |
| B-03 slider stability envelope | OBSOLETE | PR #1 removes separation-dependent failure; current long-slider tests pass. Do not infer unlimited constraint error tolerance. |
| B-04 primitive UV semantics | IMPROVED but still deficient | Current reference covers normalized source UVs and per-instance overrides. Actual batched Skate content unchanged; direct visual UV-authoring comparison not exercised. |
| B-05 audio group diagnostics | FIXED by M65 | Current docs + focused precise group failure, actual registered `sfx/music` run without script fault. |
| B-06 body-free rider entry momentum | UNCHANGED | Current bail logs show inherited hip motion, but still approximate recent observed presentation motion; explicit JS transfer retained. No continuous rigid-body ownership invented. |
| F-01 resting articulated cost/sleeping | IMPROVED but still deficient | M65 normal sleep/wake tests pass, ordinary game has sleeping bodies; **20 actual humanoid rigs remain awake after 30 simulated seconds**. Genuine impulse response works, but asleep-to-awake transition of these rigs cannot be claimed. |
| F-02 indivisible stream install spikes | UNCHANGED | Actual current units reach 14.27 ms, exceeding the 2 ms dispatch budget; separate much larger steady resident overhead is REGRESSED (N2). |
| G-01 48 joints/one mesh import | UNCHANGED | Actual optimized GLB runs; original limit remains. |
| G-02 fixed 60 Hz | UNCHANGED | Normal 1/60 simulation, no cadence reduction or fake quality policy. |
| G-03 composed worlds disable saves | OBSOLETE | Legacy `.judasstate` restriction remains; M61 slots work in composed Skate. Startup wording still misleadingly says disk saves disabled. |
| G-04 cloth/fracture unavailable at frozen baseline | OBSOLETE | M62/M63 exist, but adding those features to these games would be new gameplay. NOT EXERCISED here. |
| G-05 gamepad/deadzones/rumble | NOT EXERCISED | Original logical controller bindings preserved. No hardware-feel claim; existing input limits remain. |
| G-06 motor inappropriate for skating chassis | NOT RELEVANT | Actual game retains its rigid board. Improved walking support does not turn a walking capsule into board dynamics. |
| G-07 root motion absent | UNCHANGED | In-place content retained; no root-motion gameplay added. |
| M64 follow-up: artist half-pipe topology rejection | UNCHANGED | Accepted generator cooks load; historic winding/T-junction/nonmanifold failures retained. No location-rich diagnostics or repair pipeline; artist source was not recooked here. |
| M64 follow-up: save-busy rule | UNCHANGED | Existing one-operation save service and game exception handling retained; normal queued save/load completed. |
| M64 follow-up: ambiguous startup disk-save wording | UNCHANGED | Reproduced in current normal startup despite successful M61 slot load. |

### Rooftop Run

| Original | Classification | Current evidence / remaining boundary |
|---|---|---|
| E1 harness ignores script camera | FIXED by M65 | Actual Rooftop and moved executable captures show original runner view/UI, not the legacy default player camera. |
| E2 harness cannot drive script input | FIXED by M65 | M35 physical W/Space drive real runner wall-run/ramp scenarios; built-in ACTION input/pause checks run on exported content. Original autopilot remains optional for the full-course oracle. |
| E3 uphill support loss | FIXED by M65 | 68 positively rising supported observations on original ramp; old downhill-only tangent restriction removed. Full course 23.7167 s, zero falls. |
| E4 edge support normal ambiguity | IMPROVED but still deficient | Correctly documented contact normal, not guaranteed roof-face normal. Existing short downward query for face geometry retained. No normal fabricated from world-up. |
| E5 motor collision/trigger events | FIXED by M65 | Actual authored checkpoint sensors fire including initial overlap; full course reaches all five sensor stages in order. Actual runner probe receives two enters, 106 stays, two exits and a valid other-entity handle. |
| E6 no background color | NOT EXERCISED | Original procedural environment retained, so no game-specific improvement claimed. Collection launcher exercises generic clear background; M65 capability source confirmed. |
| E7 positional scene/UI hand authoring | IMPROVED but still deficient | Named JSON collection UI + current author tool; actual scenes load/save normally. No broad rewrite of the original map/UI. |
| E8 window title / debug overlay / paused capture | FIXED by M65 | Normal title uses combined project name, original game disables debug overlay, shipped pause capture succeeds. Hardware capture feel still human-owned. |
| Existing short wall-ray gaps / scripted mantle / no animated hands | UNCHANGED | Original game semantics kept; actual wall-run now works with ignored-self queries. No new controller or animation feature. |
| Existing session-only best / tuning properties | UNCHANGED | Original session best and authored sensitivity remain; no campaign/save framework added. |

### Void Courier

| Original | Classification | Current evidence / remaining boundary |
|---|---|---|
| S1 quadratic inert-entity scripted-world cost | UNCHANGED | Exact original generator retained; current 3000-step curve still near-quadratic. No optimization performed. |
| S2 fixed 500 m script-camera range | UNCHANGED | Current projection still `.1f,500.f`; small planets, following star shell and clamped markers retained. |
| S3 imported lexical syntax error lacks filename/line | OBSOLETE | Exact duplicate lexical-declaration repro currently includes `Assets/void/scripts/autopilot.js:114:36`. Existing exception-stack path predates M65; do not attribute this observation to a new M65 fix. |
| S4 gravity inaccessible to ordinary scripts | FIXED by M65 | Actual helper replaced by `physics.gravity`; public sample and motor acceleration match component-for-component; ship hover and raider logic use same helper. |
| S5 environment star resolution | UNCHANGED | Environment maximum width still 512; original nebula/geometry-star workaround retained. |
| S6 no runtime UI positioning | UNCHANGED | Public UI has no layout-position setter; original world markers remain. |
| S7 motor invisible to queries | IMPROVED but still deficient | Broadphase now registers capsule, but narrow cast uses zero-size box; real pilot misses. **M65 regression N1** prevents removing old bolt approximation. Restored approximation delivers 100→93 suit damage. |
| S8 no prefab initial data | UNCHANGED | Original pending-debris map remains. Actual mission kills spawn independent debris/raiders; no constructor-data convenience API added. |
| S9 fracture offline asset workflow | UNCHANGED | Existing ordinary debris retained, no new fracture cooks/game feature. |
| S10 logical input / camera / multi paused captures / title | IMPROVED but still deficient | Actual keyboard boarding/fire/view, shipped pause/camera captures and project title work. Harness outer scene/service boundaries and M56 scripted capture framing remain deficient (N4). |
| Existing no EVA / compact planets / simple raider AI / star occlusion | UNCHANGED | Original restrictions retained. No navigation/pathfinding/AI implementation added. |
| Original developer's corrected game bugs | NOT RELEVANT | Their final supplied package already includes balance, spawn shield and other game fixes. No new engine credit assigned. |

## New/reconfirmed engine findings and reproductions

### N1 — M65 query-only CharacterMotor capsule is intersected as a zero-size box

**Actual game:** ray from pilot position + local up × 1.2 toward the pilot misses
it and hits box 226 at 2.11812 m. Public gravity equals the motor's acceleration.
Removing the old damage approximation leaves suit at 100. Retaining it gives 93.
See `void-query*` and `void-query-retained-workaround`.

**Independent minimal repro:** `build/judas_consumer_capsule_repro`:

| Query, capsule radius .3 / half-height .6 | Expected | Actual |
|---|---|---|
| (-2,0,0), +X | 1.7 m | 2 m |
| (-2,.4,0), +X | 1.7 m | miss |
| (0,2,0), -Y | 1.1 m | 2 m |
| interior (.2,.4,0), +X | initial overlap | miss |

`WorldCharacter.cpp` registers `Shape::Capsule`; `CastAgainstPrimitive` in
`PhysicsCastGeometry.cpp` lacks a target-capsule branch, so it falls into the box
path with zero `halfExtents`. Broadphase bounds do not fix wrong narrow geometry.
This is a current regression, not a reason to declare the newly exposed query
body correct. Reproduction executable reports defects and returns success for
successful observation; it is **not a passing geometry-fix test**. No repair made.

### N2 — M65 typed-reference protection adds large resident streaming overhead

Current `WorldStreaming::Impl::pins` runs for every active region every frame.
For every external script slot it calls `ScriptSystem::PropertyEntities`.
That helper constructs a full `Impl` (QuickJS runtime/context + Judas module)
just to parse property JSON, including `{}`. The dependency scan is added in the
M65 commit; the previous accepted source has no such per-frame call.

This occurs after all resources are ready, without any registration occurring.
`Streaming outer integration` measures it directly. The normal flat Skate game
(one active region, 20 script instances) is visibly much slower; four active
regions multiply the cost. A minimal helper measurement and source/history trace
are preserved. No speculative solver optimization or region-size workaround.
See current profiler table below and `reference-vm-cost-serial.log`.

### N3 — Void's inert-entity scaling remains; imported ragdolls remain non-quiescent

`scaling/original-reproduction/make.py` is the original, unmodified test:
N render-only boxes, optionally one empty `fixedUpdate`, no physics/draw passes,
3000 steps. Current measured elapsed seconds (startup/profiler/teardown included):

| Boxes | No script | One empty script | Original report, one script |
|---:|---:|---:|---:|
| 0 | 0.2409 | 0.2426 | 0.24 |
| 400 | 0.2475 | 10.2483 | 5.09 |
| 800 | 0.3122 | 36.3595 | 18.68 |

400→800 remains approximately 3.55× in the scripted current test. Original timing
had different capture conditions; **no precise before/after regression factor is
claimed**. `RuntimeWorld::ScriptObjects()` copies the full entity set and uses
linear `RuntimeDefinition` lookup for each, repeatedly through synchronization
phases. Reproduction retained; no fix.

Actual Skate test: 20 shared imported 13-body rigs, 240 M45 constraints, separate
flat floor, normal gravity/filtering, 30 simulated seconds. 331 total bodies,
265 dynamic, only four sleeping (same ordinary non-articulation sleeping count).
Thus the 260 mapped ragdoll bodies did not settle into sleep. An impulse applied
through `ragdoll.body('mixamorig:Hips')` produces motion but that rig was already
awake. Generic sleep/wake tests pass; these observations do not establish the
numerical reason these real contact/joint islands fail quiescence. No sleep
threshold, cadence, fidelity, body pose or solver correction was changed.

### N4 — Harness service boundaries and profiler framing remain incomplete

`JUDAS_TEST_SCRIPT` executes `play.Frame` directly, without normal outer
`SceneSession::Apply`/save/stream service advancement. The collection F2 request
stays on the menu in that harness. **Ordinary Application switching works**, also
in the moved package. This gap was noted in M65 follow-up; it is not proof normal
scene transitions broke.

The 3000-step S1 M56 export has one frame, nested-frame reporting and dropped
scope events. Wall elapsed measurements remain valid; do not divide that giant
single-frame profile into claimed per-step costs. `ConsumerReview.cpp` uses the
ordinary application loop with actual M56 boundaries for game measurements,
exports/captures on its own after-frame observation hook, and changes no game
logic. It mixes audio offline solely to verify nonzero PCM; listening is human.

## Functional checks and lifecycle

- Review tools built Release; existing runtime hash unchanged. API/type drift
  check: **289 public symbols, 27 exports, 195 native operations, 15 callbacks,
  29 examples, three negative controls; TypeScript 5.9.3 passes**. That run did
  not repeat live runtime enumeration; no new binding was introduced.
- Actual consumer authoring: **35 checks, zero failures**. Multi-edit/undo,
  component paste, duplication and typed visual-reference remapping.
- Existing M65 focused integration: **62 checks, zero failures**, including
  support/events, arbitrary gravity, sockets/IK, materials and sleep/wake.
  Existing joint/authoring regressions also run; logs are linked in evidence.
- Rooftop actual input wall-run works. Actual ramp shows positive rising support.
  First sensor overlap works; actual motor collision enter/stay/exit callbacks also work. Original full-course autopilot completes in
  **23.7167 s, zero falls**, all checkpoints and finish. Original 23.78 s is a
  useful behavioral comparison, not an engine-performance improvement claim.
- Skate actual push/ollie/flip/bail/respawn runs without JS errors. Real IK,
  socket, physical material and runtime slider fixture succeeds on imported
  game bodies. Normal M61 quick save/load restores position, velocity, timer
  and rails; Spanish locale remains functional.
- Skate regions 1–3 unload on return to the park and reconstruct when revisited.
  The focused revisit fixture explicitly relocates the normal board twice to
  isolate lifecycle; it is not claimed as a physical full-street speed run.
  Individual real-input skating and the original manifest policy are separate.
- Void real input boards, thrusts, fires **19 bolts**, switches cockpit and
  receives ordinary damage; nonzero PCM. Original mission autopilot kills
  three raiders, exits on Ruby but gets stuck before the core (same category as
  its original straight-line-walker limitation). **No full mission completion
  claim.** Original `ruby_foot` scenario collects core at 8.5 s, reaches stage 3
  and spawns the ambush. Final pilot damage workaround independently passes.
- Each actual editor opens the copied scene, runs Play, stops and reports
  **IDENTICAL authored state**, successful isolated save, zero failed resources.
  Human multi-selection/pickers/capture feel still need operator review.
- Final standalone export: **129 assets, nine scenes, 70,303,467 bytes (~67.05 MiB)**.
  Shipped `judas` starts menu and each game from unrelated `/tmp`, with paused
  screenshots. Ordinary application observer placed alongside package marker
  verifies F1/F2/F3/F4, switching while paused, reentering Skate and reload; final
  menu has **zero bodies**, no script faults. Observer then removed from package.
- Save/load is exercised where already used (Skate). Rooftop's best and Void's
  state retain original session/game lifetimes; disk saves/streaming are not
  invented for games that never used them.

No full production rerun: this review changes no runtime code. Genuine failures
and wrong initial review fixtures are preserved separately with corrected
follow-ups. Human visual/audio/controller acceptance remains pending.

## Representative M56 performance

Sequential final real-clock runs, Release, desktop X11 graphics driver, vsync
disabled by observer; last 120 frames with first 10 discarded. Named scopes are
inclusive; they overlap. Outer intervals include the observation hook's 1 ms
sleep and offline audio mixing. Do not add script scope time to Play time or
interpret these as guaranteed production FPS. Earlier concurrent-run captures
are preserved separately. The native observer changes no gameplay quality.

| Current game | Frame interval median / p95 / max ms | Approx. uncapped observer rate | Play CPU median / p95 ms | Fixed-step median / p95 ms | JS phases median / p95 ms per frame | Bodies / draws at tail |
|---|---:|---:|---:|---:|---:|---|
| Skate, normal park + resident street-0 | 31.41 / 37.86 / 40.54 | 32 FPS | 2.91 / 3.98 | 0.684 / 0.970 | 1.38 / 1.93 | 70 / 30 |
| Rooftop, normal runner | 3.20 / 4.40 / 9.05 | 313 FPS | 1.84 / 2.97 | 0.715 / 1.009 | 0.469 / 0.771 | 67 / ~97 |
| Void, boarding/flight with raiders | 9.22 / 11.56 / 13.91 | 108 FPS | 7.92 / 10.20 | 2.580 / 3.461 | 2.36 / 3.26 | ~173–176 / ~190 |

No fixed-step cap hit in those tails. **Skate is substantially slower than its
Play CPU scope suggests:** steady `Streaming outer integration` is **27.04 ms
median / 32.91 ms p95**. With four actual regions resident in the revisit test it
is **152.55 / 166.65 ms**, outer interval **156.73 / 171.76 ms**. This is not a
one-off loading stall. That controlled-clock run cannot measure real-clock
catch-up; it does show an interactive-performance problem. Empty-property
reference extraction alone costs **1.590 ms/call** over 200 calls; a typed
reference costs **1.440 ms/call**, demonstrating that JSON size is not the cause.

20 actual humanoid ragdolls, normal 60 Hz controlled clock: fixed **8.95 / 10.48 ms**,
rigid physics **4.44 / 5.54 ms per frame**, total Play **13.79 / 16.62 ms**.
The resident pin scan costs another **57.02 / 63.86 ms** with these script slots.
Only four non-ragdoll bodies sleep. This is a smaller fixed-step number than the
original frozen 42.269 ms median, but the original capture entered catch-up and
had different loaded geometry/population; no percentage improvement or real-time
crowd-capacity claim is justified. Current source has sleep capability, yet it
has not solved this imported articulation workload.

Original Skate ordinary evidence used 156 bodies versus the already-M64 current
70-body park. Its ~0.856 ms fixed median is not a clean M65 A/B. Rooftop's course
time is a behavior oracle; original report lacks an equivalent profiler table.
Void S1 is reproduced at the same counts/steps; profiling/capture differences are
explicit above. [Full measurements](evidence/post_m65_consumers/profile-summary.json)
retain named scopes, counters, sample sizes and diagnostics.

## Ranked remaining generic problems

1. **Resident streaming dependency-scan regression (N2).** VM construction every
   external script slot, every region, every frame dominates an actual game.
   Strongest new evidence; should be addressed without bypassing reference safety.
2. **Wrong capsule query geometry (N1).** Registration is not useful if nearest
   hits/misses/initial overlaps are geometrically wrong. Prevents removing a real
   game workaround and affects any consumer casting against motors.
3. **Repeated full-world script metadata/linear lookup (Void S1).** Inert visuals
   make one no-op script much slower; preserve small standalone repro, fix generically
   in a later authorized task rather than optimizing game rules.
4. **Imported articulated quiescence/solver cost.** Generic sleeping succeeds in
   simpler tests, while the actual rider islands do not sleep. Investigate actual
   contact/constraint equilibrium before inventing a distance-quality workaround.
5. **Streaming install granularity and harness service boundaries.** Atomic units
   still exceed budgets; scripted test mode cannot prove ordinary scene/save/stream
   integration. M56 frame boundaries also need to represent harness workloads honestly.
6. **Content/developer expressiveness.** Imported-rig preparation, cooker edge
   diagnostics, procedural authoring, runtime camera range/UI position, prefab
   initial data and real skeleton-picker ergonomics remain useful generic gaps.

These are recommendations, not implementations or assigned future milestones.

## Human review checklist and controls

Launch `/tmp/JudasThreeGames-post-M65/judas` (or the cache export). F1/F2/F3 select
Skate/Rooftop/Void; F4 returns to menu, including from pause. Selecting another
game starts a fresh run. [Full controls](../projects/post_m65_consumers/README.md).

### Skate

1. W/S push/brake, A/D steer; Space ollie, J flip, K grab/manual, L grind, Q/E spin.
   Test the smooth ramps and inspect rider/deck alignment; X bail and recover.
2. Follow the streamed street, return toward the park and revisit it. **Expect
   the recorded resident slowdown**; this review intentionally does not hide it.
3. C camera distance, mouse look; Escape pause/resume and confirm pointer behavior.
4. Save/load a run from the pause menu, change to Spanish, verify score/time and
   physical state restore. Switch away/back and load the explicit Skate slot.
5. Inspect editor batch edits/copy/paste, typed deck/rider references and actual
   skeleton pickers/axis view. Human ergonomics and audio mix remain yours to judge.

### Rooftop Run

1. WASD/mouse, Space for jump/vault/climb/wall-run; build speed before wall-running.
   Ctrl/C slide/roll. Test rising ramp support and edge release separately.
2. Reach checkpoints, fall and use R; complete the finish. Watch checkpoint banners
   once per crossing; checkpoints are normal sensor callbacks.
3. Escape pause/resume, Backspace restart; confirm script camera/look/capture.
4. Verify sound, timing/feel and controller bindings if hardware available.

### Void Courier

1. WASD on foot around Terra, Space jump, Shift run; F board nearby ship.
2. WASD flight, Space/Ctrl ascend/descend, Q/E roll, Shift boost; X assist,
   V cockpit/chase; left mouse fire. Verify impact/particles/audio/body response.
3. Clear first wave, land Ruby, exit when landed, collect core, fight ambush,
   return home. Original test autopilot is not a guarantee a human route is blocked.
4. Check on-foot enemy damage (retained approximation), pause and Backspace restart.
   Inspect camera-range/star/marker limitations without claiming they were removed.
5. Confirm same behavior after switching from the other games and in the moved
   standalone. Controller feel/listening and a full human mission remain pending.

## Handoff and reproduction

Project: `projects/post_m65_consumers/post_m65_consumers.judasproj`.
Export: `.cache/post-m65-package/JudasThreeGames` (generated, untracked/ignored).
Moved package: `/tmp/JudasThreeGames-post-M65` (no repository/build dependency).
Evidence: `docs/evidence/post_m65_consumers`, with failures and exact fixtures.

Review tools: `judas_consumer_review`, `judas_consumer_authoring_tests`,
`judas_consumer_capsule_repro`, `judas_consumer_reference_repro`.
Build them with the normal Release CMake build. Script entry points:
`post_m65_review.py <case>`, `post_m65_scaling.py`,
`post_m65_package_checks.py`, `post_m65_results.py`.
Evidence-producing scripts intentionally refuse existing result directories;
use `JUDAS_REVIEW_RUN_NAME` for a distinct focused case. The initial port script
reproduces the import, not the later explicit game-side patches listed above.
Read test fixtures before interpreting a result as ordinary live gameplay.

[Exact changed-file inventory](evidence/post_m65_consumers/changed-files.txt),
[final fingerprints](evidence/post_m65_consumers/final-source-sha256.json),
[protection/HEAD verification](evidence/post_m65_consumers/protection.json).
Runtime binary remains the accepted build; no source/runtime semantics changed.
Future checkpoint subject, only if the operator later requests it:
`Review post-M65 consumer games and add combined project`.

### External consumer-source drift

Final provenance recheck: **263 of 264 original files still match**. The supplied
Void package's `Assets/scripts/ship.js` changed after import (mtime 7 October
00:00:26 local): cannon convergence now uses a nearer/raider-aware target point.
This task did not write that external file. The port retains the **hash-verified
version imported at the start**, not silently incorporating a concurrently changed
game. Both tiny source snapshots/diff are preserved in this review's new evidence.
No original workspace was overwritten to make the provenance check pass.

Final evidence consistency checker: **40 recorded checks passed**, with known
engine defects separately marked reproduced/not fixed. Existing joint regression
29 checks and authoring/diagnostic regression 20 checks also passed. Checker
fixture mistakes/follow-ups are preserved; they are not disguised engine fixes.
