# M85 — precision-safe planetary terrain

Linux operator acceptance is [recorded](evidence/m85/HUMAN_VALIDATION.md). Windows remains untested. Linux measurements and exact
candidate hashes are in `docs/evidence/m85/`. Existing `RadialTerrain` and small
flat scenes retain their old paths. This adds an optional **Planet terrain**
component, not an Earth-specific branch or a replacement global physics solver.

## Representation and precision

For each cube face, with orthonormal axes N, U, V:

```
d(u,v) = normalize(N + u U + v V),   -1 <= u,v <= 1
P(d) = C + Q [ (R + h(d)) d ]
vertex = float(P(d) - tileAnchor)        // subtract in binary64
body/draw position = float(tileAnchor - worldOrigin)
absolute actor position = worldOrigin + double(localActorPosition)
```

C, R, Q, height samples, tile anchors and world origin are binary64. The old
`float(R + centimetres)` operation is absent. Cube-grid coordinates are dyadic
integers, not accumulated float offsets. Planet owner transforms must be root,
identity, unit scale: **the descriptor owns its absolute static frame**. Normal
entity transforms never secretly become double-precision global transforms.

Validated authoring range: radius 1..10^10 m; each absolute centre/origin component
within +/-10^12 m. Absolute double spacing near 10^12 m is approximately 0.122 mm;
micrometre reconstruction measurements at Earth radius/centre zero are not a
promise of micrometre precision at arbitrary astronomical coordinates. Active
physics stays within an 8 km local chart, with explicit checked origin shifts.
No global Earth-sized float collision object is created.

## Data sources

- Analytic reference sphere plus up to 16 authored spectral elevation bands:
  `h(d) += amplitude * sin(dot(R*d, waveRadiansPerMetre))`.
- Up to 64 embedded authored/external raster patches, total <=262144 samples.
  A patch uses a local gnomonic tangent chart, bilinear samples and the compact
  C1 envelope `(1-u*u)^2 * (1-v*v)^2`. Values are **additive elevations**. The
  envelope intentionally fades patch boundaries to zero; this is not a global
  geospatial DEM importer. Patch half-width is measured in that tangent chart.
- Immutable C++ `PlanetSurface::Height` provider seam with explicit conservative
  height and curvature certificates. Providers must be thread-safe, finite and
  prepared; arbitrary disk/network streaming is not performed inside sampling.
  A violated height certificate fails explicitly.

Planet descriptors belong to the root world; local M59 regions hold nearby content.
Translated region owners cannot relocate an absolute planet component.

The canonical versioned descriptor is `planet.surface` in ordinary scene/prefab
serialization. No parallel terrain asset registry or external service is needed.
Externally supplied data may populate raster arrays or implement the provider.
A planet-wide streamed DEM/image database and cartographic Earth content are
outside this delivered scope.

## Runtime LOD and seams

Six independently addressed cube faces have quadtrees to level 26 (the lab uses
24). Selection uses conservative geometric/source interpolation error projected
into pixels using view altitude, vertical FOV and viewport height, with conservative
horizon culling. Orientation-independent coverage surrounds the camera; a camera
turn does not expose an unloaded hole. Leaves cover each whole face exactly.

A hard leaf/resident budget bounds geometry. Unsatisfied target error is exposed
as `budgetLimited` and `maximumPixelError`; this is **not** a claim that every
possible input can meet 2 pixels under any budget. Band error is bounded by
amplitude/curvature; raster error uses local patch support and a first-order bound
that remains valid at sample derivative discontinuities. Tile spatial bounds use
height variation, not the entire planet's remote maximum height.

All startup leaves are prepared before the first presented terrain frame. Later
refinement installs complete sibling groups; parents remain until replacement.
Children geomorph from the parent's actual piecewise triangle surface, not from
interpolated radii. Coarsening reverses that morph. Frame seconds drive fades.

Fine boundary vertices weld to the **coarsest incident leaf's actual triangle**,
including its current morph. The same rule applies across face boundaries and
corners. No runtime skirt hides a collision gap. Render normals use a footprint
appropriate to tile spacing so unresolved centimetre ripples do not alias into
planet-sized lighting noise. Visual tessellation and physical contact resolution
remain separate. A coarse/fine boundary straightens the fine edge; severe budget
exhaustion can still lose detail, and lighting is not an industrial smooth-surface
continuity guarantee.

Steady meshes are retained. A small Morton interval index resolves neighbours;
only tiles whose own/seam-dependency morph changed are uploaded. Counts and a
256 MiB conservative CPU/GPU geometry estimate limit authored configurations.
No per-planet worker threads or unbounded generated-asset cache are introduced.

## Collision, motion and arbitrary gravity

Nearby contact tiles are fixed-resolution, ordinary cooked one-sided triangle
meshes in PhysicsWorld. Their resolution does not change when the camera changes
LOD. Actual mesh contacts, M44 casts and CharacterMotor sweeps share the existing
solver/query path. No analytic helper floor or terrain-specific motor exists.
Collision normals are geometric; gravity remains an independent field selection.

`preparePlanetCollision` has one explicit spherical physical interest per planet.
The radius must cover its interacting actors. It prepares at most two missing
cooks per call and returns false while preparing. Existing colliders remain until
the complete replacement is ready. Scripts should pause travel/spawn until true.
A budget/cook failure reports an error and retains old support. Requests beyond
the local precision chart reject. The lab uses 0.5 m maximum face-centre spacing,
18 m interest radius and at most 96 collision tiles. Triangle positions still
sample the centimetre height field; this does not promise centimetre horizontal
collision detail everywhere. Features below the authored spacing may alias.

Cold-cook geometry omits unused visual normals/parent-morph calculations, while
retaining identical vertex positions and collision fidelity. Residency is not
planet-wide: callers are responsible for covering all active local bodies.
Long-range/high-speed vehicles require adequate prefetch interest; global CCD
against unresident terrain is not claimed.

## Origins, saves and streaming

The [JudasJS coordinate reference](judasjs/planetary-terrain.md) specifies supported
live-origin components and rejection paths. Shifts commit before fixed scripts,
translate native motion/contact/broadphase history and world anchors, and retain
velocities, sleep and generation-safe handles. Gravity regions are translated in
existing precedence order; RadicalGravity is still ordinary Judas gravity.

Origin-aware M59 retained records store their capture origin (archive version 7
only when needed). Old versions 1..6 retain their original byte layout. Pending
region transactions and unsupported opaque continuations explicitly block shifts.
Absolute source placement and immutable preparation products are not mutated.

Save slots store the current chart, ordinary body/motor state, full authored
planet descriptor and physical interest. Collision cooks are derived/rebuilt
before physical resumption; visual LOD/GPU buffers are not save data. Derived
tile contacts may generate fresh notifications after cold load: exact callback
or warm-contact continuity for these anonymous generated tiles is not claimed.
An exported package needs only its normal scenes, scripts and assets; it rebuilds
the same tiles offline without accessing the source checkout.

## Editor and lab

Add **Planet terrain** through the ordinary component menu. Inspector controls
radius, double centre, pixel error, budgets, collision spacing/radius, band
amplitudes/wave vectors, and authored raster direction/extent/sample values.
Undo/redo and scene/prefab named properties use the existing editor machinery.
Changing the descriptor replaces the edit preview; Play creates independent state.
The owner must remain identity; use the descriptor centre/rotation for placement.
Editor camera coordinates remain local to the authored chart.

Open `projects/planetary_terrain_lab/planetary_terrain_lab.judasproj`.
The lab uses a 6,371,000 m radius, kilometre relief, 2.5 cm analytic elevation,
a 12 cm authored local patch, ordinary RadicalGravity, a JS-driven motor, thrown
rigid probes and an ordinary streamed local station. F1/F2/F3/F4 select flight,
landing, face-edge visit and tour. The orbital view is a project-script free
camera, **not** a simulated spacecraft or global physical actor. Physical actors
are retired before leaving a chart and recreated only after collision is ready;
visual traversal does not reload a scene. Ground movement itself remains real
CharacterMotor motion, including crossing tile/face boundaries.

M84 bounded volumes and M75 terrain can still be ordinary nearby streamed assets.
This radial height representation cannot itself make caves/overhangs or edit
volumetric holes through the sphere; no such integration is claimed.

## Measured Linux candidate

Release on Intel i7-4790 / NVIDIA GTX 970. The final 860-frame application
sequence includes real initial landing, probe contact, two cold saves, orbit,
descent, face-edge movement and reload; its clock is fixed at 1/60 s. Timings are
CPU wall time, not GPU timer certification or an achieved display refresh promise.

| Measurement | Result |
| --- | ---: |
| Earth tile-local float reconstruction error | 6.451e-8 m |
| Maximum measured mixed-LOD/cube-face render seam | 1.318e-9 m |
| Character support across 180 native steps | 180/180 |
| Native moving collision-interest + motor/physics mean | 0.379 ms/step |
| Four-tile cold collision cook/publication | 13.022 ms |
| Application fixed-step p95 / max | 1.664 / 9.829 ms |
| Whole application CPU frame p50 / p95 | 10.469 / 24.679 ms |
| Render CPU p95 / settled-ground subset p95 | 20.662 / 16.901 ms |
| Terrain preparation p95 | 19.233 ms |
| Cold initial terrain preparation / whole-frame max | 253.159 / 281.527 ms |
| Peak leaves / resident meshes / contact tiles | 384 / 500 / 34 |

The inverse CPU frame costs are roughly 96 FPS at the median and 41 FPS at p95,
excluding present wait. Rapid LOD transitions still cost more than a stationary
view on this older host. The synchronous cold-cover preparation is a visible
startup/loading hitch, not hidden inside an average. Ground selection reports a
conservative 5.23 px error against the requested 2 px when the 384-leaf budget is
exhausted. Increase a measured budget or relax the target explicitly; the engine
does not silently certify the smaller error. No 60 Hz worst-frame guarantee is
claimed.

Focused validation: numerical geometry/source/LOD; native world collision, tags,
authored reset, motor, cold saves; origin physics and streaming; 37 real graphical
application assertions; 12 relevant existing regression executables; TypeScript
5.9.3 and API drift. Per-vertex assertions are not separate engine capabilities.
Editor, export and moved-package receipts are recorded separately in the evidence
report. Windows M85 validation remains outstanding; Linux operator acceptance is recorded above.

Generated colliders currently use default collision layer 0/all-layers and the
planet owner's runtime tags. They are anonymous derived bodies, not individually
authored terrain entities. A separate per-planet physical-material/filter editor
is not part of this version.

## References

- [Thatcher Ulrich: chunked LOD and geomorphing](https://tulrich.com/geekstuff/chunklod.html).
- [Cesium: conservative horizon culling](https://cesium.com/blog/2013/04/25/horizon-culling/).
- [Cesium: relative-to-eye positioning](https://cesium.com/downloads/cesiumjs/releases/b17/Documentation/czm_modelViewProjectionRelativeToEye.html).

The implementation is original Judas code; these references inform techniques,
not a copied third-party terrain dependency.
