# Mechanical Damage Lab

An ordinary editable Judas project for M83. Open `damage_lab.judasproj` in the
editor or launch its exported executable. Controls, watch narration, camera and
HUD belong to `Assets/scripts/controller.js`; physical deformation and failure
belong to the engine.

## Start with the structural crash

`Scenes/vehicle_crash.judas` contains two compact cars and real solid walls.
Press **V** to watch equal driver inputs accelerate both cars into their walls.
The camera follows from the side so the front structure remains visible.

Orange has a **load-bearing, plastic and fracturable front chassis**. Its hood
is the actual deforming material surface, rather than a rigid cosmetic cover.
Impact first compresses/yields that structure; internal connections can then
tear progressively and separate the complete front axle/nose, including both
front wheels. The front drive requires actual material continuity and loses
transmission when that path breaks. Rear drive remains independent.

Green has the same geometry, masses, material response and drive inputs, with
10× stronger internal connections. It can yield under impact while retaining
its structural and mechanical path. Changing authored strength changes the
physical outcome. The cabin and front axle are rigid modules: this is a small
coupled structural demonstration, not whole-car sheet-metal crash simulation.

### Actual construction

- Compact wheel placement: centres ±0.85 m, radius 0.32 m; no outboard bearing cubes.
- Rear body/cabin: 120 kg. Front axle/nose: 40 kg with ordinary collision.
- Front material: 1.20 × 0.28 × 0.90 m, 30.24 kg, eight cohesive cells,
  64 nodes / 48 tetrahedra. Total car mass: **190.24 kg**.
- Two unlimited multi-node body attachments transmit loads between those bodies.
  There is no rigid collider spanning/bypassing the material section.
- Weak section capacities: tension 100 kPa, shear 70 kPa, compression 1.2 MPa.
  Strong capacities are 10× those values. Ductile compression capacity permits
  plastic forming before tensile/shear tearing; these are game-authored values,
  not a calibrated automotive material specification.
- Both use the same yield strain 0.08, plastic rate 10 s⁻¹, shear compliance
  2e-7, four material substeps / 12 iterations, and normal **60 Hz** physics.
- Both use ordinary 80 N m / 6 kW electric sources, clutch links and M81 wheels.

The watch tour supplies throttle/brakes only. It never sets damage flags, cuts
bonds, changes a wheel's mount, prescribes bent geometry, launches a hidden ram
or resets velocities. A separate held-throttle check proves that physical loss
of the front drive does not come from the watch policy releasing its commands.

Environment, rigid assembly and material collision layers are explicit project
content. Attached material avoids its own pin bodies while still colliding with
ordinary environment geometry. Detached rigid modules keep mass, collision and
inherited motion. Visual tyres follow the existing physical wheel state.

## Controls

| Control | Action |
|---|---|
| V | Start/stop the ordinary watch tour |
| W / S | Forward / reverse source input |
| A / D | Steer the front wheels |
| Space | Mechanical brakes |
| Tab | Select the other car or an independent spawned assembly |
| Mouse | Orbit the presentation camera |
| P | Spawn a complete referenced car prefab on a spare deck position |
| 1 | New structural crash lane |
| 2 / 3 | Original bearing/load fixtures: flat / oblique gravity |
| 4 | Earlier wheel-mount crash experiment |
| F5 / F8 | Save / load actual physical damage and mechanical state |
| R | Reload the intact authored scene; replay with V |
| Escape | Pause / resume |

The tour stops driving and applies ordinary brakes after observed damage so the
result can be inspected. Tab compares the stronger car. Save/load retains actual
material positions, plastic rest, broken topology and physical bodies, rather
than a damage label. Two prefab cars can be spawned independently.

## Editing and retained experiments

Select the front material in the hierarchy to inspect/edit its generic plastic
material and named body attachments. Its `.source` asset explicitly authors the
M79 cell/bond partition and section strengths; bake with the normal
`judas_deformable_bake partition` tool. Select the root's powertrain to inspect
front links' material-path prerequisites. Existing editor facilities remain the
authoring path; no special vehicle/damage editor is required.

`Assets/prefabs/vehicle_car.judasprefab` packages the complete assembly and normal
reference remapping. The construction recipe is `tools/CreateMechanicalDamageLab.py`;
`--vehicle-only` emits the new payloads without rewriting retained fixtures.

The operator rejected the earlier demonstrations as inadequate vehicle crashes.
Their scenes, cooks and evidence remain preserved:

- **4 / prior crash:** separate physical wheel carriers and finite mounts. It
  proves mount release, but is not the new chassis-destruction demonstration.
- **2 / flat fixtures:** orange plastic arm, green stiffer control, purple
  cohesive bench and two suspension-supported buggies. B toggles sustained
  loads, I launches a real ram, H toggles overload. V runs their original tour.
- **3 / oblique fixtures:** corresponding physical geometry and real gravity
  field are rotated together.
- F3 requests/releases the retained annex in those original scenes; Tab selects
  it. Save, revisit and inspect its retained deformation/connection state.

The shared project script keeps these older controls and construction paths.
Safe-handle caches for the new fixed prefab hierarchy avoid repeatedly copying
its whole scene; physical state and presentation poses are always read live and
caches rebuild after restoration.

## Human checklist

1. Press V: both compact cars should visibly drive into their solid walls.
2. Watch orange's front structure deform, tear and separate with its complete
   front axle. Inspect the lost front drive; compare green's connected result.
3. R restores the authored cars. Try normal drive, steering and brakes.
4. F5 saves a damaged car; F8 restores the same physical damage.
5. P spawns another independent assembly; Tab selects it.
6. Escape pauses/resumes; reload and stop leave no stale state.
7. Visit 2/3 for load, plastic, retained streaming and oblique-gravity fixtures.
8. Repeat the crash/save controls in the exported standalone.

## Limits and evidence

This is an approximate small tetrahedral/rigid assembly, not industrial crash
FEM, arbitrary mesh cutting, deformable tyres or a simulation of every car panel.
Plastic rest flow conserves its authored rest volume. Extreme folding and
out-of-envelope solves retain explicit existing diagnostics. Strength/compliance
and solve quality need appropriate authoring; this does not promise stable
arbitrarily soft drivetrains.

Current implementation/limits: `docs/M83_MECHANICAL_DAMAGE.md`.
Original failures and measured follow-ups: `docs/evidence/m83`,
`m83-crash-followup`, `m83-collision-followup`, and **`m83-vehicle-followup`**.
Automated Linux results do not imply human acceptance or native Windows proof.
