# Physics Stress Lab — The Mechanical Arcade

An ordinary Judas project for M87. One shared JudasJS controller drives an editable
room of 32 independent mechanisms, each with a finite-force slider shelf, two
spring-hinged paddles and a free ball. Native rigid bodies, contacts and M45 joints
produce their motion. Motor power can be removed without disabling physics.

Open `physics_stress_lab.judasproj` in the editor, or run its normal exported package.

## Controls

| Input | Action |
|---|---|
| **F5** | Watch tour: drive, power down, then impulse one assembly |
| **M** | Toggle all finite-force motors |
| **J / K** | Impulse one / all articulated shelves |
| **1 / 2 / 3** | Reload with 128 / 512 / 1,024 loose physical pieces |
| **4** | Same 128-piece room with all geometry and gravity rotated |
| **P** | Drop 32 additional bodies; bounded to 256 runtime additions |
| **WASD, mouse, Q/E** | Free inspection camera, down/up |
| **F6 / F7** | Save / load the lab, including physical continuation |
| **R** | Reload authored state |
| **Escape** | Pause/resume and release/capture pointer |

The starting scene contains **256 dynamic bodies**: 96 articulated components,
32 cabinet balls and 128 loose pieces. Higher scenes contain 640 / 1,152 dynamic
bodies. Static cabinet walls and pit walls are real collision geometry. Colored
cabinet headers alone are decoration. There are **96 native joints** in every room.

Sleeping is the existing physical mechanism. Scripts do not freeze, hide, remove,
replace or manually sleep resting assemblies. HUD figures read `entity.sleeping`.
An off motor does not prevent impulses/contact from waking its connected bodies.
Changing population reloads an ordinary scene; it is not a physics quality switch.

**Known performance limit:** the 1,024-piece mixed box/sphere pile can enter a
low-speed continuous-impact cascade after collapsing. Even with the M87 follow-up
optimizations, individual fixed steps can take seconds and temporarily prevent UI
input from being processed. This mode is an unresolved stress case, not a 60 Hz
demonstration. The population, CCD, contacts and simulation cadence are unchanged.
See the [measured investigation](../../docs/evidence/m87/live-slowdown-followup/REPORT.md).

## Project organization

- `Scenes/room128.judas`, `room512.judas`, `room1024.judas`: identical machinery,
  different authored loose-body populations.
- `Scenes/rotated.judas`: whole authoring rotated 0.68 radians about scene Z,
  including gravity and the joint owners' world-side anchors.
- `Assets/scripts/lab.js`: input policy, finite motor commands, tour/camera, HUD.
- `Assets/prefabs`: two ordinary dynamic pieces used by **P**.
- `Assets/ui/stress.judasui`: runtime HUD and pause controls.
- `tools/CreatePhysicsStressLab.py` (repository root): repeatable content authoring.

The free inspection camera is a lab tool, not a CharacterMotor. This room is a
physics workload, not an arcade scoring game. For a real consumer game use the
existing [Tuppence](../tuppence/README.md) project. Before/after timing comes from
the controlled M87 benchmarks, not visual FPS or HUD sampling alone.

## Human checklist

1. Watch **F5**: shelves push balls and spring paddles move through physical contact.
2. Switch motors off with **M**, wait, then check natural sleeping counts.
3. Press **J** near the machines; verify one articulation wakes and reacts.
4. Press **K**; verify independent mechanisms can react simultaneously.
5. Try **1–3**, inspect the pile and use **P** to drop real colliding pieces.
6. Try **4**; motion should follow the rotated gravity, not global up.
7. Save/load while mechanisms are moving; pause, reload and editor Play/Stop.
8. Repeat the ordinary exported package from a different working directory.

Automated benchmark coverage and remaining performance limits are recorded in the
M87 evidence report. Human acceptance and Windows validation are separate states.
