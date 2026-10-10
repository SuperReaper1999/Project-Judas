# M75 human-accepted results — 2026-10-10

Starting checkpoint:
`fccbf84164cb430f78651af64cddb68afeac442a`.
The operator **accepted M75 and authorized commit/push on 2026-10-10**, including
the terrain UI activation follow-up. All 81 reviewed candidate fingerprints matched
before changing only these acceptance status notes. The original reviewed manifest
and report are preserved as `human-reviewed-candidate-fingerprints.json` and
`human-reviewed-candidate-results.md`. M74 is checkpointed
provisionally, with human review pending; M67 deferred review and physical Windows
acceptance remain outstanding. No full production marathon or fluid research ran.

## Delivered mechanism

- Version-1 local height/normalized-paint source, stable source/product IDs, explicit
  ownership/fork, bounded CPU brush scheduling and undo/redo.
- Editor current-triangle sculpt/picking/footprint, registered texture palette,
  draft/save/cook separation, guarded jobs, normal GPU ownership and Play/Stop.
- Shared CLI create/validate/brush/fork/cook; configurable project asset roots.
- Normal model/TGA/static collision products, freshness/dependency closure,
  offline navigation, scenes/prefabs/regions/saves and moved exports.
- Ordinary 64×64 m Terrain Lab, independent neutral patch, oblique/fixed-origin
  scene, dynamic props and public-JS motor/UI. No supporting hidden floor.

See `docs/M75.md` for source/product schema, units, bounds, commands and limitations.
The standalone does not link terrain authoring/brush services. No JudasJS binding
was added. The only necessary generic runtime changes enforce documented unit-scale
cooked collision placement and bound the inherited motor's cooked capsule overlap
search through its existing BVH. No scene-name branches, alternate solver, fixed-step
cadence change or terrain-quality reduction was introduced.

## Executed focused validation

| Area | Result / receipt |
|---|---|
| Source, strokes, bounds, ownership, painting, transactions | **89 checks, 0 failures**, Linux and native MSVC; `authoring-final.log`, `windows/m75-windows-final-authoring.log` |
| Navigation, paint/geometry invalidation, path, scene references, scale | **15 checks**, Linux and MSVC; `pipeline-final.log`, Windows final pipeline receipt |
| Existing cooked collision geometry | **137 checks**, 0 failures; `collision-regression.log` |
| Existing CharacterMotor | **32 checks**, 0 failures; `motor-regression.log` |
| Scene serialization | Passed; `scene-regression.log` |
| Historical radial terrain/player/rigid equivalence | Passed; `radial-regression.log`; original fixtures unchanged |
| Current API live enumeration | 9 checks, no failures; 29 exports / 336 public symbols / 233 native operations / 16 callbacks / 38 examples; TypeScript 5.9.3 and three negative drift controls passed |
| Actual application motor/drop/prefab/region/unload/revisit/reload | **15 checks**, Linux and non-elevated native Windows |
| Modern save / fresh-process load | 6 save + 8 load checks; exact motor position restored, required terrain resolved |
| Changed/missing required terrain | 6 checks each; load rejected before replacing live world, old collider retained |
| Actual editor brush/Undo/Redo/Escape/focus/Save/Cook/GPU/Play/Stop | 13 final terrain assertions, zero failures, Linux and native Windows; authored scene remained IDENTICAL |
| Editable second-project reuse | Shared CLI fork/brush/cook, actual editor stroke/Save/Cook/Play/Stop, ordinary nav rebake and moved 15-check runtime passed |
| Closure and all-assets policies | Final packages: 18 assets, three scenes; moved probes and actual standalone executables passed |
| Stale export | Required uncooked source rejected; previous valid package byte hashes unchanged (`stale-export.json`) |
| Native Windows package | Stock SDK/export audited without VM Mesa; moved path with spaces/Unicode, unrelated cwd, all authoring project copies hidden, actual collision and executable launch passed |

The final parser/conflict-baseline/custom-root safeguards received narrow Linux and
MSVC CPU/tool reruns after the executed GUI pass. They do not alter valid geometry,
brush mathematics, GPU publication or runtime playback. GUI/render/export checks
were not ceremonially repeated for these parser-only safeguards.

### Independent agreement / negative controls

Analytic ramps use independently calculated point, normal, cast extent and gravity
under identity and oblique rigid transforms. A 3×3 peak fixture exercises both
triangle halves, a crease and a finite edge, comparing emitted render triangles and
physical queries to manually specified answers. Tolerance set before checking:
**0.0002 m / 0.0002 normal-vector error**. Worst peak point error:
**1.86265e-08 m**, worst normal-vector error **0**. Sphere-cast tolerance 0.003 m;
capsule/box/closest-point tolerance 0.005 m. Render shading normals are smooth vertex
normals; physical query/contact normals are geometric face normals.

Real sphere and box bodies settle on the edited ramp, and the motor supports under
independently rotated gravity. All normal public ray/sphere/capsule/box casts and
closest point use existing cooked geometry. Off-patch casts miss; an intentionally
stale flat collider fails the independent comparison. An omitted paint publication
is rejected by source/receipt freshness; spatial paint weights and texture output
are exercised. There is no invisible floor or scripted height snapping.

## Performance and practical bounds

Linux: GTX 970, NVIDIA 580.178.04, actual desktop OpenGL (see `linux-gl.txt`),
Release, hidden uncapped **1024×768** application. Two-second warm-up plus three
seconds measured; **180 fixed-step samples** per run, 60 Hz simulation. Means/p95
are actual wall-clock measurements, not synthetic harness speed or a universal FPS
promise. `performance.json` retains maxima, M56 inclusive scopes, GPU timestamps,
startup resource scopes and diagnostic counters.

| Workload | Bodies | Geometry | Frame mean / p95 / max ms | Fixed step mean / p95 / max ms | Resident resource bytes |
|---|---:|---|---|---|---:|
| Final 64 m lab | 7 | Main 65² / 8192 triangles + independent 17² control | 0.486 / 0.717 / 6.074 | 0.326 / 0.475 / 0.600 | 5,156,578 |
| Oblique fixed-origin lab | 7 | Same geometry, separately rotated gravity | 0.477 / 0.677 / 1.705 | 0.649 / 0.840 / 1.022 | 5,156,578 |
| Bounded 129² stress | 7 | Main 32768 triangles + same control; 64 m, 512² paint | 0.502 / 0.720 / 10.660 | 1.259 / 6.445 / 9.785 | 11,746,018 |
| Zero terrain-use control | 6 | Ordinary box support, same camera and four props | 0.450 / 0.615 / 4.675 | 0.161 / 0.255 / 0.302 | 759,720 |

The stress contour is independently authored through the shared CLI; it is a
practical stress scenario, not a pure resolution-only speed comparison. Its camera,
dimensions, paint resolution and seven-body population match the final lab. The
zero-use control intentionally replaces the two terrain bodies with one box and
has no terrain authoring/runtime subsystem; it is an overhead baseline, not a
same-geometry rendering comparison. All cases had **zero steady resource uploads**.

Shared CPU stroke plus full normal generation: 65² **~1.7 ms**, 129² **~12.5 ms**
in the final authoring pass (other isolated receipts range ~1.6–2.0 / ~7.4–12.5 ms).
129² initial full cook **383.724 ms / 4,729,467 published bytes**; neutral patch
49.017 ms / 312,244 bytes. Ordinary final navigation bakes **92.34 / 96.50 ms**,
100 tiled layers / 101 polygons. Source/model/collision/texture sizes are listed in
`content-sizes.json`. Static GPU main pass averaged **~0.115 ms** in the final lab.
Retained startup M56 spikes show ordinary resource decoding up to ~18.3 ms,
mesh geometry upload ~0.584 ms and texture driver uploads ~2.87/5.12 ms. These
inclusive startup scopes cover the project resources/font, not isolated terrain-only
loading; nested/worker intervals must not be summed as exclusive frame cost.

### Real performance defect and correction

Initial useful 65² terrain timed out in editor Play. M56 attributed **~727 ms/frame**
to eight CharacterMotor evaluations, **~80–110 ms/fixed step**, catch-up outer frames
~735 ms; rigid physics ~0.37 ms/frame and GPU main ~0.14 ms. The inherited sampled
capsule sweep supplied infinite range to cooked geometry at every overlap/refinement
sample, bypassing triangle BVH culling. It only uses `distance <= 0`; passing exact
range zero preserves the overlap answer and activates the existing BVH. Independent
boundary proof now visits **143 nodes / 48 triangles** rather than scanning all
8192 faces repeatedly. Before attribution/logs and failures are preserved.

An intermediate matched six-body scene after this fix measured 0.488 ms frame /
0.322 ms fixed-step mean; the final lab adds a neutral terrain and ordinary nav.
Do not interpret those additions as an identical before/after total workload.
The causal overlap/BVH assertion and affected regressions establish the generic fix.
No step reduction, quality reduction or unrelated physics optimization was made.

Undo/redo snapshots, preview revisions, repeated exact-byte cooks, editor resource
retirement, reload, prefab destroy and region revisit were exercised. Reported memory
counters/bounds are not a complete allocator census or maximum-size certification.

## Native Windows / warnings / limitations

Existing Windows 10 VM: four vCPUs, 6 GB RAM, fixed disk; native MSVC 14.44 / VS2022.
No VM/network/security/storage/toolchain changes. Only known disposable old
SDK/package copies were cleaned; receipts/source/toolchains were retained. Native
source/asset transfers and binary timestamps/hashes are recorded. Final input proof
merges the initial transfer with final authoring, SDK and content updates.

GUI and application ran as **Conner, session 1, admin=false**. VM-only Mesa llvmpipe
was copied only into disposable GUI/moved-package copies after auditing stock SDK
and export. This proves native execution, not physical Windows GPU/input performance
or operator acceptance. Existing long Windows path limits remain; short spaces/Unicode
paths were exercised, without changing policy.

Remaining compiler notices are pre-existing HarfBuzz/FreeType CMake deprecations and
MSVC shadow/class-struct warnings outside the new authoring code; inherited Linux
RuntimeWorld indentation warnings were not cosmetically edited. New test-only MSVC
warnings were corrected, and final authoring source builds cleanly.

Bounded open static height patches only; unit scale; no auto seam stitching/LOD,
caves/erosion/spherical editing, multilayer PBR, dynamic terrain physics or fluid
bake compatibility promise. Geometry preview/cook are bounded whole-product work;
palette-identity changes currently alter model dependency metadata and require
collision/nav freshness refresh. Painting weights/tiling alone preserves exact
geometry/collider bytes. The operator accepted Linux brush feel, editor usability and visible quality on
2026-10-10; physical Windows acceptance remains outstanding.

## Human UI follow-up

Opening the original panel did not activate editing. **Edit selected terrain** now
opens and activates the source in one click; **Open source and start editing**
selects a matching instance if available. Brush controls, paint-layer choice,
readiness and errors are above the source-management controls.

The corrected smoke clicks the actual ImGui button through native SDL events before
sculpting. Linux and non-elevated Windows both passed **13 terrain assertions**,
including real activation, sculpt/paint, Undo/Redo/Cancel, Save/Cook/GPU and Play/Stop.
Native final editor source hashes extend the 819-input base audit via
`windows/m75-ui-final-source-proof.json`. A new local-name shadow warning was fixed;
final narrow editor rebuilds passed. No brush mathematics, cooked data, simulation
or standalone changes; prior runtime/export results remain applicable. See
`editor-ui-activation.log`, PNG and `windows/m75-windows-ui-summary.json`.

## Paths / preservation / proposed checkpoint

- Project: `projects/terrain_lab/terrain_lab.judasproj`
- Editable: `Imports/landscape.judasterrain`, `Imports/neutral.judasterrain`
- Linux moved game: `/tmp/M75 moved pristine é α`
- Second editable project: `.cache/m75/second-project/terrain_lab.judasproj`
- Second moved game: `/tmp/M75 moved second é α`
- Native moved game: `C:\JudasValidation\Packages\Moved M75 é α`
- Current guide: `docs/M75.md`; exact changes and fingerprint scope:
  `changed-files.txt`, `final-fingerprints.json`.

Protected historical FTFT/P1/earlier evidence, radial fixtures, accepted fluid,
M74, Lastlight/Claude projects, Kate tooling and ignored LowPolyAssets/asset_packs/
video_work are unchanged. POST_M73_REPORT.txt remains pre-existing untracked work.
Implementation was delivered unstaged/uncommitted; the operator subsequently
authorized the M75 checkpoint. No milestone tag, M76 modding or independent-project
work was started.

Authorized checkpoint commit:
`Add M75 reusable terrain authoring, sculpting, painting and cooking`
