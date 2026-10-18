# Pass 3 integration contracts

Baseline: accepted `f647d7fd95f60d3a37dba70a51f4e45dc41c2922`. This is scoped investigation and correction, not universal composition or human acceptance.

## Origin admission: demonstrated production defect

An empty script-owned world at −10¹² m admitted a destination at +10¹² m: each absolute anchor was individually legal, but the 2×10¹² m translation exceeded PhysicsWorld's existing native component limit. `CommitOrigin` then discarded pending intent and threw `invalid origin translation` before completing publication. The reverse direction behaved identically. Ordinary 512 m and exactly 10¹² m controls committed.

`OriginLimits.h` now shares the unchanged native finite/per-component bound. `RuntimeWorld::CanChangeOrigin` checks the actual delta before accepting pending intent, with an actionable intermediate-anchor diagnostic. PhysicsWorld uses the same helper and retains its original direct-call exception. No range is expanded and no physical entity is moved to hide the problem. The origin transaction was separately mechanically formatted: all 2,001 tokens match its post-correction pre-format source.

`origin-baseline.log` records the actual throw; `origin-contract-baseline.log` independently fails the new admission assertion against the frozen baseline engine. Candidate: **23 checks pass**, including both signs, exact-limit acceptance, invalid pending replacement preserving earlier intent, no-op quantization, nonfinite/domain controls, direct native rejection and exact serialized participant bytes. Final combined-source reruns are linked by the parent report.

## Liquid exact-total failure: oracle causal baseline

The original, unmodified test and both failed reproductions are preserved under `original/`, `original-normalization.log` and `original-knots.log`. They use the frozen accepted engine archive; compile/link arguments are retained. These are genuine original failures, not relabelled passes.

In normalization, a legitimate paired-flow step precedes a rejected out-of-chart request. Summed owner totals change from `2.5199999999999996` to `2.52` m³ during that legitimate flow; M54 ledger error remains exactly zero. The original atomicity assertion compared the later total with the value from **before that flow**, wrongly assigning the earlier summation change to the rejected request.

The current test takes its baseline immediately before the request and still requires **exact `==`** total equality. It also captures every physical participant before/after and requires identical participant sets, versions and bytes. The pre-existing flow-conservation tolerance is unchanged. The full normalization command passes **36 checks**. No liquid implementation, storage representation, solver budget, damping or threshold changed.

This bounded correction closes the test's atomicity allegation; it does not assert bitwise equality of a floating-point sum through all legitimate transfers. The original failed assertion remains reproducible from the exact archived source.

## Half-turn query: independently established representation boundary, still open

The full original knots command still fails after 21 checks. A 512 m binary32 chart subtraction moves the represented owner-local input by **1.7261876139525506e−5 m**, crossing cell 9→10. The disturbed finite-volume query is piecewise constant across this cell boundary: surface error **0.0400146581 m**, velocity error **0.0159996208 m/s**. Authoritative wave bytes, energy and owner ledger remain unchanged.

New, explicitly separate `--query-controls knots` checks (23 passing) query both differently quantized points **before any shift**, on unchanged state. They already observe different cells and surface samples. After the shift the query matches that same quantized point's pre-shift sample. An independent interior-cell point retains the original strict position/normal/velocity tolerances.

This is not a production fix. No snapping, averaging, wave reset or relaxed threshold is used. Double intermediates cannot recover a point already rounded by a float public coordinate. A future correction needs a consistent coordinate/reconstruction contract across physical sampling, rendering, optics and loading; these controls do not certify that work. `liquid-origin-grid-query` remains an explicitly failed, unscheduled acceptance contract.

## Debugger and networking: distinct pause policies

The actual QuickJS source breakpoint suspends the owner's call stack. Its nested editor loop pumps only UI; pumping that same world's transport could reenter ownership-sensitive work. Ordinary gameplay pause already pumps transport without advancing fixed physics or callbacks.

The editor now warns for an already-active online session that remote peers may hit the normal idle deadline; it uses `NetworkIfLoaded`, so offline debugging does not allocate networking. There is no heartbeat thread, distributed pause, timeout retuning or new API.

The extended original NetworkBoundaryTests uses two independent worlds and **real loopback TCP**, not two OS processes. An actual project instruction stop leaves the owner's exact body pose/velocity/clock unchanged. A short supplied-clock stop retains the connection; an 11-second supplied-clock jump while withholding owner transport produces `receive idle timeout`; normal implicit reconnect retains reserved identity. After each resume, the host's actual script call count advances exactly once. All original role/ordinary-pause assertions remain. Final strengthened native check count: **36** (parent final receipt).

The first sandbox run could not publish its listening socket; its 12-check failure is preserved in `network-breakpoints.log`. Native loopback validation outside the network-restricted sandbox passes; this environmental failure is not an engine correction. The separate original NetworkSession suite provides the broader transport/session regressions; this test alone makes no multi-process, WAN or transparent-debugging claim.

## Reproduction

From a Release validation build:

```sh
cmake --build build --target judas_origin_admission_tests judas_origin_environment_tests judas_network_boundary_tests -j2
build/judas_origin_admission_tests
build/judas_origin_environment_tests --bake-regression normalization NEW-normalization-output
build/judas_origin_environment_tests --query-controls knots NEW-query-output
build/judas_origin_environment_tests --bake-regression knots NEW-failed-output
build/judas_network_boundary_tests NEW-network-output
```

The full knots command intentionally returns failure. Paths must be fresh. Real socket tests need loopback permission. The new source-stop test advances the ordinary monotonic clock deterministically; it does not wait eleven wall-clock seconds or measure network performance.

## Preserved boundaries

FLU-001 and the original legacy a106 continuation snapshot remain unchanged. Regional liquid groups and active articulation retain their documented pins; online replication remains the M99 subset. No independent fluid research, protected evidence, game project or public JudasJS semantics changed in this workstream.
