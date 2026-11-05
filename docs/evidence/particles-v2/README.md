# Particles V2 qualification evidence

This record pins the implemented and Linux-qualified Experimental checkpoint
`2cd2f6d59d30c986fc5eee01ccc29cc3cceef0d7`, compared with frozen baseline
`621d6ed1cea6eefb93d3d1760ab6c262d8c09b41`. It does not certify a release,
human acceptance or a later final delivery commit. Advanced Fluids remain locked.

The corrected implementation is checkpoint
`908a6185cf8ce35f263b8cc97459aa67ab66fd04`. Its seven particle gates pass with
291 assertions, including 63 GPU controls and 49 world-lifecycle checks. The
strengthened workshop test starts F6 during actual one-cell backpressure and
passes 14 write-process and nine fresh read-process assertions. A refreshed
relocated SDK/export workflow also passes, with zero source-tree accesses in
the traced standalone run. [FOLLOWUP.json](FOLLOWUP.json) records exact source,
binary identities, commands and the separate CI status.

[Clean CI](https://github.com/SuperReaper1999/Project-Judas/actions/runs/41970505309)
passes all 161 registered cases in each existing Alpha/Experimental configuration.
Unavailable fluid cases retain their explicit channel classification; this does
not unlock or certify Advanced Fluids. Both jobs also verify the 17-tool SDK
on Ubuntu 24.04 with a maximum glibc requirement of 2.39. These are clean CI
builds, separate from the host-built human-review SDK and standalone below.

One earlier integrated NVIDIA run failed a capacity-one admission assertion.
That first failure remains **unexplained**, rather than being relabelled fixed.
Three subsequent complete native suites, final NVIDIA/Mesa integration and
400 complete small-pool lifecycles (508 assertions) passed unchanged checks.
Precise failure diagnostics were added for recurrence. This is an open diagnostic
limitation, distinct from the demonstrated and corrected Mesa allocation defect.

## What was measured

The same observer source was compiled against each revision's **own** headers
and engine archive. The frozen build has no include fallback into current `src`.
Baseline capture/build hashes, dependency receipts, identical observer source,
commands and binary hashes are inside the raw archive. Candidate measurements
identify clean `2cd2f6d` build provenance. GNU 15.2 Release, Ryzen 5 5600,
GeForce GTX 970, NVIDIA 580.178.04, OpenGL 4.6; SDL X11 under a Wayland session.
Raw results retain their UTC timestamps. CPU/memory host information in the
summary was inspected when assembling this evidence, not sampled per frame.

Each run has 60 warm-up and 240 measured fixed steps at 1/60 s. Main output is
1920×1080; the additional view is 960×540. The timer covers real particle
simulation, CPU presentation/GPU per-view preparation and drawing. **This is not
a complete game/engine frame.** It does not step MK2's rigid solver. Collision
cases use 13 fixed native boxes; one geometry snapshot is captured outside frame
timings and its cold cost is reported separately.

All 13 final invocations returned zero, retained the requested live population,
and obtained all 240 asynchronous GPU samples without dropped queries. Samples,
median/p95/p99/worst, process CPU time, cold work and process peak RSS are in
[PERFORMANCE.csv](PERFORMANCE.csv) and [PERFORMANCE.json](PERFORMANCE.json).

| Workload | CPU submission median / p95 / worst (ms) | GPU interval median / p95 / worst (ms) |
|---|---:|---:|
| Original CPU, 100k, one view | 8.501 / 8.857 / 9.968 | 0.763 / 0.824 / 0.997 |
| Current CPU, 100k, one view | 8.214 / 8.732 / 9.529 | 0.774 / 0.824 / 1.149 |
| Resident GPU, 100k, one view | 0.177 / 0.257 / 0.357 | 2.698 / 3.115 / 3.550 |
| Original CPU, 100k, two views | 15.880 / 16.620 / 17.574 | 8.059 / 8.292 / 8.962 |
| Resident GPU, 100k, two views | 0.328 / 0.395 / 0.492 | 5.354 / 5.824 / 6.179 |
| Current CPU collision, 1k | 2.168 / 2.524 / 2.626 | 0.100 / 0.104 / 0.143 |
| Resident GPU collision, 1k | 0.093 / 0.101 / 0.123 | 0.217 / 0.224 / 0.240 |
| Resident GPU collision, 100k | 0.203 / 0.234 / 0.392 | 3.970 / 4.406 / 4.584 |

The result is chiefly **CPU work moved to resident GPU execution**. One-view
GPU time increases; it is not an across-the-board GPU optimization. The CPU
path remains available and its small median differences do not establish a
meaningful speedup. A second, reversed-order two-view pair is preserved rather
than pooling away variation. These short serial runs are not a controlled
long-duration benchmark or universal hardware claim.

The 100k collision owner reports exactly **10,564,752 GPU-owned bytes**, one
geometry upload, zero particle-state upload bytes and zero query failures.
Its 14,352 bytes of bounded asynchronous readback cover the whole run; the
explicit 4,800,000-byte final diagnostic state read is outside measured samples.
Process peak RSS varies substantially (for example GPU one-view 170,266,624
bytes versus two-view 114,475,008); do not treat that as an allocation ledger.
GPU intervals also include command-submission bubbles.

Old/current CPU captures are byte-identical for the checked one-/two-view cases.
Independent analytic bounce, trajectory, finite-feature, alpha-order and ownership
tests establish correctness. CPU/GPU collision paths are not asserted identical
over all five seconds: the 1k CPU/GPU contact totals are 25,987 versus 25,690,
with zero reported query failures. Their final screenshots being identical does
not erase this numerical difference or prove universal trajectory equivalence.

## Qualification and developer workflow

[QUALIFICATION.json](QUALIFICATION.json) contains commands, statuses and source
identities. At the pinned checkpoint seven particle gates pass (284 checks):
collision 35, CPU response 45, resident GPU 58, authoring 42, world lifecycle 47,
scripting 13 and persistence 44. Eleven relevant regression invocations also
pass, including existing visual particles/consumer game, render-pass ownership,
compute/portable policy, weather, script modules, saves and 31,404 MK2 rotation
checks. The game-owned deposition application passes separate write/read
processes (12 and 9 checks), preserving receiver-local coating and friction while
visual particles stay transient. These are automated, not human, checks.

The packaged Linux SDK was relocated to a temporary directory, verified, used to
create and Play/Stop/save a new project, recook source collision, check/validate
the workshop, export it and run the relocated standalone from an empty working
directory. The consumer source and SDK were renamed out of their expected
locations. A real file-access trace contains no source-tree runtime reads; its
three matching observer entries are metadata queries for the explicitly chosen
screenshot output directory. The first over-broad trace classification is kept.
This demonstrates relocation, not Ubuntu 24.04 ABI or clean-machine certification.

Android guards and actual Emscripten syntax controls cover four GPU translation
units. They do not constitute Android NDK/link/device, browser runtime or Windows
qualification. The locked-fluid policy and existing weather path are unchanged.

## Preserved failures and corrections

- `cpu-review/runtime-red.log`: **43 checks, 10 failures**, with failing sources,
  commands and hashes. Exposed attachment-compaction identity, newborn emitter
  frames, partial-lifetime carrier speed, restored queue boundaries and allocation
  transaction defects. Corrected CPU controls retain those original cases.
- `early-gpu/native-first.log`: **35 checks, one failure** at final GL-error
  verification. A retired draw program remained bound and was subsequently
  restored by compute. Later controls explicitly check program retirement.
- `early-collision/run.log`: original 34/1 fixture failure is preserved. Native
  MK2 deliberately rejects concave-to-kinematic conversion; the corrected control
  checks that rejection and retained static collision, rather than inventing a
  supported moving-concave path. Corrected 35/35 uses the same production object.
- `cold-birth`: the per-birth global free-stack CAS caused approximately 342 ms
  full births. Batch reservation reduced the isolated 100k birth phase to about
  0.081 ms. Raw alternating phase samples, diagnostic observer and prior source
  archive remain; these are not whole-frame percentile measurements. The first
  batch diagnostic failure (stale asynchronous newest sample) is retained.
- `mesa-minimum`: production reflection independently reports a 16-byte minimum
  scalar SSBO on Mesa versus four bytes on NVIDIA. Capacities 1–3 previously
  allocated 4–12 bytes and failed validation. Padding the allocation envelope
  preserves logical capacity and indices. The original 47/9 failure, manual-link
  setup failure and all **63/63** corrected controls on both drivers are retained.
  Those independent passes do not erase the first integrated 62/63 NVIDIA
  result; `mesa-minimum/integrated/nvidia.log` retains it. Final integration,
  repeated runs and the 400-lifecycle stress pass, without an established cause
  for that one-off assertion. No speculative production change was made.
- `application-source-checkpoint`: F6 no longer pauses a blocked step before it
  can finish. It stops future emission, drains/resumes captured work, then saves
  at the actual ready boundary. The negative control forces a one-cell queue
  and verifies that already queued paint count/weight survive cold restoration.
- `portable-compile/handoff/first-harness`: an observer setup error copied some
  headers while other includes reached live copies, defining the same inline
  helper twice. Restoring the original four-CPP observer layout passes all eight
  portable syntax controls; shared header hashes are checked before and after.
  No engine change or platform-runtime claim is inferred from that setup repair.
- `measurements` is the earlier, superseded suite; **`measurements-final` alone
  supplies the table above**. The archive labels both instead of replacing rows.

## Reproduction and integrity

`raw-evidence.tar.gz` contains selected raw CSV/JSON/logs, captures, observers and
small rejected-source snapshots. It contains no executable, object, library,
entire build directory or unrelated research. Its file list and per-file hashes
are in [ARCHIVE_CONTENTS.json](ARCHIVE_CONTENTS.json), also embedded inside the
archive as `RAW_SHA256SUMS.json`. [SHA256SUMS](SHA256SUMS) checks the durable files.
The folder's scoped Git attributes preserve these exact bytes across platforms;
automatic line-ending conversion must not invalidate the recorded checksums.

```sh
cd docs/evidence/particles-v2
sha256sum -c SHA256SUMS
mkdir -p /tmp/judas-particles-evidence
tar -xzf raw-evidence.tar.gz -C /tmp/judas-particles-evidence
```

From a checkout of the pinned implementation, build the normal
`judas_particles_v2_benchmark` target. For historical comparison, use the captured
baseline archive and `tools/benchmarks/BuildParticlesV2Baseline.py`; the original
archive/library hashes are recorded rather than committing those large binaries.
`raw/measurements-final/RUNS.json` lists exact arguments and original binaries.
Choose **fresh** output directories so no earlier evidence is overwritten:

```sh
SDL_AUDIODRIVER=dummy build/judas_particles_v2_benchmark \
  --backend gpu --case free --count 100000 --views 1 \
  --frames 240 --warmup 60 --require-gpu-timers --out /tmp/new-particles-run
```

`assemble.py` reproduces these summary/archives from the retained cache using
file I/O and read-only Git source extraction only; it does not run qualification.
Future follow-up commits require their own exact-source qualification and must
not be silently relabelled as this checkpoint.
