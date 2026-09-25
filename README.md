# Project Judas

**Judas is a game engine.** It is purpose-built for one class of game —
worlds that can range from a tiny flat-terrain level to planetary and
universe scale, with spacecraft, arbitrary gravity, terrain, liquids, gas
and seamless transitions between planets, ships and open space — and it is
deliberately narrow: it is not trying to compete with Unity, Unreal or
Godot. The planets, plank, spacecraft, terrain, lake, atmosphere, fire,
doors and other objects you will meet below are the engine's **technology
demonstration**: scene content that validates engine capabilities, not the
engine itself and not "the Judas game."

Rendering targets **OpenGL 3.3 Core / GLSL 330**. OpenGL entry points are
loaded with the vendored GLAD 2.0.8 OpenGL 3.3 Core loader through the
SDL-created context; generation and license provenance are recorded in
[`third_party/glad/README.md`](third_party/glad/README.md).

## Status: Milestone 28 in operator validation

**New in M28 — Judas learns to be an editor.** Three things now exist:

1. **Real scenes.** Authored world content lives in a `Scene` (`src/Scene.h`):
   objects with stable ids, names, transforms and a small set of generic
   engine components (render, body, gravity region, light, door, light
   switch, vehicle, celestial, atmosphere, combustible, fluid volume,
   player start). A scene knows no demo concepts — "Planet A" and "the
   plank" are just names in a file.
2. **Scene files.** `.judas` is a deterministic, human-readable, versioned
   text format (`src/SceneSerialization.h`) with explicit ids and strict
   loading: malformed, truncated, unknown-key or version-mismatched data
   fails with a line number and leaves nothing half-built. The seven
   demonstrations ship as files under `assets/scenes/`.
3. **An editor.** `judas_editor` (`src/editor/`, built on the vendored
   Dear ImGui) opens, creates, inspects, edits, saves and *plays* scenes:
   a real 3D viewport with a free camera, a hierarchy, an inspector for
   transforms and every component, generic object creation/deletion,
   snapshot undo/redo, and an explicit **Edit / Play** split — Play
   instantiates the authored scene into a runtime world driven by the
   exact frame the `judas` runtime runs; Stop discards that world and the
   authored scene is untouched.

`Application.cpp` (2,404 lines at M27, holding every demo constant and the
whole loop) is now 116 lines of orchestration; its responsibilities moved
to `RuntimeWorld` (scene instantiation), `Simulation` (fixed-step order),
`WorldPresentation` (drawing/lights/shadows), `GameSession` (player,
vehicle, carrying, interaction), `InteractivePlay` (the per-frame loop
shared by runtime and editor), `GameplayHud`, `RuntimeOptions`,
`RuntimeDiagnostics` and `EngineHost`. Both demonstrations run through the
scene path: the `judas` runtime loads a file and instantiates it; nothing
is rebuilt from constants. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 28," for the boundaries, the file format, what moved where, the
measured regressions, and the limits (no gizmos, no parenting, one vehicle
and one atmosphere per scene, no OS file dialog).

The M28 portability fix: `CMakeLists.txt` no longer requires GLM's
`glm::glm-header-only` target (GLM 1.0+ only) and configures against the
`glm::glm` target that Ubuntu 24.04's `libglm-dev` 0.9.9.8 exports.

All 28 standalone suites pass; the classic and terrain 420-step gameplay
harness runs are byte-identical near and far origin; the classic run's
player and first six body columns are byte-identical to the M27 build.
Operator validation of the editor is pending; no `milestone-28` tag exists.

## Quick start

```bash
sudo apt install cmake libsdl2-dev libglm-dev build-essential   # Ubuntu/Debian
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

./build/judas                                   # runtime: the terrain demonstration
./build/judas assets/scenes/classic.judas       # runtime: any scene file
./build/judas_editor assets/scenes/classic.judas   # editor: open a scene
./build/judas_editor                            # editor: start a new scene
```

Run everything from the repository root: scene files reference assets by
paths relative to the working directory.

## Scenes

A scene is a plain text file. The header names the format and version;
`settings` carries scene-wide values; each `object` block has an explicit,
never-reused id, a name, a transform, and any components it uses:

```
JudasScene 1
settings
  name "Classic abuse chamber"
  world-origin 0 0 0
  sun-direction 0.4 0.7 0.35
  sun-color 1 0.98 0.92
  ambient 0.16 0.17 0.19
  fluid-scale 1
  next-id 26
end

object 2 "Planet A"
  position 0 0 0
  rotation 1 0 0 0
  scale 1 1 1
  render sphere
  render.radius 20
  render.color 0.3 0.45 0.35
  ...
  body static sphere
  body.radius 20
  ...
  gravity radial 9.81
  gravity.region sphere 26
end
```

Every field the writer emits, the reader requires — nothing is defaulted
silently. Saving is deterministic (objects in scene order, fixed key order,
shortest exact decimals), so scene files diff cleanly under version
control. The full grammar and every component's fields are in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 28, Scene file
format."

Shipped scenes (`assets/scenes/`):

| File | Content |
|------|---------|
| `terrain.judas` | The M25–M27 terrain planet: lake, atmosphere, spacecraft, fuel blocks (default runtime scene) |
| `terrain_rotated.judas` | The same, with the planet rotated 47° (the former `JUDAS_TERRAIN_ROTATED`) |
| `terrain_atmospheric_pass.judas` | Pilot attached, ship at the M26 analytical apoapsis (the former `JUDAS_ATMOSPHERIC_PASS`) |
| `terrain_atmospheric_pass_rotated.judas` | Both of the above |
| `classic.judas` | The two-planets-and-a-plank abuse chamber with the M24 cups, orbital bodies, door, switch, stairs, ramp, beacon |
| `classic_fluid_rotated.judas` | Classic with the M24 rotated fluid-station gravity region |
| `classic_fluid_zero.judas` | Classic with a zero-gravity fluid-station region |

The pre-M28 environment switches still work and now select these files:
`JUDAS_CLASSIC_DEMO=1`, `JUDAS_FLUID_GRAVITY=rotated|zero`,
`JUDAS_ATMOSPHERIC_PASS=1`, `JUDAS_TERRAIN_ROTATED=1`. An explicit
`./build/judas <file>` argument wins over all of them. `JUDAS_WORLD_OFFSET`
(`far` or `x,y,z`) still overrides the scene's authored world origin.

## The editor

`./build/judas_editor [scene.judas]` opens a scene (or starts an empty one).
The window is the viewport; panels sit over it.

| Editor action | How |
|---------------|-----|
| Look around | hold the **right mouse button** and move the mouse |
| Fly | with the right button held: `W`/`A`/`S`/`D`, `E` up, `Q` down, `Shift` faster |
| Select | **left-click** an object in the viewport, or click it in **Hierarchy** |
| Focus the selection | `F`, or Hierarchy → Focus |
| Create objects | **Create** menu: empty, static/dynamic box or sphere, mesh, point/spot light, player start (placed 8 m ahead of the camera) |
| Delete | `Delete`, Hierarchy → Delete selected, or the right-click menu (which also reorders) |
| Edit | **Inspector**: name, position, rotation (degrees), scale, every component's fields, Add component / Remove |
| Scene-wide values | **Scene** panel: name, world origin, sun, ambient, fluid scale |
| Undo / redo | `Ctrl+Z` / `Ctrl+Y` (Edit menu) |
| Save / open | `Ctrl+S`, File → Save / Save As… / Open… (a path field; no OS dialog yet) |
| **Play** | File-bar **Play** or `F5`: the authored scene is instantiated and played with the normal Judas controls below |
| While playing | `Escape` opens the M13 pause menu and frees the mouse so the editor panels are usable again; the inspector is read-only |
| **Stop** | **Stop** or `F5`: the runtime world is discarded and the authored scene is exactly what it was |

Runtime changes made during Play (thrown crates, burnt fuel, moved water)
are never written back to the scene. "Apply runtime state to the scene"
is a possible future feature, not an M28 one.

Viewport transform gizmos, object parenting, an asset import pipeline and
an OS file dialog are deliberately not part of M28 — see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 28, Deliberately
not implemented."

## Milestone 27 accepted baseline

**New in M27:** three ordinary pickable rigid blocks on the terrain
spacecraft carry finite combustible coatings. Their temperatures, remaining
fuel, burn rates, heat output, and local oxidizer supply are fixed-step
thermal state. Hold `C` to power an `18 kW` point heater carried by the
player's local look frame. Its radiated energy reaches each block by
geometric interception; there is no `Ignite` command. Once a block is hot
enough, atmospheric oxidizer permits a fuel-limited reaction that releases
heat. Remaining fuel uses double precision so tiny thin-air burns still
consume the mass that accounts for their released heat. Pairwise radiation
can heat a nearby second block; the farther third
block tests that proximity alone does not spread fire. These remain physical
bodies that can be picked up with `G` and thrown with `H`. The ship can carry
them into thin atmosphere or vacuum using ordinary contact and thrust.

Fire requires the M26 gas's sampled oxidizer density. The atmosphere now
also reports a temperature consistent with its existing polytropic pressure
and density. Gas composition and profile remain prescribed: combustion does
not deplete local oxygen or simulate evolving hot-product parcels. Small
flame/smoke spheres follow measured burn rate, temperature, local gravity,
and gas-relative flow for presentation only. They do not cause heat or
spread. In zero gravity with no relative flow, they form a symmetric halo.
The HUD exposes heater state, player-local oxidizer, and all three blocks'
temperature/fuel/burn rate. `R` resets their physical and thermal initial
state. `JUDAS_FIRE_DIAGNOSTICS=1` prints thermal measurements. M27 has
passed operator validation.

For the ignition exercise, approach the close pair of brown blocks on one
side of the ship deck. Hold `C` to show the small glowing heater tip; aim
it at the first block and keep heating until **Fuel A** is around `700 K`.
The reaction begins above `550 K`, but just crossing that threshold may
not sustain it after the heater is removed. Release `C` and watch **Fuel B**
heat from A's radiated energy. The separate block
on the other side of the deck is the distant comparison. These are
ordinary pickup targets under the existing `G` prompt and can be thrown
with `H`. The readouts let you distinguish heating, active combustion and
fuel exhaustion even before or after visible flame.

For the live cargo ascent, board with `F`, enable the existing SAS with
`X`, then hold `E` to ascend. The blocks ride by ordinary contact; an
off-centre load can tumble the ship and fall away with SAS off. SAS
provides corrective torque, never a cargo attachment.
If you picked a block up first, drop it back onto the deck with `G`
before boarding; taking pilot control releases held objects.

In a stationary fixed-step calibration at the *authored terrain positions*,
heating A to `700 K` took `4.50 s`; after heater release its neighbour B
ignited at `12.95 s`, while a block about `4.22 m` away remained unlit
through `60 s`. The isolated thermal
step averaged about `0.58 µs` on this host. A separate real-physics cargo
fixture carried a burning block past the `110 m` gas top; reaction stopped
in vacuum. With the actual three-block offsets and SAS enabled, all three
crossed the gas top together at fixed step `224`. Without SAS, the
off-centre A block fell away in the tested high-thrust ascent. These are
focused measurements, not a substitute for hands-on validation. `R`
restores thermal state and body poses; if `C` remains held,
the heater resumes on subsequent gameplay steps.
An unattended live terrain run with no heater engaged reported
`0.00521`, `0.00458` and `0.00430 ms` per thermal step at steps
`600/1200/1800` (including physics pose and gas samples); all blocks
remained unburned at step `600`.

## Milestone 26 accepted baseline

**New in M26:** the M25 terrain planet has a bounded gas atmosphere with
spatial mass density, pressure, and planet-frame velocity. A finite
hydrostatic polytrope (`P = K rho^1.4`) balances an inverse-square source
matched to `9.81 m/s^2` at the `80 m` reference radius. Density falls from
`0.05 kg/m^3` and pressure from `3.058 Pa` there to smooth, exact vacuum at
`110 m` radius. Actual terrain excludes gas from solid ground. The gas is a
prescribed equilibrium continuum, not a second M24 liquid solver or a
weather simulation; it does not evolve a wake or receive reaction momentum.

The existing spacecraft now spawns on a measured clear patch near the terrain
player, visible and boardable with `F`. It receives the terrain planet's
inverse-square pull and box-orientation-dependent drag from its velocity
*relative to the gas*, both applied through Judas's ordinary rigid-body
forces. The player, water, and ordinary props retain their accepted local
gravity context. The classic M20/M21 binary flight scene remains available
with `JUDAS_CLASSIC_DEMO=1`. For a reproducible atmospheric pass, run
`JUDAS_ATMOSPHERIC_PASS=1 ./build/judas` (or open
`assets/scenes/terrain_atmospheric_pass.judas`): the attached pilot and ship start at
an analytical apoapsis position/velocity (`130 m`/`100 m` initial apoapsis/
periapsis), then evolve only from forces and contacts. Look toward the planet
below the ship to watch the pass. `R` restores that initial physical state.
`JUDAS_TERRAIN_ROTATED=1` and `JUDAS_WORLD_OFFSET=far` still select the rotated
and far-origin terrain demonstrations. `JUDAS_ATMOSPHERE_DIAGNOSTICS=1` prints
gas/step cost, density, pressure, airspeed, drag and orbital specific energy
while the interactive scene runs. Two faint rendered shells visualize the gas
extent; they never determine force or pressure.

The focused M26 tests verify hydrostatic balance, smooth vacuum, moving-frame
relative velocity, orientation-dependent drag through `PhysicsWorld`, and an
actual orbital energy loss. In the measured headless pass, the post-pass
apoapsis was `127.903 m` versus `130.269 m` in its gravity-only control.
The light `80 kg` spacecraft exposes the M25 coarse lake solver's known
mass-ratio limit on a steep water entry. Its hull still displaces water,
but lake reaction impulses are not returned to the ship in the terrain
scene; ship–water exchange is one-way and does not conserve their combined
momentum or energy. This keeps the accepted M24/M25 heavy-body coupling and
ordinary ship/terrain collision intact; it is not a buoyancy model.
An unattended Release run measured about `0.0014 ms` for planetary force plus
gas/drag evaluation and `5.13 ms` per full fixed step with M25 water and
terrain active on this host; it is a scene-specific CPU result, not a GPU
benchmark.
All 26 standalone M26 suites passed. The operator accepted M26 and it was
committed as `b225be7`, tagged `milestone-26`, and pushed. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 26," for the model,
measurements, and limits.

## Milestone 25 accepted baseline

**New in M25:** the default interactive launch now starts on an authored
`80 m`-base-radius terrain planet, at a basin containing the same authoritative
M24 fluid solver. Hills, slopes, two depressions, and a low connecting channel
come from one continuous planet-local radial-height function. The visible
mesh, player support, rigid-body contact, and fluid collision query that same
surface; gravity remains a separate radial field. The initial lake experiment
uses 125 coarse particles at `0.5 m` spacing, `125 kg` each (`15,625 kg`
total). Hold `B` to add real particles above the first basin, up to 200
particles (`25,000 kg` total), so water can cross the low route under its own
motion. Each added particle contributes `125 kg`; this is deliberate external
matter input, not water created by the solver. `R` restores the original
particle set. The terrain view starts in first person (`V` toggles view).
An ordinary dense orange `2,000 kg` dynamic box near spawn can be picked up and thrown
with the existing `G`/`H` controls to disturb the water.

Run `JUDAS_CLASSIC_DEMO=1 ./build/judas` for the accepted M24 cups and
earlier player demonstration. The scripted gameplay harness keeps that
classic scene by default; `JUDAS_TERRAIN_PREVIEW=1` opts it into the M25
starting arrangement. `JUDAS_TERRAIN_ROTATED=1` rotates the terrain body
and its authored spawn/fluid setup; `JUDAS_WORLD_OFFSET=far` places either
scene at the M23 far absolute origin without enlarging local float
coordinates. One focused test starting with 200 particles counted 49 in
the second basin after six seconds. A separate test reproduces the live
`B` schedule: after two seconds of settling and 75 emitted particles,
44 occupied the second basin at eight seconds versus five without `B`;
all 44 were tracked through the low saddle. All 24 standalone suites and
near/far gameplay harness runs pass; the operator accepted the live M25 demo. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 25," for the surface and numerical limits.

A coupled lake-scale regression throws the actual `2,000 kg`, `0.9 m` box
through settled water at `8 m/s`; fluid contact impulses act back on the
real Judas rigid body, which slows to `2.913 m/s` in the measured fixture.
The coarse solver is not validated for light bodies at this lake particle
mass; a measured `80 kg` case became numerically unstable. The limit and
control-adjusted energy measurement are documented in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

For a settled first-person review image, run
`JUDAS_TERRAIN_SCREENSHOT=/tmp/judas-terrain.png ./build/judas` from the
repository root. The optional capture occurs after 600 fixed simulation
steps and leaves simulation unchanged.

**New in M24:** a small water volume now has its own authoritative particle
positions, velocities, masses, and pressure response. Two open cups on a table
near the Planet A spawn are ordinary dynamic compound-box bodies: each has a
bottom and four walls, with no invisible lid or stored `waterAmount`. Cup A
starts with the water; Cup B starts empty. Pick up a cup through the existing
`G` interaction, move it with the player, and look up or down to tip it. `G`
drops the held cup and `H` throws it. The fluid particles can cross a rim,
travel through space, contact the other cup, and leave it again; there is no
container-to-container transfer command. The automated M24 checks pass, and
the operator accepted the live first-person fluid view.

The bounded CPU solver uses position-based density constraints at the fixed
simulation step. It samples each particle's gravity from Judas's existing
local gravity contexts, including zero acceleration where no context applies.
Ordinary rigid boxes and spheres block the fluid; their previous and current
poses provide moving-wall motion, and fluid contact impulses are returned to
dynamic rigid bodies. A lit surface mesh is rebuilt from presented particle
positions for display only. It never determines fluid motion. The current
demonstration starts with 125 particles; this is a small interactive pour,
not an ocean or a general-purpose fluid framework. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 24," for the method,
numerical limits, and measured results.

**New in M23:** Judas now represents the active scene as small float
simulation/render coordinates plus a double-precision absolute coordinate
origin. Physics, gravity, reference frames, camera, lights, interaction, and
shadows continue to operate on the same precise local positions. The absolute
position of any object is `world origin + local position`; translating the
entire scene changes only the origin. This keeps the existing OpenGL 3.3
renderer and physics laws intact. The HUD shows the coordinate origin and
player's absolute position.

Run the ordinary scene at the tested billion-metre translation with:

```bash
JUDAS_WORLD_OFFSET=far ./build/judas
```

Unset the variable to run near zero. A custom `x,y,z` metre offset is also
accepted, for example `JUDAS_WORLD_OFFSET=1000000000,-2000000000,3000000000`.
`R` resets the same local scenario at the chosen absolute location. There is
no runtime rebasing or streaming yet; the active local scene must stay small
enough for float simulation. M23's measured range and limitations are in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 23." Human visual
validation has passed.

**New in M20:** the interactive scene includes two massive dynamic spheres in
unclaimed space. Their initial positions and velocities are the analytical
circular two-body values; mutual Newtonian forces and Judas's ordinary fixed
step integration produce their later motion. Both orbit their shared
barycentre. Hold `P` for prograde, `M` for retrograde, or `N` for outward
radial thrust on the cyan body; each applies a real force of `2e14 N` through
`PhysicsWorld::ApplyForce`. `R` restores both bodies and their initial orbital
velocities. The bodies are deliberately excluded from the permanent gameplay
harness; headless tests exercise the same force and integrator.
Measured at 60 Hz over five revolutions: period `8.950 s` vs analytical
`8.936 s`, separation range `29.826–30.178 m`, maximum relative energy error
`0.0138%`, angular-momentum error `0.000599%`, and barycentre drift
`0.061 mm`. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 20," for the
complete setup, limits, and measurements. M20 was accepted; this remains a
separate orbital demonstration.

**New in M21:** the 80 kg spacecraft now receives pairwise celestial gravity
from both orbiting bodies while retaining its independent 6DOF controls. When
piloting, press `X` to toggle SAS: it applies inertia-compensated
counter-torque to stop rotation and hold the attitude captured at activation.
The HUD shows a dedicated `Spacecraft SAS: ON/OFF` line. SAS never brakes
translation; rotational pilot keys are ignored while attitude hold is active.
A follow-up release-safety correction moves the player just clear on the
gravity-facing side if the craft rolled them below its hull. Ordinary movement
then resumes; the player still inherits the velocity of the original
attachment point. Zero gravity does not invent an exit side.
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 21," for the
control law, tests, and limitations. M21 is accepted.

**New in M22:** `ReferenceFrame` converts positions, directions, and
velocities between world coordinates and a translating/rotating frame. Frame
velocity includes the actual point velocity from `omega x r`. The HUD shows
world speeds for the spacecraft, orbiting body A, and pilot, plus the
spacecraft's relative speed to body A and the pilot's relative speed to the
spacecraft. These readouts do not alter physics, gravity, support, or
attachment. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 22."

A controllable player walks, jumps, and falls under real physics — Judas's
own physics engine, not a third-party library. The retained classic scene
has two independent
spherical worlds ("planets," radius `20m` each, `55m` apart) connected by a
flat plank. Each planet has its own **radial** gravity pulling toward its
own center; the plank has its own **uniform** gravity matching its own flat
surface. There is no universal "up": the player's own sense of up
continuously reorients to match whichever gravity context currently
governs it, and which context governs a given position is decided by
simple ownership — a position belongs to exactly one world, never a blend
of two. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the two
earlier gravity-context designs that each passed automated checks and
still failed interactive validation before this one, why, and why Jolt
Physics was removed entirely along the way.

Both planets and the plank host ordinary dynamic bodies (cubes and
spheres) that sample the same Judas-owned gravity the player does, purely
from their own position — proof that gravity was never player-specific or
region-specific. They fall, land, roll, and collide with the world and
each other under real physics. The player can target the six ordinary demo
objects and the two M24 cups with `G` to pick one up, carry it with
physics-backed forces, use `G` to drop it, or `H` to throw it along the current
look direction. A held cup also receives torque to follow the player's
look-relative orientation; the six older single-shape objects retain their
M18 carry behavior.

The player can walk from Planet A, onto the plank, across it, onto
Planet B, and back. Gravity hands off coherently at every boundary,
support is always collision-derived (never a gravity-region event), and
the plank reads as ordinary flat ground everywhere on its surface,
including its edges — no sideways pull toward either planet, anywhere on
it. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full,
honest retrospective on how two earlier attempts got that wrong.

**New in M19:** grounded support clearance is settled from the player's
current collision probe on every fixed step, and small gravity-driven frame
rotations are no longer discarded. The latter prevents the view frame from
holding for several ticks and snapping as the player walks around a sphere.
Automated checks cover long curved walks, direction changes, frame tracking,
flat and rotated controls, and landing. Interactive visual validation is
complete.

**New this milestone:** a controllable flying primitive rests on the
plank. Walk (or hop) onto it and press `F` to take control — WASD/Q/E fly
it around in full 3D, A/D turn it — then press `F` again to hand control
back to the player. The player stays a real, physically simulated
participant throughout: standing on the primitive while it translates,
climbs, or turns carries the player along coherently, and jumping off (or
letting go of control while it's moving) preserves whatever motion it was
imparting, rather than snapping the player back to a fixed offset. See
`docs/ARCHITECTURE.md`, "Milestone 8," for how input authority moves
between the player and the primitive and the moving-support physics fix
that made carrying the player during flight work correctly.

**New this milestone:** the renderer can load a real static model from
disk, texture it, and light it. A small hand-authored beacon (a low-poly
pyramid, `assets/models/beacon.obj`) stands near the player's spawn point
on Planet A, wearing a real texture (`assets/textures/beacon.png`) and
shaded by one small ambient term plus one directional light — every
existing box and sphere in the scene (planets, plank, player, dynamic
bodies, the flying primitive) now carries real surface normals and is lit
the same way, through the same shader. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 9," for the
model/texture format choices, the mesh/texture ownership boundary, and the
exact lighting model.

**New this milestone:** ordinary walking is deliberately pleasant to play,
not just functionally correct. Starting, stopping, and reversing direction
now accelerate and decelerate smoothly instead of snapping instantly to a
new velocity; the player can nudge their movement modestly while airborne
without losing existing momentum; and a short staircase plus one ramp
(a walk from spawn on Planet A) can be climbed and descended just by
walking into them — no jump required. The same mechanism makes the flying
primitive's own low edge naturally boardable now too. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 10," for the
exact acceleration model and the step-up/step-down design (including a
real edge-case bug found and fixed along the way).

**New this milestone:** the flying primitive from Milestone 8 is now a
proper spacecraft. Board it and press `F` as before, but control is now
full 6-degree-of-freedom: translate along all three of the spacecraft's
own local axes and pitch/yaw/roll it independently, entirely relative to
its own current orientation, never a fixed world direction. The player is
now **physically secured** to it while piloting — not merely carried like
an ordinary moving platform (Milestone 8's mechanism, still exactly what
runs whenever nobody is piloting it) — so rolling the spacecraft upside
down under gravity does not make the pilot fall off; it stays exactly
where it sat down through any combination of translation and rotation.
Releasing control (`F` again) preserves the player's exact position and
orientation and hands it the spacecraft's own real velocity at that
instant, including the extra motion a rotating spacecraft imparts at an
off-center point — no reset, no snap to a fixed seat, no freeze. It now
renders as a real imported model (`assets/models/plane.obj`) instead of a
plain box, through the same Milestone 9 model/texture/lighting path. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 11," for the
secured-pilot attachment mechanism. The project still has no general vehicle
framework, artificial gravity, or multiple spacecraft. M21 adds Newtonian
gravity from the M20 celestial bodies and one attitude-hold mode, but no
scripted orbital behavior or navigation systems.

**New this milestone:** the spacecraft's controls are genuine force/
torque-driven inertia, not a directly commanded speed. Holding a
translation key applies real force (accelerating it gradually, per
`F = ma`, using its actual mass); holding a rotation key applies real
torque (spinning it up gradually, per its actual inverse inertia tensor —
pitch, yaw, and roll genuinely accelerate at different rates, since the
spacecraft isn't shaped the same along all three axes). **Releasing every
key does not stop it** — with no force acting, it keeps moving at exactly
the velocity it had; with no torque acting, it keeps rotating at exactly
the angular velocity it had. Turning the nose does not turn existing
momentum: build up speed, let go, spin the ship around, and it keeps
travelling the original way — now effectively flying backwards — until
thrust is applied against that motion to actually slow it down, stop it,
and reverse it. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 12," for the exact force/torque magnitudes, the real numeric
evidence for all of this, and a couple of honestly-documented rough edges
(rotating while still resting on the ground is realistically stiff;
a very hard, fast collision can shed more speed than gentle friction alone
would suggest). That's it — no mantling, climbing, terrain, real planets,
more than one controllable vehicle, shadows, or further gameplay yet.

**New this milestone:** Judas has its own UI system — a persistent HUD and
a working pause menu, both rendered through a new screen-space overlay
path behind the same raw-GL boundary every other draw call already
respects. A small panel in the top-left corner always shows five genuinely
live values: grounded/airborne, current local gravity magnitude, whether
you're controlling the player or the spacecraft, pilot-attachment state,
and the spacecraft's current speed. Press `Escape` to pause: the world
freezes completely (see below), the screen dims, and a menu appears with
`Resume`, `Options`, and `Quit` — `Options` is one nested screen with a
real "Show HUD" toggle. Navigate with the arrow keys and `Enter`, or point
and click with the mouse (the cursor is released automatically while a
menu is open, and recaptured the instant it closes). `Escape` itself is
context-sensitive: it opens the menu from gameplay, backs out of the
nested screen to the root, and closes the menu entirely (resuming) from
the root — matching the required flow of gameplay -> pause -> nested ->
back -> resume -> gameplay. **Pausing freezes the simulation completely**,
not just input: no physics stepping, no gravity, no player movement,
while paused — chosen deliberately over "keep simulating behind the menu"
so a coasting Milestone 12 spacecraft doesn't keep drifting while its
pilot is stuck in a menu unable to react. Text is drawn with a newly
vendored font loader (`stb_truetype`, baking DejaVu Sans into one GPU
atlas — see `assets/fonts/DejaVuSans-LICENSE.txt` for its license). See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 13," for the
full design, the single input-ownership boundary that keeps gameplay code
from needing to know a menu exists, and the pause/simulation policy.

**New this milestone:** Judas has dynamic lighting. Press `T` to toggle a
player torch — a spotlight that originates from the player's own eye
position and points exactly where you're looking, built fresh every frame
so it never lags behind your view, works identically on Planet A, on the
plank, on Planet B, and under any local-gravity orientation. The
spacecraft carries its own small light rig: one forward-facing headlight
and two wingtip navigation lights (red to port, green to starboard) —
defined entirely relative to the spacecraft's own frame, so they stay
correctly attached through translation, pitch, yaw, roll, Milestone 12
inertial coasting/tumbling, and any gravity context (including zero
gravity) with no special-casing. Both light kinds use a smooth (never
harsh/binary) falloff — a distance-based attenuation that fades to zero
at a fixed range, and a spotlight cone that fades gently from full
brightness at its center to nothing at its edge — and combine additively
with the existing ambient/directional lighting from Milestone 9, never
replacing it. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 14," for the full design and the exact attenuation/cone
formulas.

**New this milestone:** those lights now cast real shadows. The
directional sun, the player torch, and the spacecraft headlight each
render a shadow map every frame (standard shadow mapping, with a small
soft-edged filter so shadow edges aren't harshly aliased) — walk around
an object and its shadow stays spatially correct; turn the torch on and
watch it block light on whatever's between it and a surface; board the
spacecraft and its headlight casts a shadow that stays attached through
pitch, yaw, roll, coasting, and tumbling, exactly like the light itself
already did as of Milestone 14. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 15," for the
full design. **Honest limitations, not oversights:** the two wingtip
navigation lights still cast no shadows; the directional shadow only
covers a bounded area around the player (not the whole world at once,
and not cascaded); and a surface just outside a light's own shadow
frustum is treated as unshadowed rather than checked at all.

**New this milestone:** Judas has its first environmental interaction
system. Walk up to the door on Planet A (a short walk from spawn) and a
prompt appears; press `G` and it swings open, blocking your path when
closed and letting you walk through once open — press `G` again to close
it. (`E` was the first choice, but it's already the spacecraft's own
"ascend" control — see the spacecraft's own key table below — so `G` was
used instead to avoid a real conflict.) A small lever beside the door
uses the exact same prompt-and-`G` interaction, but does something
completely different: it toggles a nearby lamp on and off, proving the
interaction system isn't secretly built just for doors. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 16," for the full design.

This is intentional — see `docs/ARCHITECTURE.md` for why, what's
deliberately not built yet, and how this small foundation avoids blocking
the much larger long-term design. Earlier milestones are preserved as git
tags (`milestone-1` through `milestone-24`) rather than kept running
alongside the current demo.

**New this milestone:** press `V` to switch between the existing third-person
follow camera and a first-person camera at the player's eye. Both use the
player's presented position/orientation and existing mouse-look angles; the
eye offset follows the player's local frame around either planet, the plank,
slopes, and steps. The player's box is hidden in first-person view but remains
fully simulated and collidable. The spacecraft keeps its existing camera while
piloted, and the selected player view returns on release. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 17."

## Building

### Requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.20+
- A C++17 compiler (GCC or Clang)
- SDL2 development headers
- GLM development headers — either GLM 1.0+ or the 0.9.9.8 that Ubuntu
  24.04 packages; both configure (see the M28 note above)

No physics-engine dependency to fetch — Judas owns its own physics (see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Physics ownership"), so
configuring needs no network access at all. Model/texture loading,
the font rasterizer and the editor UI are single-header or vendored
dependencies committed under `third_party/` (`tinyobjloader`,
`stb_image`, `stb_image_write`, `stb_truetype`, GLAD, Dear ImGui) rather
than fetched or installed separately.

On Ubuntu/Debian:

```bash
sudo apt install cmake libsdl2-dev libglm-dev build-essential
```

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

This produces the `judas` runtime, the `judas_editor` editor, and the 28
headless test executables. The engine itself is the `judas_engine` static
library both executables link; the editor additionally links the vendored
Dear ImGui (`judas_imgui`). The engine never depends on the editor.

### Run

```bash
./build/judas                                  # terrain demonstration
./build/judas assets/scenes/classic.judas      # any scene file
./build/judas_editor assets/scenes/terrain.judas
```

Run from the repository root (as shown above) — scene files, models,
textures and the UI font are loaded via paths relative to the current
working directory, and `judas` reports an error and exits if it's run from
somewhere else and can't find them.

## Controls

The mouse is captured on launch and controls where the player looks. WASD
walks the player relative to that look direction and the current surface
(not a free-flying camera).

| Action                  | Keys                  |
|-------------------------|-----------------------|
| Walk forward            | `W` or `Up Arrow`     |
| Walk backward           | `S` or `Down Arrow`   |
| Strafe left             | `A` or `Left Arrow`   |
| Strafe right            | `D` or `Right Arrow`  |
| Jump (only while grounded) | `Space`             |
| Look around             | Mouse movement        |
| Pause / back / resume   | `Escape`               |
| Take/release piloting control of the spacecraft (only while standing on it) | `F` |
| Toggle the player torch on/off | `T`             |
| Interact (door, switch, or whatever's prompted) | `G` |
| Pick up / drop an eligible physics object or cup | `G` |
| Throw a held object or cup | `H` |
| Toggle first/third-person player view | `V` |
| Power the M27 radiant heater while held | `C` |
| Reset the player and world | `R`               |
| Planet thrust: prograde / retrograde / radial outward | `P` / `M` / `N` |

While piloting the spacecraft (after pressing `F` while standing on it),
WASD/Q/E and a separate IJKL+U/O cluster mean something different — see
"The spacecraft" below.

Close the window normally (window controls / `Alt+F4` / etc.) to exit.

## Lighting

Press `T` to toggle a torch carried at the player's own eye position,
pointed exactly where you're looking — it moves and turns with you every
frame, works identically on either planet or the connecting plank, and
never assumes any particular "up" direction. It lights nearby surfaces in
a soft-edged cone (bright at the center, fading smoothly toward the edge,
never a hard cutoff) with a finite range — it doesn't reach across an
entire planet.

The spacecraft always has its lights on: one forward-facing headlight and
two wingtip navigation lights (red to port/left, green to starboard/
right — the traditional aviation convention). They're defined relative to
the spacecraft's own frame, so translating, pitching, yawing, or rolling
it — including coasting or tumbling freely under Milestone 12's real
inertia, with no input at all — carries the lights along exactly as if
they were physically bolted on, in any gravity context or none.

Dynamic lights combine additively with the existing ambient/directional
lighting from Milestone 9 — the torch and the spacecraft's lights never
replace or override it. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 14," for the
full design and the exact attenuation/cone formulas.

**As of Milestone 15, all three (the sun, the torch, and the headlight)
cast real shadows** — objects between a light and a surface actually
block it, with a soft (not harshly aliased) shadow edge. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 15," for the
full design. **Honest limitations:** the two wingtip navigation lights
still cast no shadows; the directional shadow only covers a bounded area
around the player, not the entire world at once (walk far enough from
where you last were and shadows there simply aren't being computed that
frame); and a surface just outside a light's own shadow frustum is
treated as unshadowed rather than actually checked.

## Interaction

Walk up to an interactable object (the door or the light switch, both a
short walk from spawn on Planet A) and look roughly at it — a prompt
appears near the bottom of the screen. Press `G` to trigger it:

- **The door** swings open on its hinge (visibly, over about two-thirds
  of a second, never teleporting between states) and physically stops
  blocking your path; press `G` again while near it to swing it closed,
  and it blocks the way again exactly as before.
- **The light switch**, a small lever beside the door, toggles a nearby
  lamp on and off — a completely different action, going through the
  exact same prompt-and-`G` interaction as the door, to prove the system
  isn't secretly built just for doors.

Walk away, or look somewhere else, and the prompt disappears on its own —
nothing needs to be selected/deselected explicitly. Opening the pause
menu suppresses interaction the same way it suppresses every other
gameplay input (see "HUD and pause menu" below): `G` does nothing while a
menu owns input. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md),
"Milestone 16," for the full design.

### Two-cup fluid demonstration (M24)

Select the classic scene with `JUDAS_CLASSIC_DEMO=1`. Its fluid table is near
the player spawn on Planet A. Cup A starts with water
and Cup B is empty. Look at a cup from within interaction range until the
pickup prompt appears, then press `G`. The held cup follows the player's
movement with physical force and follows mouse look with physical torque:
raising or lowering the view tips it without a separate pour key. Move Cup A
over Cup B and tip its open rim. The water can leave through the opening,
travel between the cups, contact Cup B, and subsequently leave Cup B as the
same simulated matter. Press `G` while holding to drop, or `H` to throw. If
another interactable is under the crosshair, `G` activates that prompt first;
look away from it to drop the cup. `R` resets both cup poses and the original
fluid particles.

Both cups use the same rigid-body collision and M18 pickup rules as other
eligible dynamic objects. Static world geometry, planets, and the spacecraft
are not pickable. The water uses each particle's actual local gravity; a
rotated context rotates its acceleration, and a zero-gravity region does not
create a world-down direction. Cup walls are translucent so the
simulation-derived water mesh remains visible. The surface and transparency
are visual approximations; fluid state and solid contact are CPU simulation.

Run `JUDAS_CLASSIC_DEMO=1 JUDAS_FLUID_GRAVITY=rotated ./build/judas` to give the station a gravity
direction tilted 50 degrees from local down, or
`JUDAS_CLASSIC_DEMO=1 JUDAS_FLUID_GRAVITY=zero ./build/judas` for exactly zero gravity there. The
same bounded context applies to the player, cups, and water. Outside it,
Planet A's normal gravity resumes. Either variant can be combined with
`JUDAS_WORLD_OFFSET=far`. Use an optimized build for the interactive fluid
demo; Debug is substantially slower on the measured machine.

## HUD and pause menu

The top-left panel shows player and ship state and, in the terrain scene,
atmospheric measurements. M27 adds the heater, local oxidizer, and each
fuel block's temperature, remaining coating mass, and burn rate. These
values come from simulation; flame shapes are presentation.

Press `Escape` to pause. The mouse is released automatically (no need to
press anything else to get a usable cursor) and the world freezes
completely — nothing moves, including the spacecraft, until you resume.

| Menu action              | Keys                          |
|---------------------------|-------------------------------|
| Navigate up / down        | `Up Arrow` / `Down Arrow`     |
| Activate the focused button | `Enter`                     |
| Hover / click a button    | Mouse movement / left click   |
| Back one level / resume   | `Escape`                      |

From the pause screen, `Options` opens one nested screen with a single
"Show HUD" toggle — `Escape` (or the `Back` button) returns to the pause
screen; `Escape` again (or `Resume`) closes the menu and hands input back
to gameplay, recapturing the mouse automatically. `Quit` closes the
application from the menu, same as closing the window normally. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 13," for the
full design and why pausing freezes the simulation rather than merely
suppressing input.

### Steps and slopes

A short staircase and one ramp stand a walk from spawn on Planet A —
just walk into them. Ordinary steps and low ledges (including the
spacecraft's own edge — see below) are climbed and descended automatically
while walking; nothing extra to press. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 10," for how
this works and its limits (how tall a step counts, how steep a slope stays
walkable).

### The spacecraft

A small aircraft-shaped spacecraft (`assets/models/plane.obj`) rests near
the player on the default terrain planet. In the classic demo selected by
`JUDAS_CLASSIC_DEMO=1`, it rests on the plank as before. Get onto its hull
(jump if the terrain-side approach is too high) and press `F` while grounded
on it to take control. The player is physically **secured** to it the instant control
is taken (see `docs/ARCHITECTURE.md`, "Milestone 11") — it can be flown
upside down, sideways, or through any combination of rotations without
the pilot falling off:

| Action                             | Keys        |
|-------------------------------------|-------------|
| Move forward / backward             | `W` / `S`   |
| Strafe left / right                 | `A` / `D`   |
| Ascend / descend (local up/down)    | `E` / `Q`   |
| Pitch up / down                     | `I` / `K`   |
| Yaw left / right                    | `J` / `L`   |
| Roll left / right                   | `U` / `O`   |
| Toggle SAS attitude hold            | `X`         |
| Release control                     | `F`         |

Every one of these is relative to the spacecraft's OWN current
orientation — never gravity, never a fixed world axis (consistent with
how nothing else in this engine assumes a universal up either). After a
180-degree roll, "ascend" still means "toward the spacecraft's own roof,"
whatever direction that now points in world space. Mouse look still moves
the camera freely (a free look independent of the spacecraft's own
attitude, driven purely by mouse motion) and is never also applied to the
spacecraft's own orientation — attitude control is keyboard-only,
specifically so the two never double up on the same input.

**The translation and attitude keys apply FORCE or TORQUE, not a speed.**
Holding forward accelerates gradually rather than snapping to a
fixed speed; letting go doesn't stop the spacecraft — it keeps coasting
at whatever velocity it had, indefinitely, until something (more thrust,
gravity, or a collision) changes it. The same is true of rotation: torque
builds angular velocity, and releasing the key leaves it spinning. To
actually slow down or stop turning, apply force or torque in the opposite
sense. With SAS off there is no rotational damping or automatic attitude
control; `X` enables the separate active SAS mode. See
`docs/ARCHITECTURE.md`, "Milestone 12," for the exact force/
torque magnitudes and the numeric evidence behind all of this. One
practical note: while still resting on the plank, ordinary ground friction
can make rotation feel stiff or entirely unresponsive (a real, physically
correct effect of this demo's friction coefficient, not a bug) — ascend a
little first if turning in place doesn't seem to do anything.

When SAS is enabled, it captures the current attitude and actively applies
counter-torque to stop rotation and hold that attitude; it remains active
after pilot release until toggled off or reset. It never brakes translation.

Pressing `F` again hands input authority straight back to the player,
releases the attachment, preserves exactly where the player was, and
gives it the spacecraft's own real velocity at that instant (including a
rotating spacecraft's own angular contribution) — see
`docs/ARCHITECTURE.md`, "Milestone 11," for the exact formula and what
happens if nothing is supporting the player at that point (it falls, same
as anyone else).

## Automated testing (developer tooling)

Judas can also run headlessly, driven by a scripted input sequence instead
of a real keyboard/mouse, logging player state and optionally dumping
screenshots — including a real-time mode that reproduces actual
render-frame timing for diagnosing presentation/smoothness issues. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#automated-testing) for the
full script format:

```bash
JUDAS_TEST_SCRIPT=path/to/script.txt ./build/judas
```

As of M28 the harness runs whichever scene the runtime would load (the
classic file by default, `JUDAS_TERRAIN_PREVIEW=1` for the terrain file)
through the identical `StepPlayedWorld` fixed step the interactive loop
uses, so its per-body CSV columns follow the scene's dynamic bodies in
scene order and include every body the interactive scene has (the
orbital bodies and cups were previously omitted from harness runs).

Twenty-eight standalone, headless test executables also exist (no window or GL
context): `judas_physics_tests` and `judas_collision_tests` (rigid-body/
collision/gravity-context primitives); `judas_asset_tests` (Milestone 9 —
model/texture loading, parses the real committed demo assets and checks
vertex/index/UV/normal data and texture dimensions; run from the
repository root, same reason as `judas` itself); `judas_step_climb_tests`
(Milestone 10 — the step-up/step-down primitives behind automatic stair
climbing, including the same rotate-the-whole-scenario "no global up"
check the physics suites use); `judas_pilot_attachment_tests` (Milestone
11 — the secured-pilot attachment math: acquisition/release round trips,
translation and rotation including a 180-degree roll, zero-drift under
many repeated rotations, and the release-velocity formula's angular
contribution, plus the same rotate-the-whole-scenario check); and
`judas_spacecraft_control_tests` (Milestone 11's control-mapping suite,
rewritten for Milestone 12's force/torque-driven inertia — see
`docs/ARCHITECTURE.md`, "Milestone 12" — rather than extended: it now
calls `PhysicsWorld::Step` after every control call and measures the real
integrated result, covering sustained-thrust acceleration, coasting after
release, orientation/velocity independence, counter-thrust, perpendicular
thrust, F/m mass response, angular coasting, counter-torque, the real
inverse-inertia-tensor's per-axis response, gravity+thrust composition, a
no-op while uncontrolled, and the same rotate-the-whole-scenario check);
`judas_spacecraft_flight_tests` (Milestone 21 — test-body celestial
acceleration, bounded unpowered orbit and rotated-world equivalence,
prograde/retrograde/radial thrust energy changes, thrust-driven escape,
SAS torque settling/attitude hold, translation independence, and toggle
request draining); `judas_reference_frame_tests` (Milestone 22 — position,
direction, and velocity transforms, including rotating-frame point speed,
live celestial/spacecraft/pilot data, round trips, and rotate-the-universe
equivalence);
`judas_ui_tests` (Milestone 13 — the UI navigation/input-ownership
logic: the screen stack, focus navigation and hit-testing, the pause
menu's exact required open/nested/back/resume flow, the HUD-visibility
toggle's persistent state, and the same boolean the real game loop gates
gameplay input on — pure CPU logic, no window/GL/font; UI appearance and
real interaction are human-validated instead, see `docs/ARCHITECTURE.md`,
"Milestone 13, Automated evidence"); and `judas_lighting_tests`
(Milestone 14 — the torch/spacecraft-light transform math including
rotate-the-whole-scenario invariance, the mirrored attenuation/spotlight-
cone formulas, and the torch's own input-ownership gating — pure CPU
logic, no window/GL/font; actual GLSL shader correctness was spot-checked
via a one-time offscreen render and is otherwise human-validated, see
`docs/ARCHITECTURE.md`, "Milestone 14, Automated evidence"); and
`judas_shadow_tests` (Milestone 15 — the shadow light-view/projection
transform math, including finite-matrix checks at extreme configurations
and rotate-the-whole-scenario invariance — pure CPU matrix math, no
window/GL; actual shadow-map sampling correctness was spot-checked via a
one-time offscreen render and is otherwise human-validated, see
`docs/ARCHITECTURE.md`, "Milestone 15, Automated evidence"); and
`judas_player_view_tests` (Milestone 17 — first-person eye and look transform,
arbitrary orientation and rotate-the-universe equivalence, third-person
offset preservation, view-mode changes, and paused-input gating — pure CPU
math, no window/GL); `judas_pilot_dismount_tests` (gravity-side release
clearance, actual collider support distances, zero-gravity preservation, and
rotated-world equivalence); and `judas_object_manipulation_tests` (Milestone 18 —
eligible-body filtering, shared range/facing targeting, arbitrary-orientation
carry target, rotate-the-universe equivalence, physics-backed carry, drop
velocity preservation, and mass-scaled throw direction); and
`judas_player_curved_locomotion_tests` (Milestone 19 — flat and spherical
fixed-step walking, long curved traversal with direction changes, stable
support clearance, local-frame continuity, landing, both player camera modes,
torch pose, stillness after traversal, and rotated-universe equivalence); and
`judas_celestial_gravity_tests` (Milestone 20 — inverse-square force,
force direction, barycentric motion for equal and unequal masses, analytical
period/radius comparison, momentum/angular-momentum/energy/barycentre drift,
timestep convergence, perturbation, escape, and rotate-the-universe
equivalence); `judas_world_coordinates_tests` (Milestone 23 — precise
large-offset placement, slow motion, contacts, gravity, orbit/spacecraft/SAS,
curved walking/jumping, both cameras, torch, pickup/throw, moving frames,
shadows, and combined rotation/translation); and the M24
`judas_compound_body_tests`, `judas_fluid_world_tests`,
`judas_fluid_rigid_coupling_tests`, and `judas_fluid_surface_tests` (open
compound geometry and contact, fluid gravity, zero gravity, moving walls,
geometric and real rigid-body pour/pour-back, mass accounting,
rotated/far-origin equivalence, and simulation-derived surface generation).
The M25 `judas_terrain_physics_tests` and `judas_terrain_fluid_tests` add
surface geometry/support, fluid-terrain contact, and the live water-emission
schedule. The M26 `judas_atmosphere_tests` and
`judas_atmospheric_flight_tests` cover the gas profile, hydrostatic gradient,
vacuum, moving-frame velocity, real rigid-body drag, orientation, orbital
energy loss, and rotated/translated equivalence. M26 operator acceptance
passed. The M27 `judas_combustion_tests` exercises fixed-step ignition,
finite fuel, heat transfer and spread, vacuum extinguishing, relative
motion, and rotated/translated cases. The M28 `judas_scene_tests` covers
scene save/load equivalence and byte-identical resave, stable ids across
deletion and reload, generic create/delete/modify/reorder, strict failure
on malformed or version-incompatible data, edit → play → mutate → stop
restoring authored state, every shipped scene loading and instantiating
through the runtime path, and clear instantiation errors. There are now
28 standalone suite targets. Because the harness now steps the full
played scene (fluid, combustion, doors, celestial gravity included), it
is an M1–M28 regression of the real loop rather than a reduced one.
M27 operator acceptance passed; M28 operator acceptance is pending.
Run them with:

```bash
cmake --build build
for test in build/judas_*_tests; do "$test" || break; done
```

## Assets

`assets/models/beacon.obj` and `assets/textures/beacon.png` (Milestone 9),
and `assets/models/plane.obj` (Milestone 11), are original content
authored for this project (not derived from any external asset) — public
domain / CC0-equivalent, redistributable without restriction. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 9, Assets," and
"Milestone 11," for the full provenance notes.

`assets/scenes/*.judas` (Milestone 28) are the demonstration scenes,
authored once from the M27 composition-root constants and now the only
place those placements live; edit them in `judas_editor` or by hand.

`third_party/imgui/` (Milestone 28) is Dear ImGui v1.91.9b (MIT) — see
[`third_party/imgui/README.md`](third_party/imgui/README.md) for the exact
upstream commit and which files are vendored. Only `judas_editor` links it.

`assets/fonts/DejaVuSans.ttf` (Milestone 13) is the DejaVu Sans font,
under the Bitstream Vera License (a permissive, redistribution-friendly
license) — see [`assets/fonts/DejaVuSans-LICENSE.txt`](assets/fonts/DejaVuSans-LICENSE.txt)
for the full text. Rasterized at runtime via `stb_truetype`
(`third_party/stb_truetype.h`, public domain).

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.
