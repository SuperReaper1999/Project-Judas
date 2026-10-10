# M74 candidate evidence — Borrow Moves

Candidate based on `b7d7f4142b243cd57d9998f4952857e67e5d84a8`, provisionally
checkpointed by operator instruction. **Human validation is pending.** Automated
verification is not human motion acceptance. No M75 work or milestone tag.

## Delivered ownership and workflow

`judas_model_import` owns the CPU retarget service. It is used by the existing
recipe cooker, CLI and editor comparison panel. `judas_engine` and shipped `judas`
do not link a retarget evaluator. Runtime receives normal named clips inside the
target `.judasmodel`; pose resolution, skinning, physics and JudasJS are unchanged.

One version-1 `Imports/*.judasretarget` profile stores source project identity,
target asset ID, stable hierarchy signatures, explicit full-key correspondence,
static local reference corrections, model-frame alignment, translation scale and
selected translation joints. Clip selection/trim/name/rate/root policy belong to
ordinary `motions[]` recipe operations. Native target clips are retained.

Rotation transfer measures source model-space delta from its declared reference,
converts through the profile alignment, applies the target reference, then recovers
locals through the current target parent. Source helpers participate in observations;
unmapped target helpers retain reference locals. No bind/rest/weight rewrite or
world-up convention. See [the specification and executed commands](../../M74.md).

The lab contains original CC0 source art and two proportioned, re-rolled targets:
71 source nodes, 74/72 target input nodes, 17 mapped roles, different names and
intervening helpers. The ordinary cooker adds its existing transparent motion root.
Travel, Wave and Bob reuse each target's profile. Native, preserved/extracted/in-place
travel, asymmetric wave and body bob are ordinary clips. Four scene instances include
independent shared-asset playback, a resolved palm socket and a partial M70 handoff.
A normal animated region and prefab exercise current lifecycle machinery.

## Linux focused results

| Check | Result |
|---|---:|
| Retarget core, analytic/negative controls, actual FBX→GLB | 57 passed |
| Recipe/cache/last-good/dense secondary trim | 37 passed |
| Actual lab rigs, unchanged geometry/binds/native clips | 46 passed |
| Editor draft/undo/cancel/profile save/reopen | 20 passed |
| Actual GL/public-JS/prefab/region/reload integration | 30 passed |
| Save, exit, fresh-process load with actual 3-joint mask | 8 + 8 passed |
| Existing skeletal animation / pose composition | 34 / 16 passed |
| Existing model import / reimport | 30 / 37 passed |
| Live QuickJS API surface / type and drift checks | 9 checks; 29 exports / 336 symbols |

The API check also covers 233 native operations, 16 callbacks, 38 typed cookbook
examples and three deliberately negative drift cases with TypeScript 5.9.3. There
are no new runtime public bindings or type/inventory additions to invent.

Actual editor GL comparison and ordinary Play/Stop completed. Duplicate/Undo and
Play/Stop left the authored scene byte-identical. Automated draft tests exercise
the same history and atomic profile service; mouse interaction/motion judgement
remain the operator's checklist. Preview jobs use CPU snapshots, Renderer owns GPU
work, and Play suspends preview/publication until Edit. Close/project replacement
cancels stale jobs. GUI captures are evidence of execution, not visual acceptance.

A normal all-assets export contains nine runtime assets and two scenes. It was moved
to `/tmp/Judas Retarget Lab é α`, launched from `/tmp` while the entire original
project was hidden, and exercised clip/layer/IK/physical/streaming/pause/reload controls.
No raw source art, profiles, recipes or authoring tools occur in the package. A
separate disposable target-only export removes the comparison source entity and its
cooked source rig, retains the default all-assets policy, and passes the same runtime
controls with its authoring project hidden. Source provenance is required at export
validation, not at playback. Normal exports carry neither Blender nor FBX parsing.

## Numerical and authoring costs

Tolerances were fixed before result judgement: direct 20 µm / 20 µrad; off-grid
preview versus baked tracks 0.2 mm / 1 mrad; root reconstruction matrix max 0.0002.
Maximum direct angular error **5.421e-7 rad**; explicit translation error
**2.666e-7 m**; preserved local rest-offset error **0**. Off-grid position error
**2.768e-7 m**, angular error **4.719e-7 rad**. Root reconstruction maximum matrix
coefficient error **1.908e-5**. A deliberately wrong correspondence/frame exceeds
its tolerance (~0.692 rad); normalization alone cannot pass that control.

Representative two-second/60 Hz Wave bakes: 121 samples, 17 mapped joints, 2178
keys, **11.48 / 8.86 ms** (broad/tall); complete cooked models **420059 / 418851 bytes**.
Estimated working buffers **94608 / 93984 bytes**, not complete allocator accounting.
Real original FBX one-second bake: 61 samples, four mappings, 305 keys, **1.74 ms**.

The late authoring review found inherited `TrimClip` re-sampled the entire clip per
track. It now samples each union time once and streams results into the unchanged
per-track schedules, with cancellation and existing bounded work. Actual dense test:
258 input nodes, 516 tracks, **57** shared pose samples / **14763** node evaluations,
exact output-value error **0**, cancellation after two samples retains accepted output.
The former source loop implies **29412** pose calls; this is a counted algorithmic
comparison, not a claimed measured old wall-clock speedup.

## Bounds and limitations

Compatible articulated topology only; manual correspondence, not universal anatomy
or arbitrary many-to-many synthesis. Target proportions remain its own, so hand/foot
contacts are not automatically preserved. Selected translation is explicit; no
CharacterMotor motion or hidden locomotion application. Already-extracted source
tracks, participating animated scale, singular/sheared/reflected/nonuniform paths
and nonconstant participating STEP discontinuities are diagnosed before publication.
Positive uniform importer normalization is supported. Continuous LINEAR/CUBIC source
motion is baked to ordinary LINEAR keys with recorded interpolation error.

4 MiB profiles; 4096 nodes/mappings; existing 16384 palette limit; 3600-second clips;
240 Hz; one million samples; four million output keys/256 assembled clips; intersecting
64 million sampled-node evaluations. Buffer estimates do not claim all allocation
accounting. M70's 64-participant solve bound is not a retarget hierarchy limit.

## Genuine failures and corrected follow-ups

Preserved in [development](development/): mandatory socket-offset authoring omission;
wrong world-manifest path instead of asset ID; partial-physics selection accidentally
including the physical root; strict publication serialization used for an empty editor
draft; omitted streaming scene in export list; stale source handle in the source-free
lab ablation; wrong semantic bone labels in a mask fixture; and an autofocus variable
inserted in the wrong editor helper during a narrow build. Corrected generic editor
draft identity tolerates incomplete inputs; strict Save/Validate remains strict.
The partial physical selection keeps the root animation-owned. Missing provenance
before export was an expected safety rejection, not a runtime source dependency.

The planned affected Linux Release build exposed two existing `RuntimeWorld.cpp`
indentation warnings; that source was unchanged. New Linux warnings were corrected;
remaining native MSVC warnings are identified below. No production-suite marathon or physics/
fluid research reruns were performed. Test instrumentation follow-up separates loading,
warmup and steady M56 observations and labels accumulated Renderer counters honestly.

## Preservation and handoff

Only M74 authoring/import/editor paths, focused tests, new lab and current guidance
change. Existing Lastlight/Skate/Parkour content, renderer UV fix, accepted fluid,
protected FTFT/P1/earlier evidence, TripleTake, video_work, asset libraries and local
Kate tooling remain outside this change. The pre-existing untracked
`POST_M73_REPORT.txt` is retained. Index remains empty and the starting HEAD/main/
origin-main remain aligned. M67 deferred review and physical Windows hardware
acceptance remain outstanding.

See `CHANGED_FILES.txt` and `FINAL_SHA256SUMS.txt` for exact candidate scope and
fingerprints. The manifest covers source, assets and current guidance; evidence
receipts, this report and the manifest itself are excluded. Suggested eventual commit: **Add M74 editor-time animation retargeting and
reusable bake profiles**.

## Steady playback and loading

Paired uncapped hidden Release application runs use the identical authored scene,
assets, camera, fixed simulation dt 1/60 and four animated instances. Only broad
entity 11 switches Native (one track/five keys) versus Wave (18 tracks/2178 keys);
the other three instances remain identical. These demonstrate ordinary consumption,
not an isolated universal zero-overhead claim for unequal track densities.

| M56 observation | Native | Retargeted Wave |
|---|---:|---:|
| Frame mean / p95 / max (ms) | 11.723 / 12.632 / 13.416 | 11.709 / 12.669 / 14.008 |
| Animation instances mean / p95 (ms) | 0.08618 / 0.11091 | 0.08751 / 0.13483 |
| Clip sampling mean (ms) | 0.00987 | 0.01107 |
| Skin matrices mean (ms) | 0.05905 | 0.06030 |
| Cold readiness (ms) | 946.43 | 925.93 |
| Three completed geometry uploads total (ms) | 0.21454 | 0.13507 |
| Steady asset uploads | 0 | 0 |

60 warmup frames precede 300 measured frames; M56 retains the final 120. Loading
captures include completed startup and warmup, separate from cold-readiness wall
time. Inclusive scopes overlap and are not summed. Buffer/resource estimates are
observations rather than exact allocator accounting. No steady geometry/texture
upload scopes or source retarget pass occur. The runtime symbol audit and target-only
package establish that no source evaluator is required. Renderer accumulated counters
are not represented as per-frame draw counts; use the M56 frame counters.

## Native Windows verification

Windows 10 VM, native Visual Studio 2022 x64 Release, two compile jobs, serialized
after host work. All **619** final source/fixture/project hashes match the host;
seven required executables were fresh for the planned build, with the two
warning-cleaned test executables freshly rebuilt afterward. Native core **57**,
cook **37**, lab **46** and editor draft **20** checks pass. Linux-authored profile
signatures validate unchanged on Windows. The same numerical tolerances pass;
Wave bakes measured **23.09 / 23.51 ms** in this VM, not a host performance claim.

Actual interactive-session application integration passes **30** checks; modern
save/fresh-process load passes **8 + 8**. Editor comparison rendered source 71 /
target 75 nodes, then normal Play/Stop left the scene and all authored project
bytes unchanged. The runs are non-administrator. The pristine SDK exported nine
assets/two scenes; the ordinary package contains no authoring inputs or Mesa.
The moved package at `C:\JudasValidation\Packages\Moved M74 Retarget Lab é α 20261010-182041`
passed the 305-frame controls from an unrelated working directory while the
entire original lab project was unavailable. Receipts and executed scripts are
in [windows](windows/).

The first nested cook test hit the **existing Win32 path-length boundary**:
129-character two-hash cache name plus staging suffix produced a 261-character
path. The same native creation operation fails with error 3, succeeds with an
extended prefix, and succeeds with a short name in the identical parent; the VM
has `LongPathsEnabled=0`. The unchanged cook executable passes from a short root.
Original failure and independent path probes are retained in
[development/windows-first](development/windows-first/). General Windows long-path
support is not claimed or repaired as unrelated M74 work.

The original native build exposes inherited MSVC warnings and two new editor
autotest local-shadow warnings. New test warnings were corrected without changing
their values; their narrow rebuild is warning-free. No claim of a warning-free
whole Windows build. App-local `opengl32.dll` / `libgallium_wgl.dll` with llvmpipe
are added only to disposable VM tooling/test-package copies **after** the stock
SDK/export audit. VM rendering is execution evidence; physical Windows GPU,
input/controller feel and audio-hardware acceptance remain outstanding.

## Human review checklist

1. Open `projects/retarget_lab/retarget_lab.judasproj`; Asset Browser → Import tools → Animation retargeting. Load `Sources/source.gltf`, `Imports/broad.judasimport`, `Imports/broad.judasretarget`. Inspect full-key mapping/calibration; try a draft edit and Undo/Redo/Cancel, save/reopen deliberately.
2. Compare source and broad target, then load `Imports/tall.judasimport` / `Imports/tall.judasretarget`. Play/scrub Wave and declared reference; check asymmetric arm, different lengths/rolls and helpers.
3. Save profile, save recipe operation and Bake explicitly. Confirm ordinary output still includes Native beside transferred clips; original geometry/bind pose is not replaced.
4. Play: `1` Travel, `2` Wave, `3` Bob, `4` Native, `G` crossfade. Check independent fourth instance. `M` cycles preserved/extracted/in-place root policy; extracted motion is queryable, not automatically applied to the entity.
5. `Space` pauses clip clocks, `Right` seeks; `L` toggles left-arm layer/socket composition, `I` shared IK, `P` partial physics and return. Inspect smooth ordinary skinning and the animation-owned physical root.
6. `T` requests/releases/revisits the animated region; `F6/F7` save/load, `R` reload and Editor Stop must restore/reconstruct without stale mixer/physics state. `Esc` opens the menu.
7. Run `/tmp/Judas Retarget Lab é α/judas` and compare the exported experience. Human motion quality/contacts and physical Windows GPU/input remain separate from automated execution evidence.
