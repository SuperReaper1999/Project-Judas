# Deformables V2 — revised Stage 1 crash proof

This is an **Experimental native physical proof awaiting human review**, not a
published Deformables V2 release. Stages 2–4, normal scene/editor authoring and SDK
exposure have not begun. Main, the Advanced Fluids lock and the operator's RPG SDK
remain unchanged. The starting checkpoint is `3697bb4f91025b6f8b766ec5cbaab869dcca1107`.
The previous [failed section-only proof](DEFORMABLES_V2_STAGE1.md) remains evidence.

## What changed

### Wheel assembly

The previous fixture used generic wheel locations/dimensions and spun imported
cambered geometry about an unrelated axis. It also grouped stationary calipers
with rotating wheels. The preparation now:

- Bakes the imported rest hierarchy, unit conversion and mirrored winding once.
- Fits each wheel's area-weighted principal axle, center, radius and width;
  normalizes its mesh into the engine's +X axle convention without a reflection.
- Authors the actual wheel component from that fit, including camber and nominal
  loaded suspension travel. Initial ride height follows the tyre/ground geometry.
- Keeps caliper bodies/badges on the steered unsprung frame, while tyres, rims and
  brake discs rotate with the actual wheel angle.
- Draws from Vehicles V2's actual center/rotation/travel and structural carrier.
  There is no separate visual spin animation or hidden visual-position correction.

The private `Assets/Challenger.panel-parts.txt` recipe explicitly includes the separate
bonnet vents in the physical bonnet binding; no downloaded part identifiers are
hard-coded into the engine. The source/recipe/scene hashes identify compatible saves.

The supplied Challenger has about 3 degrees of front and 4 degrees of rear camber.
The local preparation does not change Vehicles V2 wheel physics. The procedural
checks cover opposite sides, offset origins, camber, concentric forward/reverse
rotation, dimensions and exact reconstruction of the supplied rest geometry.

### Physical local panel material

The existing MK2 section graph remains responsible for large-scale chassis motion,
plastic connections, mass/inertia, wheel mounts and drivetrain reactions. A finite
25 kg bonnet uses a 36-node, 50-triangle material surface with signed dihedral
hinges. Its mass replaces 25 kg of frame mass: total mass remains 1,500 kg.

`src/deformables/PanelBending.*` owns the optional material law. The existing
DeformableInstance owns membrane solving, gravity, collision, self-contact and
finite-mass attachment reactions. The new law runs inside those same substeps;
it is not another chassis solver. It provides first-order flat-panel bending,
elastic recovery, plastic rest curvature, hardening and recorded plastic work.
Its analytic gradients are translation/rotation invariant; finite-difference,
zero-net-gradient and zero-net-moment controls verify that independently.

The proof's effective bending stiffness/yield/hardening are authored game-material
parameters, not a manufacturer steel specification. The membrane remains stiff;
there is no crash animation, artificial energy clamp or crash-specific damping.
The existing uniform-tetrahedron structural inertia correction is retained.

Two finite rear hinges and a front latch bind the bonnet to actual physical
sections. Breaking those connections changes real ownership; no release impulse
or damage-state animation is added. The detailed bonnet follows its simulated
surface, retaining the imported rest detail around it. The rest of the exterior
uses the physical section frames with whole UV/material-island ownership instead
of arbitrary volume-cell triangle cutting. UV islands are preparation candidates,
not a claim of automatic semantic panel recognition for arbitrary assets.

The proof has a stronger, finite occupant cage and weaker front crush rails.
Only this bonnet has the new local material. It is **not a fully deformable
production car**, a sheet-metal fracture solver or a crash-safety model.

### Ownership, collision and persistence

The family owner has no undeformed rigid chassis body. Structural sections and
panel nodes own their mass once. External rigid-world contacts, self-contact and
mount reactions use actual physics. The proof assigns distinct collision layers
to its cars and excludes each bonnet's own coarse inner frame, whose proxy volumes
overlap its authored surface. Other vehicles remain collidable. This inner-frame
filter is a **Stage 1 approximation**: detailed bonnet-to-engine-bay self-collision
and general structural collision-proxy authoring remain unqualified.

The local owner explicitly persists its physical panel participant alongside the
normal WorldPersistence chunks. The checksum includes section/node motion,
permanent structural damage, wheel spin, rotor state, panel positions/velocities,
material parameters, rest angles, plastic work, released mounts and target frames.
A separate writer/reader process must match that checksum before advancing.

The optional native material is not exposed as a general scene/JudasJS component.
Its owner must persist it explicitly. Adding broad authoring/export integration
belongs to the later stages and is not implied by this proof.

## Rejected experiments retained privately

1. The original 24/54 rigid-section comparison failed severe-crash fidelity.
2. A thin-volume bonnet stayed elastic at rest but inverted an element in the
   procedural barrier test. Increasing rigid-section count was not the response.
3. Opposite-vertex distance-spring bending did not give adequate first-order
   flat-panel stiffness and accumulated quiet-load plastic sag. That candidate
   was rejected; its failed checks were not relaxed.
4. The initial rear-only hinge omitted the real front latch; the corrected
   assembly has two hinges and a latch. The generic fixture's excessive starting
   ride height was also removed from the real-model setup.

Raw trials and their source remain with the private local evidence. No downloaded
mesh, texture, cooked derivative or screenshot is committed or packaged.

## Performance and targeted corrections

Measurements use the Ryzen 5 5600; rendering uses the actual desktop GTX 970
(NVIDIA 580.178.04), not software rendering. CPU comparisons pin CPU 2, use Release
and fixed 1/60 s intervals, and separate profiling/visual captures from ordinary
runtime measurements. Runs include initial settling rather than discarding it.

A matched 900-step four-car experiment isolated an actual integration bottleneck:
per-row linear/angular attachment impulses repeatedly traversed the same MK2 wake
graph. The correction retains every impulse and wakes a physical target on its
first nonzero reaction within the material solve. The local cache expires at the
fixed-step boundary; it is not an arbitrary island limit or force cutoff.

The recorded first batching comparison reduced mean panel work from 36.62 to
5.04 ms and total physics from 50.98 to 19.58 ms. Its full saved physical-state
hash was identical before/after:
`17c44818782a62dca6aaa595de4938cd6e54082844be31e7a54edb1c8923700f`.
A refined per-step cache reduced the matched four-car p50 from 49.51 to 16.60 ms
(p95 63.72 to 16.74 ms). Expanded checksums including actual MK2 body poses,
velocities, mass, wheel and material state matched both during impact and after
900 steps. The final private vent recipe was then requalified separately.

Final qualification measurements and the refined per-step batching are recorded in
[the evidence summary](evidence/deformables-v2-revised-stage1/qualification.json).

A separate correction prevents released/disabled attachment targets from keeping
retired material awake. Active targets retain their wake behaviour. Referenced
materials are compacted during local preparation; actual used textures are retained.
The profiler probe now reads one frame snapshot rather than copying its whole
history on every interval. Instrumented peak RSS must not be confused with ordinary
runtime memory or called a game leak.

**Structural impact spikes remain.** Four-car throughput and severe impact costs
must be read from the measurements, not inferred from the cheap panel kernel.
No MK2 rewrite, collision weakening or timestep reduction was made to hide them.

## Qualification and limitations

- Eleven affected native executables passed **672 checks**, including 157 structural
  checks; **27 CMake registration controls** passed. The final real-model recipe
  passed quiet driving, both crash cases at 8/16/24 m/s, repeated impacts, four-car
  simulation and a genuine fresh-process cold save/restore. The cold-state hash is
  `b99fa706ef81ecffa1bfe47691739ce6b72764781dd7f8dc272d50c591d2ad24`.
- Original procedural fixtures protect current mass/inertia, fracture, joints,
  attachments, Vehicles V2 and powertrain contracts.
- New controls cover signed bending gradients, flat-panel response, plastic return,
  no creep under an unchanged solved load, persistence and invalid preparation.
- The integrated procedural proof checks quiet gravity, real barrier contacts,
  permanent bending, mass counted once and exact restoration/continued motion.
- The real-model workload includes 8/16/24 m/s declared launch conditions for wall
  and offset-pole cases, sustained post-crash simulation and repeated impacts.
  Launch speed is not a claim of measured first-contact speed.
- Wheel-course captures cover forward/reverse driving, braking, both steering
  directions and uneven ground. Synthetic controls are automated evidence, not
  the operator's human acceptance.
- All four original downloads remain unmodified/private. The Challenger is the
  qualified fixture. Other imports retain the constraints in the prior report;
  they are not four complete crash implementations. Redistribution rights remain
  unestablished.
- Fine wrinkles, complete body-panel coverage, glass, arbitrary vehicle recipes,
  native scene/editor authoring, streaming/origin-shift integration for this private
  owner and ordinary SDK standalone exports remain outside Stage 1 qualification.
- This pass does not establish Windows/Android crash-proof execution or hardware
  compatibility. Current engine portability paths remain intact; CI results are
  reported separately and only after completion.

## Local launch and human checklist

The private editable proof is at:

```text
/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/
```

Launch `./Play-Revised.sh frontal` from that directory (the launcher already
enables panels), or `./Play-Revised.sh offset --speed=16`. For cold restoration use
the identical case/options plus `--load`. The default starts stationary; Enter launches
the selected diagnostic speed.

Use its revised launcher/README and build receipt for the exact binary/source
checkpoint. It needs the active engine data and desktop libraries; it is not a
self-contained SDK export. Native proof source is under `tools/crash_proof/`.
No external assets are supplied with the public executable target.

1. Inspect all four wheels at rest, then drive/reverse and brake. Check hub centers,
   calipers, rolling direction and concentric rotation.
2. Steer both directions and cross the uneven-ground course. Watch suspension and
   wheel-arch clearance; mounts must follow the real carrier after damage.
3. Start with an 8 m/s frontal launch. Pause/orbit to inspect local bonnet buckling
   and an intact cabin. Compare 16 and 24 m/s without treating greater damage as
   permission for stretched strips or arbitrary block separation.
4. Inspect offset-pole damage from both sides; repeat impacts without resetting.
   Check permanent shape, latch/hinge failure and damaged handling.
5. Save with F6, close the process and reload the matching case/configuration.
6. Judge the visual result and the remaining performance limitations. Stage 1 is
   accepted only by the operator; no main promotion or later stage is automatic.

W/S drive/reverse, A/D steer, Space brakes, Enter applies a declared diagnostic
launch, P pauses, Q/E orbit, wheel zooms, R rebuilds the fixture, F6 saves, Esc quits.
`--wheel-view=0..3` selects close-ups; `--wheel-course` runs the automated control
course. `--capture-steps` is opt-in diagnostic photography. Bounded `--frames` runs
ignore manual driving input. The reference proof without `--panels` remains a
comparison fixture, not an alternative engine backend.
