# Powertrain Lab

An ordinary editable Judas project. Open `powertrain_lab.judasproj`; its gameplay,
camera, input and watch-tour policy live in `Assets/scripts/controller.js`.
The engine has no vehicle layout or driver controller.

## The three assemblies

- **Orange buggy:** finite-inertia combustion crank, torque/RPM curve, selectable
  gearbox, bounded clutch, open differential and two driven rear M81 wheels.
  The front wheels retain ordinary independent suspension, steering and brakes.
- **Cyan rover:** six independently powered electric wheel terminals. Each motor
  has authored torque and power limits. No engine, gearbox or differential is
  required for this arrangement.
- **Purple shaft machine:** electric motor, reduction gear and an ordinary
  rigid flywheel constrained by an M45 hinge. Its brake is a mechanical load.
  The visible flywheel rotates because PhysicsWorld moves its body.

The HUD reads solved shaft angular velocities, source torque and link slip.
Nothing in the project assigns an RPM, an acceleration curve or a chassis
position in order to imitate a powertrain.

## Controls

| Control | Project behaviour |
|---|---|
| W / S | Throttle / reverse direction; combustion reverse selects a negative ratio |
| A / D | Steer the first axle of the selected vehicle |
| Space | Mechanical brakes, plus ordinary front-wheel brakes on the buggy |
| Q / E | Lower / higher combustion gear |
| F | Open / engage the combustion clutch |
| Tab | Select buggy, electric rover, flywheel, then any spawned assemblies |
| V | Toggle a project-JS watch tour: free-rev, clutch take-up, drive and brake, then other assemblies |
| B | Add/remove a real 225 kg vehicle payload; on the flywheel, toggle brake load |
| H | Change tyre grip between 1.0 and 0.02 |
| P | Spawn a complete independent combustion prefab |
| F3 | Request/release the registered streamed garage assembly |
| 1 / 2 / 3 | Flat / oblique uniform-gravity / true radial-gravity scene |
| C / mouse | Cockpit/chase presentation, orbit the selected assembly |
| F5 / F8 | Save/load `powertrain-current` using normal Judas save transactions |
| R / Escape | Reload / pause menu |

The starter and low-throttle idle policy are intentionally project JavaScript.
Opening the clutch lets the engine rev without driving the wheels. Re-engaging
it changes RPM through actual mechanical load. Selecting another assembly parks
the vehicles with brakes; it does not freeze them or move their transforms.
The V watch tour covers all three assemblies. The deck is finite, and real
vehicle momentum persists between tours; use steering/braking for prolonged
driving rather than expecting an endless automated course.

The authored scenes keep wheel contact geometry, chassis collision and visual
meshes distinct. `Scenes/radial.judas` uses a real spherical collider and ordinary
RadicalGravity. `Scenes/oblique.judas` rotates the entire authored setup and
Faithful/UniformGravity field; there is no world-Y drivetrain assumption.

## Authoring and reproducibility

`tools/CreatePowertrainLab.py` regenerates scene/configuration/UI/model metadata
and reusable prefabs. The hand-readable project scripts remain separate. Each
assembly stores its canonical generic graph in `powertrain.graph`; the editor
exposes the same Powertrain component. References to chassis bearings, terminal
wheels and the flywheel are ordinary scene entity IDs, remapped by prefabs and
streaming.

`Scenes/contracts.judas` adds a public-API positive/negative fixture for focused
validation; it is not a hidden engine demo mode. Normal scenes do not run it.

See [M82 architecture and limitations](../../docs/M82_POWERTRAINS.md) and the
[human checklist](../../docs/evidence/m82/HUMAN_CHECKLIST.md). Automated passes
do not constitute human acceptance.
