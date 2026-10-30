# Native 2D Stage 1 — sprites, cameras and authoring

Experimental implementation, 2026-10-30. Starting checkpoint:
`4aca1c3ff70e385dd5cc81dc36983167faa0b36c`. No v1.1.0 release, main promotion or
human acceptance is implied. [Stage 0](2D_STAGE0.md) remains historical evidence.

## Delivered

- Native `SceneShape::Sprite` in the existing render component. Loose image
  AssetIds, normalized source rectangles, pivots, flips, RGBA tint, opacity,
  nearest/linear sampling, deterministic layer/order and optional world-Y sorting.
  No extra GPU allocation until the first sprite is drawn; no implicit body.
- Optional `CameraLens` shared by main, local-player and secondary render cameras.
  Orthographic framing, project/unproject, integer pixel presentation, camera and
  sprite snapping, centered letterboxing, and per-view resource/LOD interests.
  Perspective is the unchanged default. UI retains its full output viewport.
- File → New project → New 2D game, top-down or side-view. Both are immediately
  controllable visual starters, with original pixel art, authored HUD and readable
  project JS. Controls are project-owned; unrelated legacy flight/debug bindings
  are excluded from these new templates.
- Editor XY view toggle, middle-pan, wheel zoom, frame selection; image drag/drop
  and Asset Browser creation; sprite inspector; visible-pixel/bounds picking and
  Alt-click overlap cycling. Existing root/child creation and undo are retained.
- Normal scene/prefab properties and modern save participants carry sprites/lenses.
  `view-projection` participant version 2 adds the lens only when nondefault;
  existing default-camera serialization/fingerprint bytes retain their old paths.
- Public `Entity.sprite`, `Entity.setSprite` and `world.unproject`; extended lens
  options on existing camera APIs. Named JS vector objects, synchronized types,
  inventory, structured result checks and runtime enumeration.

Implementation modules: `src/Sprite.*`, `src/CameraLens.h`,
`src/RendererSprites.inl`, `src/ScriptSpriteValues.inl`,
`src/ScriptCameraValues.inl`; existing renderer, scene, world, persistence and
editor integration paths are extended. There is no second renderer/scene system,
new physics backend or game-specific native controller.

## Try it

Editable project: `projects/sprite_workshop/Sprite_Workshop.judasproj`.

```sh
build/judas_editor projects/sprite_workshop/Sprite_Workshop.judasproj
build/judas projects/sprite_workshop/Sprite_Workshop.judasproj
```

Use the matching current Experimental build directory in place of `build`.
WASD/arrows or a supported pad's left stick move the explorer; edit `Assets/game.js`
and its inspector properties. The side-view template has no fake jumping or
collision: native planar physics/controllers are **Stage 2**, not delivered here.
[Complete workflow/API](judasjs/sprites-2d.md).

The tested standalone is outside the repository at
`/tmp/judas-2d-stage1-relocated/SpriteWorkshop-Final/`; run `./judas` there. It was moved
from its export directory and launched with unrelated `/tmp` as the working
directory. This is a development export, not a portable published SDK release.

## Verification and reproduction

Fresh Release build, GCC 15.2, Linux x86-64, existing OpenGL backend. The actual
reported graphics implementation was Mesa 26.0.8 llvmpipe / LLVM 21.1.8; these
measurements do not certify a hardware GPU, Windows or Android.

```sh
cmake -S . -B .cache/2d-stage1-build -DCMAKE_BUILD_TYPE=Release \
  -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build .cache/2d-stage1-build --target judas_editor judas judas_export \
  judas_sprite_math_tests judas_sprite_integration_tests judas_sprite_render_tests --parallel 2
python3 scripts/prealpha_validate.py --build-dir .cache/2d-stage1-build \
  --group 2d --expected-channel Experimental --output /tmp/NEW-owned-2d-results
# The integration gate writes sprite-project below its result directory.
.cache/2d-stage1-build/judas_sprite_integration_tests /tmp/NEW-owned-2d-results/sprite-project --restore
node scripts/check_judasjs_api.mjs
```

The registered `2d` group checks native value/projection contracts, real project /
script / picking / prefab / persistence paths and actual GL pixels. A separate
process reopens the original modern slot and verifies actual restored sprite and
lens values. Runtime API checks include invalid atomic patches, detached getters,
orthographic rays and independent player projections.

Rendering checks exercise alpha/sort negative controls, source-image orientation,
transparent texels, flips, source rectangle, pivot, Y sorting, main world drawing,
secondary targets, distinct split-screen views, subpixel stability and full-output
UI. A 200-sprite submission/completion probe reports mean/median/p95/worst; this
is not a separate CPU/GPU breakdown or a promised batching speedup.

Final registered Stage 1 runs passed **20 math + 51 integration + 23 rendered
checks**, plus **5** checks in a separately started restoration process.
The representative 200-sprite run (60 measured frames, after 10 warm-up frames)
reported mean **1.6072 ms**, median **1.53301 ms**, p95 **1.90493 ms**, worst
**2.21093 ms**, including submission and GPU completion. Host: Ryzen 5 5600,
12 logical CPUs, 7,856 MiB RAM, software GL above. These are diagnostic local
measurements, not comparisons against a nonexistent earlier native-sprite path.

Retained regressions also include hierarchy creation **113** checks and the
existing project/scene suites and **10,074** planetary terrain checks, all passing. Retained regressions: editor navigation **59**, geometry picking **36**, first
project **24**, local multiplayer **37**, prefab **37**, presentation **44**, and
runtime rendering **54** checks passed in the focused runs. API/type/inventory
checks include live VM enumeration, **18** structured results, **248** callable
signatures, **54** typed examples and negative drift/type controls.

A bounded before/after render-path comparison recompiles the exact starting
`Renderer.cpp` and `WorldPresentation.cpp` against shared current headers/other
modules. All six 3D PNGs are byte-identical: imported materials, multipart controls,
blending, secondary texture and two sun/shadow directions. This is an isolated
render-path comparison, **not** a full historical engine rebuild. Sources,
commands, hashes and the warning about the new unused Sprite enum in the historical
switch are retained. Ordinary perspective matrices are also checked exactly.

Editor automation opens the authored project, duplicates/undoes, Plays/Stops,
verifies the authored scene is byte-identical, and saves through the production
serializer. Additional template-creation runs exercise the same CreateProject
path used by the UI. Screenshots are automation evidence, not human acceptance.

## Corrected failures / experimental record

Original attempts and corrected follow-ups are retained under
`docs/evidence/2d-stage1-20261030/` (with larger local build artifacts in
`.cache/2d-stage1-evidence/`). They include:

- Initial missing include/UI field name and starter JSDoc/type errors: corrected;
  subsequent build/type checks passed.
- A real new sprite implementation defect: externally supplied textures had no
  owned nearest sampler. Saturated texture pixel tests exposed interpolation.
  Tiny fallback image/material ownership now creates the sampler through the
  existing resource lifecycle; nearest, orientation, alpha and atlas tests pass.
- The first save test incorrectly used legacy `.judasstate` deltas, which do not
  promise complete runtime render/view state. No legacy contract was expanded.
  Modern slot capture, reconstruction and separate-process restore pass.
- Save-fixture metadata initially used an invalid short content fingerprint;
  production storage rejected it. The fixture now uses the required SHA-256 shape.
- A pivot test used bottom-left while expecting top-left; corrected to the declared
  coordinate convention. A prefab override test initially supplied unquoted JSON
  instead of the generic serialized property value; it now uses ObjectProperties.
  No production geometry/override behavior was changed to accommodate either test.
- An initial gate invocation lacked opt-in validation entrypoint wrappers. It was
  correctly unavailable, not a pass. Gates are built with validation enabled.
- The first editor run could not save preferences outside sandbox writable roots;
  subsequent isolated runs use owned XDG data directories. User preferences and
  projects are preserved.

## Boundaries and remaining work

- No native planar collision/controller, sprite animation, named sheet frames,
  automated slicing, tilemap or palette workflow. Manually authored normalized
  image regions are available; stable named sheet-frame migration belongs later.
- One mesh submission per sprite; no sprite batch/instance throughput claim.
  CPU alpha-picking cache is bounded to 64 MiB. Large image decode/picking and
  image reimport require broader production qualification in later stages.
- Sprites sort against sprites and depth-test against opaque geometry. General
  interleaving with transparent 3D meshes, water and particles is not promised.
  They are unlit and do not cast shadows. Existing scene-wide HDR/exposure still
  applies when explicitly enabled; default 2D templates use the existing display
  path. Linear-filtered atlas regions need authored padding to avoid edge bleed.
- Pixel presentation is integer-scaled when a reference canvas fits. Smaller
  windows fractionally downscale. Arbitrary rotated/non-pixel-sized artwork is
  not promised to land on a crisp source-pixel grid. Full-viewport UI follows its
  own existing layout/reference-size policy.
- Orthographic cameras reject portal frustum/oblique clip combinations. Existing
  perspective portals remain supported. Atmospheric volume effects and planetary
  ground/orbit art are not qualified as 2D orthographic workflows in this stage.
- Authored lenses and modern main-view saves persist. Per-player view policy is
  reconstructed by project JS under the existing local-player lifecycle; this
  stage does not invent a new persistent player-camera ownership system.
- Windows/Android implementation uses the shared backend paths, but Stage 1 has
  not been built/tested on those platforms. Human UI/gameplay acceptance pending.
- Main, release tags/packages/channels, physics, the Advanced Fluid lock, protected
  historical evidence and Terra/Solbert fluid work remain unchanged.

## Human checklist

1. Open Sprite Workshop, Play and move; inspect clean image edges and stable follow.
2. Stop; pan/zoom in 2D, switch to 3D and back, frame/select the explorer.
3. Drop your own PNG into the scene, change pivot/flips/tint/order and undo/redo.
4. Click transparent padding; toggle bounds selection; Alt-click overlapping sprites.
5. Parent/move a sprite, save/reopen, make a prefab and inspect an instance override.
6. Create both New 2D project templates and Play immediately; inspect game.js and controls.
7. Resize the standalone through wide/tall windows; inspect letterboxing and UI.
8. Open an existing 3D project; confirm its perspective/editing workflow.

Stage 1 is an Experimental foundation for review, not completion of v1.1.0 or
permission to begin Stage 2 automatically.
