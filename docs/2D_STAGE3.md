# Native 2D Stage 3 — sheets, animation and events

Experimental development toward v1.1.0, from
`9e3d528632bfd692539f7e4d613b1bd55d3b7700`. Not a published release or human
acceptance. Main, Advanced Fluids and independent research remain untouched.

## Implementation

- `.judassprite` is a native typed CPU resource referencing an ordinary texture.
  Sheet frames have stable keys, editable display names, pixel rectangles and
  pivots. Clips own ordered variable-duration frame references and named markers.
  Import validation, scene/prefab references and export dependency closure include
  the new asset. Runtime resource preparation checks image dimensions, bounds,
  references and version. Malformed/removed keys cannot silently select new art.
- `SpriteAnimation` owns bounded parsing, sampling and transactional clock steps.
  Marker intervals are shared with existing skeletal animation through the small
  typed `AnimationEventIntervals` traversal. Sprite marker times are double;
  historical skeletal marker storage and binary-save layouts stay unchanged. Existing skeletal public APIs retain
  their wrappers and timing rules. No skeleton, new renderer or gameplay framework
  is introduced. Frame blending is a deliberate cut, not a false crossfade.
- Optional `SceneRenderComponent::spritePlayback` stores instance-owned clip,
  double phase, speed, loop and pause policy. `WorldSpriteAnimation` updates only
  registered owners at the existing fixed boundary. Render/camera passes sample
  without advancing or dispatching events. The absent-feature path exits before
  world traversal. Animation geometry changes visual size/pivot, never body mass,
  collider or physical position. Runtime visual records publish in one linear pass.
- Sprite samplers already disable mipmapping. Smooth atlas sampling now stays
  within each frame's texel centres; nearest mode preserves exact pixel regions.
  Frame flips, tint, layer/order and Y sorting reuse the existing sprite renderer.
- The sheet/clip editor supports cell/margin/spacing slicing, clickable cell
  outlines, named/manual frames, pivots, clip frame ordering/durations and markers.
  Preview and scrub have a separate sampler/clock and cannot run gameplay events.
  Explicit validated saves, bounded draft undo, disk-conflict checks and guards
  for unsaved drafts protect authoring. The preview remains beside scrolling
  authoring controls. Panel visibility uses the existing workspace registry.
- `Entity.spriteAnimation` and `setSpriteAnimation` expose detached observations
  and deliberate atomic patches. Gameplay reads the prior completed sample once
  per fixed callback. Seeks clear observations without delivering events; pause
  freezes phase. Overflow publishes no partial markers or partial clock advance.
- Normal scene/prefab definitions carry instance clocks through streaming and
  modern saves. A conditional sprite-content participant pins sheet semantics;
  incompatible changed content requires explicit migration. SaveService validates
  that participant against actual snapshot components. Restoration does not
  replay the last completed sample's markers.

Full API, content schema and exact boundaries:
[Sprite animation reference](judasjs/sprite-animation.md).
Key source modules: `src/SpriteAnimation.*`, `src/WorldSpriteAnimation.cpp`,
`src/editor/SpriteSheetEditor.*`; integration stays in existing scene/resource/
script/persistence/presentation owners. Existing Stage 1/2 projects are preserved.

## Playable content

[`Sprite Animation Workshop`](../projects/sprite_animation_workshop/Sprite_Animation_Workshop.judasproj)
is an editable, ordinary consumer project using Stage 2 MK2 bodies and JavaScript.
Original pixel art and deterministic generation settings are in
`tools/demos/generate_sprite_animation_lab.py`; the source PNG and JSON are included.

WASD/arrows move, Space jumps, **E** winds up/strikes/recovers. Only the strike
marker applies a real impulse to a nearby native physical object; the practice
crate begins to the right of the platformer spawn. Idle/run/jump/interaction
selection lives in the project script, not C++. Footsteps are observable markers;
the sample counts them, rather than claiming an unprovided sound asset.
**1/2/3** switch side-view/top-down/tilted scenes, **R** restarts,
**F6/F7** use modern save slots. Scripts and content can be edited independently.

## Verification

The evidence directory records exact source/binary hashes and focused results.
Linux Release, GCC 15.2, Ryzen 5 5600; actual OpenGL application checks use Mesa
llvmpipe. No Windows/Android or hardware GPU qualification is inferred.

- Timing/content/editor-draft tests cover variable frame times, exact boundaries,
  loops, reverse, speed, pause, seek, completion, bounded overflow, corruption,
  retained keys, missing keys, deterministic serialization and stale-disk saves.
- Actual gameplay integration verifies real marker-driven body movement, no early
  or duplicated strike, walking/jumping transitions, shared-content/independent
  prefab clocks, genuine unload/revisit/resume and repeated scene construction.
- A separate process restores the checksum-protected save and matches the
  uninterrupted clip, exact double phase and strike count. Changed sheet content
  and removed frames have independent rejecting controls.
- Actual GL pixels prove red/green sheet frames select correctly at the exact
  boundary, smooth sampling has no adjacent-frame bleed, and repeated render
  passes do not advance clocks. These are pixel assertions, not screenshots alone.
- Full application interaction, scene modes, modern save/load, editor Play/Stop,
  theme/scale checks and relocated export exercise the ordinary owners. A previous
  application load failure exposed the missing SaveService participant-inventory
  registration; that failed run is preserved along with its corrected rerun. A decimal endpoint
  marker also exposed float rounding beyond the sprite clip boundary; the
  preserved negative case passes with native double sprite marker storage.
- Focused existing planar/sprite, skeletal/event/root-motion/cold-save/application
  and MK2 rotation regressions protect prior behaviour. This is a selected
  qualification, not a full-engine/platform certification.

`judas_sprite_animation_benchmark <project.judasproj>` measures optional playback
publication with 128/1024 bodyless sprites, off/on, 30 warmups and 240 measured
fixed updates. It reports median, nearest-rank p95, worst and actual final phase.
It excludes rendering, physics, scripting and initial content preparation; no
whole-frame FPS or GPU speedup claim follows from it.

## Boundaries

- Fixed-size source images and explicit re-slicing/reload; no automatic atlas
  repacking, gutters/mip-chain generation, skeletal sprites or clip crossfading.
- Hard bounds and marker-overflow rejection are explicit. Developers must observe
  diagnostics and fix unreasonable time/content rather than relying on catch-up.
- Modern slots and supported regional snapshots preserve clocks. Legacy world
  delta saves and universal online replication are not claimed for playback.
- Initial mod-package sprite sheets are explicitly rejected until separately
  qualified. Base-project sheets, textures and prefabs are supported.
- Tilemaps/animated tiles and final cross-platform v1.1.0 qualification remain later
  stages. No Stage 4 work, release/version changes or main promotion was started.

## Human checklist

1. Open the workshop; walk/run, jump, press E by the practice crate. Confirm the
   visible wind-up precedes physical contact and there is only one hit per action.
2. Open `Assets/traveller.judassprite`. Preview/scrub run and interaction clips;
   the live world must not receive markers while previewing. Click frame outlines.
3. Change a display name/pivot, reorder frames, edit a duration and strike time;
   use draft undo. Save explicitly and start Play to see the new timing.
4. Try removing a referenced frame: save should explain the missing key. Undo it.
   Try closing/playing/exporting with a dirty sheet: save/discard remains required.
5. Pause/resume or reverse a clip through JS. F6/F7 save/load, then test the
   relocated standalone. Try 1/2/3 and Stop/Play without losing authored content.
6. Repeat the editor actions in light/dark themes at your preferred UI scale.
