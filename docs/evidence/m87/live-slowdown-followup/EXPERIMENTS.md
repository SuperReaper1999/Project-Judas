# M87 live slowdown: retained experiments

This is the experiment record for the human-reported slow Physics Stress Lab. It supplements the original M87 evidence without altering it. The final corrective candidate, final-source validation and release assessment belong to the separate follow-up report; the intermediate measurements below do **not** certify final-source 60 Hz performance.

## Original failure and content

The original human-live package identifies source checkpoint `cee55a898aa0e4938665903f3b207d828acdb1df`. Its exact executable/core/debug-type hashes are retained in [original provenance](provenance/original-failure/original-core-provenance.json); those binaries and memory dumps are deliberately excluded from this repository evidence. Selected extracted physical state, reproduction scripts and original assembly proving the counter extraction are included.

The selected scene was the **1024 loose-body room**, not the initial 128 room: 1285 total bodies, 1152 dynamic bodies, 96 joints. The dynamic bodies comprise 1024 loose pieces, 96 cabinet articulation bodies and 32 cabinet balls. The authored mixed box/sphere piles have positive initial gaps. The exhaustive initial AABB scan found **zero positive-volume loose-body overlaps**. Cabinet assemblies are separated from the loose-body pit; none of their dynamic bodies participated in the captured impact cascade. See [content classification](provenance/content-classification/REPORT.md) and its exact [source/layout measurements](provenance/content-classification/source-layout-results.json).

The original unfinished step 188 had 396 accepted events and 314709 TOI queries after advancing only 0.003821390 s of its 0.016666668 s interval. The maximum recorded body speed was 5.15 m/s; highly repeated loose pieces could move only about 0.02 m/s. There were no spinning-support pairs. The observed ledger times made positive, sometimes nanosecond-sized progress: this was not demonstrated to be an infinite zero-time loop or an energy explosion. It is a genuine low-speed dense-pile event-processing scalability problem. The watch tour's early simulation-time phase consequently stretched across much longer wall time.

The original M56 live capture overflowed: 115452 dropped events and 50504 truncated records. Its raw file remains in [original-human-overflow.json.gz](raw/profiles/original-human-overflow.json.gz). It cannot establish a complete scope-time breakdown. Later bounded captures with zero dropped/truncated records are included separately; collection overhead is retained in their measurements.

## Comparable short fixed-input cases

All rows in this table retained 155 fixed steps at the unchanged binary32 `1/60` timestep, 1024 loose-body scene, 1152 initially awake dynamic bodies, no tour inputs, hidden rendering and swap interval 0. Profiler scopes were disabled for these cases. The diagnostic observer still writes CSV, hashes physical state and renders frames; these are **narrow repeated-input reproductions**, not pure solver benchmarks. 155 steps represent only about 2.58 simulated seconds. A completed run must not be described as 60 Hz acceptance or a long steady-state pass.

Times are observed fixed-step wall milliseconds: median, nearest-rank p95, maximum, sum. These are single runs with ordinary host scheduling variation; small differences are not robust speedup claims. Each intermediate source stage differs as named. The identity rows are compared to `dense-fixed-before` over equal step indices. Zero mismatches means the recorded body pose/velocity/sleep digests matched, not a mathematical proof covering all other workloads.

| Intermediate case | Median ms | p95 ms | Max ms | Sum ms | Impact events | Digest mismatches |
|---|---:|---:|---:|---:|---:|---:|
| `dense-fixed-before` | 30.017 | 110.056 | 909.772 | 8600.008 | 3780 | 0 |
| `dense-fixed-loop-translation` | 29.738 | 108.877 | 930.122 | 8545.732 | 3780 | 0 |
| `dense-fixed-loop-rotation` | 29.529 | 84.296 | 712.052 | 7365.313 | 3780 | 0 |
| `dense-fixed-geometry` | 30.177 | 74.637 | 635.543 | 7074.060 | 3780 | 0 |
| `dense-fixed-pinned` | 30.113 | 75.770 | 624.357 | 6968.030 | 3780 | 0 |
| `dense-fixed-boolean` | 29.941 | 72.991 | 618.884 | 6937.794 | 3780 | 0 |
| `dense-fixed-parallel-2` | 30.162 | 74.293 | 635.205 | 6923.692 | 3780 | 0 |
| `dense-fixed-parallel-4` | 30.129 | 74.303 | 560.264 | 6769.741 | 3780 | 0 |
| `dense-fixed-narrow-4` | 22.041 | 64.058 | 644.239 | 5761.017 | 3780 | 0 |
| `dense-fixed-persistent` | 29.749 | 78.058 | 644.377 | 7046.487 | 3780 | 0 |
| `dense-fixed-future-tree` | 30.001 | 71.180 | 592.577 | 6794.835 | 3780 | 0 |
| `dense-fixed-iterations-10` | 29.870 | 72.820 | 620.420 | 6975.975 | 3780 | 0 |
| `dense-fixed-iterations-40` | 43.358 | 397.221 | 2482.389 | 18605.849 | 12219 | 151 |
| `dense-fixed-iterations-80` | 34.567 | 301.145 | 1480.391 | 15314.192 | 12232 | 151 |

[All case summaries](raw/CASE_SUMMARY.csv) also retain initial-collision/broadphase/solver sums, body counts and finite-state results. Full raw CSV and actual retained run logs are in `raw/live-cases/<case>/`. The residual-capture case is instrumented separately and is not an uninstrumented timing comparison.

### Profiler-on diagnostic cases

These rows must remain separate from the profiler-off table. They provide internal-stage evidence and collection diagnostics, not clean throughput comparisons.

| Diagnostic case | Median ms | p95 ms | Max ms | Sum ms |
|---|---:|---:|---:|---:|
| `dense-fixed-tight-query` | 31.289 | 78.338 | 662.785 | 7370.959 |
| `dense-rotation-profile` | 29.639 | 87.575 | 745.642 | 7537.064 |
| `dense-fixed-manifold` | 30.391 | 82.840 | 708.647 | 7420.794 |
| `dense-fixed-lazy` | 29.854 | 167.486 | 3180.466 | 14754.958 |
| `dense-fixed-lazy-prefix` | 30.743 | 86.272 | 722.279 | 7532.329 |

The additional `dense-before` and `dense-early-before` live cases are also preserved. Their early helper did not retain a complete configuration line; no later setting is invented for those runs. `startup-watch-before` exercises the initial 128 room with measured frame intervals/tour, and is not interchangeable with the fixed 1024 reproduction. [Profile index](raw/PROFILE_INDEX.json) identifies raw capture coverage/overflow.

## Experiment dispositions

These dispositions combine the actual measured results with their mechanism. Preparation-time README files inside source bundles deliberately retain their earlier “unmeasured” status; this current ledger records the subsequent observation.

- **Loop bookkeeping and conservative bounds:** source/base/patches are retained under `experiments/loop-bookkeeping/` and `bounds-prototypes/`. Replace repeated membership scans without changing traversal order; reuse discovered canonical manifolds; retain equivalent represented geometry and original event/solver ordering. Intermediate results preserve all 155 recorded physical digests. Final acceptance depends on final-source focused tests and measurements, reported separately.
- **Manifold cache — rejected:** cache full zero-margin primitive geometry by directed pair/primitive indices and all 14 pose bit words. The worst retained step 153 had 3452 hits / 140236 misses, only 2.40% hits, 132044 replacements and 6225920 additional bytes. The geometry remained correct in this reproduction, but hash/copy/storage overhead did not justify adoption. Exact source, matching base and patch are preserved under `experiments/manifold-cache-rejected/`.
- **Older Boolean truth-cache proposal — not adopted, not performance-validated:** this separate earlier design keyed actual truth including exact absolute penetration target. Source and patch are retained under `experiments/truth-cache-proposal/`. It must not be relabeled as a measured successful optimization or confused with the later full-manifold experiment.
- **Lazy/prefix queries — rejected:** defer full interval queries behind conservative inclusive absence certificates, retaining original full query interval and event ordering. The first variant increased the retained worst step to 3180.466 ms with profiling on. The refined prefix variant matched physical digests but did not establish a useful reduction sufficient to justify its extra state/complexity. Source variants, proof notes and raw zero-overflow profiles are preserved under `experiments/lazy-rejected/`.
- **Pinned common-start query samples:** retained as an intermediate stage in the raw ledger; exact pose bits/ledger remain authoritative. No separate speedup is asserted from a single small difference.
- **Primitive Boolean contacts:** share existing validated boundary predicates while avoiding unnecessary witness/manifold construction for zero-margin simple queries. The dedicated focused parity tests establish existence/diagnostic parity separately; the 155-step scene also preserved physical digests.
- **Parallel CCD queries 2/4 lanes — rejected:** dispatch independent queries with private scratch, merge in original order, keep island/solver publication on the owner. Both preserved all 155 physical digests. Aggregate benefit was small relative to scratch/service complexity and the remaining serial work. Do not infer a general parallelism improvement from the small maximum-time fluctuation. Source/base/patch/build inputs are under `experiments/query-workers-rejected/`.
- **Parallel initial narrowphase — selected for final-candidate evaluation:** only immutable centered Box/Sphere initial pair geometry is dispatched; owner consumes outputs in original pair order, with all jobs joined before publication. Four lanes reduced this intermediate normal-step median from about 30 ms to 22.041 ms, with unchanged 155-step physical digests. It did **not** remove the cascade: 644.239 ms maximum remained. Production service ownership and final-source validation are reported separately. Original prototype source and later production conversion are preserved under `experiments/initial-narrowphase/`.
- **Persistent conservative deadline heap — rejected:** keep certificate deadlines across unrelated events, refine globally earliest pending items without advancing physical time, compact stale heap nodes. Exact digests matched, while query-count reduction did not translate to a useful observed fixed-step improvement; prefix-bound construction/refinement still costs work. Source/base/patch/ordering proof are retained under `experiments/persistent-deadline-rejected/`.
- **Future-trajectory private broadphase — rejected:** lazily build an event-pass tree using conservative remaining-trajectory bounds, preserve public broadphase/history and sorted actual-contact discovery. The 155-step digests matched, but the small total-time change did not justify another broadphase plus bounds/update work. Source/base/patch are under `experiments/future-tree-rejected/`.
- **More cold impact-solver sweeps — rejected:** the existing production count remains 10. Isolated 40/80 variants worsened full-scene event counts to 12219 / 12232 and sums to 18605.849 / 15314.192 ms, versus 3780 events / 6975.975 ms for the matched 10 diagnostic. They changed physical trajectories starting at step 5. This is a numerical experiment, not an exact-behavior performance optimization.

## Frozen-island solver diagnosis

The first eligible accepted no-joint impact at step 150 was captured before solving: 22 bodies (21 dynamic), 49 contacts, unchanged dt/materials, force-inelastic flag. The snapshot stores every rigid-body field as 29 exact binary32 words, and each contact as 9 binary32 plus 16 binary64 words with flags/body indices. It also preserves the actual accepted post-solve body/impulse-row bit words. Shapes are unnecessary for this solver-only replay because exact contact geometry is already included. The format is a private research fixture, not a Judas save format.

The replay reproduced the original 10-iteration physical endpoint and all 49 impulse rows bit-for-bit, including 100 repeated solves. Alternative iteration counts intentionally changed body velocities and impulses.

| Sweeps | Maximum closing residual m/s | Median isolated Solve ms | Largest body linear change vs 10 m/s |
|---:|---:|---:|---:|
|10|0.0675045|0.054589|0|
|20|0.0468324|0.093009|0.05040|
|40|0.0175640|0.162621|0.10191|
|80|0.00449613|0.293857|0.12458|

The float solver expression and independent double point-velocity evaluation differed only by about 4e-8 m/s. Over 1096 accepted events from steps 150–155, maximum-closing residual had median 0.0347033, p95 0.0990075, maximum 0.173024 m/s; maximum float/precise disagreement was 1.0005e-7 m/s. Thus finite-iteration coupled-contact residual is real; float anchor evaluation does not explain its magnitude. Higher sweeps nevertheless worsen whole-scene event processing, so this does **not** establish raising the budget as a cure. Normal complementarity is measured after the final friction sweep; no convergence claim for the friction cone is invented.

Exact input, capture/replay sources, dependency snapshots, raw residuals/timings, build commands, logs and historical hashes are in [impact-quality](experiments/impact-quality/). Source dependencies captured at collation must be checked against the preserved historical fingerprints before assuming a binary-identical rebuild. The recorded binary hashes identify binaries actually used; this archive does not claim to reproduce those binaries from a later engine archive.

## Exact-arithmetic hypothesis ruled out for captured stall

Offline analysis used the original executable's assembly: `GetContactGeometryDiagnostics` reads TLS at `fs:-96`; `PhysicsWorld::Step` saved its starting snapshot at `rbp-0x940`. Subtracting the saved starting words from the original unfinished step 188 gives 3187842 predicates, **all interval-resolved**, with zero exact fallback, numeric-gap fallback, prepared-orientation miss, unresolved or invalid inputs. All seven resource-worker counters were zero. The entire captured history contains only 10 exact fallbacks among 200450511 predicates.

Existing M56 captures do not publish those per-step predicate fields, so normal/worst counts cannot be reconstructed from their JSON. The raw core extraction does establish that expensive exact arithmetic was not active in the captured stall. See [delta proof](provenance/original-failure/original-core-predicate-delta.json) and reproducible GDB scripts. Interval arithmetic and repeated geometry work remain real CPU costs.

## Reproduction and provenance limits

Preserved helper build scripts record the actual compile/link flags and original absolute paths. No hidden test/native gameplay path is introduced. Recreate a compatible Release build, restore the selected source variant and fixture, then use the ordinary application observer. An equivalent bounded fixed-input reproduction is:

```text
live <frozen-package>/Scenes/room1024.judas <output-directory> 120 155 0 0 1 0.016666666666666666 0 0
```

The generous wall limit avoids truncating the requested 155 steps. The full original argv was not retained for every early run; the actual log/settings and raw samples are authority, not a retroactively invented command. Keep the project rooted beside its required packaged assets; the archived scene/script/config are selected fixtures, not a self-contained executable package.

The isolated diagnostic appends `<iterations> <residual-csv-0-or-1>`. `M87_IMPACT_RESIDUAL_FROM_STEP=150` narrows CSV collection. The separate capture helper accepts `M87_CAPTURE_IMPACT=<path>` with defaults step >=150, minimum 20 dynamic bodies, no joints. Replay:

```text
replay <frozen-island.bits> <output-directory> 100
```

These names are private diagnostic controls, not public JudasJS or supported runtime configuration. The archived scripts retain their exact original paths; adjust copies for another checkout rather than editing preserved evidence. No new builds or simulation were run while collating this directory.

[Copied artifact manifest](provenance/COPIED_ARTIFACT_MANIFEST.json) records original/stored hashes, timestamps, deterministic gzip settings and missing inputs. [Source-pair verification](provenance/EXPERIMENT_SOURCE_PAIRS.json) reapplies retained patches and checks byte-exact variant recovery. The old lazy patch's immediate base was not the separately named later pre-lazy snapshot; its exact matching base is explicitly reverse-reconstructed from the retained patch and variant, with that derivation recorded. This is source preservation, not a new simulation or a retroactive binary identity claim. No executables, object files, static build archives, core dumps, environment dumps or secrets are included. Original protected/history files are untouched. Remaining dense-pile scalability limits must be reported by the final follow-up report; no 60 Hz or human acceptance is inferred here.
