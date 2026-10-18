# Physics-world extraction: independent preservation evidence

## Scope and source identity

The accepted numerical baseline is `7da56c2cc6ce7acda827a6e4019d610622b237e2`.
This stage moves the existing world into cohesive implementation modules and
formats those modules. It does not include the later CCD or dense-scheduling
corrections. The separate CMake completeness guard diagnoses partial direct
fixtures; it does not alter numerical behavior.

`baseline/PhysicsWorld.cpp` and `baseline/PhysicsWorld.h` preserve exact accepted
bytes. The independent verifier checks their pinned SHA256 values and works
without Git history. Its report is `INDEPENDENT_TOKENS.json`.

The coordinator now has 859 readable lines, compared with 4080 dense lines in the
old file. The private state is 487 lines; the largest cohesive C++ module is the
1036-line body implementation. Formatting expands the overall source rather
than hiding statements or moving the monolith into another oversized file.
See `src/physics/WorldModules.md` for ownership and module boundaries.

## Mechanical checks

The independent lexer imports no extraction-generator records. It preserves
string, character and raw literals, and ignores whitespace/comments only.
It establishes all of the following:

- All 61 private method bodies and 113 public/coordinator/particle method bodies
  have identical tokens.
- The entire ordered private declaration/default-initializer stream is identical:
  2183 tokens. This compares actual ordered contents, not just a count.
- Five shared helper types and seven free helper bodies are unchanged.
- All `constexpr` values and the public `PhysicsWorld.h` bytes are unchanged.
- In-memory negative controls detect a changed method, an absent method, a changed
  initializer and a changed public header.

Shared types use the single private `JudasPhysicsDetail` namespace, avoiding
per-translation-unit anonymous types in the same `Impl`. Small handle, filter and
cache checks remain available inline; the modules share one physical owner and
one ordered state. No second solver, world or alternate collision path is added.

The coordinator separately records exact pre/post-format token preservation for
15 implementation files. Source locations and profiler source IDs naturally
change; identical physical behavior is checked independently below.

## Same test objects, real native execution

Ten unchanged test sources were compiled once using their existing Release flags.
The exact same object bytes were linked against the frozen accepted engine
archive and the refactor-only archive. All ten old/new executions passed.
Eight complete output streams were byte-identical. The remaining two differed
only in four explicitly printed performance measurements; all original lines and
values are preserved in the raw logs.

| Existing fixture | Checks in each successful execution | Exercised contracts |
|---|---:|---|
| Impact timing | 3197 | Actual collision endpoints, energy/momentum, chronological impacts and motion ledger |
| Contact cache | 13327 | 823 unchanged fixture rows / 887 primitive pairs, exact prepared/uncached bounds, invalidation and sweeps |
| Kinematic | 96 | Actual prescribed motion, rider motion, support impulses and rotated motor support |
| Physics continuation | 112 | Joint/contact history and reconstruction of real physical/query state in fresh worlds |
| Contact lifecycle | 1076 | 30 authored fixtures, 90 physical steps and actual observed contacts |
| CharacterMotor | 32 | Collision-aware motion, support, filters, independent instances and lifecycle |
| Joints | 29 | Actual joint geometry, limits, motion and arbitrary rotation |
| Particle contact batch | 77 | Existing finite-mass particle/solid coupling and prescribed boundaries |
| Rigid motion | 10239 | Represented motion states, event segments and indexed-ledger agreement |
| Impact robustness | 4412 | Actual impact/contact, mesh and penetration gates |

These counts are the actual unchanged fixtures, not additional capability
certification. The continuation fixture restores fresh worlds inside one process;
it is not mislabeled as a new cross-process save qualification.

### Exact final formatted archive

The final formatted Release archive is SHA256
`b6f091c5bcd77606a851ef73d676de21445e523452ab3ccf8cd43d9d4b5d85ac`.
The same previously compiled objects were relinked to this exact archive. All ten
checks pass, with eight exact output streams and only the same four explicit
performance lines differing in the other two. `FINAL_VALIDATED.json` records
archive, source, object, executable and output fingerprints.

A genuine harness mistake is retained: the first formatted-only invocation ran
from `build/`, making contact-cache fixture inputs unavailable (0 rows instead of
823). The other nine checks passed. The follow-up reran only contact-cache, using
the identical executable bytes from the repository root. It passed all 13327
checks and produced the exact original output hash. No source, fixture, expected
value or executable changed. Original failure and corrected invocation are both
retained under `raw/formatted/` and `raw/formatted-followup/`.

Four short scaling workloads also pass against both extracted archives,
including the exact final formatted one. Baseline/refactor physical digests match:

| Workload | Physical digest |
|---|---|
| Free 1024 | `beba012f5dfe2273` |
| Pile 256 | `63b0f10cd668de91` |
| Active machinery 256 | `76999a0d1f196c6b` |
| Naturally sleeping machinery 256 | `294c73ad4249153b` |

These workloads exercise real driven mechanisms and an impulse wake control.
Final formatted runs are correctness evidence; unrelated Windows work may have
been active, so their timings are not used for performance claims.

## Strict matched full-workload comparison

The accepted baseline and refactor-only archive each ran the exact original
ordinary Application/RuntimeWorld room1024 workload for 270 fixed steps. Windows
was shut off, other agents suspended heavy work, and no owned build, archive
compression or other simulation ran during either measurement. The scene,
settings, synthetic 60 Hz cadence, profiler and acceptance conditions were
unchanged. The coordinator's formatting-only token proof connects the measured
extraction stage to the final formatted source; the final archive also has the
independent execution checks above.

| Metric | Accepted baseline | Extracted world |
|---|---:|---:|
| Median fixed step | 26.470501 ms | 26.375681 ms |
| p95 floor statistic | 389.449361 ms | 368.512104 ms |
| Maximum fixed step | 1911.877507 ms | 1902.212560 ms |
| Process wall time | 52.835444 s | 52.540283 s |
| Child-process maximum RSS | 408940 KiB | 409772 KiB |

There is no measured material regression in this matched pair. Individual runs
and mixed small differences do not establish a general speedup or memory saving.
The approximately 1.9-second worst step remains a serious pre-existing limit.

All 270 complete physical digests and recorded physical counters are identical.
The digest includes every live body's represented pose, velocities and identity;
final digest is `192f0f4bed728953`. Actual endpoint quality is identical, including
1024 loose pieces, 2475 contact points, deepest penetration
`0.007217108528522143` m and zero uncertain/unsupported candidates.

`PERFORMANCE.json`, unchanged CSV samples, actual profiler captures, endpoint
quality, usage reports and binary fingerprints preserve the evidence. This is
behavior preservation for the measured contracts; it does not make the original
curved-mesh failure pass or establish universal collision correctness.

## CMake completeness controls

One `JUDAS_PHYSICS_WORLD_SOURCES` list supplies the engine and direct headless
fixtures. Configuration checks normalized relative and absolute source paths.
A deliberately incomplete fixture using an absolute `PhysicsWorld.cpp` path
failed with an actionable “compiles a partial PhysicsWorld” diagnostic; restoring
the complete fixture configured successfully. `CMAKE_CONTROLS.json` and the
unaltered configure logs preserve the negative and positive cases. This is a
build-integration correction, not a physics change or a native-platform pass.

## Reproduction and boundaries

From the repository root, the compiler-free mechanical check is:

```
python3 docs/evidence/prealpha-pass4/refactor/reproduction/verify_extraction.py
```

It intentionally fails after substantive physics changes unless run against the
recorded refactor checkpoint. Its baseline oracle is fixed accepted source, not
silently regenerated from a later candidate.

The same-object and workload drivers, exact commands, old/new fingerprints and
raw logs are included. Rebuild the accepted checkpoint and this refactor
checkpoint with the recorded Release configuration to reproduce binary
comparisons; place the two archives at the paths accepted by the reproduction
drivers. Binary archives and executables are not committed as evidence.

Windows/Android correctness is not inferred from Linux results. No human
acceptance, Alpha readiness, resolved dense tail, fixed curved CCD failure or
new fluid capability is claimed by this extraction stage.
