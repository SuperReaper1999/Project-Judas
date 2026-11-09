# M83 practical physical crash sections

Current production replacement for the unsuccessful dense implicit-node experiment.
The original source, numerical successes and genuine failures are preserved under
`docs/evidence/m83-structural-corrective/architecture-reassessment/` and in the
research artifact archive. They are not the current runtime representation.

## Open the current lab

`projects/coarse_crash_lab/coarse_crash_lab.judasproj`

Use V for torque-driven autopilot in a case, T for the tour, W/S throttle,
A/D steering, Space brake, Tab selection, P prefab spawn, J collision overlay,
F3 streamed annex, F5/F8 save/load, R reload, Escape pause.
1 normal road; 2 low impact; 3 medium; 4 high; 5 offset;
6 weak/nominal/strong; 7 generic force-loaded cantilever; 8 oblique gravity.
The four-car scenes are `Scenes/group.judas` (15 m/s) and
`Scenes/group_high.judas` (25 m/s).

## Representation and ownership

The native M79 rigid material-partition path owns ordinary generation-safe bodies
and M45 connections. Finite material connections are optional; existing hard
fracture, bending, wheel and powertrain content keeps its defaults.

A car has four front crush sections and one supported hollow cabin/rear floor:240 kg
of partitioned front material plus a336 kg cabin/core. Roof and pillars have actual collision.
Four independent M81 wheels act on ordinary bodies. Front wheels resolve stable
partition-part keys through the existing owner, not cached native body slots.
M82 power sources, couplings and brakes act through the actual wheel DOFs.

The partition mesh follows the physical sections. Plastic rest strain changes their
relative positions/orientations, collision and wheel frames. A detached section
keeps its body, mass, motion and contact; it is not decorative spawned debris.
There is no aggregate-to-fragments activation or replayed collision. The modest
section assemblies remain physically active, avoiding an unproven activation seam.

## Finite connection law

Each connection has six local spring/damper axes. In a physical interval h:

- Translation error e is attachment separation minus plastic rest translation.
- Rotation uses the relative quaternion logarithm in A's joint frame, minus its
  plastic rest rotation vector.
- On an axis, yield occurs when `k abs(e) > Y + H alpha`.
- Return increment is `dp = (k abs(e) - Y - H alpha)/(k + H)` when positive.
- Rest strain advances by `sign(e) dp`; accumulated travel alpha advances by dp.
- Recorded plastic work advances by `(Y + H (alpha + dp/2)) dp`.
- The remaining elastic spring and dashpot are integrated through native
  accumulated-impulse rows, with `gamma = 1/[h(d + h k)]` and
  `bias = e h k gamma`.
- Exceeding authored translation/rotation plastic travel breaks the connection.

This is a coarse, componentwise constitutive model, not industrial FEM or an
exact finite-rotation continuum law. Damping and native constraint discretization
also dissipate energy; plastic work is not a complete system energy ledger.
Instantaneous CCD impact rows do not turn a finite spring into a rigid weld or
inject a spring position-correction impulse. The normal physical intervals supply
its finite effort. Contact, gravity and wheel reactions remain normal Judas physics.

The lab runs authoritative 60 Hz updates, two native intervals and 12 rounds.
These are authored generic settings, not scene-name quality branches. No global
nonlinear node/beam root solve runs in this implementation.

## Authoring

Use the ordinary deformable component and `.judasdeform` resource. Choose a rigid
partition, select an interface in the inspector and enable **Finite elastic/plastic
connection**. Author translation/rotation stiffness, damping, yield force/torque,
hardening and plastic travel capacities. Existing field/asset editors and prefab
references remain authoritative. Re-baking changes save content compatibility.

The existing explicit partition importer accepts:

```
material "interface-key" kTranslation kRotation dTranslation dRotation yieldForce yieldTorque hardeningForce hardeningTorque translationCapacity angleCapacity
neighbour-filter 1
```

Units: N/m, N m/rad, N s/m, N m s/rad, N, N m, N/m, N m/rad, m, rad.
The material record follows its already-authored bond. Nonfinite/negative values,
invalid capacities, duplicate records and material use on a soft partition are rejected.

`judas_deformable_bake material-sections output x y z width height depth strengthFactor [plasticTravel]`
creates a small generic regular partition with finite connections and named part
groups. `partition output source` imports hand-authored geometry/materials.
`tools/CreateCoarseCrashLab.py` reproduces current authored lab assets.
The cantilever's `.source` is an editable non-game-specific example.
The car authors0.20 m of accumulated plastic travel per connection (about17%
of a1.2 m section length), identically in every speed case. Strength variants
change yield/hardening, not mass, geometry, solver quality or the travel capacity.

**Neighbour filtering is explicit.** Face-connected cells already suppress contact
at their intact interface. With the opt-in filter, intact local edge/vertex neighbours
and fixed support pairs are suppressed too: overlapping welded shapes must not fight
one another's constraints. Distant self-collision remains active. Separate components
and released supports regain collision. The failed full-car runs and isolated controls
that established this requirement are retained in the evidence.

## Mechanical mounts and public JS

`wheel.part` optionally names a stable rigid partition part; `wheel.chassis` then
references the material owner. Mount coordinates are relative to that part's body
frame. An absent/removed/unpublished part makes the wheel unavailable; another
allocation cannot substitute. The editor exposes the owner and key.

A powertrain connection can explicitly set `groups: true`, with `attachment` and
`rootAttachment` naming cooked node groups. It requires an intact material path
between those groups, without adding an unnecessary attachment constraint.
Default false retains existing attachment prerequisite semantics. A game command
cannot repair a physically broken path.

`entity.fracture.state.interfaces` additionally reports `material`,
`plasticTranslation`, `plasticRotation`, and `plasticWork`; these are value snapshots,
not mutable solver memory. Broken interfaces retain their final observations.

JS in the project owns ordinary driver control, lane steering, cameras, tour,
loads, UI, save and scene requests. It does not set vehicle velocity, prescribe
crush, change part angles or create a collapse sequence. The demo uses no native
scene-name or object-name physics branches.

## Persistence, streaming and lifetime

Finite partition assets use `JudasDeformable5`. Full saves use deformables participant
6, preserving material history, ordinary body motion, joint warm history, wheels and
powertrains. Existing default asset/save versions retain their routes.

Retained material families use `Judas.DeformableState.4`. Their bounded bundle owns
its generated bodies and remaps their logical identities during staged restoration.
Private restored bodies stay disabled until their region publishes. Their owner
retires their constraints/bodies on destruction, reload or suspension. Streaming
retains external dependency pins; it does not silently suspend an externally used
material support or motor ground contact.

Lazy first publication uses the current parent frame and its point velocity.
A delayed asset must not instantiate in the old authored pose after its parent moved.

## Limits and review

A sufficiently large impact can yield and exhaust a connection within one
physical interval; positive plastic work precedes its failure in the constitutive
update, without an artificial frame delay.

Rigid sections do not deform internally; local crush is represented by relative
section strain and permanent joint rest geometry. There is no arbitrary mesh
cutting, sheet folding, fatigue, thermal damage or fluid-bearing fractured geometry.
Authored section count, mass distribution and contact neighbourhood matter.
The original accepted nodal/lumped part inertia approximation remains in use.
Visual wheel meshes remain separate from M81's suspension/contact representation.
No Windows or human acceptance is implied by Linux checks.

Measurements, exact checks, failures and fingerprints are in the final evidence
report. The performance target is one car physics p95 <=3 ms, four complete cars
<=10 ms and engine frame work p95 <=16.67 ms on the Linux host. Report maxima and
instrumentation/scheduling exceptions separately; these targets do not certify every
scene, CPU, display or Windows VM.
