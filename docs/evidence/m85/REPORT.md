# M85 final Linux candidate

Status: Linux automated validation passed; human review pending. Windows M85
validation is unperformed and nonblocking. No main promotion is authorized.
Starting Experimental: `e421645b3c310508ff618fe1e880a1b8cdbc019b`.

## Delivered mechanism

Optional binary64 cube-sphere terrain, bounded adaptive render tiles with actual
parent-triangle geomorphs and shared coarse-edge/corner resolution, independent
ordinary cooked local colliders, explicit checked chart changes, origin-aware M59
retention, ordinary editor component/prefab/scene authoring and modern save/export.
Legacy RadialTerrain, old projects and fluid/research paths remain unchanged.

Detailed equations, public API, measurement definitions and limitations:
[model](../../M85_PLANETARY_TERRAIN.md),
[API](../../judasjs/planetary-terrain.md),
[lab](../../../projects/planetary_terrain_lab/README.md).

## Final-source tests

`final/results.json` records exit status and individual stdout for four native M85
executables, twelve affected existing regressions, the graphical application and
TypeScript/API check. All passed. Numerical assertions include per-vertex finite
checks; 10074 assertions are not 10074 independently certified capabilities.

- Terrain: 10074/0; descriptors, negative budgets/provider certificate, binary64
  centimetre elevations, distant/rotated centres, independent seams, parent planes,
  deterministic full-face coverage and hard residency bounds.
- World: 220/0; actual cooked two-centimetre detail, 180 supported motor steps,
  pending-cook old-support retention, meaningful exhaustion negatives, cube-face
  contacts, generation-safe rebase, runtime tag propagation, authored reset,
  prefab named properties, cold physical endpoints and destruction.
- Origin physics: 13/0; independent translated/untranslated warm joint/contact
  endpoint, moving kinematic target, sleep/velocity/event point/query history,
  nonfinite rejection before mutation.
- Streaming: 20/0; actual async in-flight negative, retained ordinary/motor state,
  remote capture origin, unload/revisit and fresh archive-v7 physical endpoint.
- Graphical project: 37/0 across 860 frames; supported runtime prefab, real probe,
  cold save/publication, actual restored rolling motion/contact, orbit/descent,
  mixed-LOD grids, rotated RadicalGravity, face-edge walk, rebased save and reload.
- Regressions: CharacterMotor 32, gravity selection 70, M23 coordinates,
  joints 29, kinematics 96, casts 26, rigid contacts, collision geometry 1303,
  streaming 82, save storage 28, heightfield authoring 89, volumetric terrain 69.
- TypeScript 5.9.3 / drift: 373 public symbols, 32 exports, 261 native operations,
  42 declaration examples, four result-shape checks and four negative drift controls.
  This run is static/source/type verification, not a claim of live enumeration.
- Existing editor autotest on isolated project copy: duplicate/undo IDENTICAL,
  edit/Play screenshots, no failed resources/jobs, Play/Stop authored scene IDENTICAL,
  save succeeded. Numeric component gestures still require operator inspection.
- Normal exporter: seven assets/two scenes, 54,906,650 bytes. Moved from its export
  location, launched actual packaged executable from `/tmp`, bounded probe/orbit,
  pause/resume and modern save/load service completion with screenshots, exit 0.
  Full graphical sequence against moved asset closure separately passed 37/0.

Release targets and relevant executables built successfully. Two pre-existing
RuntimeWorld material misleading-indentation warnings remain; no zero-warning
claim. The complete production suite was not repeated for this focused addition.
No Windows M85 functional/performance result is claimed.

## Measurements

Host details are in `final/host.txt`. CPU i7-4790, GPU GTX 970.
Main application CPU frame median/p95/max: 10.469/24.679/281.527 ms, excluding
present wait; fixed-step p95/max 1.664/9.829 ms. Cold visible-cover preparation
253.159 ms; transition preparation p95 19.233 ms; ground render subset p95
16.901 ms. No worst-frame 60 Hz certification. Peak leaves/resident/contact tiles
384/500/34. Exact measured mixed-LOD seam 1.31708901597e-9 m; local float
reconstruction 6.45080608199e-8 m. Lab budget can report 5.23 px vs requested 2 px.
See per-run stdout for separate moved-package values and native timings.

## Reproduce

From the repo root, configure an ordinary Release `build`. Build judas,
judas_editor, judas_export, judas_planet_{terrain,world,streaming,application}_tests,
judas_origin_physics_tests and the regression targets named in the runner.

```
python3 scripts/m85_validation.py --output /tmp/m85-receipt --regressions --graphics \
  --typescript /path/to/typescript/lib/typescript.js
JUDAS_EDITOR_AUTOTEST=/tmp/m85-editor build/judas_editor \
  projects/planetary_terrain_lab/planetary_terrain_lab.judasproj
build/judas_export projects/planetary_terrain_lab/planetary_terrain_lab.judasproj \
  /tmp/m85-export build/judas
```

Use a copy for editor tests. TypeScript is tooling only, installed outside the
repository for this run; no runtime/npm dependency was added. Graphical tests need
an available desktop GL context. Exported executable discovers its adjacent package
from an unrelated working directory; see retained package harness fixtures.

## Scope and preserved work

One physical interest/planet, explicit prefetch and loading readiness; 8 km checked
live-physics chart, one active origin, supported-component rejection rather than
silently translating unknown physics. No global DEM database, volumetric holes in
the new sphere, arbitrary global simulation of retained vehicles/fluids, or exact
anonymous tile contact callback/warm-cache save continuity. Cold initial LOD work
is synchronous and transitions have bounded but measurable CPU upload costs.

No old evidence/prototype, independent research, accepted fluid implementation,
existing game or main branch file was changed by M85. Unrelated existing untracked
M78/M83 sources/tools remain outside its staging inventory. Development failures
and their actual logs are retained in DEVELOPMENT_NOTES.md/development/.

## Human checklist

1. Open the lab in Editor; move/jump/throw probes and inspect local relief.
2. F1 ascend, F2 descend; F4 automatic continuous tour. Watch LOD and budget HUD.
3. F3 face-edge visit; walk/jump across it and inspect radial gravity/support.
4. F6/F7 before and after chart changes; check real supported endpoints/probes.
5. Leave/revisit the streamed station; inspect position/state.
6. Edit a band/raster sample and budgets, undo/redo, save, Play/Stop.
7. Run the moved standalone and repeat traversal/save/pause checks.

Human acceptance remains pending. Claude PR #15 is reviewed separately; this M85
receipt describes the candidate before portal integration.
