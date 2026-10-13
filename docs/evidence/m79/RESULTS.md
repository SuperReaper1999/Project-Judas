# M79 focused Linux handoff — 2026-10-13

Starting Experimental: `e49099e37820c5cf97cf0fb123cbf8b303c3d751`.
Human validation **pending**. Stable/main remain at the branch-split baseline.

## Implemented scope

Reuse M63's physical partitions, real PhysicsWorld bodies, M45 fixed joints,
load observations, irreversible topology, callbacks and ownership. Add an
explicit optional section-stress cook: actual planar area moments, combined
axial/bending stress, approximate transverse shear/torsion, per-interface strength
and dimensionless section conditioning. `.judasdeform` version 3 carries the
extension; legacy v1/v2 bytes and mechanics remain unchanged. No parallel solver,
new gameplay binding, scripted collapse or replacement debris.

Existing editor asset bake/inspector/preview, scene/prefab resource references,
save participant/owned joint continuation, source-region pinning and export paths
are used. Public JudasJS methods are unchanged; current fracture documentation
explains the new authored model and the family-default strength snapshot fields.

## Observed outcomes

Same eight-cell geometry (8 × 0.6 × 2 m), 768 kg family mass, actual 20 kg contact
load. Before removing right supports: A/B peak tension approximately 37.64 kPa;
extra-pier C approximately 7.99 kPa; all intact. Under identical support loss,
at 240 simulation steps after resource publication, A has 5 failed connections,
stronger B has 1, redundant-support C has 1. C can acquire another failure later;
no indefinite-survival claim. A's failures publish progressively (steps
131, 172, 175, 178 in the recorded run), not as a scripted all-at-once operation.
Real retained pieces fall, collide and settle. Family material mass remains 768 kg.
Actual impact and sustained JS overload also produce sequential physical failure.

## Focused validation

All final affected checks pass:

| Check | Assertions |
|---|---:|
| Independent section equations, rectangular moments, combined compression/bending, shear/torsion, override/format/scale/malformed/source-key controls | 27 |
| Actual project support loss, strength/support comparisons, oblique field and material ownership | 22 |
| Actual impact | 19 |
| M35 input → JS gravity-directed overload → physical failure | 18 |
| Prefab, stream publication/pinning, owner teardown, scene reload | 22 |
| Public JS save after collapse | 22 |
| Separate-process restore: exact deformable/owned joint bytes; resumed physical impulse and actual changed body position | 18 |
| Spatial RadicalGravity / ordinary higher-precedence oblique uniform region | 23 |
| Existing M63 core | 33 |
| Existing deformables | 66 |
| Existing M45 joints | 29 |
| Existing fracture momentum/ownership/lifetime mechanics | 35 |
| Existing fracture support-redistribution regression | 17 |
| Existing fracture physical/query/presentation probe | 23 |

Release targets rebuilt with no compiler warnings in final build logs. This was
an affected-target Release build, not another clean full production-suite run.
171 M79 assertions and 203 existing focused regression assertions; counts are
receipts, not comprehensive physics certification.

Editor copied-project automation: duplicate/undo exact, Play/Stop authored scene
IDENTICAL, resource failures 0, save succeeds. Actual new strength-field mouse
interaction remains in the human checklist. Offscreen SDL cannot provide relative
mouse capture; this automated backend limitation is not human desktop acceptance.

API drift/type checker: 345 public symbols, 30 exports, 239 native operations,
16 callbacks, 39 examples, 3 negative drift checks, TypeScript 5.9.3 pass.
No new JS runtime binding; no claim of live enumeration in this check.

Export: 11 assets, 3 scenes, 54,252,997 bytes. Actual package executable launched
with no project arguments from `/tmp`, then copied to a path with a space and run
again from `/tmp`. Named input drives support removal, pause/resume expectations
and transition to `Scenes/radial.judas`. Both exit successfully. No development
working directory or repo assets are needed. Export/package binaries stay in `/tmp`.

## Linux host CPU measurements

Four eight-cell structures (32 actual rigid cells) plus proof loads, supports,
floor and CharacterMotor query body. 340 measured application frames per run,
60 Hz fixed simulation. Timings are total fixed-step CPU cost, **not GPU/FPS
certification**, and include normal scripts/contacts/constraint observations.

| Scenario | Total bodies at endpoint | Median ms | p95 ms | Max ms |
|---|---:|---:|---:|---:|
| Uniform support-loss/collapse | 41 | 0.538753 | 1.808030 | 6.642062 |
| Impact | 44 | 0.434282 | 0.916562 | 1.672495 |
| Sustained overload | 44 | 0.655219 | 1.854652 | 5.748290 |
| Actual radial field and support loss | 41 | 0.529012 | 1.716689 | 7.498108 |

No solver cadence, iteration quality or contact fidelity was reduced. No large
building scalability promise follows from these modest workloads.

## Preserved development failures

- `development/first-build.log`: new target referenced before its test file existed.
- `development/first-check-build.log`: test used a nonexistent diagnostics method,
  treated an existing void impulse function as Boolean and had count-format warnings.
- `development/support-first` and `support-measured`: first test incorrectly assumed
  stronger/redundant structures must never break and expected moving debris to retain
  speed after settling. Actual peak loads were recorded: B's first section reached
  about 1.018 MPa bending tension; C reached about 136 kPa. The meaningful independent
  comparison is reduced failure at the same load/geometry, plus actual falling mass.
  Engine physics/strength/cadence were not retuned to force survival.
- `development/input-harness`: new test initially sent SDL keys while modern test
  mode intentionally suppresses real SDL device events. Fixed the new test to use
  QueueTestPhysical. The game already uses normal logical input.
- `development/save-environment`: sandbox's default user save directory was read-only.
  Scoped XDG_DATA_HOME to `/tmp/judas-m79-data`; no engine save behaviour changed.
- `development/export-material`: copied scene helper's material default was replaced
  too early, before helper completion. Export correctly rejected its missing required
  ID. Corrected new project generation to bind its own outer/inner materials; then
  repeated game/save/package checks on the corrected content.

New M79 traces are compressed losslessly; `compressed-traces.json` retains exact
uncompressed hashes. Protected historical failures were not modified.

## Platform and limits

Windows VM `win10` was checked and is shut off. Native M79 Windows compilation and
functional smoke **not run**, outstanding and nonblocking; no Windows pass claimed.
Linux is the acceptance platform. No Stable promotion or human validation claimed.

Brittle section resultants, rigid authored boxes, ideal world pins; torsion and
transverse shear are approximations. No FEM, fatigue, fracture-energy accounting,
mesh cutting, healing, or automatic breakable hinges/sliders. Finite-iteration
joint/contact transients affect failure. Live source regions pin rather than
unloading moving families. CharacterMotor is kinematic: its standing weight is
not an implicit static load; the orange rigid bodies provide the real proof load.
Existing liquid-bearing fracture prohibition and soft-fragment query limits remain.
Protected history, independent fluid research, accepted games and deferred FLU-001 /
legacy saves / curved CCD findings remain untouched.

## Deliverables / reproduction

- [Architecture, authoring and human checklist](../../M79_STRUCTURES.md)
- `projects/structural_lab/structural_lab.judasproj`
- `/tmp/judas-m79-structural-lab-20261013/judas` (current export)
- `/tmp/judas-m79-moved/Structural Lab/judas` (moved-package proof)
- `tools/CreateStructuralLab.py` reproducible content bake
- `FINAL_SOURCE_SHA256.txt`, `BINARY_SHA256.txt`, `CHANGED_FILES.txt`

Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, then build the targets
listed in `linux/release-build.log` with `-j2`. Run `build/judas_structural_tests`
from repo root. For application checks set `SDL_VIDEODRIVER=offscreen`,
`SDL_AUDIODRIVER=dummy`, `XDG_DATA_HOME=/tmp/judas-m79-data` and run
`build/judas_structural_application_tests PROJECT MODE OUTDIR`, with modes
support/impact/overload/lifecycle/write/read/radial. Run write before read in
separate processes. Radial mode uses `projects/structural_lab/Scenes/radial.judas`.
`export-smoke.txt` runs through JUDAS_TEST_SCRIPT; its screenshots go to `/tmp`.
Editor automation used a copied project so the tracked authored scene was preserved.

Commit-ready for Experimental on focused Linux evidence. Human checklist remains
pending; these receipts do not promote Stable or close unrelated audit limitations.
