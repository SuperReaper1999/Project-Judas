# Deformables V2 — investigation and proposed scope

**Investigation only, 2026-11-09.** Source checkpoint
`6274ecdb651a955de39a092de2a33672c229a1bf`; no deformable, structural, collision,
material, rendering or persistence implementation was changed for this report.
Disposable probes use existing public/native APIs. Vehicles V2 towing is a separate
correction at that checkpoint. Main, the RPG SDK and Advanced Fluids lock remain untouched.

## Recommendation

Build V2 around **MK2-owned physical structural sections with finite elastic/plastic
connections, detailed tear-aware render bindings, and the retained XPBD soft-material
solver for cloth and genuinely soft objects**. These are different material laws
under one ownership/lifecycle architecture, not two solvers competing to move the
same chassis. Do not start a new dense vehicle FEM or node/beam engine.

The existing rigid-section path already supplies real collision, force, permanent
rest change, breakage, wheel mounts, powertrain continuity and durable ownership.
Its most important missing pieces are correct volume mass properties, a convincing
continuous exterior, practical collision/detail authoring and qualified cross-system
contact/sleep behavior. Improve these before increasing simulation resolution.

This recommendation deliberately means **coarse structural crash deformation**,
not BeamNG-level node density, exact sheet-metal folding or arbitrary mesh cutting.
Detailed skin must reflect actual physical sections; it must not disguise a scripted
crash replay or a second invisible undeformed chassis. Approval of implementation
should explicitly accept that fidelity envelope.

## What actually exists

| Area | Current implementation and evidence | Disposition |
|---|---|---|
| Soft material | `Deformable.cpp`: CPU XPBD membrane/shear, opposite-vertex distance bending, six Green-strain tet constraints, determinant volume constraint, bounded plastic rest flow | Retain useful cloth/soft-solid capabilities; improve contact/phase ownership, not replace merely because old |
| Structural material | `WorldFracture.cpp`, `Fracture.cpp`, `JointSolver.cpp`: ordinary MK2 bodies, finite six-axis spring/damper/yield/hardening connections, irreversible failure | Retain as crash foundation; correct mass properties and refine laws against independent controls |
| Physical ownership | `RuntimeWorld` owns instances and generated body/joint lifetimes. Rigid section nodes are observations of MK2 motion, not another vehicle integrator | Preserve |
| Vehicles V2 | `wheel.part` resolves stable section keys; normal wheel DOFs and powertrains act on those bodies; broken connectivity can interrupt torque paths | Preserve and qualify on actual deforming vehicles |
| Geometry | Each structural part gets one box collision proxy. Node/tet partition and proxy bounds are validated. Primitive/cooked geometry contacts exist for soft material | Broaden authored proxies where justified; retain immutable geometry and MK2 CCD |
| Presentation | Stored soft render weights, UV/material seams, interpolation and normal/tangent updates. Rigid partitions require one boundary-corner vertex with a single-node binding | Retain soft bindings; replace the rigid exterior restriction so a detailed skin can span intact sections |
| Rendering | `WorldDeformables::DrawDeformables` maps vertices on CPU and updates dynamic mesh data. Renderer already supports skin palettes and portable capability checks | Reuse Renderer V2; do not build another renderer |
| Editing | `DrawDeformable` can bake sheets/tet blocks, select groups, configure body/bone/world attachments, inspect interfaces and finite materials, and preview physical cells | Functionality exists; authoring is still numerical forms with milestone terminology, not an approachable crash workflow |
| Scripting | `scripting/modules/Mechanics.cpp` owns deformable/fracture handles, state, ray picking, forces, impulses, attachments and events in the shared JudasJS runtime | Extend locally; no new scripting engine |
| Persistence | Durable node/plastic/attachment state; finite rigid-family bundles restore physical children, material history and references. Streaming remaps dependencies and pins external mounts | Preserve modern ownership; qualify graph/skin topology and cold restoration together |
| Precision | Soft nodes/rest matrices/material math use binary64; MK2 poses/velocities and render vertices use binary32; conversions occur at those boundaries. Origin shifts explicitly translate soft state | No evidence for high-precision arithmetic or a blanket FP32 conversion programme |

Current bounded assets allow 8,192 nodes, 32,768 triangles/tets, 65,536 render
vertices/indices, 256 structural pieces and 1,024 bonds. These are validation limits,
not claims that the largest supported input is real-time. The encoder actively writes
formats 1/2/3/5 for different present capabilities: these are not all dead compatibility
readers. A future unified format may replace them intentionally, preserving capabilities
without promising historical asset/save compatibility.

## Confirmed problems, independently reproduced

### 1. Structural section inertia is a lumped-node approximation

The runtime computes inertia by placing tet-lumped node mass at the corners.
A generated, uniformly dense **1 m cube, mass 1 kg**, instantiated as a real MK2
structural part, gives:

| Quantity | Uniform-volume answer | Current runtime |
|---|---:|---:|
| Ixx (also Iyy/Izz) | 0.166667 kg m² | 0.5 kg m² |
| Ixy/Ixz/Iyz | 0 | −0.0833333 kg m² |
| Angular velocity after unit X angular impulse | (6, 0, 0) rad/s | (2.14286, 0.428571, 0.428571) rad/s |

This changes rotational crash and wheel-mount response. It is not a general MK2
inertia defect: normal body mass properties are a different path. Correct analytical
volume integration of the already-known tetrahedra belongs in V2's first stage.
Changing the tensor will intentionally change structural trajectories; document this
as a physical correction rather than attempting to preserve the approximation.

### 2. Soft material ignores capsule contacts

A stationary capsule enclosing the soft test patch produces **zero contacts** and
only roundoff-level motion. A sphere countercontrol at the same patch produces
contacts. `RigidContacts` explicitly handles spheres, boxes, compounds and cooked
assets, but has no standalone capsule or analytic-radial-terrain branch.

Terrain V2's **cooked triangle geometry does have a source path** through
`SegmentGeometry`; the old documentation's blanket terrain exclusion is stale.
That does not prove sustained soft contact on edited, paged or planetary terrain.
Those combinations need explicit qualification. Direct analytic planet contact is
not supplied by the cooked-mesh branch.

### 3. Sleeping soft material misses changed static geometry

A sleeping patch remains asleep, unmoved and at zero contacts after inserting an
overlapping static sphere. Explicitly waking it produces contacts. The sleeping
watch checks candidate body velocity, not static geometry membership/revision.
Terrain edits and static teleports therefore need an explicit invalidation contract,
not a camera-dependent workaround.

### 4. Stationary dynamic attachments prevent soft sleep

A quiet patch attached to a truly sleeping dynamic body never sleeps:
`body_sleeping=1`, body speed 0, `cloth_sleeping=0`, quiet seconds 0.
The source treats any dynamic attachment as moving regardless of actual motion.
This makes stationary attached material unnecessarily active.

[Probe sources, raw output, source hashes and existing-gate results](evidence/deformables-v2-investigation/)
are retained. These findings have **not** been fixed during the investigation.

## Architectural limitations and coverage gaps

- **Soft two-way coupling occurs after MK2 integration.** `Simulation.cpp` updates
  soft deformables after rigid synchronization, animation and ragdolls. Reactions
  change current body velocities after that body's collision solve/pose advance.
  Existing finite-reaction controls pass, but stiff suspension-like soft mounts do
  not share the chronological solve that Vehicles V2 uses. This is a design limit,
  not proof that every soft attachment is unstable.
- **Structural parts are internally rigid.** Plastic deformation moves relative
  section frames. The current boundary mesh cannot provide smooth local dents or
  arbitrary high-detail external skins; one box per cell also limits collision shape.
- **Soft contact is sampled and discrete against rigid geometry.** Vertices, edge
  midpoints and triangle centroids are tested repeatedly; cooked contact uses a
  point/thickness query, not a swept soft surface. Small fast objects can cross
  gaps between samples. Existing self-contact has its own accelerated candidates
  and swept tests; do not conflate that with full rigid/soft CCD.
- **Shared preparation is incomplete.** Candidate bodies are queried per substep;
  shape/pose access and sample-vs-shape work recur inside material iterations.
  Use existing broadphase/immutable collision geometry and revision-aware preparation;
  continue reading live finite-body response during the coupled solve.
- **Default vehicle example controls need a physical reference.** Existing native
  wheel/shaft part mounts are usable, but a material-family owner is not itself a
  moving chassis body. The current default driver/camera reads velocity and pose
  from its ordinary chassis entity. A deforming-vehicle example must explicitly use
  a real cabin/section as the driving/camera reference while the family owns its
  wheels and powertrain; do not add an invisible duplicate chassis or copy poses to
  pretend the family owner is a body.
- **Quality settings can affect unrelated world cost.** Active finite-material
  settings select the world's structural subdivision maximum; a high-quality
  assembly can subdivide the entire rigid step. Vehicle contact preparation is now
  independent, but that does not eliminate extra global physics intervals.
- **Rendering is CPU-heavy by construction.** Mapping rebuilds positions, normals
  and tangents; fracture mapping also rebuilds face lists. World revision increments
  even for unchanged sleeping instances. Shared per-frame publication/GPU palettes
  should replace redundant uploads, retaining the portable CPU path.
- **Queries are not automatically soft-surface-aware.** Structural pieces are real
  MK2 colliders, so ordinary rigid queries see their proxies. Soft picking has a
  separate deformable raycast; generic character, particle and fluid queries do not
  thereby acquire arbitrary cloth/soft-surface contact. Keep support claims explicit.
- **Streaming has deliberate limits.** Finite material families have self-contained
  retained bundles; some non-finite fracture families remain residency-pinned.
  Existing external attachment and drivetrain dependencies are useful and must remain.
- **Current crash workflow evidence is incomplete.** `COARSE_CRASH.md` refers to a
  removed lab/generator and historical reports. Mechanical crash application targets
  remain, but their old scene inputs are absent from the current checkout; compilation
  is not a new crash-car qualification. Replace this gap with a current small editable
  acceptance project during implementation, rather than resurrecting historical demos.

## Measured existing baseline

Seven existing numerical/integration executables passed **270 checks**:
soft deformables 68, fracture 33, structural sections 32, bending 40, finite material
27, mechanical connections 37 and material attachments 33. Two existing CPU
benchmark executables also completed. These are current Linux executions, not new
Windows/Android/browser or human crash acceptance.

Ryzen 5 5600, GCC 15.2 Release, CPU 2 pinned, no simultaneous compiler jobs:

| Existing fixture | Median | Reported p95 | Worst |
|---|---:|---:|---:|
| Active cloth + soft solid: 1,585 nodes, 14,612 constraints, 4×4 | 5.353 ms | 5.379 ms | 5.379 ms |
| Same quiet/sleeping fixture | 0.0134 ms | 0.0176 ms | 0.0176 ms |
| Eight loaded soft specimens, 63 nodes/144 tets each, 4×6 | 6.407 ms | 6.435 ms | 6.849 ms |
| 30-piece **soft** cohesive strip, self-contact off | 0.722 ms | 0.728 ms | 0.784 ms |
| 90-piece **soft** cohesive strip, self-contact off | 2.159 ms | 2.163 ms | 2.169 ms |

The first fixture retains only 20 timing samples; its driver's reported p95 indexes
the last sample. Strip rows use 30 samples. These are short diagnostic baselines,
not reliable p99 distributions or complete rendered vehicle crashes. Steady hot
allocations were zero in the reported intact fixtures. Startup/cook, render mapping,
terrain contacts, four complete crashing cars and peak memory still need representative
measurement in Stage 1. Do not advertise the soft-strip timing as a rigid-car result.

## Viable approaches

| Approach | Benefit | Cost / decision |
|---|---|---|
| Repair existing XPBD soft tetrahedra as the entire vehicle | Already has soft strain, volume and plasticity | Stiff dense cars amplify contact sampling, inversion limits and split coupling. Keep for genuine soft objects, not the default car architecture |
| **MK2 structural graph + detailed skin** | Existing CCD, rigid contacts, real wheel/shaft ownership and finite material joints; controllable cell count | Chosen primary path. Coarse collision and local deformation remain declared approximations; fix inertia and binding restrictions |
| Clustered shape-matching / local deformation modes | Promising robust large deformations and local visual detail | Useful later if coarse sections cannot meet agreed visual acceptance; do not add a second authoritative chassis mass/pose solver now |
| Dense implicit FEM / full node-beam vehicle engine | Potentially finer deformation | New solver, contact, authoring and performance programme. Not justified by current requirements or evidence |

Primary references inform tradeoffs, not claims of implemented support:
[XPBD](https://matthias-research.github.io/pages/publications/XPBD.pdf),
[physically based shape matching](https://matthias-research.github.io/pages/publications/Physically_Based_Shape_Matching___SCA_2022.pdf),
[Projective Dynamics](https://www.projectivedynamics.org/),
and BeamNG's own [node/beam physics](https://www.beamng.com/game/about/physics/) and
[render-skin mapping documentation](https://documentation.beamng.com/modding/vehicle/sections/flexbodies/).
Their architectures are alternatives to evaluate, not dependencies to copy into Judas.

## Proposed authoritative ownership

- **Assets/cooking:** immutable material topology, exact volume mass properties,
  stable part/interface keys, authored collision proxies and render bindings.
- **Soft solver:** retained cloth/soft-solid material laws, attachments and bounded
  surface constraints. Each node's mass has one owner.
- **Structural assemblies:** real MK2 bodies and finite material connections.
  MK2 owns chassis/section velocities, collision timing and actual rigid reactions.
  Joint plastic rest state owns permanent structural change; no duplicate pose loop.
- **Contacts/scheduling:** revision-aware geometry preparation, live rigid response,
  explicit prepare/solve/publish boundaries and topology/invalidation notifications.
- **Presentation:** detailed binding to physical material frames, stable normals,
  existing Renderer V2 palette infrastructure where suitable, portable CPU fallback.
  Broken components must stop sharing skin influences; never stretch detached debris
  back to the original body.
- **Runtime/services:** existing world lifecycle, region dependencies, conditional
  save participant, event delivery, debugging and subsystem-owned JudasJS bindings.

Extract cohesive implementation modules from `Deformable.cpp` and related integration
as they change. The world remains a coordinator; avoid speculative generic frameworks,
new collision engines or an engine-wide SDF system. Keep structural and soft material
models distinct where their physics is genuinely different.

## Four-stage implementation proposal — requires approval

### 1. Integrated structural foundation and visible crash proof

Correct volume inertia; build a drivable Vehicle V2 with bounded physical sections,
finite plastic joints and a detailed continuous bound exterior. Exercise wall,
offset/pole and free-body impact cases immediately. Preserve the existing basic
material, attachment and vehicle negative controls. Establish one/four-car simulation,
render, memory, cook and import baselines before increasing cell count.

Acceptance: analytic mass/inertia and rotation-invariance controls pass; actual
wheels/drive remain connected until their real structural path fails; permanent
visible deformation matches physical sections and survives an initial cold reload.
No new invisible rigid chassis, scripted crash poses or cosmetically detached debris.

### 2. Contact, coupling and stable material response

Fix demonstrated capsule and wake/sleep gaps. Integrate required soft/rigid reaction
rounds at a defined MK2 boundary, retaining sampled bone/world target ownership.
Improve cloth bending and local structural contact only where independent controls
show the need. Scope structural quality work to affected assemblies/islands where
measured global overhead justifies it; no unrelated MK2 rewrite.

Acceptance: static/dynamic/kinematic contact, terrain edits under sleeping material,
resting attachments, momentum/reaction/energy, unloading, repeated impacts, bending,
strength ordering and failure timing pass declared tolerances. No arbitrary damping,
velocity reset or solver-quality reduction hides defects. Fast-contact limits are
stated and tested, not silently called full soft CCD.

### 3. Practical authoring, API, persistence and skin integration

Create material presets, visual part/interface/group selection, clear stiffness/yield
units, proxy overlays and actionable diagnostics. Bind imported detailed meshes to
physical sections with proper fracture seams/material interiors. Use existing GPU
palette paths for structural presentation where beneficial; retain CPU portability.
Provide project-owned driving/damage examples, stable events, prefabs and SDK tools.

Acceptance: a new developer can create cloth, a deforming prop and a deforming vehicle
without manually writing solver arrays. Detached skin/collision/wheel mounts stay
consistent. True cold-process saves, origin shifts, region unload/return and relocated
SDK exports preserve geometry, plastic state and ownership. Explicitly document any
current V2 format/API breaks; no historical migration tool by default.

### 4. Sustained integrated qualification and human handover

Use an editable crash/structural workshop plus representative existing vehicle and
terrain workflows. Cover low/high/offset impacts, a four-car pile-up, beams, cloth,
stationary fleets, repeated deformation, moving supports, edited terrain, origin
shifts, streaming, cold saves and relevant particle/water proxy interactions while
keeping the fluid lock intact. Run available Linux/portable gates; report unexecuted
Windows/Android/browser coverage honestly.

Acceptance: p50/p95/p99/worst, physical outcomes, active/sleep counts, peak memory,
preparation and rendering are reported separately over sustained matched workloads.
Provisional engineering goals are <=3 ms physics p95 for one modest structural car
and <=10 ms for four, with cell counts and complete scene overhead declared; establish
real Stage 1 baselines before treating these as firm gates. No silent geometry/event
coverage reductions. Visual close-up, collision-proxy agreement and fresh-object
editor usability are human acceptance requirements alongside automated checks.

## Proposed exclusions and decision boundary

Defer arbitrary runtime mesh cutting/remeshing, detailed sheet-metal creasing inside
one physical cell, full node-beam vehicle replication, GPU-only physical authority,
scientific material calibration and universal fast soft-body CCD. Do not remove
existing cloth, soft solid, fracture or bending capabilities because another material
uses a different representation. Native structural/network replication and detailed
fluid-bearing fractured cavities are not implied.

The product decision is the proposed **MK2 structural graph plus detailed bound
skin** fidelity envelope and the four-stage scope. If close-up crash tests show that
rigid sections cannot achieve the required look/behavior, report that early rather
than growing a hidden dense solver behind this plan. No Deformables V2 implementation
is authorized by this investigation itself.
