# Particles V2 / Surface Workshop

An editable consumer project for bounded visual particles, native collision and
game-owned deposition. Advanced Fluids are neither enabled nor required.

Open `particle_v2_lab.judasproj` in the updated editor, then press Play. The
starting scene uses small CPU populations; it never automatically enters stress.
Standalone launch: `build/judas projects/particle_v2_lab/particle_v2_lab.judasproj`.

| Control | Action |
|---|---|
| 1 / first button | Surface workshop: reliable paint, bounce tray and native rotating receiver |
| 2 / second button | Explicit 100,000-particle GPU visual stress; requires the qualified compute path |
| 3 / third button | Entire workshop rotated 90°, with actual +X uniform gravity for particles |
| T | Watch camera / mouse-look free camera |
| WASD, Q/E | Move free camera horizontally / vertically |
| P | Pause/resume particle simulation and emission |
| B | Burst particles within existing capacities |
| C | Clear game-owned coating and restore dry body friction |
| F6 | Stop emission, finish/drain captured contacts, pause, then save the world and coating |
| F7 | Load that save; coating reconstructs without saving the visual particle population |
| R | Reset the current authored scene |
| Escape | Pause menu |
| F8 (editor) | Toggle the editor profiler; standalone uses the on-screen counters |

The blue droplets use CPU **reliable** impact delivery and disappear on impact.
Orange particles bounce one-way against real collision geometry and do not
push rigid bodies. The receiver in the right tray uses an actual native
kinematic angular trajectory. Undulating terrain and the cave arch use cooked
triangle collision; collision is independent of camera visibility and render
LOD. The tunnel is deliberately authored two-sided.

`Assets/scripts/lab.js` owns the gameplay meaning of contacts. It drains bounded
native batches during gameplay updates and records count and weight in receiver-
local 0.5-metre cells, plus receiver totals. The colour/roughness of receiving
materials and **actual physical friction** change with coating. The physical
paint floor is one continuous body; 64 authored render-only 1-metre tiles show
its local coating without adding 64 physical actors. Floor friction and the
cave/terrain tint are receiver-wide. This is not a fluid solver, snowpack simulation or per-pixel
decal system. An explicit 4,096-cell game budget retains excess weight/count in
receiver overflow cells rather than silently losing reliable events.

The script state is the durable coating. On restore it reapplies material and
friction from that state. It does not persist or recreate individual visual
droplets, and it does not create duplicate decoration entities. Native pending
reliable records remain the engine's responsibility. F6 stops future emission,
drains queued observations and lets any backpressured captured tick finish.
It then pauses and requests the save at the actual ready boundary, without a
fixed frame delay. P resumes after loading.

HUD counts are actual runtime readback. GPU population samples are asynchronous;
they may lag. The stress scene explicitly requests `gpu`, so an unsupported
backend is diagnosed rather than silently switching 100,000 collision particles
to CPU. CPU reliable effects remain available independently.

## Editing and source content

Select emitters in the Inspector or edit their ordinary scene fields to change
backend, capacity, collision radius/response, gravity, restitution and impact
delivery. Shared public settings use `cpu`, `auto`, `gpu`; `bounce`, `slide`,
`stick`, `remove`; and `off`, `cosmetic`, `reliable`. Reliable gameplay delivery
uses the CPU path. All decisions about what paint means stay in project JS.

The project contains original procedural terrain/cave OBJ source and generator
settings in `Tools/build_lab.py`. It also retains the engine workshop's small
original disc texture/math helper and DejaVu Sans with its licence. No external
service, generated ML model, unrelated game asset or independent fluid research
is used.

Regenerate source content only when intended:

```sh
python3 projects/particle_v2_lab/Tools/build_lab.py --replace
```

Then run `python3 projects/particle_v2_lab/Tools/cook_collision.py` (or pass
`--tool /path/to/sdk/judas_collision_cook`). The helper resolves the two
relative commands in `Tools/COOK_COMMANDS.json`.
The generator deliberately does not launch a build or modify the engine.
The ordinary asset metadata, scene files and project closure support normal
prefabs, editor Play/Stop, saves and standalone export.

## Qualification and human checklist

`Tests/smoke.test` is an automation route through paint/save/clean/load,
watch/free camera, menu pause, real rotated geometry and explicit stress.
Run with a new writable user-data directory and world-state reuse disabled:

```sh
JUDAS_WORLD_STATE=none XDG_DATA_HOME=/tmp/new-particle-workshop-user \
JUDAS_TEST_SCRIPT="$PWD/projects/particle_v2_lab/Tests/smoke.test" \
build/judas projects/particle_v2_lab/particle_v2_lab.judasproj
```

This is functional fixed-clock automation, not performance evidence. The
dedicated `ParticlesV2Benchmark` measures complete simulation and presentation
separately. Source generation/syntax checks alone do not establish gameplay or
export acceptance; record actual runs in the engineering evidence.

- Watch droplets collide and coat individual floor tiles, terrain and cave.
- Check the rotating receiver catches particles between its endpoint poses.
- Pause, save, clean, load: coating totals, colours and friction must return;
  visual droplets begin transiently rather than being part of the saved paint.
- Close the process, reopen/load and repeat that persistence check.
- Try rotated gravity: geometry and acceleration change, not merely the camera.
- Select stress explicitly; inspect actual active backend, population and errors.
- Exercise Play/Stop and a relocated standalone export before acceptance.

Moving concave meshes and arbitrary CPU terrain callbacks are not promised as
GPU collision. Unsupported combinations and bounded sweep exhaustion must
produce diagnostics; they must not be represented as successful no-hit queries.
