# Consumer lifecycle follow-up — GitHub #25–#28

Development on Experimental, based on `3a022594b95968185edb9e58fdeb0e086f4fe330`.
This is not a release, main promotion or human acceptance. Independent fluid
research, fluid implementations, existing projects and unrelated experiments are
unchanged. GitHub issue descriptions are retained in the evidence directory.

## Findings and changes

| Request | Verified mechanism | Disposition |
|---|---|---|
| #25 UI exception silently stops gameplay | Exceptions deliberately fault the entire owning script slot. Other slots continue; stderr/editor diagnostics existed, but standalone presentation gave no visible explanation. | Added a noninteractive editor/development fault banner and bounded `scripts.faults()` query. Slot faulting and deliberate reload policy remain unchanged. |
| #26 No shared lifecycle busy query | The native session already has one gate covering saves and scene replacement; JavaScript only had individual save tokens. | Added allocation-free `saves.busy`; strict conflicting-request errors remain. No implicit queue or save-on-quit. |
| #27 Save waits ~90 frames | Uncapped harness time is synthetic while hashing and disk workers use real time. Every request revalidates content before capture. | Reproduced high frame counts, but not a one-second wall-clock delay in the available fixture. Added real elapsed/queue/identity/worker-queue timings and wait reasons; documented clock semantics. No speculative hash cache or latency-fix claim. |
| #28 Harness ignores quit | The normal application checked quit; the harness never did. | Harness now exits on quit, with deadline/no-quit assertions and explicit failures if required later evidence would be skipped. |

The available `/home/conner/Documents/Hinge.zip` contains 76 entities in the tested
level and produces approximately 41 KB saves. The report described roughly 300
entities/68 KB. It is not the exact reported fixture; its alleged native latency
remains unverified. The ZIP hash is recorded in `HOST.json`.

## Measured save behaviour

Ten sequential samples, same extracted game and benchmark configuration. Software
GL/offscreen runs, synthetic 1/60-second frames; no isolated benchmark claim.

| Workload | Before | After |
|---|---|---|
| Uncapped harness total frames | 58–180 | 60–125 |
| Harness real elapsed | 18–32 ms, median 23.5 ms | 18–24 ms, median 23.5 ms |
| Rendered application real elapsed | 42.06–53.91 ms, median 52.54 ms | 43.57–47.55 ms, median 44.75 ms |
| Rendered application capture | 1.17–1.45 ms | 1.07–1.49 ms |
| Rendered application worker | 9.05–12.31 ms | 9.74–16.37 ms |

No speedup is claimed from these small, non-isolated distributions. The change is
observability and lifecycle correctness. The harness includes five running and
five modal-paused samples; the original title screen makes every rendered-loop
sample paused. Full dependency rehashing remains a potential cost for larger
projects and is not skipped merely to lower a frame count.

The exact old runtime executes all 12 frames after the quit fixture requests
shutdown on callback three. The corrected runtime ends at zero-based frame 2.
The Hinge save-then-quit wrapper completes all ten required saves and exits at
frame 1090 in the recorded corrected run.

## Validation

- New production-path regression: actual UI button activation → `onUI` exception
  → faulted owner slot → healthy observer reads bounded detached diagnostics;
  rendered banner pixel test and override persistence across Play sessions.
- Lifecycle regression: modal save, status timing fields, conflicting scene
  request rejection, busy observations for save/list/load/delete and pending
  scene replacement, queued cancellation, and no service allocation from polling.
- Harness positive quit deadline plus negative controls for missing quit,
  unexpected quit, future assertions/captures and incomplete service waits.
- Existing scripting: 42/42; storage: 41/41; excluded-body persistence: 77/77;
  scene transitions: 23/23; seven real Hinge save/load cycles passed.
- Existing pointer/harness: 33/33 on native X11/GTX 970. Initial sandbox run used
  SDL offscreen and failed its resize checks because that backend explicitly
  rejects resize. Its failure is preserved, not counted as a pass. The native
  desktop rerun used unchanged tests.
- Editor duplicate/undo, Play/Stop, save: authored scene IDENTICAL, zero resource
  failures, isolated sprite-animation workshop copy. No operator project edited.
- Real export and relocated package run from `/tmp` with development root missing:
  packaged quit script exits correctly. No export compatibility policy changed.
- API/type/inventory checker passed, including negative drift controls and the
  new structured fault/timing fields. Validation tooling: 14 tests passed;
  registration: 8 tests, 2 environment-dependent skips.
- Native desktop and software-rendered lifecycle runs passed. Repeated poll
  assertions make their check counts scheduler-dependent; these are the same
  contracts, not additional independent coverage.

Windows and Android were not executed for this follow-up. No device acceptance,
new patch publication or human testing is inferred.

## Files and reproduction

`SOURCE_SHA256SUMS` records the exact implementation, declarations, tooling and
reference changes. `BINARY_SHA256SUMS` identifies the qualified local binaries;
`RESULTS.json`, logs and screenshots preserve observed outcomes. Initial fixture
setup mistakes remain in `.cache/consumer-lifecycle-evidence/regression-*.log`;
they were test initialization errors, not additional engine defects.

```sh
cmake -S . -B .cache/2d-stage1-build
cmake --build .cache/2d-stage1-build --target judas judas_editor judas_export \
  judas_consumer_lifecycle_tests judas_pointer_harness_tests judas_script_tests \
  judas_save_storage_tests judas_save_exclusion_tests judas_scene_transition_tests \
  judas_save_performance_tests judas_save_latency_benchmark --parallel 2
# Set XDG_DATA_HOME to an isolated directory when exercising persistence.
.cache/2d-stage1-build/judas_consumer_lifecycle_tests .cache/lifecycle-proof
SDL_VIDEODRIVER=x11 .cache/2d-stage1-build/judas_pointer_harness_tests .cache/pointer-proof
node scripts/check_judasjs_api.mjs
# Optional real consumer benchmark; requires the separately owned Hinge project.
.cache/2d-stage1-build/judas_save_latency_benchmark /path/to/hinge/hinge.judasproj
```

The new native test is registered as `consumer-lifecycle` in
`tools/validation/suites.json`. `SaveLatencyBenchmark.cpp` is an explicit utility,
not an automatic test requiring a private ZIP. Hinge source is not redistributed
in this evidence snapshot. The wrapper, raw timings and ZIP hash are retained
for reproduction with the operator's copy.

## Human checklist

1. Throw an error from a project's UI action: confirm the banner identifies the
   source and callback, other slots continue, and deliberate reload clears it.
2. Save while paused: show pending state, inspect real timings, and wait for that
   token's success before requesting a scene change or quit.
3. Run a quit tour: verify early exit, then add a later required screenshot and
   confirm the harness reports a failure instead of silently omitting it.

Remaining: the exact newer Hinge fixture's alleged one-second latency is not
reproduced; very large projects still rehash content; no automatic recovery of
faulty slots, implicit lifecycle queue, automatic save flushing or new platform
qualification is claimed. These are explicit boundaries, not hidden workarounds.
