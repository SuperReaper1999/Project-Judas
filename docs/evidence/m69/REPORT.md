# M69 — paired controller input and batched physics queries

**Uncommitted candidate ready for operator review.** Starting/current
HEAD/main/origin-main: `c042797c755df68b45e36aa917c4ca72818b9bcd`.
M67's outstanding human authoring review remains deferred. No M70 work.

## APIs / ownership

- `input.stick('left'|'right')`: copied normalized device pair before Judas axis
  processing; same selected controller, X right+, Y down+, raw diagonals retained.
- `input.stickDelta(side)`: latest render-pump endpoint difference, independent of
  fixed-step count. Out/back can have zero net delta.
- `input.vector(name)`: normal authored paired binding, optional circular response,
  signed X/Y scales, normal component-bounded map summation. Unknown name is neutral.
- `input.stickSamples(side, after=0)`: ordered copied observations with independent
  reader cursors, receipt seconds, global sequence, reset/overflow and capacity 128
  per stick. The backend cannot recover observations it never delivered.
  Focus/device/ownership/pause/session changes clear stale history and rebaseline.
- `physics.raycastMany(rays, filter={})`, `sphereCastMany(casts, filter={})`,
  `capsuleCastMany(casts, filter={})`: at most 256 requests, one bridge/filter
  preparation, whole-request prevalidation, input-order scalar hit snapshots/null
  with normal safe entity handles and authoritative geometry/filtering.

Binding example (entry in the normal named project's input data):

```json
{"name":"flick_stick","axis":false,"vector":true,"bindings":[{"control":"stick:Right","scale":1,"scaleY":1,"deadzone":0.2,"circular":true}]}
```

Circular response is `(v/length(v))*min(1,(length(v)-d)/(1-d))` outside the inner
radius, neutral inside. Scale/inversion follows processing. Raw values are
unaffected. Old scalar maps retain their previous behaviour and canonical v1
identity; vector legacy maps use v2. Named/compact authoring and the existing
editor inspector support the fields. See [input](../../judasjs/input.md),
[physics](../../judasjs/physics.md), [types](../../judas.d.ts) and
[M69 overview](../../M69.md). No trick/ledge gameplay policy is native.

## Actual validation / preserved failures

The first build hit stale parent-Make rules after CMake generated new test targets.
It is preserved unchanged in [RESULTS.json](RESULTS.json) and [release-build.log](release-build.log).
The operator-authorized follow-up Release build passed in 30.579 s with zero C++
compiler warnings/errors; upstream CMake/HarfBuzz notices from the original
configuration are recorded separately. Native input passed **43 checks**;
real-VM/play input passed **21 checks**. These were not repeated.

The batch fixture initially used a ray-clear path that its sphere/capsule could
legitimately hit. The miss origin was corrected to clear every shape; strict
witness/distance expectations remained unchanged. Only that test target rebuilt,
then passed **80 JS correctness assertions + 19 C++ checks**. The engine/query
implementation was unchanged. [FAILURES.md](FAILURES.md) preserves both failures
and the corrected follow-up.

Pending cookbook cases passed: paired-stick **10**, ray-fan **10**, surface **9**.
API/source/types/live verification passed using TypeScript **5.9.3**: **304 public
symbols / 27 exports / 207 internal operations / 33 type-checked examples**,
including three negative drift controls. This is coverage, not proof of every
structured-value semantic. No production suite or unrelated consumer campaign ran.

The gate orchestration now generates targets before Make parses the multi-target
request; one configure-only check passed with the existing Release cache. No
successful test was repeated for this correction. Exact completed commands,
logs and result are in [completed-checks](completed-checks/RESULTS.json), plus
[configure-only check](completed-checks/ORCHESTRATION.json).

## Matched 100-ray measurement

Same VM/world/build/rays/filter, five warm-up callbacks and 21 measured callbacks
per mode; no simulation advance. Callback includes JS/marshalling/query work.

| | Scalar | Batch |
|---|---:|---:|
| Native query bridge calls | 100 | 1 |
| Filter preparations | 100 | 1 |
| Geometric queries | 100 | 100 |
| Warmed median callback | 705.637 us | 437.359 us |
| Maximum measured callback | 825.816 us | 592.994 us |
| Geometry in separate profiled callback | 0.103704 ms | 0.090240 ms |

[Raw measurement](completed-checks/measurement.json). The reduction is bridge and
marshalling work; geometry count is unchanged. Separate profiled geometry timing
is not an assertion of a geometry speedup or a universal game performance promise.

## Operator fixture / human check

Ordinary private project: `.cache/m69/example/m69_example.judasproj`.
Created by [generator](../../../scripts/create_m69_example.py) from a disposable
copy of character_demo; source project content was not modified. It attaches the
exact two public examples through small public-API display scripts and normal
.judasui. Normal application startup/capture passed: the HUD displayed **100
queries / 66 hits / 34 misses / scalar comparison YES**. Screenshot is
[example.png](completed-checks/example.png). Automated desktop GL capture is not
human controller-feel acceptance.

Launch: `build/judas .cache/m69/example/m69_example.judasproj`.

1. Right stick: cardinal/diagonal reads; compare raw vs circular output.
2. Flick out/back: inspect ordered samples even when endpoint/delta is neutral;
   reset/overflow must be visible. Real controller feel/timing remains untested.
3. C: compare the 100-ray results with scalar mode. Escape pause/resume,
   R reload, Q quit. No recognizer, ledge selection or climbing is implemented.

## Integrity / handoff

All **29 final candidate SHA-256 hashes** match [FINAL_FINGERPRINTS.json](FINAL_FINGERPRINTS.json).
[Original hashes](ORIGINAL_FINGERPRINTS.json) and [two accepted corrections](CORRECTIONS.json)
are retained; expected hashes were not silently regenerated to hide a discrepancy.
[Candidate diff](CANDIDATE.diff) covers tracked/new implementation files;
[exact changed-file list](CHANGED_FILES.txt) includes current evidence. Evidence
reports are excluded from their own fingerprint to avoid self-reference.

Nothing committed/pushed/tagged. Existing project content, protected historical
FTFT/P1 and milestone evidence/research remain unchanged. Physics/fluid solver,
broadphase, Claude's game workspaces and M68 lifecycle caching are not redesigned.
M67 human review is not marked complete. No M70 work.

Suggested commit only after operator acceptance:
`Add M69 paired stick input and batched physics queries`.
