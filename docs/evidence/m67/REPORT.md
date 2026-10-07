# M67 — integrated world building and supported content authoring

Starting HEAD: `b9a8cf3b8d8cd3786272480a8abe6cbe9fffe859`.
**PROVISIONAL checkpoint authorized by the operator on 7 October 2026.**
Human authoring, visual and interactive validation is deferred to a later date;
this checkpoint does not claim human acceptance. The implementation remains the
recorded candidate; only documentation status was changed for checkpointing.
Current guide: [M67_WORLD_BUILDING.md](../../M67_WORLD_BUILDING.md).
Format/CLI contract: [NAMED_AUTHORING.md](../../NAMED_AUTHORING.md).

## Implementation and source of truth

M65 document commands, mixed properties, typed references and history are extended;
M66 multipart import/material remaps/reimport/subasset stability/cooker diagnostics
are reused. There is no second scene database, resource scheduler, UI layout engine,
physics engine or gameplay mode. `judas_world_authoring` supplies shared editor/CLI
recipe evaluation, publication, source history, dependency discovery and selected-
chain skeleton fitting. Stable existing entity/asset/subasset IDs remain authority.

Schema-1 `{kind,schema,data}` source documents load/save directly through the normal
project, scene/prefab, UI, input and world-manifest paths. Canonical fingerprint
schema remains 5; save/cooked schemas are distinct. Current writer-field coverage,
Unicode/locale/ordering/negative-zero compatibility and explicit legacy conversion
are tested. Unknown fields/types/versions and invalid references fail rather than
being silently omitted. Complex old JSON strings preserve exact legacy semantics;
authors can explicitly use structured `json` payloads. Named compound physics
records are documented. Recipes retain stable elements/overrides; geometric
revisions use immutable product IDs so undo cannot refer to overwritten meshes.

Publication stages and validates all outputs, records recovery information,
rechecks external bytes and publishes via per-file renames. Rollback never overwrites
an external edit. This is **not** filesystem-wide or power-failure atomicity.
Dirty drafts and validated documents remain separate. Scene/project switching keeps
source drafts in the editor process, with no autosave/hot-reload claim.

The main view now authors/controls near/far and projects world points using the
actual viewport/projection; secondary cameras use their normal range. UI layout
patches are atomic, detached snapshots with common render/input/focus cache
invalidation. Prefab construction options preflight existing schema slots,
properties/references/plain-JSON state and root motion before publication. Ordinary
script resource synchronization still determines construction/start timing. M61
restores saved state, not fresh initialization; its optional view-range chunk is
explicitly bounded. These additions are generic public APIs, synchronized with
types/reference/inventory/example/live coverage.

## Workflow proof and limits of automation

World Workshop contains 364 ordinary authored objects: several hundred array
instances, path rails, stairs/ledge, a cooked curved ramp/corridor, original six-part
non-humanoid figure with socket/limb target/fitted ragdoll, localized HUD, 800 m marker,
initialized debris and two streamed regions with an adopted referenced traveller.
Its orchestration invokes shipped CLI templates/patches/import/fit/cook/recipes;
the art helper writes glTF only, not positional scene/physics records. Presets and
scene-owned recipes have an explicit source policy. Restricted original M66 art
remains local and read-only; repository content is a redistributable original fixture.

- CLI: create/convert/edit/patch/validate/dependencies/recipes/cook/export.
- Shared editor services: batch transforms, parent policy, groups, references,
  duplication, cancellation, undo/redo, fitting and staged publication.
- Actual editor: panels, Renderer UI preview and Edit/Play/Stop/source save.
- Actual runtime: normal public JS, camera/resize/UI input, spawn/readiness,
  socket/IK/ragdoll, streamed adoption/revisit and separate-process save/load.
- Region automation uses controlled repositioning, not a claim of human walking.
- Native GUI click automation is unavailable. Visual widget usability, arbitrary
  hand-edited content and physical traversal remain the combined human checklist.

## Validation

The first clean Release build failed before any suite ran because standalone input
targets did not link the scene codec. Shared bounded JSON/input helpers removed that
dependency. The **same clean build** was resumed; all targets built with **zero
warnings**, and **146 discovered production suites** passed once. Source hashes
were unchanged throughout this successful gate. Async integration passed **12 cases /
246 checks**; normal editor/Play-Stop, blocking reference and M31 resource stress
also passed. See [gate result](final-followup/RESULTS.json) and
[complete suite results/source hashes](final-followup/production/results.json).
[Original failures and corrections](development/FAILURES.md) remain present.

| Final affected proof | Result | Evidence |
|---|---|---|
| Shared editor/CLI/source/recipe/skeleton services | 190 checks, 0 failures | [services](offline-final/services.log) |
| Actual-GL camera/projection/UI/runtime integration | 34 checks, 0 failures | [runtime](final-followup/production/judas_world_authoring_runtime_tests.log) |
| Workshop Application, including positive paused-state assertion | 16 checks, 0 failures | [application](pause-followup/probe.log) |
| Separate-process save/load | 16 write + 9 read checks, 0 failures | [write](pause-followup/cold-write.log), [read](pause-followup/cold-read.log) |
| Public cookbook in real VM instances | All 31 examples, 260 checks, 0 failures | [cookbook](workflow/cookbook-all.log) |
| Types, drift negative controls and live enumeration | 297 symbols, 27 exports, 202 native operations, 15 callbacks; strict TypeScript 5.9.3; PASS | [API/types](workflow/api-types-live.log) |
| Final CLI generation in independent project roots | Scene and all cooked/generated products byte-identical | [offline result](offline-final/RESULTS.json) |
| Actual editor: batch/undo, localized UI preview, Play-Stop/source/save | PASS; authored scene after Play-Stop identical | [editor](offline-final/editor.log), [edit view](offline-final/editor.edit.png), [Play view](offline-final/editor.play.png) |
| Existing Three Games consumer workload | 820 frames, switching/movement/pause; PASS | [consumer log](workflow/three-games.log) |
| Original and edited project export; moved/source-hidden no-argument startup from `/tmp` | PASS, including positive pause and resume | [export](export/RESULTS.json), [edited follow-up](offline-final/edited-runtime.log) |

Invalid source syntax/reference/physical profile cases fail with useful diagnostics
and leave last-good content intact. Recipe checks include actual CharacterMotor
traversal over a cooked ramp, stable IDs/overrides, stale preview rejection and
reference-aware removal. The restricted original 65-bone M66 fixture is read-only
local input for fitting; it is not distributed with the workshop.

### Late changes and exact coverage

After the broad gate, the exported workshop exposed a project-JS pause bug: Resume
focus and the opening frame's Back event were treated as activation. JS now gates
the opening frame and checks event type. Runtime UI/input behaviour did not change.
The application test now explicitly asserts that the menu remains paused. Application,
cold save/load, editor and both final exported projects were rerun successfully.

Two offline/editor corrections followed: the UI document controls/canvas now have a
resizable two-column window; new geometry-revision IDs derive deterministically from
recipe identity, product role and geometry digest. Matching accepted revisions retain
their IDs. Final services, two-root CLI determinism, editor, authoring performance and
edited export passed. No runtime source changed after the full gate. The six source/
test files whose bytes differ from the gate are `src/authoring/WorldBuilder.cpp`,
`src/editor/EditorApplication.cpp`, `src/editor/EditorPanels.cpp`,
`src/editor/EditorPanels.h`, `tests/WorldAuthoringApplicationTests.cpp` and
`tests/WorldBuildingTests.cpp`; workshop JS changed separately. The full suite is
**not** represented as having run on these later offline/editor/test bytes.

Rerunnable tools: `scripts/m67_validation.py`, `verify_workflow.py` and
`verify_late_authoring.py` in this evidence directory. They use isolated outputs and
user-data namespaces. The late runner is a parameterized copy of the successful
recorded workflow; its exact executed commands are in `offline-final/RESULTS.json`.
No protected runner/assertions were rewritten. [Curation](CURATION.json) keeps the
complete raw gate under ignored `.cache/m67/raw-final-gate`; repository evidence
retains reports, logs, assertions, measurements and useful screenshots, not generated
executables/packages, copied font binaries or restricted original art.

## M56 authoring measurements

364 authored objects, batch selection of 32; one cold and 31 warm samples. Times
below are milliseconds; p95/max are warm samples. [Measurements](offline-final/performance/authoring-performance.json)
and the accompanying M56 profile retain all values.

| Operation | Cold | Warm median | Warm p95 | Warm max |
|---|---:|---:|---:|---:|
| Selection | 0.107 | 0.006 | 0.015 | 0.021 |
| Cached hierarchy filter | 9.115 | 0.010 | 0.016 | 0.022 |
| Changed filter query | 8.795 | 7.236 | 11.948 | 12.738 |
| Batch translation + undo | 50.942 | 41.585 | 45.824 | 46.746 |
| Array regeneration | 21.700 | 21.959 | 24.825 | 25.137 |
| Arc generation + actual collision cook | 19.840 | 20.046 | 21.632 | 21.701 |
| Source normalization/reload | 115.631 | 112.547 | 117.755 | 118.271 |
| Runtime UI layout mutation | 0.011 | 0.003 | 0.013 | 0.014 |

The final editor Play snapshot reported 0.518 ms fixed step, 17 bodies, 465 draws
and 7,472 triangles. This is a snapshot, not a whole-session FPS benchmark. Source
reload and full-revision batch history remain noticeable costs; no speculative
optimization or reduced demonstration size hides them.

## Product and frozen candidate

- Project: `projects/world_workshop/world_workshop.judasproj`.
- Package: `.cache/m67/packages/workflow-export-followup-2/WorldWorkshop`.
- Moved package: `/tmp/judas-m67-offline-workflow-export-followup-2/WorldWorkshop`.
- Package: 25 assets, 53,090,883 bytes, export 0.109 s on this machine.
- Standalone runtime SHA256: `a6ce064c47bfe02a3abdca6b0cd78d26345b6c3077ae8323ba63649ab639503d`.
- [Exact changed files](CHANGED_FILES.txt), [final fingerprints](FINAL_FINGERPRINTS.json),
  [verification receipt](FINGERPRINT_CHECK.json) and [repository/protected scope](FINAL_STATE.json).

Fingerprint verification is a read-only check:
`python3 docs/evidence/m67/verify_fingerprints.py`. It compares recorded files and
change scope, requires the starting HEAD and an empty index, and reports mismatches;
it does not regenerate expected hashes. The four self-referential receipt/manifest
files are explicitly excluded from hashing.

## Known practical limitations

- Closed convex profiles and bounded straight/Catmull-Rom paths; no CAD/CSG,
  concave profile decomposition or arbitrary topology repair. Sweep/arc frames use
  unit entity scale; resize physical geometry through authored dimensions.
- Explicit shape sizes remain separate from visual scale. Origin surface placement
  is not automatic shape-bottom fitting or a gravity definition.
- Full-revision 200-entry scene history and normal full-scene validation incur
  measurable batch/source costs. Preview generation/cooking is cached/job scheduled;
  cancellation may await bounded cooking. No whole-world performance rewrite.
- Skeleton fitting uses rest geometry and selected stable identities. It estimates
  boxes/ball anchors, retains intent and needs explicit review; no anatomical claims.
- Semantic diagnostics can report normalized legacy positions; syntax errors carry
  original JSON locations. No complete JSON semantic source map is promised.
- UI draft history is document-local; save/reopen/Play are explicit. No arbitrary
  gameplay hot reload, rich text, IME or replacement UI runtime.
- Far clipping trades depth precision; world coordinates remain fixed-origin,
  **not live rebasing**. Projection refers to the active main view/viewport.
- Spawn script construction follows normal async synchronization; a body spawned
  from a fixed callback can integrate before a later constructor observes velocity.
  Initial data is already applied, but exact post-integration velocity is not promised.
- Per-file recoverable publication cannot certify crash atomicity. Old generated
  revisions remain available for undo; no aggressive automatic garbage collection.

## Human handoff

Use the single eight-step [combined checklist](../../M67_WORLD_BUILDING.md#one-combined-human-review).
WASD/sticks + mouse; Space launch; V view; Z debris; G ragdoll; L English/Arabic;
Escape pause; R reload; F6/F7 save/load. Travel along −Z through east/west/east.
Build/change/undo recipes, select/batch/reparent, text/CLI/reopen the same document,
fit socket/skeleton mappings, edit localized UI and run the moved package.

The operator authorized commit and push as a **provisional** checkpoint, with human
validation deferred. Commit subject:
`Add M67 integrated world building and supported content authoring (provisional)`.
No milestone tag or claim of completed human validation.
No later milestone work, Claude-workspace/PR/process changes or fluid solver work.
Protected historical evidence and accepted projects are preserved.
