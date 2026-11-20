# Cameras and render targets

Cameras render ordinary scenes to owned targets for supported material/UI
consumers. No general portal, recursive pass or reflected-camera clipping system
is supplied by this camera component. See [renderer ownership](RENDERER_V2.md).

## Ownership and path

An optional `SceneRenderCameraComponent` belongs to an ordinary stable scene
object. Its object transform defines local **-Z forward / +Y up**. Perspective
settings are vertical FOV, near/far planes, width/height (1–4096), enabled state,
an integer rendered-frame cadence and a shadow setting. A camera may share a rigid-body object;
presentation reads that body's resolved/presented pose, not a second simulation.

`RuntimeWorld` owns transient camera presentation records and borrows the host's
Renderer, which must outlive the world, as with existing GPU-backed world data.
Enabled cameras allocate lazily on their first scheduled render. Disabled cameras
perform no render work and allocate no initial target. Disabling an already used
camera retains its last image until world destruction. Stop destroys the targets.

Renderer alone creates/deletes the framebuffer, RGBA8 colour texture and depth24
renderbuffer. Creation checks framebuffer completeness and reports failure.
Resize is transactional: failure preserves the old target; successful replacement
invalidates its old borrowed texture handle. Failed runtime allocation is reported
and retried after a resolution change or world rebuild. Opaque table entries follow
the existing append-only texture-handle convention; deleted GPU objects are not
kept alive. Raw OpenGL remains in Renderer.

`RenderWorldFrame` renders scheduled secondary targets before the main camera.
All views use the same interpolation sample and scene lighting. Each shadowed
view selects its own lights and cascade coverage. Clear **Shadows** on a render
camera to skip shadow-map draws and shadow sampling for that target while keeping
direct lighting, render layers and cadence. The main camera retains its own shadow
policy. See [JudasJS camera controls](judasjs/effects-camera.md#shadows-on-render-target-cameras)
for the equivalent runtime setting. The target pass restores the draw/read
framebuffer, viewport and camera matrices.

## Ordinary material consumption

`SceneRenderComponent::textureCamera` references the camera object's stable ID.
It is mutually exclusive with `textureAsset`. Box, sphere and imported-mesh
renderables resolve the generated colour through the ordinary `TextureHandle`
and lit `DrawMesh` shader. Generated textures are not disk assets and have no
AssetDatabase identity. Compound/terrain target-texture consumers are not supported.

Sampling the active target is intercepted at the actual texture binding in
Renderer and replaced with its white fallback; `feedbackFallbacks` counts this.
Disabled cameras retain their last completed image; destroyed cameras resolve to
the white fallback. References identify stable authored definitions, so consumers
may retain a reference after camera destruction without invalidating delta preflight.
Other targets may display their last completed image. Passes run in authored order;
there is no recursive traversal, dependency scheduling or portal rendering.

## Author a camera-fed surface

1. Open an editable 3D project with a working player or main view. To start
   from scratch, follow [First project](FIRST_PROJECT.md), then open its saved
   scene in the editor.
2. Create a subject and a separate display using **World → Create → Static
   box**, or use existing scene objects. Give the subject a distinctive colour.
   Put the display where the main view can see it.
3. Choose **World → Create → Render camera**. Set its transform so local
   **−Z** points at the subject and the display is outside its view. Set the
   target size, for example 640×360, and leave the camera enabled.
4. Select the display. In its **Render** component, choose the new camera
   under **Camera texture**. This clears any disk texture assignment. The feed
   uses the display's ordinary lit material.
5. Save and Play. The target starts rendering in Play or the standalone game.
   Edit mode shows the material fallback. Stop, adjust the camera or its
   update cadence, then Play again to check the change. The main camera keeps
   its own view and shadow policy. Camera and material edits support undo/redo.

The registered `render-cameras` case is separate automated evidence. It creates
its own fixture and checks target rendering and readback, including the white
fallback for self-feedback. From a configured source checkout, build and run it
through the [validation manifest](../tools/validation/suites.json) runner in a
graphics-capable session:

```sh
cmake --build build --target judas_render_camera_tests -j2
python3 scripts/prealpha_validate.py --build-dir build --test render-cameras
```

## Serialization and compatibility

Scene format 3 gains optional `render-camera` / `camera.*` fields and
`render.texture-camera`. Existing scenes remain readable and default to shadows
enabled. The optional `camera.shadows` field records an authored opt-out; world
snapshots store the current runtime setting. Invalid settings and
missing/conflicting camera references fail rather than silently selecting an asset.
Camera settings and stable material references enter canonical fingerprint
**schema 3**. Under the existing strict FTFT1 policy, schema-2 saves are rejected;
there is no silent migration, deletion or reinterpretation. Authored scenes remain
separate from runtime images, cadence counters and world deltas.
