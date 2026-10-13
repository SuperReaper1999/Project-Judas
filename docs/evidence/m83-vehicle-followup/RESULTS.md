# M83 structural vehicle follow-up

**Experimental candidate; human review pending.** Baseline:
`287708d4918ceefa7c03de887d2baba222b4954a`. Main remains accepted M82
`7bfa1ac3f11221ecdc450a5778023f8aff380c52`. The earlier generic finite-triangle
correction is retained. This follow-up changes ordinary content, its JS policy,
focused verification and current documentation; **no engine implementation or
public API changes**.

## Genuine design failure

The second human rejection is preserved in `HUMAN_FAILURE.md`. A single rigid
chassis with outboard bearing cubes could lose a wheel mount but could not
crumple or fracture its chassis. Its narrow passing tests did not establish the
requested useful vehicle destruction. The earlier scenes/cooks/prefabs and all
original M83/crash/collision evidence remain unchanged.

## New physical construction

`projects/damage_lab/Scenes/vehicle_crash.judas` is the new ordinary startup scene.
Each compact car has a 120 kg rear body/cabin, a 40 kg front axle/nose and a
30.24 kg plastic/cohesive front structure. The front module owns both front
wheels; the rear body owns both rear wheels. Wheel centres are ±0.85 m with
0.32 m radius. There are no separate outboard bearing cubes.

The actual hood/crumple geometry is 1.20 × 0.28 × 0.90 m, eight cells,
64 nodes / 48 tetrahedra. It is the only load-bearing connection between the
rigid modules: their colliders meet its ends without a rigid bypass. Two
unlimited multi-node body patches exchange physical impulses. Interior M79
sections can tear; M83 front-drive prerequisites read the actual surviving
material/attachment topology. Rear drive is independent. Detached bodies retain
mass, collision, gravity and inherited motion.

Both cars use identical material/quality/source settings: yield 0.08, plastic
rate 10 s⁻¹, shear compliance 2e-7, density 100 kg/m³, four material substeps,
12 iterations, 60 Hz; 80 N m / 6 kW electric drive. Weak tension/shear/compression
capacities are 100 kPa / 70 kPa / 1.2 MPa; strong capacities are 10×. The test
compares exact material serialization and whole cooked assets after normalizing
**only strengths**, as well as solve budgets and actual module masses.

JavaScript owns logical controls, narration, cameras, HUD, spawning and save
requests. Autopilot supplies ordinary equal throttle/brake input and reads the
result. It writes no damaged transforms, velocities, plastic rest, failure flags,
mount changes or hidden impact impulses. A side view keeps the front section
visible. The substantial axle separates rather than a decorative loose wheel.

The new fixed prefab hierarchy uses safe-handle caches instead of repeatedly
copying/enumerating the scene for every child/callback. Physical state is read
live; ordinary resource publication and restore rebuild the cache. This is a
content-side usage improvement, not a new engine optimization or pipeline.

## Constitutive investigation — original failures retained

| Trial | Result | Interpretation |
|---|---|---|
| First core, plastic rate 0.5 s⁻¹ | Real fracture, but only 0.25 mm plastic rest; objective crush check fails | Slow flow and early brittle compression failure |
| Rate 10, same shear/strength | 4.863 mm plastic; 30.059 mm edge change versus 4.850 mm ride baseline; two gates fail | Supported faster flow alone insufficient |
| Shear compliance 2e-7, same strengths | 4.908 mm plastic; 30.214 mm edge versus 4.900 mm baseline; two gates fail | Still breaks on first yielding step |
| Double all strengths | 24.994 mm plastic / 37.273 mm edge, but no fracture; three failure/mechanical-loss gates fail | Useful forming occurs if compression path survives |
| Ductile compression capacity, original tensile/shear | Forms while intact, then progressive tensile/shear tearing and full front-path loss | Final physically justified construction |

In the brittle case, tick 292 strain 0.0723 is below yield 0.08; tick 293 first
flows and six sections break. Tensile/shear demand is below their capacities,
identifying compression failure. After disconnection strain falls below yield
and plastic flow stops. The intact control continues forming through ticks
294–297. At four substeps / 60 Hz, the supported rate bounds flow at 10/240 per
substep. No missing plasticity mechanism or solver-budget defect was established.
The final authored compression capacity permits ductile forming; retained
100/70 kPa tensile/shear capacity then produces progressive tearing. No cadence,
quality budget or original crush threshold was reduced. A 20 s⁻¹ rate was ruled
out by the existing supported-range validation before execution; it was not run.

Exact trial sources/cooks/settings, failures, traces and witnesses are retained
under `development/` and `linux/`. Small compliance/strength trials are not
advertised as engine performance improvements or calibrated automotive metal.

## Final physical endpoints

The unchanged meaningful deformation gate is **>5 mm plastic rest and >30 mm
invariant tetrahedron-edge change above the pre-wall ride baseline**. It now
requires the witness strictly before the first physical/scripted bond failure
and before any attachment release. Rigid fragment displacement cannot count as
non-rigid deformation. Quality, logical input and geometry remain fixed.

| Quantity | Held drive / save case | Ordinary V watch tour |
|---|---:|---:|
| Forward approach | 7.578 m/s | 7.589 m/s |
| First solved wall contact tick | 291 | 471 |
| Intact plastic rest change | 17.689 mm | 22.248 mm |
| Intact edge-length change | 37.273 mm (ride 4.900) | 36.767 mm (ride 4.900) |
| Maximum connected compression | 83.684 mm | 82.618 mm |
| First physical bond failure tick | 296 | 477 |
| Complete front/rear material split tick | 299 | 479 |
| Physical failed bonds | 4 | 5 |
| Scripted failed bonds / released mounts | 0 / 0 | 0 / 0 |
| Actual axle motion relative to cut pose | 1.296 m | 1.343 m |
| Permanent rest change after crash | 25.180 mm | 24.550 mm |
| Stronger car failed bonds | 0 | 0 |

Both front drive rows become disconnected/inactive with zero torque while their
commands remain enabled; rear rows survive. Both modules retain actual mass,
colliders and available front wheels. Material remains healthy. The watch tour
finishes and releases throttle through ordinary driver policy; the held-drive
case independently proves mechanical disconnection with commands still held.

## Focused Linux results

- New watch **17/17**, no-drive **9/9**, held-drive + ordinary save **17/17**.
- Fresh-process physical restoration **12/12**; all authoritative entity records,
  material, wheel and shaft state match (see contract clarification below).
- New two-prefab/independent drive/pause/resume/reload/scene roundtrip **12/12**.
- Retained bearing checks: write **19/19**, fresh read **10/10**,
  prefab/retained-stream/lifetime **15/15**, oblique gravity **19/19**.
- M83 attachment **33**, M83 JS **18**, M80 numerical **40**, wheel **30**,
  powertrain **56**, all passing.
- Release fixture builds: zero warnings. Existing shipping runtime/editor/export
  remain unchanged. No full production-suite rerun.
- Editor disposable-copy smoke: duplicate/undo and authored Play/Stop identity,
  save successful, four meshes, zero resource/job failures. Reported zero steps
  is the last render frame's step count, not evidence of a full no-physics run.
- M38 export: 22 assets / five scenes. Copied package launched from an unrelated
  working directory; watch/captures, save/reload/load, pause/resume and all four
  scene selections passed. Exported source/assets match the candidate.

The first retained-scene invocation used a helper binary seven seconds older than
its already-committed scene-selection source. It selected the new startup scene,
so its fixture-ready deadline failed. `retained-write.txt` is preserved and not
credited as a regression run. Rebuilding that helper produced the actual passing
original-scene checks; no engine/content correction was necessary.

### Cold-save test assumptions — no hidden engine fix

The original `final-read.txt` and exact snapshot remain preserved. Comparing the
whole entity participant as physical bytes was invalid: it combines regenerated
scene presentation text, binary physical records and a script-owned view. The
independent decoder proves all **7,328 authoritative bytes / 84 records / seven
physical bodies** identical. Only normalization/quaternion sign-equivalent
visual transforms and a **0.115 mm** presentation-camera change differ.
The final check retains exact counters/flags/body poses/velocities/forces, exact
parsed component/settings/reference identities, independently bounded semantic
transform equivalence and a separate presentation-view comparison. It does not
omit the entity participant or replace physical equality with a broad epsilon.

The original 20 N s impulse/40 kg nose correctly produces the immediate I/m
velocity change. Its first resolved interval moves 4.160 mm, with 7.372 N s
measured tyre lateral reaction and real rear-body/wall contacts. The final
46-step net motion is 4.544 mm. A >10 mm net-travel assertion was not guaranteed
for a module already resisted by tyres, contacts and attached material.
The impulse, timestep and all 46 resumed steps remain unchanged. The corrected
mechanistic check proves exact I/m, awake/enabled body, actual first-step
movement/contact/tyre reaction, running fixed clock, and continued physical
fracture/drive loss. The failed arbitrary net-travel assertion is retained and
is not reported as passing. No save implementation or physical friction was
changed. `cold-analysis/` and `cold-diagnostic*` make this independently auditable.

The watch/rest/write evidence uses fixture edition e5fa8c…; the final edition
0d062d… changes only the read contract/diagnostics and retains the same drive,
watch, rest and lifecycle physics/gates. Its read/lifecycle checks ran natively.
Archived editions and binary fingerprints preserve this narrow rerun boundary.

## Performance

Native Linux, Release, actual GL rendering, synthetic 1/60 s application frames,
VSync disabled; no concurrent native CPU-heavy workload. These are measured
uncapped work costs, not a hardware-independent FPS promise. Screenshot readback
and encoding are outside reported frame-work samples.

| Final case | Fixed median / p95 / max ms | Frame-work median / p95 / max ms |
|---|---|---|
| Rest | 4.474 / 6.618 / 28.214 | 6.146 / 9.174 / 30.682 |
| Held drive + save | 6.915 / 10.186 / 26.165 | 8.589 / 12.507 / 28.969 |
| Watch tour | 6.373 / 9.314 / 39.457 | 8.260 / 12.523 / 42.570 |

Watch median frame work corresponds to about 121 uncapped FPS. The isolated
~43 ms maximum is disclosed; this is not a promise of zero hitches. Workload:
seven bodies / four dynamic, eight wheels, two powertrains, two materials,
128 nodes / 96 tetrahedra at 60 Hz. These differ from the rejected mount-only
construction; no incomparable before/after engine optimization claim is made.

The first bounded M56 diagnostic includes script hierarchy-enumeration overhead
and ordinary material attachment/contact work. Worker queue wait spans are not
CPU execution cost and must not be summed into frame-work claims. Raw profile
is retained, explicitly diagnostic, with profiling overhead; final performance
numbers above are unprofiled.

## Scope, limits and handback

Current docs: `docs/M83_MECHANICAL_DAMAGE.md`, project `README.md`.
Reproduction: `REPRODUCE.md`. Exact inventory/fingerprints are alongside this file.
Main is held pending operator acceptance. Windows follow-up was not run and is
informational/nonblocking. No new human validation is claimed.

Only the front structure plastically deforms/fractures; cabin/nose remain rigid.
This is a useful small coupled assembly, not industrial crash FEM, cutting arbitrary
meshes, deformable tyres, fatigue or a calibrated vehicle crash test. Existing
splitting/conditioning/folding limits remain. No M84, unrelated physics, fluid,
independent research or protected historical work is changed.

Human review: watch the actual chassis forming/tearing and complete front axle;
compare green, drive remaining wheels, save/load damage, spawn a car, pause/reload,
try the retained oblique/streaming fixtures, and repeat in standalone.
