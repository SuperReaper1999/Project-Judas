# Save latency — latest Hinge and issue #27

Experimental follow-up to `01af9b928b91e9b4bcdc92cfdccf9e1039dc5117`.
This is not a main promotion, release or human acceptance.

## Reproduction and actual cause

Inspected `Hinge_30_10_26_Latest1.zip` from Documents, SHA-256
`a8cd1b68e1085ac1c276f0372cb3c85d5e991be60592915db52e2087f94fc599`.
It has 534 entries, 9,289,900 uncompressed bytes, 181 map entities and 171 level-1
entities. The original extraction is unchanged in `.cache/save-latency-latest/original`.
The updated report and comment are preserved in `ISSUE_27.json`.

The map-versus-level frame-count disparity is reproducible. In one matched
baseline pair the map save took **365 synthetic frames / 46 ms**, and level 1
**57 synthetic frames / 39 ms**. Their identity jobs took 30.76 and 30.39 ms.
Startup listing took 190 frames / 33 ms on the map and 57 frames / 33 ms in the
level. No nine-second map wait or special late capture boundary was established.

The uncapped harness advances 1/60 second per outer frame while workers use real
monotonic time. The map has no rigid bodies and cheap frames; level gameplay has
more per-frame work. The map therefore accumulates more synthetic frames during
comparable wall-clock I/O. Multiplying these counts by 1/60 does not measure real
latency. This conclusion applies to the supplied fixture/current baseline; it
is not proof that every historical machine or larger project has low latency.

Every operation still revalidates scene/asset bytes before capture. That accounts
for approximately 31–34 ms here. It is a real cost, not an arbitrary frame delay.
We did not introduce stat-only caches, skip changed files or weaken integrity.

## Targeted optimization

`SaveService` used `EncodeGameSnapshot(...).size()` for telemetry, then
`SaveStorage::Write` encoded and checksummed exactly the same snapshot again.
Storage can now return the size of its already-encoded envelope. Saves therefore
perform one encoding rather than two. POSIX and Windows implementations share
the same optional size-observation contract. Existing source call forms remain
valid. SDK/tools must be rebuilt with the matching engine, as usual.

The size output is set after encoding. It is **not proof of publication or
durability**: subsequent I/O may fail. Early failures can leave it unchanged;
SaveService initializes its product count to zero. Save-container bytes, checksum
algorithms, backups, locking, failure injection and synchronization are unchanged.
No save-format or JudasJS interface change was introduced.

Measured cost of the removed encoding pass, 20 retained samples after one warmup:

| Participant payload | Median | p95 | Worst |
|---|---:|---:|---:|
| 64 KiB | 0.199 ms | 0.221 ms | 0.231 ms |
| 1 MiB | 4.512 ms | 4.664 ms | 4.711 ms |
| 7 MiB | 33.782 ms | 35.383 ms | 36.020 ms |

These are encoding-only measurements, not whole-save speedup claims. This is
particularly useful near the supported snapshot size bound, while the small
Hinge snapshots save only a fraction of a millisecond of CPU work.

Three runs per scene, alternating before/after call order, five running and
five modal samples per run: **15 samples in each table cell**. Same project
sources, inputs, 800x450 configuration and synthetic timestep; separate fresh
save directories. Baseline binary retained before changes. No compiler jobs ran
during these paired measurements. Software GL/offscreen, shared host; not an
isolated distribution qualification. With n=15, nearest-rank p95 equals worst.

| Scene/state | Before median / p95 wall ms | After median / p95 wall ms |
|---|---:|---:|
| Map running | 45 / 53 | 44 / 51 |
| Map modal | 46 / 55 | 44 / 64 |
| Level running | 46 / 49 | 44 / 59 |
| Level modal | 45 / 49 | 45 / 55 |

No robust overall Hinge latency or tail improvement is claimed. Identity hashing
and disk synchronization remain dominant, and some measured tails were worse.
Raw phase rows and worker timings are retained in `PAIRS.json` and the logs.

## Regression results and a separate game-side boundary

- Storage: 43 checks passed, including exact reported size versus envelope/file,
  early failure output, corruption recovery, symlink rejection, locking and
  child termination at publication boundaries.
- Consumer lifecycle: passed, including public busy/timing fields, modal saves,
  cancellation, actual UI faults and harness quit controls.
- Save evolution: 48 checks passed, including version/rejection contracts.
- Seven latest-Hinge **level** save/load/resume cycles passed with 171 entities,
  15 bodies and 19 audio emitters. No gameplay source changes in that fixture;
  only its copied project startup scene was selected.
- A generic round-trip benchmark initially saved/loaded the **map**. This fails
  because `map.js` has no `restore()`: `uiUpdate` accesses `this.doc`, which only
  `start()` initializes. Modern loaded slots deliberately call `restore` instead
  of `start`. The same explicit map round-trip fails on the preserved baseline
  and corrected runtime. This is not introduced by the optimization.
- Hinge's normal Continue uses a level slot, whose script implements `restore`.
  Its map profile snapshot supplies metadata to the Store; it is not loaded as
  the map world in the current game flow. If the author wants map-world resume,
  the map must reconstruct its transient bindings/state in `restore()` first.
  No engine fallback or game-script repair was added to disguise this boundary.
- Latest map probe exported and relocated under `/tmp`, with the development
  root unavailable, completed ten saves and quit correctly.

Windows/Android were not executed. The Windows source change mirrors POSIX, but
native Windows qualification remains outstanding. Existing games and independent
fluid research are unchanged. Advanced Fluids remain locked.

## Reproduction

```sh
cmake --build .cache/2d-stage1-build --target judas judas_export \
  judas_save_latency_benchmark judas_save_storage_tests \
  judas_consumer_lifecycle_tests judas_save_evolution_tests \
  judas_save_performance_tests --parallel 2
.cache/2d-stage1-build/judas_save_latency_benchmark --encode
.cache/2d-stage1-build/judas_save_storage_tests .cache/m61-storage-speed-proof
.cache/2d-stage1-build/judas_consumer_lifecycle_tests .cache/save-speed-lifecycle
.cache/2d-stage1-build/judas_save_evolution_tests .cache/save-speed-evolution
```

The evidence directory retains map/level wrapper sources, tour, raw measurements,
ZIP identity and qualification results. Wrappers subclass the actual game;
they do not replace simulation or implement a substitute save service. The map
wrapper enters the real first-world view, records startup listing, requests five
running and five paused saves, then quits. To reproduce, extract the ZIP into an
owned copy, rename the original `map.js` or `game.js` to `*-core.js` with an
ordinary registered script metadata file, and install its matching wrapper.
Select map or level1 as startup. Keep before/after copies identical and isolate
XDG_DATA_HOME. `measure_pairs.py` retains the exact local execution commands.

`SOURCE_SHA256SUMS` and `EVIDENCE_SHA256SUMS` identify this checkpoint's changes
and evidence. Qualified local binary hashes are in `BINARY_SHA256SUMS`.

## Remaining work / human check

The exact current report is explained by synthetic time; no new queued-boundary
correctness defect was found. Large mutable projects still pay for full identity
rehashing. A future immutable-package fingerprint or change-tracked pipeline
needs its own integrity contract, not a blind cache. Full slot-list rescanning
is another candidate for measurement at high slot counts, not an established
bottleneck in this fixture.

Human check: pause a game, save, verify success before leaving, reload a supported
level and confirm progress. Report actual milliseconds from status if it feels
slow. Faulted or failed saves must remain visible rather than being treated as
successful merely because the busy gate cleared.
