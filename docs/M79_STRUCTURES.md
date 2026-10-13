# M79 — practical load-driven structures

Current implementation on Experimental; **human validated by the operator on 2026-10-13**. This is an
optional extension of M63 fracture, not a second structural physics solver.

[M80 bending](M80_BENDING.md) extends this same section model to optional soft
cohesive cells. The rigid-cell contracts and original M79 evidence below remain
unchanged; soft cells retain their actual deforming nodes rather than becoming
rigid proxy bodies.

## Ownership and authoring

A normal `.judasdeform` solid partitions material into explicitly authored rigid
cells and planar interfaces. Every cell is an ordinary PhysicsWorld body with
its own collision box, material mass, inertia and motion. M45 fixed constraints
carry loads across intact interfaces. Losing a support changes those constraints;
subsequent solves redistribute demand and may break more interfaces. No unsupported
island detection, scripted collapse, spawned replacement debris or health counter
is involved. The logical family owns lifetime, not an additional collider.

In the editor, select a deformable entity, create/import a rigid fracture asset,
and enable **Structural section stress (M79 opt-in)**. Edit family tensile/shear
strength in Pa; compression is optional (0 disables crushing). Select an interface
in **Interface preview** to set tension, shear and compression overrides. Zero
inherits the family value. Geometry preview shows cells and interfaces. Strength
editing rebakes the shared asset: it affects all instances and invalidates saves
whose asset fingerprint differs. Density and attachment groups use the existing
Deformable inspector. World attachments are ideal prescribed supports, not finite
strength fasteners; author a pinned foundation cell and a breakable interface if
that connection itself must have finite capacity. Body attachments use normal
finite-body constraints. Do not also put a rigid body on the logical family.

CLI: `judas_deformable_bake structure OUTPUT nx ny nz sx sy sz [tension shear compliance]`.
This creates regular cells and left/right/middle attachment groups. Explicit
`JudasFractureSource 1` imports additionally accept `structural 1` and
`strength "interface-key" tension shear compression` **after** that bond record.
Existing source records remain valid. Arbitrary mesh cutting is not provided.

## Resultant section model

After the ordinary authoritative rigid-body/contact/joint solve, use the reaction
impulse on A divided by the fixed timestep as force F. Subtract `r_anchor × impulse`
from its angular reaction impulse before dividing by dt to obtain interface
moment M. Rotate both into the section's local frame. Nothing is world-Y based.

For area A, unit normal n and area-centred offsets r, integrate the actual authored
triangles once at bake/load:

- Q = integral(r r^T dA), with its two in-plane modes inverted;
- J = trace(Q), the polar area moment;
- q = n × M, gradient = Q_inverse q;
- normal stress sigma(r) = dot(F,n)/A + dot(gradient,r);
- shear traction tau(r) = F_tangent/A + dot(M,n)/J (n × r).

The triangular second moment is exactly
`area/12 * [outer(sum(r_i),sum(r_i)) + sum(outer(r_i,r_i))]`.
Normal stress reproduces the resultant axial force and bending moments. Evaluate
vertex extrema of the affine stress field; compare positive tension, negative
compression and shear magnitude to their authored limits. Combined compression
can offset bending tension. Torsion uses a **polar-moment approximation**, not a
Saint-Venant warping solution. Uniform transverse shear omits its nonuniform
continuum distribution. Section centroids and nondegenerate geometry are validated.

Failure is irreversible and published in source-interface order after the complete
step. Retire the owned fixed joint and restore normal inter-piece collision.
Bodies are not recreated, moved or assigned new velocity on failure. Later loads
are resolved by the same PhysicsWorld; there is no parallel load graph solver.

## Compatibility and lifecycle

Legacy `.judasdeform` versions 1/2 and their square-section estimate remain unchanged.
Only opted-in assets write **JudasDeformable3** with an explicit extension containing
the model flag and interface strengths. Section caches are derived, not persisted.
Scene/prefab properties and runtime save participant layout remain unchanged.
Source assets are hashed normally; old executables cannot read version 3.

Public `entity.fracture` and `entity.deformable` APIs are reused without new bindings.
`state.interfaces` reports peak stress and physical/explicit failure causes;
`state.tensileStrength/shearStrength` remain the **family defaults**, not overrides.
`force`, `impulse`, ordinary `applyImpulseAtPoint`, support `deformable.release`, and
`onFracture` retain fixed-boundary/safe-handle semantics. Gameplay chooses loads and
cleanup. A runtime family shares the immutable cook but has independent topology.

The existing save path retains broken connections, tombstones, supports, physical
poses/motion and owned joint continuation. Saves do not heal or replay fracture
callbacks. Destroying an owner cleans its bodies and constraints. Live region
families explicitly pin their source region; this is retention, not streaming
of freely moving rubble. Normal M38 export includes the cooked assets.

## Lab and controls

Open `projects/structural_lab/structural_lab.judasproj` in the editor or runtime.
Three eight-cell bridges use the same 8 × 0.6 × 2 m geometry and 80 kg/m³ material
(768 kg each), plus ordinary 20 kg contact loads:

- A: two supports, 120 kPa tension;
- B: two supports, 1 MPa tension;
- C: A's strength with an extra middle support.

An independently rotated fourth bridge has a matching oblique gravity region.
Scene 2 uses the actual spatial RadicalGravity field, with that explicit uniform
region taking precedence locally. It is not a rotated flat-scene physics branch.

WASD/mouse move/look; Space launches; click applies a 500 N·s hit-point impulse;
B toggles a 1000 kg-equivalent force in the sampled gravity direction; R removes
right anchors and actual right pedestals; H removes C's middle anchor/pedestal;
P spawns an independent bridge prefab; F3 requests/releases the annex;
1/2 uniform/radial scenes; F5/F8 save/load; F9 reload; Escape pause.
G/T retain ordinary pickup/throw for tagged proof loads. The three comparisons
show **different physical outcomes**, not a promise of an unbreakable strong bridge.

## Limits

This is practical brittle load-driven failure, not engineering certification.
No fatigue, fracture energy propagation, elastic beam FEM, arbitrary cutting,
healing, reinforced composite material model or automatic mesh collision cooking.
Fixed-joint reaction estimates inherit finite-iteration/contact transients from
normal physics. Abrupt support removal can fail even a stronger connection under
its transient peak; increased capacity and redundant support reduce damage rather
than guaranteeing survival. Ideal world pins have unlimited capacity. Cells use
validated authored box proxies and uniform nodal material density. Liquid-bearing
fracture and deformable-surface general casts remain the existing unsupported paths.
No fluid, independent research or unrelated deferred CCD behaviour was changed.

## Human checklist

1. Observe all bridges supporting their orange proof loads at rest.
2. Click a slab and watch actual impact motion and physical failure.
3. Reload; toggle overload with B and compare the three structures.
4. Reload; press R, watch successive failure/falling pieces, and compare A/B/C.
5. Remove C's middle support with H; verify its load path changes.
6. Walk on and collide with rubble; pick up/throw an orange proof load.
7. Inspect/modify an interface strength in the editor, reload, compare the result.
8. Test the rotated assembly and radial scene (2).
9. Spawn a prefab, save/load after collapse, request/release the annex, reload/Stop.
10. Run the exported standalone and repeat pause, loads and support loss.

See [focused evidence](evidence/m79/RESULTS.md), [Fracture API](judasjs/fracture.md),
[Deformables](judasjs/deformables.md), [physics](judasjs/physics.md),
[saves](judasjs/saves.md), and [streaming](judasjs/streaming.md).
