# Physics Stress Lab — The Mechanical Arcade

An ordinary Judas project, retained from M87 and used for M91 collision validation. One shared JudasJS controller drives an editable
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

## M91 comparison and remaining limit

M91 keeps these exact authored populations and mechanical settings. The ordinary
lab now exercises zero-load contact identities and interval-checked support
continuation after impacts. It retains the same 60 Hz cadence, materials,
constraints, actual collision geometry and CCD for new encounters. Population
selection never changes simulation quality.

The original M87 seconds-long dense-pile failure remains preserved in its
[historical report](../../docs/evidence/m87/live-slowdown-followup/REPORT.md).
M91's matched measurements and physical checks are in the
[current report](../../docs/evidence/m91/REPORT.md), with the numerical contract
[explained separately](../../docs/M91_COLLISION_CONTINUITY.md).

**The 1,024-piece pile remains an extreme stress workload.** Cost is substantially
improved in the measured failure region, but isolated stalls remain. It is not a
60 Hz certification. The default room is the practical watch-tour demonstration;
press **3** deliberately to inspect the worst case. No body is frozen or removed
to improve the result. M91 was human accepted at the checkpoint recorded in the
[current architecture](../../docs/ARCHITECTURE.md#m91-collision-continuity-retained).
Pre-Alpha changes remain pending separate human acceptance.

## Pre-Alpha Pass2

[Matched Pass2 evidence](../../docs/evidence/prealpha-pass2/physics/REPORT.md)
retains the same 1,024-piece workload and every measured trajectory digest.
Query/event scratch and whole-world teardown improve selected costs; the extreme
pile still has multi-second fixed steps and is not 60 Hz certified. A separate
curved-mesh first-arrival repair passes its asserting contract, while later
one-sided-sheet continuation remains a documented failure. The lab never changes
physics quality to hide these limits.

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
the controlled M87/M91 benchmarks, not visual FPS or HUD sampling alone.

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
M91 evidence report. Human acceptance and Windows validation are separate states.
