# Authoring Deformables V2

Stages 2–4 are being qualified on Experimental. The Challenger Stage 1 proof is
human accepted; the complete milestone still awaits its final review. This is an
unsupported V2 source preview, not a published SDK upgrade.

## Choose the physical representation

- **Cloth / soft props:** one coarse sheet or tetrahedral material. Render detail
  does not need to match simulation resolution. Tetrahedra have an explicit
  inversion limit; this is not an arbitrary-folding solid solver.
- **Sheet-metal panels:** independently authored coarse sheets with signed bending,
  plastic rest angles and local irreversible work. Use multiple panels around a
  vehicle rather than wrapping its complete closed exterior onto one sheet.
- **Vehicle / structural frame:** actual MK2 sections joined by finite elastic and
  plastic connections. Sections carry collision, mass, momentum, wheel mounts and
  drivetrain connectivity. There is no hidden undeformed chassis beneath them.
  A detailed exterior follows these physical frames; independent panels supply
  local bends that coarse rigid sections cannot represent.

## Create a new object in the editor

1. Create an object and add **Deformable** in the Inspector. Open **Create/import
   deformable asset** and choose an unused Assets-relative `.judasdeform` path.
2. Create a sheet/block or import an indexed cloth mesh. For a vehicle-style frame,
   **Create 24-section physical vehicle frame** supplies a 2 × 2 × 6 layout,
   finite material connections, neighbour filtering and 1500 kg total frame mass.
   It is a starting preset, not a calibrated complete car; account separately for
   independently simulated panel masses.
3. For a detailed physical panel, choose an imported mesh and a separated
   **Imported panel region**, choose its normal axis and fit it. A bonnet/roof
   usually uses Y; a side panel usually uses X. The cooker retains detailed
   normals, UVs, materials and source offsets while simulating the chosen grid.
   Choose the steel panel preset, then tune stiffness, yield moment and hardening.
4. For a structural exterior, select the physical frame, choose its detailed source
   mesh and use **Bind detailed exterior to current structure**. The destination
   must be a new asset. Use a source with independent panel surfaces where tearing
   is intended. Imported surface islands are hints, not semantic hinge discovery.
   Do not also display an undeformed copy of that exterior.
5. Save the scene. Asset assignment uses normal undo/redo. A destination that
   already exists is rejected rather than overwritten. Source changes require
   explicitly rebaking the binding; the resource loader rejects stale content.

The selected part list loads through the ordinary asynchronous asset service.
Hierarchy/rest-skin transforms are flattened once, including imported 100× and
mirrored transforms. Inspect the imported scale warning and prepare models in
metres. A panel region must contain at most 65536 vertices/indices; split large,
closed or overhanging shells into appropriate regions. The physical envelope is
an approximation, not collision for every detailed polygon.

## Attach panels and author failure

Use the panel's **Groups** list, or preview a local selection box and bake a named
node group. Attach that group to a world frame, a prescribed skeleton joint, an
ordinary dynamic body, or a named **Structural section** of another deformable.
Section keys remain stable across runtime child-body creation and restoration.

An edge group acts as a hinge/seam; a small patch acts as a latch or point mount.
Separate left/right groups permit partial failure. The hinge/seam and latch
buttons supply editable example hardware capacities. Their physical degrees of
freedom come from the constrained nodes, not an animation or a speed trigger.

The target transforms the group's asset-local rest nodes plus the target-local
`offset`. Author panels and their carrier in a common source-local frame; the
section-centre offset must match the chosen physical section. The accepted
Challenger authoring tool writes explicit mount data. Arbitrary independently
rotated attachment frames are not automatically inferred.

- **Force/moment limits:** actual resultant patch reaction in N and N m. Finite
  strength requires a dynamic body/section. World and bone targets prescribe
  motion and do not silently claim finite physical failure strength.
- **Mounting-sheet tear energy:** local accumulated irreversible bending work in J,
  separate from hardware failure. Zero disables this criterion. Distant panel
  work is not charged to an unrelated mounting group.
- **Partial release:** remaining connections continue to constrain the material.
- **Complete release:** the same material retains mass, momentum, permanent shape
  and independent collision, including against its former carrier.

No arbitrary internal tetrahedral tearing is added to sheet panels. Use physical
panel boundaries and mounts for intended seams and detachment. Existing cohesive
solid fracture is a distinct supported representation.

## Inspect and control the simulation

During Play, enable **Live physical mesh, mounts and collision proxies**. The
coarse mesh is blue, retained mounts yellow, released mounts red, and rigid section
proxies green. Runtime diagnostics show mass, contacts, strain, element Jacobian,
sleep state and actual attachment force/moment/work. Detailed render polygons are
not drawn as solver elements. Advanced material and local quality settings remain
available under collapsed sections.

Local substeps and constraint iterations refine soft material; they do not
subdivide the entire world or change an unrelated rigid body's timeline. MK2 owns
rigid structural motion. Bone targets intentionally use the final resolved
animation phase; they are prescribed targets, not finite-mass rigid reactions.

Project-owned JudasJS uses `entity.deformable`, `entity.fracture`, wheel and
powertrain handles. See [the exact deformation API](judasjs/deformables.md),
[fracture](judasjs/fracture.md) and [wheels](judasjs/wheels.md) and [powertrains](judasjs/powertrains.md).
`setPanelMaterial` retains damage. `onAttachmentFailure` and `onFracture` report
completed physical connection changes. Restore does not replay old failures.

## Save, export and limits

Normal SaveService/WorldPersistence owns material positions, velocities, plastic
rest state, mount failure, structural connections and detached parts. Use actual
process-exit/restart tests rather than treating an in-process reset as a cold save.
Current V2 material snapshots explicitly reject retired incomplete formats.

Assets and source bindings belong to the ordinary dependency/export pipeline.
Prepare a matching SDK, export normally, move the result, and run with the SDK and
source project inaccessible. Private downloaded vehicle models and derivatives
must never enter public SDKs or the engine repository.

Terrain V2 supplies explicit solid ownership for material buried by a published
edit. Instantaneously sculpting solid terrain through an object can impart a
contact impulse; this is not a hidden velocity reset. Soft contact is sampled,
not general deformable-mesh CCD. Dynamic/kinematic capsule creation, character
blocking by arbitrary soft sheets, soft-material buoyancy and per-render-polygon
particle collisions are not implied by this milestone. Actual MK2 structural
section geometry remains available to existing world integrations.

See [implementation and qualification](DEFORMABLES_V2_IMPLEMENTATION.md) for the
measured scope, remaining costs and platform evidence.
