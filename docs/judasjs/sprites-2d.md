# Native sprites and 2D cameras — Stage 1

Experimental development toward v1.1.0; this is not a published v1.1.0 release.
Sprites share Judas's scene, renderer, resources, prefabs and persistence. They do
not create bodies. Native planar collision/controllers, tilemaps and animation
tracks are later stages. Existing perspective projects keep their defaults.

## Start in the editor

File → New project → **New 2D game – top-down** or **side-view**. Both create an
ordinary `Assets/game.js`, original `explorer.png`, authored HUD and editable
scene. WASD/arrows move in XY; the side-view starter deliberately has no jumping
or collision yet. The toolbar **2D view** switches to XY orthographic editing;
middle-drag pans, wheel zooms, F frames selection. Switch back to 3D at any time.
Project Settings → General stores the default editor view. This never enables
or disables a runtime subsystem.

Import an image through the Asset Browser, then drag it into the scene. It is
created, selected and sized at 32 pixels per world unit, preserving image aspect.
Asset context menu → **Create sprite in scene** does the same. Hierarchy/Create →
**Sprite (2D image)** creates a unit sprite; choose its image in the inspector.
Root/child creation, transform editing, undo/redo and prefabs remain ordinary
scene operations. Click visible pixels; disable that option for bounds selection.
**Alt-click** cycles overlapping selectable objects. Transparent pixels don't
steal selection from the sprite underneath.

## `Entity.sprite` and `Entity.setSprite(settings)`

`sprite` is a detached snapshot or null for other render shapes. `setSprite` is
an atomic partial patch, returning false on a non-sprite; invalid values or
unknown options throw. It changes presentation only, not collision or velocity.
The image remains the renderer's ordinary texture AssetId, authored in the
inspector. Runtime image replacement and atlas import automation are not part
of this stage.

```js
entity.setSprite({size:{x:1,y:1.25},pivot:{x:.5,y:.9},flipX:true,
                  tint:{x:1,y:.8,z:.7,w:1},sortingLayer:1,order:3});
```

- `size`: positive finite world width/height, maximum 1,000,000.
- `pivot`: image top-left normalized coordinates, each 0..1. Geometry flips
  about this pivot; the entity's transform stays fixed.
- `rect`: normalized top-left image rectangle `{x,y,width,height}`; positive
  extent contained in the image. Supports manually authored sprite-sheet regions.
- `tint`: RGBA `{x,y,z,w}`, each 0..1, multiplied by render color/alpha.
- `flipX`, `flipY`: mirror geometry/UV about the pivot.
- `nearest`: nearest sampling (default true), or linear. Clamp-to-edge; linear
  atlas rectangles need authored padding to avoid neighbouring texel bleed.
- `sortingLayer`, `order`: integers −32768..32767; higher draws later.
- `ySort`: within equal layer/order, smaller world Y draws later. Stable identity
  breaks ties. Use consistent Y sorting within a layer; world XY convention.
- `pixelSnap`: presentation grid snapping in pixel-perfect views (default true).
- `alphaPick`: editor visible-pixel selection (default true); zero alpha excluded.

Sprites are unlit, two-sided and alpha blended. Opaque 3D geometry can occlude
them through depth; sprite ordering controls sprites, not arbitrary transparent
meshes, water or particles. They don't cast shadows. This initial queue uses one
mesh submission per sprite; batching is a later optimization, not a scalability
claim. Bounds/alpha picking caches decoded images with a 64 MiB bound.

## Orthographic and pixel-perfect views

`world.setView(pose,fov,range)` and `LocalPlayer.setView` accept the same optional
lens keys in `range`. `near`/`far` retain existing semantics. Omitted runtime lens
keys use defaults on each call; no implicit accumulation.

```js
world.setView({position:{x:0,y:0,z:10},rotation:{x:0,y:0,z:0,w:1}},60,
  {near:.01,far:100,projection:'orthographic',height:8,pixelPerfect:true,
   referenceWidth:320,referenceHeight:180,pixelsPerUnit:32});
const point=world.project({x:0,y:0,z:0});
const ray=world.unproject({x:point.x,y:point.y});
```

`projection` is `perspective` (default) or `orthographic`. `height` is the visible
world height in ordinary orthographic mode. Pixel-perfect mode derives height
from `referenceHeight/pixelsPerUnit`, uses the largest fitting integer scale,
centers the content rectangle and snaps camera/sprite presentation to the pixel
grid. If the window is smaller than the reference, it downscales fractionally;
that case cannot promise one display pixel per source pixel. Reference dimensions
are integers 1..4096. PPU is positive finite, maximum 65536. Pixel mode with
perspective is rejected. It does not quantize gameplay/physics poses.

`world.viewport` still reports full viewport dimensions. `world.project` returns
normalized full-viewport coordinates including letterbox offsets.
`world.unproject` returns `{origin,direction}`, or null without a view/outside
its content. Orthographic origins lie on the near plane; rays are parallel.
`world.viewRay` uses the center ray. Local-player `project`/`ray` follow their
existing per-player coordinate conventions, with content bars rejected.

Render-camera inspectors and `Entity.setCameraProjection` support these same
lens fields. `Entity.camera.lens` reports them, separately from the render-target
`width`/`height`. Projection patches retain omitted lens fields on render cameras.
Orthographic cameras reject portal frustums/oblique clip planes; clear those
explicitly before switching. Main, split-screen and secondary cameras use their
own projections and LOD/resource interests. Lens settings survive authored
serialization and new cold saves; existing default-camera saves remain unchanged.

See [Stage 1 report](../2D_STAGE1.md) and [camera APIs](effects-camera.md).
