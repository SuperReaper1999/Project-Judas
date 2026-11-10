# Deformables V2 Stage 1 — physical crash proof and fidelity stop

**2026-11-10. Experimental investigation/implementation checkpoint; Stage 1 is not
complete or human-accepted. Stages 2–4 have not begun.** The baseline is
`b6a4fedc3f66d75d2917601663d6873ea72da016`. Main and the Advanced Fluids lock are unchanged.

## Decision

The integrated prototype proves real MK2-driven plastic crush, bending, wheel-mount
changes, detachment and cold restoration on an imported vehicle. It **does not meet
the requested severe-crash visual quality**. A severe offset collision exposes
cell-shaped cuts and stretched/open interior triangles. Increasing from 24 to 54
physical sections does not remove that failure and increases the worst measured
physics interval to 196.79 ms. This is a failed visual acceptance gate, not a completed
Deformables V2 feature or permission to advance the programme.

The rejected exterior-binding implementation remains under `tools/crash_proof/`;
it is not installed as an authoritative engine/SDK skin API or added to normal
scene rendering. Its numerical tests demonstrate mapping correctness, not artistic
or gameplay acceptance. The mass correction and physical carrier reference are
separate, tested engine improvements worth retaining.

## Engine corrections and prototype ownership

- `StructuralMass`: analytical uniform-tetrahedron volume moments and affine-field
  angular momentum. Rigid fracture creation/restoration use these values; soft
  simulation retains its existing discrete node masses.
- A powertrain bearing may reference a wheel's **physical carrier**, including a
  generated structural section. It does not refer to wheel spin or create another
  chassis. The authored ID uses the existing codec, dependency and save mechanisms.
  Runtime topology setters remain restricted.
- The car's actual mass belongs to generated MK2 section bodies. The family owner
  has no hidden rigid chassis. Finite plastic joints retain permanent offsets and
  rotations; ordinary wheel constraints and the normal powertrain act on sections.
- The default original procedural structure has 24 sections/40 bonds: lower rails
  and a raised cabin/roof. Removing empty upper bonnet/boot cells improves both
  collision shape and cost. Mass is an authored 1,500 kg, distributed over those
  volumes; it is not a manufacturer-accurate structural model.
- The 54-section/102-bond comparison refines the same profile. Interface area and
  length scale its effective connection calibration. These are different physical
  discretizations, not an identical-trajectory speed comparison.
- The exterior follows four weighted physical section frames through Renderer V2's
  existing palette path. A CPU reference maps the same field. Connected-component
  filtering prevents blending between fully detached fragments; nearest-section
  triangle ownership is **not** a panel/seam-aware fracture model.
- Geometry/moment calculations use binary64; existing MK2 body state and GPU
  matrices use binary32. No new high-precision solver, energy clamp or rotation lock.

The tool is a native developer proof, not a finished scene component, editor
workflow or consumer SDK/export feature. Those integration stages remain unapproved.

## Four operator-supplied assets — local only

No licence or origin metadata was present in the four archives. Redistribution and
commercial rights are **not established**. Originals were read, not altered. No
source model, texture, cooked model or screenshot enters Git, CI fixtures, SDKs or
public packages. Archive hashes, import logs and conversion settings are retained
with the private local evidence.

| Download | Import and geometry | Suitability and required work |
|---|---|---|
| 2015 Dodge Challenger LBPerformance | Nested FBX; 286,095 imported vertices, 95,365 triangles; body, interior, glass and wheel assemblies | Primary real-model crash proof. Evaluate rest hierarchy before binding; local 100× fixture scale produces about 2.192 × 1.409 × 5.022 m. Paint uses a local blue inspection override. Coarse collision proxies approximate the exterior, not every panel. |
| 1978 Chevrolet Blazer K5 Cheyenne | Nested FBX; 111,045 vertices, 37,015 triangles, 21 material primitives | Imports and binds. Rest hierarchy plus local 100× scale gives about 2.448 × 2.004 × 4.816 m. Needs its own taller SUV frame/mount authoring; not qualified on the Challenger's profile. |
| 2013 Ford Shelby GT500 Patrol | Nested FBX initially rejected: embedded 4,268² image exceeds the decoder's 64 MiB RGBA bound | Replacing an adjacent PNG does not replace embedded FBX data. A **local** Blender derivative resized oversized images to 4,096 and exported GLB; Judas then imported 251,835 vertices/83,945 triangles. Local fixture scale gives about 2.035 × 1.521 × 4.973 m. Separate car profile still needed. Original decoder bound was preserved. |
| Ural 4320 Soviet Army Truck | GLB; 678,111 vertices, 226,037 triangles, 81 primitives; about 2.631 × 2.994 × 7.895 m | Imports and binds; source units already metres. Needs a truck-sized structural profile and three-axle/six-wheel layout. The four-wheel car proof is not a qualified truck configuration. |

These are static assemblies, not supplied crash rigs. Material/UV seams and separate
parts are useful, but visual topology is not a watertight physical volume partition.
FBX shader conversion emits explicit approximation/opacity diagnostics. Mirrored
rest transforms require winding/tangent correction when baking the local proof.
Whole visual meshes also exceed the collision-cleanup input bound; use small
self-contained collision proxies or bounded selected parts, not a giant visual
triangle mesh as a moving chassis.

## Physical results

A real 1 kg cubic section previously had diagonal inertia 0.5 kg m² and spurious
−0.0833333 off-diagonals. It now has diagonal 1/6 and only roundoff off-diagonals.
A unit X angular impulse changes `(2.14286, .428571, .428571)` rad/s to the independently
expected `(6, 0, 0)` rad/s. Simplex, rectangular, translated and affine-motion controls
also pass. Structural trajectories intentionally change; old corner-lumped
structural saves are not certified or migrated. Ordinary rigid-body inertia is unchanged.

The original procedural integration fixture exercises frontal, offset, side-pole,
rear and vehicle-to-vehicle collisions, checks quiet support without plastic damage,
counts mass once, witnesses actual contacts, observes irreversible work and restores
its physical family. All downloaded geometry is absent from these tests.

The real-model repeated-launch run accumulates plastic work from approximately
49.5 kJ to 136.8 kJ; it does not reset damage between impacts. An offset-damaged
powered run reaches a 0.2388 rad wheel-carrier/reference misalignment, versus
0.00517 rad in the undamaged control, with different subsequent driving paths.
These are actual carrier transforms, not visual wheel offsets.

At the severe 24 m/s **launch** condition, the default graph breaks two connections
and separates into two physical components. Its remaining wheel drive routes remain
connected; not every broken bumper connection should disable the drivetrain. The
fine graph breaks seven connections and leaves two of four drive links connected.
Launch speed is a declared initial condition, not a claim of independently measured
speed at the first contact. Manual driving uses the normal motor and tyres.

A separate writer and reader use current snapshot construction, preparation,
restoration and publication. Before advancing the reader, an exact SHA comparison
passes for node positions/velocities, permanent deformation, broken/removed sections,
wheel angles/spin and rotor momentum/initialization. Rendering is reconstructed from
that restored physical state; there is no saved crash animation.

## Measured costs and limitations

Ryzen 5 5600, CPU 2 pinned, Release build. Each CPU row has 900 fixed 1/60 s steps,
including 120 initial settling steps. Tests ran sequentially without our compiler
or graphics runs overlapping. This is a shared desktop, not an isolated laboratory.
Milliseconds; no measured steps or spikes were discarded.

| CPU workload | Mean | p50 | p95 | p99 | Worst |
|---|---:|---:|---:|---:|---:|
| One 24-section car, frontal launch 12 m/s | 1.775 | 1.774 | 1.814 | 2.489 | 2.741 |
| Four such cars | 13.850 | 14.100 | 14.213 | 18.380 | 19.673 |
| Repeated impacts, same car and retained damage | 2.096 | 1.806 | 2.428 | 2.724 | **103.818** |
| Severe offset, 24 sections, launch 24 m/s | 1.948 | 1.692 | 2.034 | 2.855 | **55.237** |
| Severe offset, 54 sections, launch 24 m/s | 4.961 | 3.816 | 5.771 | 9.733 | **196.794** |

Profiling identifies repeated mechanical powertrain work inside the coupled rigid
solve as a substantial fleet cost. Inclusive parent/child timings must not be added
together. The structure observation and palette mapping are much smaller. This pass
has not optimized away correct drivetrain reaction or reduced collision coverage.

On the actual desktop GTX 970, the same frontal presentation's CPU reference mapping
has p50 11.622 ms versus 0.00371 ms for physical palette preparation. CPU render
submission has p95 0.550 ms in that GPU-path run. A subsequent one-car whole-render
GPU timestamp run reports p95 1.141 ms, worst 6.759 ms. These are presentation costs,
not a claim that physics became that much faster.

All four cars are frustum-submitted in the fleet overview (min=max=4). CPU render
submission p95 is 1.459 ms, but a topology/material rebuild spikes to 109.190 ms.
Its GPU timestamp capture contains an impossible interval (~1.79e12 ms): **the fleet
GPU mean/worst distribution is invalid and unqualified**, with the raw failure
preserved. It is not evidence of GPU execution taking that time. Earlier mixed-pass
GPU aggregates were also rejected rather than relabelled as full-frame timings.

Peak RSS is 596,092 KiB (about 582 MiB) in the headless one-car process, versus 1,877,912 KiB ≈1,834 MiB in the four-visible-car graphics
process. The latter includes duplicated detailed asset/material data. Single-car
CPU preparation is roughly 1.3–1.7 s; initial graphics-resource creation is about
123 ms for one and 571 ms for four. Numeric skin payload is about 25.7 MB per car,
excluding material images and allocator overhead. These are prototype costs, not
shipping memory budgets.

## Qualification scope

Eleven affected executables pass **633 assertions/checks**: structural 118, soft
68, fracture 33, bending 40, section materials 27, mechanical connections 37,
material attachments 33, powertrain 58, lifecycle 105, scripting 17 and Vehicles V2
97. The CMake contract suite passes 27 checks including omission negative controls.
The first scripting invocation encountered old generated sidecar metadata in its
fixed default work directory; a fresh directory through its existing CLI passed.
The old files and failed log were preserved; no assertion was weakened.

Matched desktop screenshots at the same physical step differ by at most one RGB
level between CPU and GPU mapping, with zero pixels above two levels. Forced
portable-path output is pixel-identical to the modern path. This validates numerical
presentation parity, **not** the failed severe-crash aesthetics. The NVIDIA desktop
was used explicitly; restricted-tool llvmpipe results are not hardware measurements.
Windows, Android, full editor authoring, streaming and SDK exports have not been
qualified for this new prototype. No RPG project or selected SDK was modified.

## Why the fidelity gate fails, and recommended decision

Each current physical section is internally rigid: all of its nodes are reconstructed
from one MK2 transform. There is no within-section bending/buckling state. More render
vertices or better shaders cannot create the missing physical material degrees of
freedom. The four-frame field approximates large-scale crush, while triangle ownership
and tearing follow volume-cell ownership rather than real exterior panel seams.
Fully disconnected pieces stop blending together, but local cracks and intact-panel
continuity need more than global component membership. Thus the severe cuts are not
merely a lighting problem, and increasing section count alone did not meet the brief.

**Do not proceed to Stages 2–4 on the assumption that this skin is good enough.**
Retain the MK2 frame, correct mass/impulse accounting and useful vehicle integration.
Reconsider Stage 1 around a bounded panel/local deformation representation with real
within-panel strain/plasticity, seam-aware connectivity, and shared collision/render
boundary state. It must not double-count chassis mass, play a damage animation or
leave an unchanged hidden collision chassis. Evaluate one front body/hood and pole
impact before extending to another complete car. Existing soft-material kernels are
candidates to reuse, not evidence that an attached dense soft vehicle is affordable.
This is an architecture proposal requiring review, not implemented follow-on work.

## Reproduce and inspect locally

Build `judas_crash_proof`; supply an operator-owned normalized model and a fresh
writable working directory. No downloaded assets are shipped with the target.

```sh
cmake --build build --target judas_crash_proof -j 3
build/judas_crash_proof "$MODEL" "$WORK" --case=offset --speed=24 --xray
```

W/S drive, A/D steer, Space brake, Enter launches a declared test impact, P pauses,
Q/E orbits, mouse wheel zooms, R resets the authored structure, F6 saves, Esc quits.
Restart with `--load` and the same model/case/quality to inspect a cold save.
`--frames=230` makes a bounded automatic launch at step 120 and captures the result;
`--headless` removes presentation. `--repeat`, `--post-drive`, `--count=4`,
`--cpu-skin`, `--profile` and `--segments=12 --columns=3` reproduce the comparisons.
`--xray` changes obstacle rendering only. Inspector-ready scene components and
normal project/export integration remain future approved work.
