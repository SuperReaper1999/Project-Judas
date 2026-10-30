# Sprite sheets and frame animation

Stage 3 on Experimental. Native frame animation shares the ordinary scene,
renderer, resources and fixed-step lifecycle. It does not use a hidden skeleton.

Right-click a tracked image in **Assets → Create/edit sprite sheet**, or open a
`.judassprite` asset. The sheet/clip window supports grid slicing (cell, margin,
spacing), manual rectangles, per-frame pivots, display names, clip frame ordering,
variable frame durations and named events. Click an outlined cell to select it.
Preview and scrubbing are local samples and never deliver gameplay events.
Use **Save sheet and clips** explicitly; draft undo/discard and stale-disk checks
protect unsaved edits. Unsaved sheet drafts block leaving, Play and export until saved or explicitly discarded. Read-only mod assets and Play mode cannot edit assets.

In **Sprite → Sprite sheet / animation**, assign the sheet, select a clip or
still frame, choose looping, speed and Play-on-start. The source image stays an
ordinary texture. Frame dimensions / pixels-per-unit determine visual size;
physics geometry is never resized by animation. Style, sorting and flips remain
in the Sprite controls. Texture origin is top-left, with the shared runtime UV
conversion. Nearest sampling has no mipmaps; smooth sampling stays inside frame
texel centres to prevent adjacent-cell bleed. There is no automatic atlas packing.

## Content and stable references

A version-1 JSON `.judassprite` contains `texture` (AssetId), integer `width` and
`height`, `pixelsPerUnit`, `frames` and `clips`. A frame has immutable reference
`id`, editable display `name`, `pixels:[x,y,width,height]` and `pivot:[x,y]`.
A clip has a unique `name`, `frames:[{frame:<key>,seconds:<duration>}]` and
`events:[{id,name,time,data}]`. Sprite event times and frame durations are binary64. Clip references use keys, never vector indices.

Grid keys are `cell-<row>-<column>`. Re-slicing preserves names/pivots for retained
keys. Removing a referenced key rejects the edit instead of selecting another
frame. Changing grid layout is intentional authoring, not automatic reimport.
Manual keys stay stable when their display names change. Sprite resources verify
image dimensions; resized source images require an explicit sheet update.
Saving a valid sheet invalidates the resource generation; existing shared-resource
publication rules discard stale jobs. External edits require a normal resource
reload or a fresh Play session. Malformed content reports an error and cannot
publish a partial sheet.

## JudasJS

`Entity.spriteAnimation` returns a detached `SpriteAnimationInfo` or null:
`sheet`, `clip`, `frame` (still-frame selection), `time`, `speed`, `playing`,
`loop`, `ready`, `sampledFrame`, `events`, `finished`, `overflow`, `error`.
Loading is `ready:false` without a terminal error. A missing/removed frame or clip
is a useful error, not a silent fallback to another frame.

`Entity.setSpriteAnimation(settings)` atomically patches `SpritePlaybackSettings`:
`sheet`, `clip`, `frame`, `time`, `speed`, `playing`, `loop`. Unsupported fields,
nonfinite values, unknown clips and invalid resources throw a catchable TypeError.
Call it after `ready`, or supply a normally loaded sheet. The method does not
advance time. Set `time:0` explicitly to restart a clip; changing `clip` alone
retains phase. Set `playing:false/true` for pause/resume; negative `speed` reverses.
A nonloop clip holds its endpoint and stops; `finished` is one completed-step
observation. Transitions are deliberate frame cuts controlled by project scripts,
not skeletal crossfades or an engine-owned character state machine.

```js
const a = entity.spriteAnimation;
if (a?.ready) {
  for (const event of a.events) {
    if (event.name === 'strike') performGameOwnedStrike();
  }
  if (a.finished) entity.setSpriteAnimation({clip:'idle',time:0,playing:true,loop:true});
}
```

Observe events **once per fixedUpdate**, which reads the previous completed fixed
sample. Repeated reads return the same snapshot; they do not consume it. Update
or presentation callbacks are unsuitable for once-only gameplay dispatch.

Markers share the skeletal animation interval implementation: forward `(from,to]`,
reverse `[to,from)`. Distinct start/end markers may both occur at a loop seam;
identical event IDs cannot repeat within one clip. Reverse hits carry `reverse`.
Large deltas enumerate at most 256 markers, with bounded cycle/time ranges. Overflow
freezes that step's clock and publishes no partial markers, sets `overflow/error`;
fix the timestep/content deliberately. Seeks, editor previews, paused clocks and
resource waiting emit no events. Each event has `id,name,data,time,reverse`.

## Lifetime and support

Scene/prefab/export dependencies include the sheet and underlying texture.
Prefab instances own independent clocks. Runtime definitions carry exact double
phase and policy through region suspension and modern cold saves. Restoring does
not replay the previous sample's markers. Saved sheet content identities reject
changed clip content unless the developer explicitly migrates the save; there is
no claim of arbitrary content-update compatibility. Legacy delta saves do not
provide sprite playback continuation; use modern save slots.

Bounds: 4096 frames, 256 clips, 4096 entries per clip, 1024 authored markers,
4 MiB JSON, frame durations 0.0001..3600 s, clip duration at most one day,
finite speed within ±100 and absolute playback time ≤1e9 s. JSON nesting is bounded.
Sprite sheets in mod packages are explicitly unsupported in this initial stage;
base-project assets, prefabs, scene changes, streaming and ordinary exports work.
Tilemaps/animated tiles belong to the next stage. Windows/Android qualification
is separate from the Stage 3 Linux checks. Main/release versions remain unchanged.
