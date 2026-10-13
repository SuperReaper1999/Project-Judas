# Wheel Lab

Open `wheel_lab.judasproj` normally in Judas. The project supplies all driving,
camera, input, UI and watch-tour behaviour in `Assets/scripts/controller.js`.
The engine supplies independent wheel contact, suspension and rolling physics.

## Controls

- **W/S**: forward/reverse wheel torque; **A/D**: steer; **Space**: brake.
- **Tab**: select the other assembly; **mouse**: orbit the follow camera; **C**: close view.
- **V**: toggle a scripted watch tour. It sends ordinary wheel commands; it does not move vehicle transforms.
- **B**: add/remove a real 225 kg payload; **H**: toggle high/low tyre grip.
- **P**: spawn an independent four-wheel prefab; **F3**: request/release the streamed garage.
- **1/2/3**: flat / oblique uniform / actual radial-gravity scenes.
- **F5/F8**: save/load; **R**: reload the authored scene; **Escape**: pause/resume.

## Authoring

Each visual wheel has a `wheel` component referencing its ordinary dynamic
chassis. Its mount and orientation are chassis-local. The axes describe the
mount: suspension down is local -Y, axle is local +X and forward is local -Z.
These local axes rotate with the chassis; they are not a universal world-up.

The teal buggy has four wheels. The orange rover has six wheels, a longer
chassis and different spring settings. Both use the same wheel implementation.
The third scene uses an actual sphere collider and RadicalGravity centred on
that sphere. The oblique scene rotates the entire physical fixture and its
ordinary uniform field together.

The original wheel mesh is flat-shaded and redistributable. Its visible width
is presentation only: the physical tyre uses the documented sphere-cast contact
approximation. The simple cabins are visual geometry; the low chassis box is
the rigid-body collider. This is a compact mechanical lab, not a full vehicle
or drivetrain simulation.

Regenerate the authored assets/scenes with `python3 tools/CreateWheelLab.py`.
The generator does not replace the project controller script.

`jsconfig.json` uses the repository's current `docs/judas.d.ts` for JavaScript
editor completion and API checking. `Scenes/contracts.judas` is a small executed
positive/negative API fixture used by the application tests; it is not one of
the three playable lab scenes.
