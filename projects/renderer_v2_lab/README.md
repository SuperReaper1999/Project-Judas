# Judas Renderer V2 Workshop

An ordinary editable project for comparing real rendering workloads. It reuses
The Light Factory's authoring foundation and public JudasJS/scene APIs, with a new
save identity. The original `projects/rendering_lab` and its fixtures are unchanged.

Start here:

```sh
build/judas projects/renderer_v2_lab/renderer_v2_lab.judasproj
build/judas_editor projects/renderer_v2_lab/renderer_v2_lab.judasproj
build/judas_export projects/renderer_v2_lab/renderer_v2_lab.judasproj /tmp/judas-renderer-v2-workshop build/judas
```

The modest **Ordinary gallery** starts with a moving watch camera. Click the
labelled case buttons or press **1–6** to select a workload. Stress cases are
explicit choices: the watch camera never silently escalates the workload.

| Key | Editable case | What to look for |
|---|---|---|
| **1** | Ordinary gallery | 256 repeated modules, 16 authored lights; a modest control. |
| **2** | Mixed PBR materials | 1,024 modules, 64 original material assets with colour/normal maps, roughness and metalness. |
| **3** | Ordered sprite field | 4,096 overlapping sprites; sorting layers, Y sorting, alpha, flipped pivots and different sampling modes. |
| **4** | Generic particles | Eight ordinary emitters, each bounded to 1,250 particles; up to 10,000 CPU-simulated visual particles. This is separate from weather precipitation. |
| **5** | Light overlap | 1,024 modules and 256 overlapping point/spot sources; observe bounded selection and overflow diagnostics. |
| **6** | Repeated-object stress | 4,096 compatible modules; compare grouping/culling costs with the modest case. |

Additional controls:

| Control | Action |
|---|---|
| **T** | Watch camera / free camera. Watch mode releases the pointer for clicking the case buttons. |
| **WASD + Q/E, mouse** | Move vertically/horizontally and look in free 3D flight. In the sprite case, WASD pans the orthographic view. |
| **L** | Toggle individual authored light sources. |
| **H** | Toggle authored spot-shadow requests. Directional sunlight remains part of the scene. |
| **C** | Toggle the two real live camera monitors in 3D cases. Initially off. |
| **V** | Toggle two independent local-player views over the same world. |
| **F8** | Open the built-in profiler for actual CPU/GPU passes, draws and resource counters. |
| **F6 / F7** | Save / load the workshop state using its own save identity. |
| **R** | Reset the current authored case. |
| **Esc** | Pause/resume and show the simple menu. |

The HUD reports **authored** counts, not guessed visible draws. Its approximate
presentation rate counts real presentation callbacks against wall time; it is a
convenience observation, not an isolated CPU/GPU benchmark. The matched native
benchmark in `tests/RendererV2Benchmark.cpp` records those costs separately with
fixed workload settings, raw samples, query validity and exact build provenance.

## Editing and extending

Each case is a normal file under `Scenes/`. Select a module, light, sprite or
emitter in Editor V2 and use the normal Inspector. `Assets/materials/` contains
64 real PBR finishes; their texture maps are source PNGs under `Assets/textures/`.
The particle case uses ordinary `ParticleEmitter` authoring, with an explicit
capacity on each emitter. It does not run a fluid solver or rigid-body droplets.

`Assets/scripts/lab.js` owns only camera, case selection, lights, local views,
UI and save policy. Its `CASES` list explains each case; each controller's normal
script property `caseKey` selects that entry. The script creates and retires only
its own local-player views. No project name or scene ID branches exist in C++.

To regenerate the original procedural source content:

```sh
python3 projects/renderer_v2_lab/Tools/build_lab.py --replace
```

This deliberately overwrites the generated scenes/materials/textures. Preserve
manual edits first. Pillow is an authoring dependency for regenerating the PNGs,
not a runtime/export dependency. The generator itself is retained as source.
The copied DejaVu font includes its redistribution licence in `Assets/fonts/`.
All new material/texture source artwork is original procedural workshop content.

## Functional smoke route

```sh
JUDAS_WORLD_STATE=none \
XDG_DATA_HOME=/tmp/judas-renderer-v2-workshop-user \
JUDAS_TEST_SCRIPT="$PWD/projects/renderer_v2_lab/Tests/smoke.test" \
build/judas projects/renderer_v2_lab/renderer_v2_lab.judasproj
```

Use a fresh writable user-data location. This route checks actual scene changes,
save/load, pause, local views, monitor toggles and reset through the game's normal
logical inputs. It is a functional control, not real-time performance evidence:
the fixed-clock harness does not draw every unrequested frame.

## Scope and human review

The scene contains no physics bodies: it isolates rendering without changing any
engine physics setting. Real games must budget simulation and rendering together.
GPU path/capabilities are selected by the engine, not altered by this game. A
newer OpenGL version alone does not establish faster rendering. Android/browser
and native Windows results belong to their own qualification records.

1. Let the starting watch camera run, then try free flight and hand control back.
2. Select every case. Check material normals, sprite transparency/order and
   particle appearance; try F8 instead of assuming a low presentation rate is GPU cost.
3. Toggle shadows, monitors and split views separately. Verify all views remain
   correct and ordinary controls work after returning to the first case.
4. Save, change case and load; pause/resume; reset. Edit/save/reopen the project
   and play a relocated standalone export.

Human acceptance and reproducible renderer measurements are recorded separately;
this lab does not assert that every GPU can run every stress case at 60 Hz.
