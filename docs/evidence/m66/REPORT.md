# M66 candidate — professional model import

Starting HEAD: `3e5147a4ed97102200da91b4181c97b2a98942ba`.
Uncommitted candidate. Human visual/import acceptance is **pending**.
Current user workflow: [MODEL_IMPORT.md](../../MODEL_IMPORT.md).
This report is current evidence, not a rewrite of the protected consumer audits.

## Source → runtime

One editor/CLI import service uses ufbx 0.23.1 (MIT, exact pin/hash in
`third_party/ufbx/PROVENANCE.json`), existing cgltf 1.15 and tinyobjloader.
The import worker evaluates/normalizes source units, basis, pivots, hierarchy,
per-mesh skin bindings, clips and supported materials. Named `.judasimport`
recipes own copied sources, explicit settings/remaps and generated asset IDs.
A bounded version-1 CBOR `.judasmodel` publishes one coherent mesh/rig/material/
clip generation to the normal AssetDatabase/ResourceManager. Runtime has no ufbx
parser. nlohmann/json 3.11.3 MIT provides archive encoding/decoding.

Hierarchy (4096 nodes) and per-skin GPU palettes (16384 entries, additionally
hardware checked) are distinct. Eight vertex influences are retained. GL3.3
texture-buffer matrices feed main/shadow/secondary-camera skinning. Fixed affine
bind/default transforms are retained; clip poses, mixer, IK and physical subsets
remain ordinary contributions to the existing resolved skeleton. No FBX player,
second resource system, engine gameplay or runtime source cooker was added.

Supported format/material bounds and honest deformation/retargeting limits are
in the current import contract. Multiple rigs/mesh instances, inverse binds,
static attachments, UV0/UV1, data/colour maps and mirrored tangents have fixtures.

## Original asset and prior workflow

The accepted old Skate rider has one mesh, 24 hierarchy nodes, a 22-joint skin and
11 reduced-rig clips. It remains unchanged. The original character has **65 source
bones, 72 source hierarchy nodes, six parts, 109 mesh-specific palette entries**.
Recipe cooking adds one generic motion-root helper (73 runtime nodes). Dimensions
are approximately **1.781 × 1.801 × 0.325 m**. Four embedded maps retain Body/Hair
material slots. Fingers and nondeforming hierarchy are not amputated.

Original Push/Cruise/Walking motion files map against verified hierarchy/binds;
reordered node indices are not assumed. The lab exposes Push/Cruise/Walking,
WalkingExtract and PushInPlace. No Blender join, bone reduction, unit surgery,
root stripping or NLA/export assembly is required. Manual choices remain take
names/settings, root policy, material remaps and an explicitly authored 13-body
physical subset. See `projects/import_lab/ASSET_PROVENANCE.md` for source hashes.
Original artwork/motions/half-pipe and cooked derivatives stay local/ignored;
rights are not newly asserted. CC0 fixtures are committed candidate content.

The local comparison project is `.cache/m66/three-games-import-review/`.
It copies the accepted Three Games content and adds a full original avatar next
to the reduced Skate rider. Original trick poses are retained, not retargeted by
name. Logical F1/F2/F3/F4 experience switching and public presentation transforms
remain normal project JS. Its sources and packages are local because of rights.

## Root motion, identity and lifetime

Per-clip preserve/inPlace/extract selects model-space translation and optional
axis twist about the initial offset. The complementary pose stays in M47; the
extracted track reconstructs source placement within sampled bake tolerance.
Generic `animation.rootMotion(clip,from,to,loop)` supports interval/reverse/loop
queries; it is not a live blended locomotion controller. Lab B supplies intent to
CharacterMotor; the wall still resolves movement.

Stable asset/part/material/joint identities and explicit aliases survive source
reordering. Renamed/deleted ambiguous joints reject; author remaps remain required.
Separate motion products reuse unaffected geometry/material cache products.
Cancellation, malformed or missing dependencies, source/settings changed while
staged and failed publication preserve accepted content. Cache deletion rebuilds
accepted bytes. Corrupt cache is diagnosed rather than silently trusted. Play
replacement is staged until Edit/Stop; no live rig buffer mutation.

M61 modern saves continue content-based compatibility. Schema 5 is unchanged.
Animated streaming uses the same per-instance serializer and retains exact pose/
mixer/layer/part state, then restores fresh handles. Streaming participant v3 is
used only when animation snapshots exist; v1/v2 remain supported. Active ragdolls
and visual return transitions stay pinned until physical authority ends.

## Collision source and honest artist outcome

Visual import does not require collision topology to pass. Selected source parts
carry original node/face/vertex locations through normalization and derivation.
Explicit degenerate removal, bounded collision-only welding and patch orientation
operate on derived geometry and rerun strict validation; source render seams are
unchanged. No automatic arbitrary topology reconstruction or concavity-filled
hull is represented as faithful geometry.

The artist half-pipe imports visually: 66 parts/84 hierarchy nodes. The full direct
selection pass accepted 64 parts and rejected two real T-junction sections. Final
recipe keys sort pCylinder1/2 into slots **64/65** (direct traversal used 0/1).
`collision-final/64.json` reports pCylinder1 original face49, vertices96/90/43 at
approximately `(0.603387,0.130289,-0.438853)` metres after explicit seven-degenerate
removal/two patch flips. `65.json` reports pCylinder2 face13, vertices43/37/95 at
`(0.603387,0.130263,-0.039851)` after two-degenerate removal. Neither is magically
repaired. Artist source SHA-256 stays unchanged.

The CC0 clean concave ramp is a separate normal source/cooked collider, not a hull.
Its visible analytical surface/raycast at x=-6 yields y≈0.277655; a normal motor is
supported by that collider at y≈0.290459 (skin/tolerance), not an invisible flat
substitute. Both examples are present in the normal import lab.

## Focused observations and preserved failures

- `development/application-proxy-final.log`: 22 checks, zero failures; ordinary
  input/mixer/IK/parts/prefab/root intent/ragdoll/reload/stream exact-state path.
- `development/save-stream-final-{write,read}.log`: fresh processes, 11 and 10
  checks, zero failures, including retained region mixer payload and socket.
- `development/changed-save-final.log`: five checks, zero failures; legitimate
  source recook rejects the incompatible old slot without replacing the world.
- `development/editor-import-final.log`: shared import/GPU six-part preview,
  one-operation placement/undo, duplicate/undo and Play/Stop authored identity.
- `development/cookbook-final.log`: 252 checks, zero failures.
- `api-final.log`: 292 symbols / 27 exports / 198 native operations / 15 callbacks,
  30 examples, live enumeration, TS5.9.3 and three negative drift checks pass.
- `consumer/run.log`: real-loop 1400-frame Skate/Rooftop/Void/menu smoke with
  input and paused screenshots. Concurrent clean-build CPU work means this is
  functional/lifecycle evidence, not an uncontended performance comparison.

Preserved development records include parser/motion/compile/recipe/GL failures
and corrected follow-ups. In particular: FBX transparency was initially inverted;
GPU material copies initially copied encoded photos every draw; a near-axis
fallback used integer `abs`, generating a nonfinite tangent on artist geometry;
all were corrected generically. The independent near-axis tangent fixture guards
the real root cause. Early `halfpipe-parts` selected part0 repeatedly and is
**invalid/superseded** by `halfpipe-selected-parts`. Early application proxy
failures were fixture start-height / first-fixed-step readiness errors, not motor
or fluid changes. A stale-export failure correctly rejected content but omitted
its diagnostic due to C++ argument-evaluation order; its narrow follow-up is
recorded separately, without rerunning the broad gate.

## Final candidate validation (one broad run, affected follow-ups)

`final/production/results.json` is preserved exactly: clean Release **zero warnings**,
**143 discovered production targets**, 141 passing on the first run. The two
nonpassing targets were (1) the save-storage test's mandatory `m61-storage*` output
basename guard, corrected in the M66 adapter and followed by 28 passing assertions;
(2) the newly added near-axis/degenerate-UV tangent assertion, corrected by
projecting Mikk's candidate tangent against the normal and choosing a finite
orthogonal fallback. The raw broad result remains false; it was not rewritten.
Actual async integration: **12 cases / 246 assertions, zero failures**. All broad
sources stayed unchanged during that run, and protected-path checks passed.

Late changes are narrow: tangent correction and derived cook revision, explicit
glTF buffer/image remapping (the field previously worked only in FBX/OBJ), expanded
alias/twist/remap proofs, and an export diagnostic evaluation-order correction.
Cook revision is `M66-ufbx-0.23.1-cgltf-1.15-cook-9-model-1`; archive version remains 1.
The following affected checks use final revision-9 content and current binaries:

| Check | Result / evidence |
|---|---|
| Import numerical/independent source and analytic fixtures | 30/0, `followup/numerical-units.log` (final units proof; earlier revision-9 log retained) |
| Reimport, aliases, remaps, twist/pivot, cancellation/cache | 37/0, `followup/reimport-final9.log` |
| Desktop actual GPU/material/256 joints/full rig/lifetime | 23/0, `followup/runtime-final9.log` |
| Existing M57 material / application paths | 40/0 + 36/0, `followup/materials-desktop.log`, `material-application.log` |
| Normal imported application/motor/proxy/stream/reload | 22/0, `followup/application.log` |
| Final fresh-process saved inactive-region restore | 11/0 write + 10/0 read, `followup/save-stream-handoff-*.log` |
| Changed asset rejects incompatible save | 5/0, `followup/changed-save-handoff.log` |
| Editor shared import/preview/place/one undo/duplicate/Play/Stop | PASS, `followup/editor-final9.log` |
| Cookbook execution | 252/0, `development/cookbook-final.log` |
| Typings / live enumeration / negative API drift checks | PASS, `api-handoff.log` (292 symbols, 27 exports, TS5.9.3) |
| Stale-source export rejects with actionable filename | PASS expected failure, `export/stale-final9.log` |
| Final artist bad collision sections | expected T-junction rejection at exact original locations, `followup/collision-final9/` |
| Moved/read-only/offline lab + Three Games | PASS, `export/lab-offline.log`, `three-games-offline.log` |

A software-renderer invocation of the affected M57 material check failed; it is
retained as `followup/materials.log` and superseded by the GTX970 desktop run.
The first moved lab exposed a **lab-only pause script** error: focus on Resume was
mistaken for a click and the opening pause edge was also treated as Back. Only the
new lab script/generator changed; `export/lab-offline-first.log` is preserved.
Final exported scripts guard click events and the opening edge. The affected
exports/pause checks and fresh-process saves were repeated. No unrelated broad
suite was repeated or assertions weakened. `FINAL_ACCEPTANCE.json` maps coverage
and the narrow post-gate changes; final source/product hashes are separate files.

Numerical tolerances: bind/default matrix max error 2.384e-7, original selected
vertex checks below 2 mm (observed max 4.77e-7 m). Analytic selected twist and
nonzero-pivot reconstruction is below 1e-5 matrix-entry error. Separate FBX motion
off-grid comparison maxima are Push 0.000483662, Cruise 0.000283897, Walking
0.00214394, against the declared 0.005 **mixed matrix-entry** bake tolerance;
those are not asserted as universal metre/radian error bounds. The final test also
checks explicit **5 mm translation / 0.01 radian transformed-axis** bounds. Observed
Push translation/angle maxima: 0.000132105 m / 0.000560827 rad; Cruise 0.0000654175 m /
0.000313418 rad; Walking 0.000583193 m / 0.00237446 rad. These are selected off-grid
source comparisons, not certification at every possible time. At 60 Hz, FBX bake
is approximate between evaluated samples; the glTF analytic fixture independently
checks normalization/pivot behavior rather than trusting the parser twice.

### M56 measurements and costs

Serial Release measurements on NVIDIA GeForce GTX970 (GL4.6 driver, GL3.3 path),
640×480 test framebuffer. OS file cache was **not** flushed. `performance-summary.json`
links raw M56 profiles, wall timing and peak RSS; no old-reduced/new-full speedup claim.

| Work | Measured result |
|---|---|
| Original full recipe cold cook | 13.240 s wall, 1,391,872 KiB peak RSS (~1.33 GiB) |
| Unchanged recipe | 6.642 s, 857,540 KiB (~838 MiB); accepted archive decoding/preview still costs time |
| Small external glTF cold / unchanged / changed texture | 18.07 / 16.50 / 23.86 ms |
| Small compatible-motion baseline / changed clip | 68.80 / 64.40 ms; unrelated base cache products unchanged |
| Cooked original cold load + owner GPU publication | 3509.87 ms |
| Editor first/reload installation / preview scope maxima | 559.39 / 7.23 ms; a real indivisible publication stall remains |
| Editor warm retained frame median / p95 / max | 9.96 / 10.33 / 56.03 ms, includes vsync/capture, not pure render cost |
| One full rig pose median / p95 | 0.0135 / 0.0355 ms |
| Ten full rigs pose median / p95 | 0.1449 / 0.2367 ms |
| One full rig GL finish-inclusive median / p95 / max | 0.3713 / 1.4905 / 1.5100 ms |
| Ten full rigs GL finish-inclusive median / p95 / max | 2.1681 / 3.3151 / 4.0445 ms |
| One full rig actual delayed GPU median / p95 / max | 0.1994 / 0.2152 / 0.2289 ms |
| Ten full rigs actual delayed GPU median / p95 / max | 1.5980 / 2.8479 / 2.9546 ms |

Six part submissions and 6976 palette bytes per instance: 6/60 model draws for 1/10
instances. Original geometry has 150,561 tangent-expanded vertices; direct original
archive is 142,027,817 bytes. Full-rig material data no longer copies photos every
draw, but decoding/publication memory and large-asset cold stalls remain limitations.
The final runtime executable is 49,389,600 bytes; editor 52,349,496; CLI 50,343,120.
`export/runtime-ufbx-symbols.txt` has no ufbx load/evaluate/bake/free symbols. Runtime
cooked archive/JSON decoding is included; no fabricated pre-M66 binary-size delta.
System runtime requirements are in the package and `export/runtime-libraries.txt`.

### Final ordinary packages

- Lab project: `projects/import_lab/import_lab.judasproj`; local export
  `.cache/m66/packages/lab/`; moved launch `/tmp/judas-m66-offline/lab/judas`.
- Three Games comparison: `.cache/m66/three-games-import-review/post_m65_consumers.judasproj`;
  local export `.cache/m66/packages/three-games/`; moved launch
  `/tmp/judas-m66-offline/three-games/judas`. F1 Skate / F2 Rooftop / F3 Void / F4 menu.
- Lab **13 assets / 2 scenes**, 209,773,313 bytes (~200.06 MiB), 7.38 s wall export.
  Three Games **143 assets / 9 scenes**, 229,805,879 bytes (~219.16 MiB), 8.18 s wall.
- Both final runs launched the absolute executable with **no arguments** from `/tmp`.
  Packages have files 0444/executable 0555/directories 0555 and no authoring sources,
  import recipes, warm cache or import tool. Writable data uses a separate XDG root.
- `unshare` private network + mount namespaces masked the entire GitHub source
  directory with an empty directory **only inside each validation process**. No
  external workspaces/processes/files were changed. No editor/FBX cook is available.
  Lab 1050 real frames: controls, stream retire/revisit, ragdoll/return, pause/resume,
  reload. Three Games 1400 frames: each game, input, paused screenshots and return
  to menu. Raw commands, captures and M56 profiles are under `export/`.

These original-asset packages stay local under the existing rights policy. A fresh
checkout has CC0 fixtures but needs lawful original content to regenerate the full
lab. Visual acceptance is still the operator's decision.

## Human import checklist

1. Open `projects/import_lab/import_lab.judasproj` in the editor; use Assets → Model
   import/reimport. Inspect all six original parts, dimensions, bones/fingers/maps.
2. Add the copied separate motions; select/scrub clips and root policies. G crossfades.
3. Inspect independent/oblique instances; J mask, K additive, I IK, F ragdoll/return,
   P prefab, T stable part toggle; observe the secondary-camera screen/shadows.
4. Change only copied texture/motion content, reimport and check references/overrides.
5. Try a missing dependency or artist pCylinder1/2 collider; select its diagnostic
   location and confirm last-good visual content survives.
6. Inspect the clean concave collider/proxy fit and compare its query/motor support.
7. F6 save/F7 load; U request/retire/revisit; F5 reload. Launch the moved read-only
   package without Sources/Imports/cache or an editor.
8. Judge fidelity/usability/responsiveness. Human visual/interactive review is the
   authority; screenshot/numerical success is not claimed as human acceptance.

Mouse/WASD drive the independent inspection camera; Esc opens authored pause UI.
No commit/push/tag, no M67, no changes to protected historical/research, accepted
fluid implementation or accepted consumer baseline.
