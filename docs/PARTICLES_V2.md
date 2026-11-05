# Particle emitters

Particles V2 is implemented on Experimental for the proposed Alpha v2.0.0.
It has not been promoted to main or published as an accepted release.

[JudasJS reference](judasjs/effects-camera.md#particles) ·
[Declaration file](judas.d.ts) · [Copyable script](judasjs/examples/particles.js)

Particle emitters are optional visual effects, not Advanced Fluids or one
rigid body per particle. Existing emitters keep CPU simulation, no collision,
no wind and no impact delivery unless the project authors those options.

## Authoring

Add **Particle Emitter** to a scene object. The inspector groups everyday
emission/appearance, motion/backend, collision response and aggregated events.
Advanced groups start collapsed. Typed texture selection, undo/redo, scenes,
prefabs and standalone exports use the existing component workflow.

The scene format remains `JudasScene 3`. The original required emitter fields
are unchanged. Optional V2 keys are omitted at their defaults so old canonical
scene output and fingerprints stay unchanged. Nondefault V2 options participate
in the authored compatibility fingerprint.

| Scene property | Value / default |
| --- | --- |
| `particle.backend` | quoted `"cpu"`, `"auto"`, `"gpu"`; CPU |
| `particle.collision` | boolean; false |
| `particle.wind` | boolean; false |
| `particle.collision-radius` | metres `(0,1000]`; 0.025 |
| `particle.restitution` / `particle.friction` | `[0,1]`; 0.35 / 0.2 |
| `particle.response` | quoted `"bounce"`, `"slide"`, `"stick"`, `"remove"`; bounce |
| `particle.collision-layers` | unsigned decimal 64-bit mask; all bits |
| `particle.max-contacts` | integer `[1,32]`; 8 |
| `particle.impacts` | quoted `"off"`, `"cosmetic"`, `"reliable"`; off |
| `particle.impact-cell-size` | metres `[1e-6,10000]`; 0.5 |
| `particle.impact-weight` | finite `[0,1000000]`; 1 |
| `particle.max-impact-cells` | integer `[1,65536]`; 1024 |

These are component keys beneath `particle-emitter`, not project-wide toggles.
The named authoring codec preserves the same properties. Negative, nonfinite,
unknown enum and invalid budget values fail loading without partial publication.

The existing limits remain: rate `[0,100000]`, lifetime `(0,3600]` seconds,
capacity `[1,100000]`, startup burst `[0,capacity]`, finite nonnegative sizes,
nonnegative position spread and velocity variation, and RGBA alpha `[0,1]`.
Native validation is `src/particles/ParticleSettings.cpp`; the component editor
owns authoring controls in `src/editor/ParticleEmitterEditor.cpp`.

## Backend and collision contract

CPU is portable and deterministic for a fixed seed and update/query sequence.
Automatic selection can use resident GPU compute only when that context and the
requested feature combination support it. Required GPU never means silently
pretending to run unsupported work. Query `entity.particleStats` for requested
and active backends and useful failure/fallback details. `active` is `null` when
required GPU is unavailable, rather than claiming that CPU ran.
`stateTransferBytes` counts exceptional backend state copies; normal resident
GPU simulation performs no full-state transfer.

| Requested capability | CPU | Resident desktop GPU |
| --- | --- | --- |
| World-space billboards, seeded emission, bounce/slide/remove | Yes | Yes |
| Stationary spheres, capsules and boxes | Native snapshot geometry | Packed native snapshot geometry |
| Stationary cooked triangle meshes | Native cooked BVH | The same cooked triangles/BVH, uploaded and reused |
| Moving sphere/capsule/box owners and captured angular motion | Yes | Auto selects/transfers to CPU; required GPU rejects |
| Convex hull collision | Yes | Auto uses CPU; required GPU rejects |
| CPU `RadialTerrain` callback collision | Yes | Auto uses CPU; required GPU rejects |
| Stick response and local-space particles | Yes | CPU required |
| Reliable gameplay impact delivery | Yes | CPU required |
| Cosmetic impact observations | Yes | Bounded, asynchronous and allowed to drop |
| Gravity | Existing selected/custom field | Explicit uniform/radial descriptor; spatial/custom fields use CPU |

The GPU path requires a qualified modern desktop compute/SSBO context. GLES3,
WebGL2 and forced portable desktop profiles select CPU for `auto`; `gpu` reports
unsupported work instead of starting an expensive hidden CPU substitute. An
unsupported object anywhere in the captured collision set can require CPU,
even if the current particles do not hit that particular object.

Auto can transfer an already running GPU effect to CPU when its requirements
change, for example when a formerly stationary receiver starts moving. This is
an explicit, **one-time blocking** transition: finish the old work, drain pending
cosmetic observation batches, read live state, then retire GPU resources. Transfer
bytes and the fallback reason are diagnostic. Normal GPU ticks and extra cameras
do not download the particle state. A CPU effect is not repeatedly transferred
back and forth while its current particles remain live.

Collision samples an immutable, bounded snapshot of the completed MK2 fixed
step. It does not mutate MK2 or issue a world query for every particle. Moving
boundaries use the captured motion and COM frame. Response is one-way visual
collision: bounce/slide use boundary-relative velocity; stick follows a captured
surface anchor; remove ends the particle. It does not push rigid bodies.
The emitting object's own native body is excluded; other bodies follow
`collisionLayers`. Use a separate emitter object when particles must also hit
the source body's geometry. This owner filter is independent of camera culling.
Gravity and wind come from the engine's existing fields when explicitly enabled.

An empty, known-CPU emitter does not prepare a collision snapshot. Its emission
clock still advances: rate and authored initial-burst births are created at the
end of the empty step, then receive fresh geometry before their first movement.
Explicit bursts already present at step entry require geometry immediately.
Queued Auto/GPU births and GPU populations are not considered empty from delayed
GPU diagnostics. Pending reliable CPU work retains its original frozen snapshot.
After returning to idle, cached geometry history is retained but `snapshotMs`,
`snapshotBodies`, `snapshotColliders` and `snapshotTriangles` report zero work.

Active emitters still capture independently, with their own layer/owner filters
and geometry revision history. There is no general cross-emitter snapshot cache.
The standalone collision microbenchmark excludes capture cost; world-level
preparation must also be measured when assessing a game. A headless reproduction
is available with `build/judas_particle_world_tests --idle-benchmark`.

On 5 November 2026, Release GCC/Linux on Ryzen 5 5600, three alternating-order
before/after pairs (250 static boxes, 30 warm-up + 300 measured ticks per process)
measured twelve idle CPU collision emitters at **3.244–3.248 ms median before**
and **0.00385–0.00389 ms after**. Before p95 was 3.273–3.692 ms, worst
4.038–5.834 ms; after p95 was 0.00388–0.00393 ms, worst 0.00957–0.01189 ms.
This isolates `UpdateVisualParticles`; it is not a measured whole-game speedup
or a reproduction of the RPG's reported 61/25-second end-to-end comparison.
Raw results, original failing controls and qualification metadata are retained
in `docs/evidence/particle-idle-cost/`. The RPG and its selected SDK were untouched.

Wind is sampled once at the emitter per fixed step, then applies the simple
acceleration `(windVelocity - particleVelocity) * 1/s`. It is a visual drag
approximation, not a per-particle weather query or aerodynamic solver. Gravity
sampling and weather queries remain independent of graphics quality.

The current native world capture covers local coordinates `[-8192,+8192]` metres
on each axis, after the scene's double world origin is removed. This is an
8,192-metre half-width chart, not an unbounded planetary collision field. The
default capture limits are **512 bodies, 2,048 colliders, 262,144 unique triangles
and 8,192 recorded motion segments**. A failed capture leaves the emitter
unadvanced for that step, with a diagnostic; it does not silently remove bodies
to fit. Unsupported captured geometry remains an explicit query/selection
failure rather than disappearing from collision. Origin shifting is the existing
way to move the supported chart through a large world.

Each particle also has a contact budget (`maxContacts`, default8, range1–32).
Queries have bounded search iterations. A query failure, unsupported geometry,
departure from the captured domain or exhausted contact budget stops the
affected movement at its last verified point and reports `queryFailures` and a
diagnostic. It is not reported as a successful no-contact sweep. These limits
make failure visible; they do not make arbitrarily complex scenes inexpensive.

Capture/query/contact budgets and unsupported geometry produce diagnostics;
there is no claim of universal collision support for every engine subsystem.
CPU and GPU floating-point trajectories need not be bit-identical. The GPU step
accepts a finite fixed duration of at most 0.25 seconds; unsupported durations
are diagnosed before simulation. GPU visual effects are unsuitable as authoritative gameplay state. Android and browser
contexts use the portable path unless compute capability is genuinely present;
desktop compute availability alone is not Android/browser qualification.
This document does not assert that the new particle implementation has passed
native Windows, Android-device or browser runtime qualification. Those results
must be recorded separately against an exact build.

CPU and GPU use the same seeded birth recipe for the extended emitter path, but
their update timelines are not identical. An explicit CPU `burst` creates its
particles immediately, so a call before simulation can advance in that tick.
A queued GPU burst is born at the end of its next fixed tick; its first movement
can therefore be one fixed tick later. Both backends integrate the final
partial lifetime and test its contacts before retiring an expired particle.
GPU birth identities count requested attempts; CPU identities count successful
births. Saturated emission can therefore choose different future identities
across the two backends, although a GPU batch admits a deterministic prefix. Choose CPU for deterministic or
gameplay-authoritative effects.

The native 100,000-birth near-origin control uses a 4e-5-metre / 4e-5-m/s
trajectory tolerance over four gravity/wind steps; analytic collision controls
use 1e-3 metres and m/s. These are qualified fixture tolerances, not a promise
of the same absolute error at every chart location or after arbitrary long runs.

## Impact delivery and lifecycle

Events aggregate in bounded receiver-local spatial cells, retaining receiver,
child, captured geometry identity and contact direction. They contain historical
absolute and COM-local coordinates so origin changes do not reinterpret an old
impact. The engine supplies observations; project JavaScript decides what a
collision means for damage, collection, sound or other gameplay.

GPU cosmetic readback uses at most three bounded staging slots and at most
1,024 contact records per submitted batch, further reduced by the authored
`maxImpactCells` value. CPU aggregation then reduces those observations into
receiver-local cells. Cosmetic drops are counted; these caps do not apply to
the lossless CPU path.

Cosmetic delivery may drop observations when its budget is exhausted. Reliable
delivery uses CPU, stalls the emitter explicitly when full and retains the
captured step until delivery succeeds. Drain in `fixedUpdate`; no script array
allocation failure may discard reliable native observations. Destructive control
commands refuse to discard pending reliable records or a captured blocked step.
This makes the cost and backpressure visible instead of silently losing events.
A blocked producer resumes its captured particle tick; it does not accumulate an
unbounded backlog of every world tick waited. Its visual timeline can lag the
world under backpressure. Consume promptly; cosmetic delivery is appropriate
when continuous visual cadence matters more than retaining every observation.
New producer evolution remains paused at that captured boundary until capacity
is available; the engine does not skip ahead and recompute the old impact against
newer geometry.

`play`, `pause`, `stop`, `clear` and `restart` are deliberate runtime controls.
Pause freezes the effect; stop halts new emission while existing particles age.
`clear` and `restart` also refuse a nonempty native cosmetic queue: drain its
observations first. This conservative control boundary prevents a settings or
control command from silently erasing observations already available to scripts.
The persisted `enabled` flag keeps its legacy meaning: disabled particles age
but do not emit or draw. Changing local/world space clears live visuals.
Backend transitions and settings edits must respect pending reliable work.

Scene/prefab data stores settings. Modern cold saves retain runtime settings and
durable pending impact aggregates through a versioned participant. Live visual
particle state, GPU handles, camera-sort scratch and render history remain
transient. Extra cameras affect sorting/drawing rather than simulation cadence.

Reliable observations use optional save participant `particle-impacts`, version1,
including the next sequence even when its queue is empty. A blocked partial
captured step defers save preparation until its queue is consumed and the step
finishes. Regions containing an undelivered producer or receiver remain pinned.
After delivery, the empty queue's sequence survives regional suspension through
optional streaming archive10; ordinary worlds retain their earlier format.
Project migrations retain opaque observation bytes and cannot silently discard
an unsupported participant. Records are validated before publication.
Restored nonempty queues are sealed until one drain: new contacts cannot merge
into historical saved cells or alter their meaning before delivery. Reliable
producers backpressure behind the sealed batch; cosmetic queues can report
drops. Draining acknowledges the historical batch once and permits new cells.

This does not serialize every airborne particle or promise identical future
effects after cold loading. Gameplay should persist its own authoritative action
state and deliberately resume/recreate visual effects. Reliable delivery protects
the already captured observations and exposes backpressure; it is not a complete
saved particle trajectory.

## Intentional API correction

`burst(count)` now requires an actual finite integer number in `[0,100000]`.
It no longer coerces numeric strings, truncates fractions or wraps large values
through uint32. Existing integer calls within the old bound keep their meaning;
the new upper bound matches the authored maximum capacity. Bursts still respect
remaining capacity and do not allocate an unbounded queue.

## Focused validation

- `ParticleAuthoringTests.cpp`: legacy defaults, exact masks, canonical/named
  roundtrips, prefab component retention, every V2 option in equality/fingerprint,
  editor undo/redo and invalid authoring controls.
- `ParticleScriptTests.cpp`: actual shared VM, complete setting readback,
  transactional rejection, strict burst integers, named/full-width masks,
  lifecycle control, portable fallback and runtime diagnostics.
- `ParticlePersistenceTests.cpp`: independent write/read processes, actual
  collision observation, double world position, transactional corruption,
  sequence continuity, partial-step deferral and real producer/receiver region
  pins followed by suspension and revisit.
- `ParticleWorldTests.cpp`: actual graphics host, startup GPU bursts, disabled
  emission, lifecycle controls, origin invariants and a moving-owner Auto
  transition; forced portable profiles exercise fallback/refusal instead.
- Collision, CPU/GPU and integration suites separately qualify physical contact,
  timing, bounded aggregation, streaming, origins and save behaviour. Passing
  authoring tests alone is not evidence that every backend or platform passed.

## Measured scope and review project

[Surface Workshop](../projects/particle_v2_lab/README.md) is the editable project
for this change. Its project-owned paint cells survive cold saves and restore
both material appearance and real receiver friction. They are not saved decals
or retained airborne particles. The workshop includes native cooked terrain,
a moving receiver, rotated geometry/gravity and an explicitly selected GPU
stress scene; the starting scene uses small CPU populations.

Matched Release measurements used a Ryzen 5 5600, 8 GiB RAM and GTX 970 4 GiB
(NVIDIA 580.178.04), at 1920×1080, fixed 1/60-second updates, 60 warm-up frames
and 240 measured frames. The second view is 960×540. The desktop compositor was
present. Values below are milliseconds, median / p95 / worst.

| Workload | CPU simulation + draw submission | GPU elapsed interval |
| --- | --- | --- |
| Previous CPU particles, 100k, one view | 8.501 / 8.857 / 9.968 | 0.763 / 0.824 / 0.997 |
| Retained CPU particles, 100k, one view | 8.214 / 8.732 / 9.529 | 0.774 / 0.824 / 1.149 |
| Resident GPU, 100k, one view | 0.177 / 0.257 / 0.357 | 2.698 / 3.115 / 3.550 |
| Previous CPU particles, 100k, two views | 15.880 / 16.620 / 17.574 | 8.059 / 8.292 / 8.962 |
| Resident GPU, 100k, two views | 0.328 / 0.395 / 0.492 | 5.354 / 5.824 / 6.179 |
| Resident GPU, 100k, 13 static collision bodies | 0.203 / 0.234 / 0.392 | 3.970 / 4.406 / 4.584 |

These isolate particle simulation and presentation, not a complete game frame:
there is no rigid solver stepping, and the fixed collision snapshot is captured
once outside the timed loop. GPU intervals include submission bubbles. The GPU
path moves substantial work off CPU and increases single-view GPU elapsed cost;
this is not a universal GPU speedup claim. The collision run retained 100,000
particles, recorded 2,726,447 contacts and reported zero query failures.
Resident allocations were about 10.1 MiB; ordinary ticks made no full-state
transfers. Process peak RSS varied by run, so this is not a general RAM-saving
claim. Moving concave worlds, reliable events and unsupported combinations
are outside the 100k GPU performance result.

The first 100k birth dispatch exposed real global atomic contention. Replacing
per-birth reservation with one bounded batch reservation reduced that isolated
GPU phase's paired median from 341.590 to 0.081 ms. Raw samples and the rejected
path are retained; this phase measurement is not a whole-frame p95 result.

Qualification evidence is retained in the source repository's
[Particles V2 register](https://github.com/SuperReaper1999/Project-Judas/blob/Experimental/docs/evidence/particles-v2/QUALIFICATION.json), including
commands, fingerprints, raw measurements and preserved first failures. It
distinguishes the measured implementation checkpoint from later diagnostics
and portability corrections. Native Linux driver validation, portable compile
checks and human review remain separate categories.

For human review, use the single checklist in the
[Surface Workshop guide](../projects/particle_v2_lab/README.md#qualification-and-human-checklist).
No main promotion or human acceptance is implied by these automated results.
