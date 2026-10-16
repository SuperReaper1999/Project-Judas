# M93 — Judas Learns to Pack Light

Status: Linux validated Experimental candidate. **Human review pending.** Main
remains accepted M92 `bb3dbc402629a60c22738da8520e5017998709f1`.
Windows/Android M93 were not run and remain outstanding. No fluid/physics model,
protected history, existing game content or gameplay policy was changed.

## Architecture and scope

- Versioned `.judastex`: full mip chains, semantic-aware color/data/normal filters,
  BC1/BC3 or RGBA, lossless disk supercompression and actual unsupported-format
  RGBA fallback. Imported rigid material maps are self-contained and cooked by role.
- `.judasmesh`: lossless compacted base vertex/index data and separate render-only
  LOD tails; material/primitive/provenance identities remain. Base physical triangle
  order/positions/source identity are tested. Attribute-aware simplification with
  stronger normal preservation and a default 0.5-pixel estimated error tolerance.
- Every actual viewport/projection selects LOD independently, including batched
  draws. Authorable scene/prefab `render.lod-multiplier` (0 forces full detail).
  Runtime deformation disables invalidated offline error estimates.
- Optional project `visualBudgetMiB`, default zero preserving old eager ownership.
  Enabled main/split/secondary views retain demanded cooked resources between
  camera updates; publication and render masks apply. Inactive resources evict LRU.
  Explicit/nonvisual dependencies remain pinned; active overcommit is visible.
- Generation-checked, reusable mesh/texture/material GPU slots; stale operations
  cannot alias replacement resources. CPU metadata is released on eviction.
- CPU worker cooking, owner-thread publication, deterministic relative recipes,
  source recheck, stable recook asset ID, old-output preservation and stale-export
  rejection. Closure exports exclude authoring recipes/source artwork.
- Existing saves retain asset IDs/author settings; GPU objects reconstruct.
  Changing the visual memory policy alone does not invalidate compatible saves.
- Editor controls, resource/M56 diagnostics, CLI, documented contracts and ordinary
  scripted lab. No new JudasJS binding: existing public APIs remain unchanged.

Meshoptimizer v0.25 (MIT) and stb_dxt have pinned provenance/notices. No new
runtime service, package manager, compute API or mobile dependency is required.
See [contracts](../../M93_ASSET_PIPELINE.md) and [reproduction](REPRODUCTION.md).

## Measured results

Native Linux, Intel i7-4790, GTX 970 4 GiB, NVIDIA 580.178.04. Release `-O3 -DNDEBUG`.
Exact timestamps, binary hashes, commands and load observations are retained.
128 detailed instances, shared 2048² texture, matched cameras and 120 steady samples
per row across two alternating rounds. Wall time includes GPU completion, excludes
swap/pacing. This is an asset workload, not an entire-game FPS guarantee.

| Mode | Views | Median ms | p95 ms | Worst ms | Triangles/frame |
|---|---:|---:|---:|---:|---:|
| M92 source | 1 | 3.351 | 3.976 | 4.605 | 2,048,000 |
| M92 source | 2 | 6.135 | 6.706 | 6.963 | 4,128,768 |
| Candidate source | 1 | 3.062 | 3.404 | 3.889 | 2,048,000 |
| Candidate source | 2 | 6.153 | 6.967 | 7.644 | 4,128,768 |
| Cooked / full geometry | 1 | 1.217 | 1.413 | 1.957 | 2,048,000 |
| Cooked / full geometry | 2 | 2.327 | 2.677 | 5.675 | 4,128,768 |
| Cooked / automatic LOD | 1 | 0.348 | 0.474 | 1.805 | 284,302 |
| Cooked / automatic LOD | 2 | 0.584 | 0.665 | 3.101 | 447,094 |

Source/candidate images are pixel-identical. Source timings are broadly comparable;
no general speedup is claimed for uncooked assets. Full cooked geometry already
benefits from shared vertices/cache reuse and compressed textures without reducing
its triangle count. LOD adds the separate reduction shown above.

| Asset / process measurement | M92 source | Final cooked LOD |
|---|---:|---:|
| Managed GPU payload | 25,318,740 B | 3,566,352 B |
| Fresh-process asset load, two runs | 139.76 / 140.24 ms | 25.42 / 24.73 ms |
| Peak observer-process RSS, two runs | 144,176 / 146,348 KiB | 113,856 / 113,976 KiB |

Payload is exact owned asset VBO/EBO/mip storage for this fixture, excluding driver,
framebuffer, built-in and text-atlas allocation. Baseline mip accounting is corrected
analytically; the old ledger rounded by one byte. Process RSS is a different metric.
OS cache was not flushed. All runs use fresh processes; no concurrent Judas game
was detected. Desktop/GPU scheduling variation remains, including the reported tails.

M56 GPU timestamps and CPU opaque-submission wall scopes remain separate:

| Mode | Views | GPU opaque median/p95 ms | CPU submission median/p95 ms |
|---|---:|---:|---:|
| M92 source | 1 | 3.244 / 3.716 | 0.026 / 0.052 |
| M92 source | 2 | 5.833 / 6.296 | 0.053 / 0.115 |
| Cooked LOD | 1 | 0.211 / 0.216 | 0.040 / 0.077 |
| Cooked LOD | 2 | 0.315 / 0.319 | 0.069 / 0.108 |

LOD selection has a small CPU cost; it does not promise lower submission CPU time.
GPU work and loading/memory improve. CPU scopes can include driver/scheduling waits.
Raw sample counts and profiler diagnostics are in `measure/` and `SUMMARY.json`.

### Storage and visual quality

- `column.obj`: 1,578,121 source bytes → 310,139 cooked bytes.
- `tile0.png`: 1,165,233 source bytes → 1,344,838 cooked bytes.

The cooked texture file is larger than its source PNG (it stores a full mip chain),
while using much less GPU memory. Compression does not guarantee smaller files for
every source. Combined fixture storage falls; source artwork remains author-owned.

Main-view foreground RGB RMS (0–255): source → cooked full **1.31**;
cooked full → final LOD **5.06**. Background-threshold mask
change for the latter is **1.45%** of foreground union.
These are explicit image metrics, not a certified geometric bound. The initial
1-pixel/weak-normal default produced RMS 23.76 and a 12.49% mask difference; it was
rejected in favor of stronger normal preservation and 0.5-pixel default tolerance.
The slower, better-looking final result is the performance table above. Screenshots
are preserved for review; discrete LOD changes can still be visible.

## Validation actually run

Final-source Release build and 18 focused executables passed, plus API drift/type
checking. No production-suite marathon was performed.

- **44 asset checks:** deterministic codecs, BC1/3/RGBA, semantic mips, normals/alpha,
  corruption/no partial publication, topology/base ordering, LOD reduction and
  viewport relevance, actual GPU fallback/images, safe reuse (1,000 churn cycles),
  real resource-publication vertex/LOD identity and eviction/reload.
- **25 residency checks:** enabled sparse secondary cameras, independent split
  views/mask negative control, unseen assets, overcommit, default eager ownership,
  real save reconstruction, portable recipes, stale/conflicting publication.
- **15 ordinary application checks:** public-JS view controls, split views, actual
  saved bytes and restored camera endpoint, full/LOD scene replacement, pause/focus,
  reload, actual instancing and Stop without script faults.
- Renderer architecture/cameras, local-multiplayer rendering, materials and their
  application path, model import/reimport, staged resource handoff, runtime render
  state/authoring, save storage, scene streaming and export regressions passed.
- Async integration: **12 cases / 246 checks**, including real pending-read,
  CPU-ready and queued-work teardown. The old unreachable shutdown setup also fails
  on the frozen M92 baseline. Corrected its observation boundary, preserving all
  original ownership/cancellation assertions. GPU identity assertions now include
  generation when slots are reused.
- TypeScript 5.9.3/API check: **37 exports, 424 public symbols**, live enumeration,
  four negative drift controls and 46 statically checked cookbook examples. The
  focused live surface example ran; this is not a claim that all 46 examples were
  executed in M93. No public binding/type inventory change was needed.
- All **20 actual lab cooks** recooked byte-identically. Editor undo/save/Play-Stop
  preserved authored scene bytes. Intentionally stale source rejected export.
- Closure package exported, copied byte-identically, then launched from an unrelated
  `/tmp` working directory. Split views, pause, save/load, district change, full/LOD
  scene switch and reload passed through normal public input/services.

Two pre-existing RuntimeWorld misleading-indentation warnings were observed during
shared-header rebuilds; no new warning was retained. New cook controls still need
human usability/visual review. Windows/Android compression and authoring remain
unvalidated; forced unsupported-BC fallback passed on Linux, not on a phone.

## Lab and handoff

`projects/asset_lab/asset_lab.judasproj`: **The Packed Museum**, 576 sculptures,
four rigid mesh forms, sixteen textures, four districts, one enabled cached remote
view and optional split views. Existing local presentation coordinates accompany an
Earth-scale world origin. No extra physics world or scene-name runtime branch.

Controls: **F5** tour/free, **WASD + Q/E + mouse** free flight, **1–4** district,
**V** split, **C** remote camera, **F2** full/automatic LOD, **F6/F7** save/load,
**R** reload, **Escape** pause/resume. The HUD describes content/settings, not fake
profiler counters. Resource diagnostics show actual residency. The eight-MiB budget
is soft; required or explicitly pinned data may exceed it.

Export: `.cache/m93/delivery/package/judas`. Relocated launch path and complete
package hashes are in `delivery/RESULTS.json` / `PACKAGE_FILES.json`.

### Human checklist

1. Watch all districts; inspect nearby texture detail and sculpture silhouettes.
2. Fly closer/further and compare F2 full geometry against automatic LOD.
3. Use split views and cached monitors; check both views stay correct.
4. Disable monitors, visit distant districts, return; check resources reload.
5. Save, move elsewhere, load; pause/resume and reload.
6. Use Editor Play/Stop; cook a source, assign its output and recook it.
7. Run the relocated standalone and repeat these checks.

## Limits / preservation

- Whole-asset residency; no per-mip streaming, virtual texturing, sparse GPU memory
  or hard VRAM cap. Active/pinned working sets may overcommit. A large indivisible
  upload can exceed the normal resource-pump time slice.
- BC1/BC3 only; normal maps retain RGBA. No BC5/ASTC/ETC2 cooking. Source decoded
  texture limit is 64 MiB RGBA, cooked maximum dimension 4096. Inspect masks/UI/text;
  select lossless RGBA where appropriate.
- Rigid index LOD only; no skinned LOD, morph/crossfade or independent per-LOD VBOs.
  Simplifier errors are estimates and cannot certify arbitrary asset appearance.
- Default-zero projects keep eager ownership. Explicit material/override and
  nonvisual dependencies remain pinned; this is not universal asset virtualization.
- First bound-header reads are synchronous and cached. Relevance prepass scales
  with published visual owners × active views. Many unique cold assets can still
  cause startup/upload bursts; common repeated instances reuse resources.
- Recipes are generated cook receipts. Change settings through cooking/recooking.
  Ordinary failed publication preserves output; process/filesystem interruption can
  leave recovery files. No fsync/crash-atomic filesystem transaction is promised.
- Content hashing remains conservative: recooking bytes can invalidate dependent
  baked/save content identities even if base physical geometry is unchanged.
- Original failures and rejected quality defaults remain classified in development
  evidence. Protected historical evidence, independent research, accepted fluids,
  existing games and `main` remain untouched. No human acceptance is claimed.

See `CHANGED_FILES.txt`, source/binary hashes and raw receipts for exact scope.
