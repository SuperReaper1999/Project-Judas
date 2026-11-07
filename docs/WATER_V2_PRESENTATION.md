# Grid Water V2 presentation

Experimental source preview. The Alpha Advanced Fluid Dynamics lock and published
releases remain unchanged. The [physical support boundaries](GAME_WATER_PREVIEW.md)
still apply. Visual quality is a human acceptance requirement; compiling shaders
or rendering a blue surface does not establish that acceptance.

## Physics and presentation

The coarse native grid owns water quantity, pressure, transport, air and body
loads. Its binary64 ledgers and existing physical tests are unchanged. Rendering
uses float32 heights, normals and optical intervals in the grid's local chart;
origin shifts are handled by the existing world origin and field revision.

`fluid/GameWaterSurface` reconstructs connected visible water layers. Consecutive
occupied coarse cells form a visible column rather than separate glass sheets.
A genuinely dry cell preserves the gap between different pools or cave air.
Sub-2 mm films stay physical but do not generate shimmering render sheets.

Moving-body exclusions are continued from wet witnesses through the whole
connected exclusion, across open **static** connections. Equal-distance fronts
blend their witnesses deterministically; a large/held body cannot leave an
unfilled interior simply because it spans more than one cell. Ordinary dry cells
and separate dry layers are not filled. The opaque body normally hides this visual continuation;
a CharacterMotor capsule must not leave an apparent air cavity around its camera.
A closed wall does not give a dry swimmer an optical water volume. This field is
never passed back to physical immersion, forces, gas pressure or inventory.

Fine surface topology uses one patch per column water run, cubic interpolation
and clipped shorelines. Interpolation follows overlapping run intervals, rather
than assuming adjacent water tops occupy the same vertical cell. Rising water
around a moving body therefore does not create duplicate patches or internal
curtains down to the floor. Dry atlas entries link to the next wet run, so CPU/GPU
lookups skip empty vertical spans instead of scanning every cell. Close,
medium and distant views use four, two and one subdivisions per physical cell.
Changing heights/normals are reconstructed in the vertex shader; the fine mesh
is retained until its footprint/layers change or its reference needs reanchoring.
Each region retains a bounded set of three LOD meshes. Views share the same
field revision. Nothing is reconstructed or uploaded for a region rejected by
the camera's layer mask or frustum. Hidden **physical** water still advances when
needed for flow, buoyancy and gameplay; visibility does not change its forces.

## Renderer ownership

`render/WaterPass` owns lazy shader programs, a reusable colour/depth snapshot
and per-surface field textures. Atlas packing retains every physical cell's 3-D
identity without imposing a 3-D texture-axis limit. Only changed field revisions
upload. Refraction scratch allocations grow in small buckets and are reused by
smaller cameras rather than reallocating for every split-screen/secondary view.
Mesh destruction retires its field texture; renderer shutdown clears all owners.

Finite water closing faces defer to existing opaque scene depth when a pool wall
occupies the same interface. A small depth-precision tolerance is visual only;
neither collider nor surface is moved. Exposed water boundaries remain visible.

`render/WaterShaders` owns the portable shader sources. Above water these provide
continuous animated normal detail, small depth-tapered waves, depth colour and
refraction with a foreground rejection test. Fresnel reflections use bounded
screen-space ray queries, the existing environment radiance when available, or
the authored sky colour. Lighting follows scene sun/ambient values. Thin shoreline
foam is an inexpensive depth cue, not additional physical material.

The underwater pass composites at the end of each camera's world rendering,
before colour output and HUD/UI. It uses the near-plane position per pixel, resolved water
intervals, coloured exponential attenuation and a subtle animated caustic cue.
It also covers sky and the underside of the surface. It shares the surface's wave
phase and blends through a 24 mm near-plane transition band. Opaque scene depth
limits travel; bounded stepping/refinement finds the first visible medium exit.
The scene colour is sampled from a separate copy, never the active attachment.

The existing Basic Water and M54/M55 optical paths remain independent. Runtime
world rendering excludes Grid Water from their coarse optical map so the new
water is not fogged twice. HDR views keep linear colour until the existing output
transform; display-encoded views decode and encode at the pass boundary. The pass
restores framebuffer, viewport, pixel-unpack, program, vertex-array, texture,
scissor, depth, winding and blend state.

## Current limits

- This is bounded game-resolution water, not a global ocean or microscopic
  geometry renderer. Shorelines and small voids follow the declared coarse field.
- Screen-space reflections cannot see offscreen objects. The environment/sky
  fallback avoids empty reflections; it is not a planar reflection render or
  ray-traced scene. Refraction is a depth-aware screen-space approximation.
- Interface highlights currently use the scene sun, environment and visible
  reflections. This pass does not add a second clustered-light/shadow system.
- Underwater interval stepping can approximate thin gaps along a ray; the actual
  physics and air accounting retain their own, stricter geometry contracts.
- Refraction uses an opaque-depth snapshot. Transparent objects/particles do not
  get exact layered refraction; underwater attenuation of later transparent
  effects uses the available background depth. A separate transparent-water
  compositing solution is not claimed.
- The small wave/ripple/caustic style is currently built in. Fluid Volume colour
  and opacity are authorable; opacity changes absorption/tint, not water mass.
  A clear interface still has reflections. Detailed spray, thick foam, wet lenses
  and volumetric light shafts are not claimed.
- Geometry changes can require a new surface mesh. Larger regions or many
  independent water views still require representative profiling; the maximum
  physical cell budget is not a rendering performance guarantee.
- Linux native and forced portable checks are separate from Windows, Android and
  browser hardware qualification. No new device coverage is implied.

## Inspect and measure

Open `projects/water_v2_workshop/water_v2_workshop.judasproj`. Test calm water,
grazing views, the steps, floating/sinking props, B splashes, F circulation and
F6/F7 saves. Swim and look both along the pool and upward through the surface.
Move repeatedly across the waterline; inspect tilted, radial and original basin
scenes with 1/2/3/4. Check that the physical level changes when adding/draining,
while decorative waves never change the quantity ledger.

```sh
cmake --build build --target judas_water_visual_tests -j2
build/judas_water_visual_tests /tmp/water-visual-quick
build/judas_water_visual_tests /tmp/water-visual-measured --measure
JUDAS_GL_PROFILE=portable build/judas_water_visual_tests /tmp/water-visual-portable
```

The ordinary gate advances the same 180 physical steps per fixture, captures
all declared views, and samples rendering during the disturbance interval.
`--measure` renders all 180 frames and retains CPU timings and asynchronous GPU
timestamps. `--gpu-only` runs the numerical/GPU/pixel controls for shader iteration.
Measured CPU render submission/presentation includes display waits; use the
`Grid water presentation` and `Grid water underwater` GPU intervals together for
actual GPU water cost (the latter exists only for a camera touching the medium). The
`Grid water visual field` and `Grid water fine surface` CPU scopes separate
snapshot preparation from occasional topology construction. Requested water GPU
texture bytes are reported independently of physical grid memory.

Human acceptance must judge the actual moving game from all these views, rather
than treating any screenshot or numerical gate as automatic visual approval.

The [7 November qualification report](evidence/game-water-visuals-20261107/REPORT.md)
contains full-resolution captures, measured CPU/GPU costs, preserved failed
controls, cold-export results and a practical human checklist.

The [wall and transient-pocket follow-up](evidence/game-water-visual-followups-20261107/REPORT.md)
records reproduced failures, independent negative controls and the subsequent
connected-column/opaque-wall corrections.
