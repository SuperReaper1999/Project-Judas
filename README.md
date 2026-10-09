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

## JUDAS STATUS: READY FOR NEW FEATURE DEVELOPMENT

Current accepted engine checkpoint: M71 (`02b540f083bcf3d99b0dc6eb9aa70e63c91a31c3`).
[M72 lighting, render visibility and instance materials](docs/M72.md) is an
checkpoint authorized without human validation; desktop review remains pending.
M67 authoring review remains provisional/deferred. M69 controller behaviour was
accepted using synthetic delivery/VM checks; physical controller feel and Windows
validation for the later M70/M71/M72 changes remain outstanding. The operator's
earlier Windows-update acceptance retains its historical scope.
Start with [Architecture](docs/ARCHITECTURE.md), [JudasJS](docs/JUDASJS.md) and
[documentation maintenance](docs/DOCUMENTATION_MAINTENANCE.md).

**M38 standalone export (accepted):** Project settings can export a movable Linux
Release game package with its project input, scenes and registered runtime assets.
See [export workflow and platform requirements](docs/M38.md).

**M33 secondary cameras (accepted):** authored cameras can render into generated textures
consumed by ordinary scene materials. See [camera authoring and demo](docs/M33.md).
Operator acceptance is recorded; portals are not implemented.

FTFT1–9 are checkpointed, and the final clean Release validation passed; see
[`docs/STABILIZATION_STATUS.md`](docs/STABILIZATION_STATUS.md). See
[`docs/FTFT.md`](docs/FTFT.md) for the exact status and evidence. No new
milestone tag is implied. Full high-fidelity M32 liquid/solid coupling remains
future research, separate from the approximate production model.

**Historical FTFT9 foundation / retained legacy PBF path:** it uses particle/PBF liquid motion with sampled
approximate hydrostatics/drag, explicit geometric container cavities, and custom
player swimming. Exterior collision and analytic support have distinct ownership;
exact exterior liquid/body momentum conservation is not claimed. The historical
M24–M26 results below retain their original scope and do not describe this new
coupling. Current behavioural evidence and accepted numerical limits are recorded
in the ledger and the [final gate](docs/evidence/stabilization/final-gate/README.md).

Current conserved liquid uses [M54 reservoirs/containers](docs/LIQUID_RESERVOIRS.md)
and [M55 dynamic surfaces](docs/LIQUID_SURFACES.md). Quantity and surface evolution
have separate ownership; neither claims final fluid fidelity. Protected P1 research
remains separate. The [legacy particle-fluid demo](docs/FLUID_DEMO.md) is an ordinary registered
project with a flat swimming pool, a radial-gravity planetary basin, thrown
bodies and empty geometric containers that can carry actual liquid particles:

```sh
./build/judas projects/fluid_demo/fluid_demo.judasproj
```

Use F1/F2 to select the two scenes. Historical FTFT/P1 fixtures remain evidence,
not current demonstration launchers.

Historical FTFT4 rigid-physics results (the numerical costs below describe that gate):

1. A dynamic AABB-tree broadphase with exhaustive-oracle coverage, including
   player sweeps. **1,500 resting crates: 14.266 ms median whole physics step**
   in the latest seven-run comparison; the older approximately 4 ms result
   belongs to less-robust geometry.
2. Pair-local robust primitive geometry, preserved signed gaps and precise
   local anchors; accumulated warm-start contact impulses, box manifolds
   and Coulomb friction for sustained support.
3. Event-time isolated impacts with authored restitution; deliberately
   inelastic coupled/multiple-contact impacts. Remaining step time is
   consumed. A transient anchored motion ledger preserves out-and-back
   travel for dynamic/player queries. Low-speed/event-cap capture and
   finite-resolution grazing CCD are explicit approximations.
4. Passing current-engine timing, energy, stack, slope, platform, broadphase,
   lifecycle and geometry checks. **43 production suites** and protected
   async/persistence/gravity/near-far integration checks pass for FTFT4.

See [FTFT4 results](docs/evidence/stabilization/ftft4/RESULTS.md) for exact
budgets, limitations and costs. Active rotating compounds and moving supports
cost more than the old endpoint/speculative path; no broad speedup is claimed.
The earlier zero-gravity stopping, premature bounce and energy-gain failures
remain preserved as historical evidence, separately from the passing repair.

**Milestone 31 (accepted) — Judas learns it doesn't have to wait.** Expensive
independent work no longer stops the simulation/render thread:

1. **Job system** (`src/JobSystem.h`). A bounded worker pool (count derived
   from the hardware, capped at 16; never a thread per job, never detached)
   runs submitted jobs at High/Normal/Low priority with a fairness rule
   so low work is delayed but never starved. Jobs have handles and
   Queued/Running/Completed/Failed/Cancelled state, report errors, can be
   cancelled before they start (and cooperatively while running), and
   shut down deterministically with work outstanding: queued work is
   cancelled, running work is waited for, every worker is joined.
2. **Asynchronous file IO** (`src/AsyncFile.h`). A read request completes
   as Succeeded (bytes), Failed (message) or Cancelled; the worker owns its
   own reference, so a caller may drop the request at any time.
3. **Asynchronous resources.** The M30 `ResourceManager` now moves an asset
   through Unloaded → Queued → Loading → CpuReady → Ready. Workers read
   and decode (OBJ parse, image decode); **only** `Pump()` on the GL thread
   creates or destroys GPU objects, at most a few uploads per frame.
   Requests never block; presentation draws a grey placeholder until the
   asset is Ready (magenta when it failed) and switches to the real mesh
   the frame it arrives. Repeated requests join one in-flight load;
   released or superseded loads are rejected by generation, never
   resurrected; dropped demand (reference counts) cancels queued loads.
4. **Budget and eviction.** Resident bytes are estimated per resource; over
   a configurable budget, unreferenced Ready resources are evicted least-
   recently-used first, referenced ones never; an evicted asset simply
   loads again.
5. **Diagnostics.** The Profiler shows workers, queued/running/completed/
   failed/cancelled jobs, worker utilization, resources loading/ready/
   failed, resident bytes versus budget, uploads, evictions, cancellations
   and stale discards; the Asset Browser shows each asset as loading /
   ready (with size) / FAILED (with the reason) / unloaded.

Measured (`judas_resource_stress`, 40 synthetic real assets totalling 137 MB
of OBJ/PNG, tiny-game scene playing, software GL under Xvfb, 3 workers):
asynchronous loading finished in 0.57 s wall with a worst main-thread frame
of 29.6 ms and no frame over 33 ms; the same 40 loads in the M30 blocking
path took 1.75 s with 27 of 40 frames over 33 ms and a worst frame of
67.8 ms. The work itself is not free (1.48 s of worker CPU); the main
thread just no longer waits for all of it. All 31 suites pass (new:
`judas_job_tests`); harness runs are byte-identical to the references.
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 31," for
the ownership law, the contracts and the limitations.

**Milestone 30 (accepted) — Judas learns to actually make games with
itself.** — Judas learns to actually make games with itself.** The
engine now has a real **project** concept, stable **asset identity**, one
**resource manager** boundary, and an editor that can make a small game
without touching engine source:

1. **Projects.** A game is a directory with a `.judasproj` file naming the
   project, its startup scene and its Assets/Scenes/Saves folders. The
   runtime launches a project (`./build/judas my_game.judasproj`); the
   editor creates, opens and edits projects (File → New project / Open
   project / Project settings) and can **Run project** as a separate
   runtime process. The technology demonstration is an ordinary project
   (`judas_tech_demo.judasproj`); a tiny flat game ships in
   `projects/tiny_game/`. No engine code branches on which project it is.
2. **Asset identity.** Every importable asset (`.obj`, `.png/.jpg/.bmp/
   .tga`, `.ttf`) carries a 32-hex-digit id in a `.judasmeta` sidecar.
   Scenes reference meshes and textures **by id** (scene format version
   3), so renaming or moving an asset in the **Asset Browser** keeps every
   scene reference valid — proven by an automated test, not by filename
   guessing. Missing and broken assets are reported, never guessed around.
3. **Resource manager.** `ResourceManager` is the only place a project
   asset becomes a GPU resource: request → load → ready → cached → failed
   → released → reloaded, synchronous, GL only inside `Renderer`, no GL
   handle in authored data.
4. **A usable editor.** Viewport **translate/rotate/scale gizmos** (world/
   local space, `Ctrl` snaps, one undo step per drag), an **Asset Browser**
   (import by path, rename/move, missing indicators, drag onto the
   viewport or an inspector field), hierarchy **rename/duplicate/
   indicators**, a **component editor registry** instead of an inspector
   switch, **Project settings**, a **Debug** menu of engine
   visualisations (collision shapes, player capsule and support, contacts,
   gravity vectors and regions, frame axes, lights, interaction ranges,
   lifecycle/fidelity, sampled terrain normals, fluid particles,
   atmosphere radii) and a **Profiler** panel (frame time, fixed steps,
   step time, bodies, contacts, lifecycle counts, draw calls, triangles,
   shadow passes, lights, fluid particles, resource hits/misses).

All 30 M30 test suites pass (the new `judas_project_tests` covers the project
format, asset identity through rename/move, the resource-manager
contract, project → startup scene → runtime for both shipped projects,
gizmo mathematics with undo/redo, the inspector data round trip and
Edit/Play separation); the classic and terrain harness runs are
byte-identical to the M27/M28 references at the near and far world
origins. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone
30," for the boundaries, the formats, the evidence and the limitations
(no OS file dialog, sphere-approximate picking, no object parenting, no
asynchronous loading).

**Milestone 29 (accepted) — Judas learns that existing does not mean being fully
simulated.** Three engine capabilities, built together because they are
one problem:

1. **Variable simulation fidelity.** Every dynamic body is a persistent
   entity that can be represented at **Full** fidelity (an ordinary live
   physics body), **Coarse** fidelity (no physics body; its pose and
   velocities are engine data advanced cheaply each step — settled
   entities stay put, free-flying ones integrate under the same gravity
   fields, no contacts), or **Dormant** (no body, no per-step work, state
   retained exactly). Transitions preserve identity, pose, linear and
   angular velocity; reconstruction back to Full rebuilds the live body
   from that state with no impulse and no duplicate.
2. **Entity/world lifecycle.** *Active* (Full/Coarse), *Unloaded*
   (Dormant) and *Destroyed* are distinct: an unloaded entity comes back,
   a destroyed one never does. Which fidelities an entity *can* take is a
   capability (fluid/atmosphere/combustion/vehicle/compound content is
   Full-only); *when* it changes is a **policy** the scene chooses. The
   only policy shipped is distance-from-player; scenes that set no policy
   keep every entity Full and never touch any of this.
3. **Persistence as baseline + deltas.** Runtime changes — an entity moved,
   destroyed or created, a door/switch toggled — save to a small
   `.judasstate` file over the *unchanged* baseline scene, and load back
   after a full restart. The scene file is never rewritten by gameplay;
   delete the delta and the pristine baseline returns.

Measured on 1,500 managed crates: 343 ms per fixed step with everything
Full (1,501 physics bodies) versus 1.0 ms with the distance policy (20
bodies: 19 Full, 41 Coarse, 1,440 Dormant); promoting all 1,500 costs
0.65 ms. New scenes: `assets/scenes/fidelity_demo.judas` (a row of managed
crates receding into the distance, two free-flying satellites, distance
policy 25 m / 70 m) and `assets/scenes/flat_playground.judas` (fifty
entities, no policy — the small conventional game that ignores all of
this). All 29 suites pass; the classic harness is byte-identical to M27.
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 29," for
the model, the delta format, the measurements, and the limits (no reduced
form for fluid/gas/fire/player/vehicles; coarse inertial motion has no
contacts; a pre-existing resting-creep solver artefact M29 exposes).

**Milestone 28 (accepted)** made Judas an engine with real scenes, a
deterministic versioned scene format, a scene-driven runtime, and the
`judas_editor` executable — see "Milestone 28 accepted baseline" below.

## Quick start

```bash
sudo apt install cmake libsdl2-dev libglm-dev build-essential   # Ubuntu/Debian
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

./build/judas                                        # the project in the current directory (the tech demo), startup scene
./build/judas judas_tech_demo.judasproj              # the same, explicitly
./build/judas projects/tiny_game/tiny_game.judasproj # the tiny flat game
./build/judas assets/scenes/classic.judas            # one scene of the enclosing project
./build/judas_editor projects/tiny_game/tiny_game.judasproj   # editor: open a project
./build/judas_editor assets/scenes/classic.judas             # editor: open a scene (and its project)
./build/judas_editor                                 # editor: the project enclosing the working directory
```

Paths in project and scene data are relative to the **project root** (the
directory holding the `.judasproj`), never to the working directory. The
one engine-owned file, the UI font `assets/fonts/DejaVuSans.ttf`, is found
beside the executable's directory, its parent, `$JUDAS_ENGINE_ROOT`, or
the working directory — so a build tree anywhere under the repository
works from anywhere; set `JUDAS_ENGINE_ROOT=<repository>` for a build
tree elsewhere.

## Projects

A project is a directory with one `.judasproj` file:

```
JudasProject 1
name "Tiny Game"
startup-scene "Scenes/main.judas"
assets-dir "Assets"
scenes-dir "Scenes"
saves-dir "Saves"
```

`./build/judas <project>` plays the startup scene; F6/F7 world-state
deltas live in the project's saves directory. The technology
demonstration is `judas_tech_demo.judasproj` at the repository root
(assets under `assets/`, scenes under `assets/scenes/`, startup scene
`terrain.judas`); the tiny game is `projects/tiny_game/`. The editor's
**File → New project** creates the directory tree plus a starter scene
(ground, player start, lamp) and sets it as the startup scene; **Project
settings** edits the name and startup scene and lists the project's
scenes; **Run project** launches `./build/judas <project>` as a separate
process — the game as a player would start it, not the editor's Play.

Assets are referenced by **id**. Every asset file in the assets directory
has a sidecar:

```
Assets/models/beacon.obj
Assets/models/beacon.obj.judasmeta      JudasAssetMeta 1 / id "<32 hex>" / type mesh / source "..."
```

The sidecar travels with the file, so renaming or moving an asset (Asset
Browser → Rename / move) changes no scene. A file without a sidecar is
listed as *untracked* until imported or tracked; a sidecar without its
file is reported as *missing*; duplicate ids and corrupt sidecars are
listed as *problems*. Nothing is guessed from file names.

## Asynchronous resources (M31)

Assets load in the background: the runtime and editor request an asset,
keep simulating and drawing (a grey placeholder box where a mesh is still
loading, magenta where it failed), and switch to the real mesh the frame
it is Ready. Workers read and decode; only the GL thread uploads, at most
two resources per frame. `JUDAS_RESOURCE_MODE=blocking` restores the
synchronous M30 path (the scripted harness always uses it). The Profiler
(View → Profiler) shows job and resource state; the Asset Browser shows
each asset as loading / ready / FAILED.

```bash
./build/judas_resource_stress [report.json]   # the M31 stress demonstration (needs a display; Xvfb is fine)
```

FTFT2 adds deterministic real-GL integration coverage through the ordinary
`Application::Run` loop, including cancellation, project handoff, residency and
shutdown. The older scripted harness remains a **blocking reference**, and the
M31 stress tool is supporting load/performance evidence.

```bash
python3 scripts/ftft2_validation.py  # builds; async integration, editor/runtime, production suites
```

Needs CMake/C++17, SDL2/GLM development packages, Python 3, and a working OpenGL
3.3 context. The runner uses SDL offscreen/Mesa software GL; the late-after-new
completion case requires at least two hardware-derived workers. See
[FTFT2 evidence](docs/evidence/ftft2/README.md) for exact scope and limitations.

## Scenes

A scene is a plain text file. The header names the format and version;
`settings` carries scene-wide values; each `object` block has an explicit,
never-reused id, a name, a transform, and any components it uses:

```
JudasScene 3
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
  ...                            (a mesh render carries render.mesh-asset "<id>"
                                  and render.texture-asset "<id>" — asset ids, never paths)
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
| `fidelity_demo.judas` | **M29:** flat ground, 60 managed crates receding to 240 m, two free-flying satellites, distance policy (Full ≤ 25 m, Coarse ≤ 70 m, Dormant beyond) |
| `flat_playground.judas` | **M29:** the tiny conventional case — fifty entities on a small map, no policy, nothing managed |
| `rigid_stability.judas` | **M32:** dry rigid contacts — stacks of equal boxes, loose boxes of several sizes, a 20° slope with a high-friction box that holds and a low-friction one that slides, a sphere |
| `broadphase_stress.judas` | **M32:** 1,500 crates, all Full rigid bodies (no fidelity policy), in a grid with a few stacks — the profiler's broadphase lines under load |

The pre-M28 environment switches still work for the no-argument launch
from the repository root and select these files from the project's
scenes directory: `JUDAS_CLASSIC_DEMO=1`, `JUDAS_FLUID_GRAVITY=rotated|zero`,
`JUDAS_ATMOSPHERIC_PASS=1`, `JUDAS_TERRAIN_ROTATED=1`. An explicit
`./build/judas <project>` or `./build/judas <scene>` argument wins over
all of them. `JUDAS_WORLD_OFFSET` (`far` or `x,y,z`) still overrides the
scene's authored world origin.

The shipped scenes are generated by `./build/judas_scene_author
assets/scenes projects/tiny_game/Scenes` (`tools/SceneAuthor.cpp`).
Regeneration overwrites the committed files and is no longer faithful to them.
The serializer now also writes later fields at their defaults, and the generator
predates later scene edits: FTFT9 (`1b3a134`) added fluid cavities to the two cups
in `classic.judas`, `classic_fluid_rotated.judas` and `classic_fluid_zero.judas`
directly, so regenerating removes them. Treat the committed scenes as the source of
truth and regenerate only deliberately. Run bare, the tool prints usage and writes
nothing.

## The editor

`./build/judas_editor [project.judasproj | scene.judas]` opens a project
(and its startup scene) or a scene inside its enclosing project. Edit mode has a
dedicated central scene view, resizable hierarchy/inspector rails and a bottom
asset browser. Running Play retains the normal full-window runtime view.
See the [editor guide](docs/EDITOR_GUIDE.md) and
[making a game by hand](docs/judasjs/getting-started.md).

| Editor action | How |
|---------------|-----|
| Look around | hold the **right mouse button** and move the mouse |
| Pan the scene view | press the **middle mouse button** in the viewport and drag; movement follows the view plane and the most recent focus depth; release or `Escape` ends the drag |
| Zoom the scene view | scroll the **mouse wheel** in the viewport; wheel up moves closer, down moves away from the current focus; panel scrolling leaves the camera still |
| Fly | with the right button held: `W`/`A`/`S`/`D`, `E` up, `Q` down, `Shift` faster |
| Select | **left-click** visible primitive/mesh geometry in the viewport, including nested objects, or click it in **Hierarchy** |
| Focus the selection | `F`, or Hierarchy → Focus |
| **Move / rotate / scale** | `W` / `E` / `R` pick the gizmo; drag a red/green/blue handle (arrow, ring, box); `X` toggles local/world space; hold `Ctrl` (or Edit → Snap) to snap 0.5 m / 15° / 0.25; a drag is one undo step |
| Create objects | **Create** menu: empty, static/dynamic box or sphere, mesh, point/spot light, door, gravity region, player start (placed 8 m ahead of the camera); or drag a mesh from the **Asset Browser** onto the viewport |
| Rename / duplicate | double-click a Hierarchy row (or right-click → Rename); `Ctrl+D`, Hierarchy → Duplicate, or right-click → Duplicate (a new id, placed after the original) |
| Delete / reorder | `Delete`, Hierarchy → Delete, or the right-click menu (Move up / Move down — order is authored data: earlier gravity regions win) |
| Hierarchy indicators | Hover a row for component letters and stable identity; `(!)` marks a missing or unknown mesh; paused runtime inspection shows `[Full]`/`[Coarse]`/`[Dormant]`/`[destroyed]` state |
| Edit | **Inspector**: name, transform, every component's fields (one editor per component type from a registry; Add component… / Remove), asset fields as combos or drop targets with an honest status line (unknown id, file missing) |
| Scene-wide values | Toolbar **Scene** or View → **Scene settings**: name, world origin, sun, ambient, fluid scale, fidelity policy |
| Assets | **Asset Browser** (View menu): every tracked asset with type, path, state and id; **Import** a file by path into the assets directory (validated by the engine's own loader); select → **Rename / move**, **Remove**; **Track** untracked files; problems listed; drag rows onto the viewport or an inspector field |
| Project | File → **New project…**, **Open project…**, **Project settings** (name, startup scene, scene list — double-click opens), **Run project** |
| Debug view | **Debug** menu toggles: collision shapes, player capsule + support, contacts, gravity vectors + regions, frame axes, lights, interaction ranges, lifecycle/fidelity, sampled terrain normals, fluid particles, atmosphere radii — drawn in Edit (authored) and Play (live) |
| Profiler | View → **Profiler**: frame ms / FPS (rolling), fixed steps per frame, last step ms, live/dynamic bodies, contacts, entity counts, draw calls, triangles, shadow passes, dynamic lights, debug lines, fluid particles and surface/solve times, resource counts and hit/miss/failed |
| Undo / redo | `Ctrl+Z` / `Ctrl+Y` (Edit menu) — gizmo/shared field gestures and create/duplicate/delete/reorder are grouped; some specialized inspector controls commit each change |
| Save / open | `Ctrl+S`, File → Save scene / Save scene as… / Open scene… (path fields relative to the project root; no OS dialog); unsaved scene replacement/close offers Save, Discard or Cancel |
| **Play scene** | Toolbar **Play** or `F5`: the authored scene is instantiated with its project scripts/legacy compatibility policy; edit input edges are cleared first |
| While playing | Modern projects supply their own pause/input policy; historical compatibility can retain the M13 menu. Paused inspection is read-only. |
| **Stop** | **Stop** or `F5`: the runtime world is discarded and the authored scene is exactly what it was |
| **Run project** | launches `./build/judas <project>` beside the editor binary as a separate process on the startup scene |

Runtime changes made during Play (thrown crates, burnt fuel, moved water)
are never written back to the scene; M29 world-state deltas go to the
project's saves directory. "Apply runtime state to the scene" is a
possible future feature, not an M30 one.

Object parenting, an OS file dialog, mesh-precise picking, asynchronous
loading and an asset cooking pipeline are deliberately not part of M30 —
see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 30,
Deliberately not implemented."

## Lifecycle, fidelity and persistence (M29)

Every dynamic body in a scene is a **persistent entity** with a stable id
(its scene object id; runtime-created entities take ids from a separate
high range). At any moment it is at one **fidelity**:

| Fidelity | Physics body | Per-step work | State lives in |
|----------|--------------|---------------|----------------|
| Full | yes | ordinary simulation | `PhysicsWorld` |
| Coarse | no | settled: none; inertial: one cheap integration under gravity, no contacts | the entity record |
| Dormant | no | none — time is frozen for it | the entity record |

and one **lifecycle** state: *Active* (Full or Coarse), *Unloaded*
(Dormant) or *Destroyed* (permanent). A scene opts an entity into
automatic management with the body's `Managed` flag and chooses a policy
in the Scene panel (`Fidelity policy: distance`, with the Full and Coarse
radii). Without a policy nothing is ever demoted. The editor's Inspector
shows the selected entity's persistent id, lifecycle, fidelity, coarse
motion kind and reconstruction count while playing, with **Force Full /
Coarse / Dormant** debug buttons; the HUD shows the counts.

**Persistence** is a delta over the unchanged baseline scene:

| Key | Action |
|-----|--------|
| `Z` | create a persistent crate in front of the player |
| `Y` | permanently destroy the targeted (or held) pickable entity |
| `F6` | save the world-state delta (`saves/<scene>.judasstate`) |
| `F7` | delete it — the next launch is the pristine baseline |

The runtime (and the editor's Play) apply `saves/<scene>.judasstate`
automatically if it exists (`JUDAS_WORLD_STATE=<path>` overrides,
`JUDAS_WORLD_STATE=none` disables). The delta records only: entities
moved (pose + velocities), destroyed, created (their full definition),
and door/switch states. FTFT1 saves use format **2**, with a versioned SHA-256
fingerprint of canonical authored scene data. Scene names alone never authorize
loading. IDs, object order, transforms, components, asset IDs and authored
settings must match; comments, whitespace and numeric spelling do not matter.
Malformed/incompatible saves are rejected before any live-world mutation.
Legacy version-1 saves are unverifiable and rejected without changing the file;
archive or delete one explicitly before starting a new save. The baseline
`.judas` file is never written by gameplay. This is strict scene compatibility,
not migration or a hash of external asset bytes. See [FTFT ledger](docs/FTFT.md).

## Milestone 28 accepted baseline

M28 established real scenes (`src/Scene.h`), the `.judas` scene file
format (`src/SceneSerialization.h`; strict, deterministic, versioned — now
version 2 after M29's two additions), a scene-driven runtime
(`RuntimeWorld`, `Simulation`, `WorldPresentation`, `GameSession`,
`InteractivePlay`) that reduced `Application.cpp` from 2,404 lines to ~120
of orchestration, and the Dear ImGui `judas_editor` with an explicit
Edit/Play split. The operator accepted M28; it is commit `57e69a0` on
`main`. The M28 portability fix (`glm::glm` as well as
`glm::glm-header-only`) stands.

## Current thermal scope (FTFT8)

Finite combustible coatings use temperature, fuel and sampled oxidizer; there
is no burn timer controlling the reaction. The atmosphere is an **open prescribed
reservoir** and flame/smoke shapes are visual output, not conserved gas CFD.
Thermal materials also work in ordinary scenes without planetary/atmosphere
components: absent atmosphere means vacuum radiation and zero oxidizer.

Passive pair/reservoir heat exchange is limited for stiff coefficients so its
explicit step cannot overshoot the sampled temperatures. Both ends of a pair
receive the same transfer, and diagnostics expose any limiting. Chemical and
heater inputs are accounted separately. This is an approximate thermal model
with fixed carrier mass/heat capacity and no radiation occlusion, not detailed
chemistry. See [the method and independent checks](docs/evidence/stabilization/ftft8/METHOD.md).

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

**Historical M25 coupling evidence (superseded production path):**
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

**Historical M24 mechanism:** the bounded CPU solver uses position-based density constraints at the fixed
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
`R` resets the same local scenario at the chosen absolute location. This is
**fixed-origin placement**: a compact simulation can have a very large
absolute location while physics and rendering retain small local float
coordinates. Automatic/live origin rebasing and travel arbitrarily far
through a continuously rebasing region are not implemented; they are
future capabilities, not guarantees of M23. The active
local region must remain small enough for float simulation. M23's measured
range and historical visual acceptance are recorded in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 23."

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

### Windows candidate

Native Windows preparation is **unvalidated, not final**. See
[Windows build, SDK and export testing](docs/WINDOWS.md). Linux remains the
accepted platform; Windows acceptance requires native testing.

### Linux requirements

- Linux (developed and tested on Ubuntu)
- CMake 3.20+
- A C++17 compiler (GCC or Clang)
- SDL2 development headers
- GLM development headers — either GLM 1.0+ or the 0.9.9.8 that Ubuntu
  24.04 packages; both configure (see the M28 note above)

No physics-engine dependency to fetch — Judas owns its own physics (see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Physics ownership"), so
configuring needs no network access at all. Every other dependency is
committed under `third_party/` rather than fetched or installed separately:
GLAD, Dear ImGui, QuickJS-NG, miniaudio, Recast/Detour, cgltf, ufbx,
tinyobjloader, nlohmann/json, quickhull, MikkTSpace, verblib and the stb
headers, plus FreeType, HarfBuzz and ICU as source tarballs in
`third_party/text/` that CMake builds locally (the first build takes a while
because of ICU). See `third_party/RUNTIME_NOTICES.txt` and
`third_party/text/PROVENANCE.md` for licences.

On Ubuntu/Debian:

```bash
sudo apt install cmake libsdl2-dev libglm-dev build-essential
```

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

This produces the `judas` runtime, the `judas_editor` editor, content
tools such as `judas_scene_author` (scene generation and the M67 named
authoring CLI), `judas_collision_cook`, `judas_model_import_cli`,
`judas_export` and the navigation/liquid/environment/deformable bakers,
and roughly 150 test executables (147 `judas_*_tests` at M67; see
"Automated testing"). The engine itself is the `judas_engine` static
library both executables link; the editor additionally links the vendored
Dear ImGui (`judas_imgui`). The engine never depends on the editor.

### Run

```bash
./build/judas                                        # the enclosing project's startup scene (terrain demonstration)
./build/judas projects/tiny_game/tiny_game.judasproj # a project
./build/judas assets/scenes/classic.judas            # a scene of the enclosing project
./build/judas_editor projects/tiny_game/tiny_game.judasproj
```

With no argument, `judas` and `judas_editor` look for the project
enclosing the working directory (the repository root holds
`judas_tech_demo.judasproj`); with a project or scene argument the
working directory does not matter. Only the engine's UI font is located
relative to the executable / `$JUDAS_ENGINE_ROOT` (see "Quick start").

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
| Create a persistent entity / destroy the targeted one (M29) | `Z` / `Y` |
| Save / delete the world-state delta (M29) | `F6` / `F7` |

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

Standalone test executables also exist; the early suites described here are
headless (no window or GL context), while many later ones drive the real windowed
application. The M32 `judas_broadphase_tests` checks the dynamic AABB tree
and the broadphase + narrowphase against a brute-force all-pairs oracle
(including the player's sweep and a 1,500-crate floor), and
`judas_rigid_contact_tests` covers resting contact, stacking, friction,
moving supports, restitution, energy and momentum; the M31 `judas_job_tests` covers the job system, asynchronous
file reads and the asynchronous resource manager; the M30
`judas_project_tests` covers projects, asset identity, the resource
manager's synchronous contract, project launch, gizmo mathematics and
Edit/Play separation; the rest are: `judas_physics_tests` and `judas_collision_tests` (rigid-body/
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
through the runtime path, and clear instantiation errors. The M29
`judas_lifecycle_tests` covers identity across full → coarse → dormant →
full with no duplicates, preserved pose/velocities and impulse-free
reconstruction (measured against an untransitioned reference run), unload
versus destroy, Full-only capability limits, coarse inertial evolution
matching the live integrator, the distance policy and policy-free explicit
commands, baseline + delta reproduction across a rebuilt world, delta
validation before apply, far-origin and reference-frame equivalence, and
the reduced-work measurement. (This list stops at M29; later milestones added many
more suites, 147 `judas_*_tests` at M67.) Because the harness now steps the full
played scene (fluid, combustion, doors, celestial gravity included), it
is an M1–M28 regression of the real loop rather than a reduced one.
M27–M29 operator acceptance passed. These results describe their historical gates.
Run them with:

```bash
cmake --build build
for test in build/judas_*_tests; do "$test" > /dev/null 2>&1 || echo "FAILED ($?): $test"; done
```

About fifteen suites need arguments (a project, mode and/or output directory)
and print a usage line and exit 2 when run bare; a few M66 model tests also need
the local-only original character. The milestone validation scripts supply
those arguments, e.g. `python3 scripts/m67_validation.py --output /tmp/fresh-dir`
(without `--output` it writes into the committed `docs/evidence/m67/final`).

## Assets

`assets/models/beacon.obj` and `assets/textures/beacon.png` (Milestone 9),
and `assets/models/plane.obj` (Milestone 11), are original content
authored for this project (not derived from any external asset) — public
domain / CC0-equivalent, redistributable without restriction. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), "Milestone 9, Assets," and
"Milestone 11," for the full provenance notes.

`assets/scenes/*.judas` (Milestone 28) are the demonstration scenes,
authored once from the M27 composition-root constants and now the only
place those placements live; edit them in `judas_editor`, by hand, or
regenerate them with `judas_scene_author` (`tools/SceneAuthor.cpp`; see "Scenes" above).
`assets/**/*.judasmeta` (Milestone 30) are the asset identity sidecars;
`judas_tech_demo.judasproj` is the demonstration's project file and
`projects/tiny_game/` the tiny flat game project.

`third_party/imgui/` (Milestone 28) is Dear ImGui v1.91.9b (MIT) — see
[`third_party/imgui/README.md`](third_party/imgui/README.md) for the exact
upstream commit and which files are vendored. Only `judas_editor` links it.

`assets/fonts/DejaVuSans.ttf` (Milestone 13) is the DejaVu Sans font,
under the Bitstream Vera License (a permissive, redistribution-friendly
license) — see [`assets/fonts/DejaVuSans-LICENSE.txt`](assets/fonts/DejaVuSans-LICENSE.txt)
for the full text. Rasterized at runtime via `stb_truetype`
(`third_party/stb_truetype.h`, public domain).

`third_party/miniaudio/` (Milestone 34) is miniaudio v0.11.23 by David Reid,
Copyright 2025 David Reid. Judas uses its **MIT-0 (MIT No Attribution)** option.
The original header notices and complete dual-license text are preserved in
[`third_party/miniaudio/LICENSE`](third_party/miniaudio/LICENSE). Attribution is
retained voluntarily; no in-game credit screen is required by that license.

## License

MIT — see [`LICENSE`](LICENSE). Do what you like with it.

## M35 — Project input (accepted)

Named actions/axes, keyboard/mouse/controller input, project bindings and rebinding.
See [M35 workflow and scope](docs/M35.md); M69 extends paired input.

M36 adds linked prefab assets, stable hierarchy IDs, property overrides and runtime spawning. See [M36](docs/M36.md); demo: `judas assets/scenes/prefab_demo.judas`.

M37 adds per-camera frustum culling and authored visual particle emitters. See
[docs/M37.md](docs/M37.md); demo: `./build/judas assets/scenes/particle_demo.judas`.

M39 adds project-defined tags, collision layers/masks, independent query filters
and per-camera render masks. See [docs/M39.md](docs/M39.md). Demo:
`./build/judas_editor projects/classification_demo/Classification_and_filtering.judasproj`.

### M40 scripting (accepted)

Projects can author ordered JavaScript behaviours, typed inspector properties,
controlled saved state, and normal prefab-spawned instances. The embedded
QuickJS-NG runtime exposes engine primitives instead of game-specific C++ rules.
See [scripting](docs/SCRIPTING.md) and `projects/script_demo/script_demo.judasproj`.
Operator acceptance is recorded in the milestone history.

### M41 — authored runtime UI (accepted)

Projects can author menus/HUDs as UI assets and control them through JavaScript.
Canvas/panels, text/images, buttons/sliders/toggles share reference-resolution
layout, clipping and M35 focus/input ownership. See [runtime UI](docs/RUNTIME_UI.md)
and `projects/ui_demo/ui_demo.judasproj`. M58 adds Unicode/localization.

M42: [authored collision/sensor events](docs/COLLISION_EVENTS.md), with a scripted [demo project](projects/touch_demo/touch_demo.judasproj).

M43 adds queued runtime scene transitions and bounded JavaScript session data.
See [scene transitions](docs/SCENE_TRANSITIONS.md). The two-scene demonstration is
`projects/scene_demo/scene_demo.judasproj` (E send/return, P spawn, R reload).

## Scriptable character motor (M49, accepted)

Open `projects/character_demo/character_demo.judasproj`: F1 flat course, F2 radial
planet, F3 JS swimming on the existing pool content. Generic capsule motion and
support belong to Judas; input, launch, swimming and camera behaviour are project
JavaScript. See [M49 authoring/API and checklist](docs/M49_CHARACTER_MOTOR.md).

### JudasJS developer reference (M50)

Start with [JUDASJS.md](docs/JUDASJS.md) for the current API, lifecycle, safe handles,
copyable executed examples and offline editor setup. [judas.d.ts](docs/judas.d.ts)
provides completion/type information without a TypeScript runtime dependency.

## Engine/game boundary (M51, accepted)

New projects default to script-owned gameplay. Existing projects without the
optional `legacy-gameplay` setting retain historical controls for compatibility.
The current boundary demonstration is an ordinary project:

```sh
./build/judas projects/boundary_demo/boundary_demo.judasproj
./build/judas_editor projects/boundary_demo/boundary_demo.judasproj
```

It composes queries/tags, physical joints, force-driven holding/throwing,
spacecraft control, CharacterMotor, cameras, authored UI and scene loading in
project JavaScript. See [M51 workflow/controls](docs/M51_ENGINE_BOUNDARY.md) and
[the source-backed boundary audit](docs/evidence/m51/AUDIT.md).

## Spring Range (M52)

A small ordinary JudasJS shooting-range game: [project and controls](projects/shooter_game/README.md), [architecture and candidate results](docs/M52_SHOOTER_GAME.md).
All movement/camera/shooting/score/menu policy is project JavaScript. Generic native additions expose impulse-at-point and interpolated presentation poses; authoritative gameplay remains separate.

## Later milestones (M53–M69)

Each document below is that milestone's architecture/workflow record. The
current scripting API for all of them is [JUDASJS.md](docs/JUDASJS.md).

| Milestone | Capability | Document | Example project |
|---|---|---|---|
| M53 | Navigation meshes, agents, obstacles, links | [NAVIGATION.md](docs/NAVIGATION.md) | |
| M54 | Conserved liquid reservoirs | [LIQUID_RESERVOIRS.md](docs/LIQUID_RESERVOIRS.md) | `projects/liquid_reservoir_demo` |
| M55 | Dynamic liquid surfaces | [LIQUID_SURFACES.md](docs/LIQUID_SURFACES.md) | `projects/liquid_surface_demo` |
| M56 | Integrated performance profiler | [PROFILER.md](docs/PROFILER.md) | |
| M57 | Materials and environment lighting | [M57.md](docs/M57.md), [MATERIALS.md](docs/MATERIALS.md) | `projects/material_lab` |
| M58 | Unicode text shaping and localization | [M58.md](docs/M58.md) | `projects/text_lab` |
| M59 | Additive worlds and region streaming | [M59_WORLD_STREAMING.md](docs/M59_WORLD_STREAMING.md) | `projects/streamed_range` |
| M60 | Streaming audio and acoustics | [M60_AUDIO.md](docs/M60_AUDIO.md) | `projects/audio_lab` |
| M61 | Save slots and game persistence | [M61_SAVES.md](docs/M61_SAVES.md) | `projects/save_lab` |
| M62 | Cloth and volumetric deformables | [M62_DEFORMABLES.md](docs/M62_DEFORMABLES.md) | `projects/deformable_lab` |
| M63 | Structural fracture | [M63_FRACTURE.md](docs/M63_FRACTURE.md) | `projects/fracture_lab` |
| M64 | Mesh, convex and compound collision | [M64_COLLISION.md](docs/M64_COLLISION.md) | `projects/collision_lab` |
| M65 | Developer integration: joint reads, IK, sockets, runtime joints, physical materials | [M65_INTEGRATION.md](docs/M65_INTEGRATION.md), [post-M65 review](docs/POST_M65_CONSUMER_REVIEW.md) | `projects/m65_integration`, `projects/post_m65_consumers` |
| M66 | Model import (FBX/glTF/OBJ) and reimport | [MODEL_IMPORT.md](docs/MODEL_IMPORT.md) | `projects/import_lab` (needs local-only originals) |
| M67 (provisional) | World building recipes and named authoring documents | [M67_WORLD_BUILDING.md](docs/M67_WORLD_BUILDING.md), [NAMED_AUTHORING.md](docs/NAMED_AUTHORING.md) | `projects/world_workshop` |
| M68 | Measured scalability, bounded resource handoff, import caches and export closure | [SCALABILITY.md](docs/SCALABILITY.md) | `projects/world_workshop` |
| M69 | Paired controller input and batched physics queries | [M69.md](docs/M69.md), [input](docs/judasjs/input.md), [physics](docs/judasjs/physics.md) | [cookbook examples](docs/judasjs/cookbook.md) |
