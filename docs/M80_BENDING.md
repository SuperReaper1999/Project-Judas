# M80 — Judas learns to bend

Current **Experimental candidate; human acceptance pending**. M80 extends the
existing M62 tetrahedral solids, M63 cohesive fracture and M79 section model.
It does not add a second body solver or replace physical deformation with a
render effect. Focused validation and measured results belong to the final
candidate evidence; this page describes the implementation and its limits.

## One material, geometry and ownership path

`RuntimeWorld` owns each deformable instance. The immutable `.judasdeform` asset
contains material nodes, positive-volume tetrahedra, boundary faces, named groups
and render bindings. The instance owns positions, velocity, plastic rest matrices
and irreversible interface state. Nodes remain material points, not Entities or
rigid bodies. The logical owner supplies initial placement and cannot also own a
rigid body, CharacterMotor, skeleton or ragdoll; unit owner scale is required.

The same current node positions define material constraints, deformable contact,
`deformable.raycast` and bound render vertices. A bending or dented solid therefore
changes its physical boundary. Broken cohesive interfaces stop transmitting load;
the existing nodes retain mass, velocity and plastic deformation, and newly exposed
faces participate in presentation and contact. No fresh rigid proxy replaces a
bent piece. M79's rigid-cell path remains available for structures that need rigid
fragments and ordinary M44/CharacterMotor contact.

## Elasticity and plasticity

The existing XPBD solve runs at the authoritative fixed boundary with bounded
substeps and iterations. For each tetrahedron, current edge columns `D_s` and
per-instance rest columns `D_p` produce a deformation gradient. Six scalar Green
strain constraints resolve extension and shear, with a separate `det(F) - 1`
volume constraint. The generalized compliance is divided by tetrahedral reference
volume; XPBD uses compliance divided by substep duration squared. Solid strain
uses **shearCompliance**; **volumeCompliance** controls volume preservation.
Cloth's stretch/bend settings do not become beam or panel material parameters.

M80 evaluates elastic columns in the authored material basis:
`F = D_s * inverse(D_p) * R_initial`. This removes dependence of the componentwise
strain response on global placement rotation. Saved rest matrices retain their
existing authored world-frame representation. The original material basis is now
saved independently of the owner's current transform, which may move prescribed
supports without resetting the material.
Gravity is sampled through the selected Judas field at each node, independently
of support geometry and material orientation.

Below yield, deformation is elastic. The existing plastic flow uses the magnitude
of Green strain, an explicitly authored yield strain and a rate in inverse seconds.
It removes rigid rotation by polar iteration, updates the rest columns and rescales
their determinant to preserve original rest volume. **maximumPlasticStrain** bounds
that rest-shape update. Zero yield or zero plastic rate disables plastic flow.
The cap is not a break threshold. Unloading lets elastic strain relax around the
new rest state, leaving the permanent bend/dent.

These are approximate game materials, not measured Young's modulus, a complete
elasticity tensor or engineering certification. Mesh resolution, finite iteration
count and compliance affect the response. Damping is an explicit rate, not a hidden
reset toward the original shape.

## Bending load and physical failure

Optional soft structural cooks use the same explicit material partition as M63,
with duplicated interface nodes joined by vector XPBD cohesive constraints.
For the completed substep, each pair contributes its **accumulated** multiplier
as a force `f_i = -lambda_i / h²`. Individual iteration corrections are not
counted repeatedly as impacts.

The interface resultants are `F = sum(f_i)` and `M = sum(r_i cross f_i)`, using
current paired-node midpoints relative to the current reference-area-weighted
centroid. A volume-weighted polar material frame rotates the resultants into the
section frame. M79's reference section area, second moments and vertex extrema
then resolve combined axial/bending stress and approximate shear/torsion. These
are **corotated nominal reference-area stresses**, not exact Cauchy stress on a
strongly warped section. The same interface strength overrides apply to rigid
and soft section representations.

Exceeding authored tensile/shear strength, or enabled compression strength,
records irreversible failure. It publishes after the complete fixed step before
`onFracture`; later loads redistribute through the surviving constraints. Repeated
or increasing loads can create more failures, but there is no damage-per-frame
counter, fatigue model or arbitrary crack propagation. Plastic yield and brittle
interface strength remain separate settings. Element inversion/nonfinite state
stops with a diagnostic; that diagnostic is not material-strength fracture.

## Authoring

Add a **Deformable** component in the normal inspector. Use **Create tetrahedral
block** for an unpartitioned elastic/plastic solid, or **Create bendable structural
block** for a cohesive solid with section-based failure. Set resolution and size,
then density, solid shear/volume compliance, damping and optional plastic settings.
Named node groups provide world/body/bone attachments and force application.

For a partitioned asset, **Structural section stress (M79/M80 opt-in)** is available
for both soft and rigid cells. Edit family strengths in Pa, select an interface
to edit its overrides, and preview material groups/interfaces. Zero override
inherits the family value. Changing a shared cook affects every instance and
changes strict saved-content compatibility. World/static/bone attachments are
prescribed supports; dynamic-body attachments exchange finite reaction.

```sh
./build/judas_deformable_bake flex-structure OUTPUT.judasdeform \
    nx ny nz width height depth [tension shear compliance]
```

The existing explicit partition importer also accepts a non-rigid structural
cook. Geometry, positive tetrahedral orientation, interface correspondence,
nonoverlap, area and section conditioning are validated. Existing versions 1/2
remain valid; opted-in section cooks use `JudasDeformable3`. There is no arbitrary
mesh cutting or tetrahedralization service.

## JavaScript, persistence and residency

The current [Deformable](judasjs/deformables.md) and
[Fracture](judasjs/fracture.md) APIs are reused. Project scripts choose forces,
impulses, support release and gameplay meaning in `fixedUpdate`; they cannot
mutate node memory or substitute their own deformation solver. This milestone
adds no public binding, declaration or API inventory entry.

Ordinary scene/prefab properties, async resources and standalone export carry
the same component/cook. Saves retain node motion, plastic rest, attachments,
sleep/enabled state and committed broken interfaces. Derived section weights,
collision candidates and GPU data rebuild; restoring does not replay fracture
events. New world saves use **deformables participant version 3**, including the
historical constitutive reference frame. Retained streaming state has the
`Judas.DeformableState.1` envelope and carries that same reference. Asset cooks
remain version 1/2 or the existing `JudasDeformable3` section format.

Earlier save participants (1/2) and raw retained-state blobs still decode using
the initialization reference. They cannot guarantee exact continuation after a
historical owner rotation: that missing reference is not invented from deformed
plastic matrices. Unsupported newer versions fail diagnostically. Plain solids
can suspend/revisit with independent retained state and fresh
handles. A live fracture family retains its existing explicit source-region pin
until destruction/adoption, rather than silently discarding travelling material.
See [saves](judasjs/saves.md) and [streaming](judasjs/streaming.md).

## Collision and limits

Current node, edge-midpoint and face-centroid samples contact authoritative
PhysicsWorld geometry through the existing broadphase. Finite node/body mass,
world inertia and hit-point impulses provide two-way reaction; reciprocal M39
layer/mask filtering remains in force. Self/deformable contacts use current boundary
geometry and topology exclusions. For cohesive solids, self-contact within one
still-connected material component is suppressed; separated components can contact
each other. Deep folds of an intact cohesive component therefore remain unsupported.
Renderer maps/interpolates the same node state;
rendering never advances simulation. No universal world-up is introduced.

This sampled contact model can miss small/fast objects between coarse samples.
It is not exact deformable CCD, general M44 deformable casts, CharacterMotor
blocking or M42 soft-surface event delivery. Strong compression/inversion and
extreme element distortion remain outside the supported envelope. No cloth
tearing, remeshing, fluid-cavity fracture, FEM certification, fatigue or automatic
soft-to-rigid conversion is added.

## Editable lab and human review

Open `projects/bending_lab/bending_lab.judasproj`. Its registered flat and radial
scenes contain a paired elastic/plastic cantilever, a bendable failing partition,
a plastic panel under an ordinary physical press and an independently oblique
elastic specimen. The annex and elastic/plastic prefabs use normal residency and
runtime construction. All interaction policy is project JavaScript.

WASD/mouse move/look; Space launches; **B** toggles equal end loads on the elastic
and plastic pair; **H** increases/cycles the failing beam's load; **click** applies
an impulse at the current deformed ray hit; **G/T** pick up/throw ordinary props;
**P** alternates elastic/plastic prefab instances; **F3** requests/releases the
annex; **1/2** select uniform/radial scenes; **F5/F8** save/load; **F9** reload;
**Escape** pauses. The physical press contacts the actual panel boundary.

Before accepting a candidate, check elastic return, permanent deformation after
unloading, physical interface failure under increased load, contact with deformed
geometry, rotated/gravity equivalence, prefab independence, save/load,
streaming/reload/Stop and exported standalone behaviour. Automated measurements
do not establish human physical or visual acceptance.

See [M62 foundations](M62_DEFORMABLES.md), [M63 fracture](M63_FRACTURE.md),
[M79 section equations](M79_STRUCTURES.md) and [Architecture](ARCHITECTURE.md).
