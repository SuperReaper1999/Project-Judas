# Pre-Alpha Windows portability follow-up

Baseline: `c62cfc6c1f3ad4ddf772e02e7c2c574dc53bf3d6`. Development is on
Experimental; main is not promoted by this work. This is not M100 or release
qualification. The operator's Windows 11 summary (52/59 checks and a separate
32/32) is an external handoff without its original logs. New reproductions here
use Linux and the existing **Windows 10 VM**, not Windows 11 hardware.

## Subsequent operator acceptance

On 25 October 2026 the operator accepted this exact correction checkpoint on
Windows 11 hardware. Native receipts and the explicitly tested project/save/game/
audio scope are preserved in the [acceptance record](evidence/windows11-acceptance-20261025/OPERATOR_ACCEPTANCE.json). The original
VM findings below remain historical evidence; controller and clean-machine
certification are not inferred from that later acceptance.

## Corrections and causes

| Reported finding | Mechanism and correction |
| --- | --- |
| Liquid rotation monotonicity | Reproduced at the original 65th check with binary64 intermediates. De Casteljau's `a + t*(b-a)` can round one ULP beyond a Bernstein endpoint when `long double` is binary64, as on MSVC. Use monotone C++17 lerp arithmetic in storage, area and energy-primitive evaluation. Positive storage controls, geometry, tolerances and solver budgets remain unchanged. This bounds interpolation rounding, not the result of a liquid solve. |
| Surface serialization roundtrips | The test writes `/tmp/m55-surface.judasbasin`; that is not Windows' temporary directory. Use a unique file in `std::filesystem::temp_directory_path()` and remove it after loading. The original serialization assertions remain. |
| Transport capacity and refusal | The fixtures assume immediate server-side TCP admission and advance a refusal clock much faster than the real OS. Wait for actual admission within bounded real time, without spending rate/idle simulation time; admit the first client before connecting the second so kernel backlog rejection is not confused with Judas' peer-capacity rejection. Use real elapsed monotonic time for OS connection refusal. Retain framing, FIFO, rate, capacity, timeout and diagnostic assertions. No transport implementation change. |
| MK2 stack overflow | A 1 MiB Linux reproduction and backtrace identify recursive `CCDQuery::ImpactTime` suffix searches, retaining a full frame per trajectory leaf. Replace the suffix recursion with a chronological walk over both trajectories' knot union. Keep the original absolute penetration target, mesh episode, earliest-hit ordering and leaf searches. Do not increase stack reserve, reduce trajectory resolution or alter CCD/solver budgets. |
| Preferences cleanup | Close the backup `std::ifstream` after reading, before removing its directory. Assertions already pass; Windows correctly refuses deletion while that handle remains open. |
| Build-organization encoding | Read/write source, fixture and trace text explicitly as UTF-8; decode captured CMake output explicitly. No dependency on `PYTHONUTF8=1`. Original 18 positive/negative tests remain. |
| Gate registration | Discover with `ctest -C Release`, compare path arguments as paths rather than slash spellings, and add an explicit Ninja Multi-Config control. Existing absent-target and removed-known-failure-guard negative controls remain. |

## Final focused results

| Check | Native Windows 10 VM | Linux |
| --- | --- | --- |
| MK2 rotation | 31,404 checks; default 1 MiB stack | 31,404 checks with 1 MiB stack |
| Curved/complex mesh contacts | 372 checks | 372 checks |
| MK2 physics and genuine cold rotation restoration | Pass; cold reader 3,139 checks | Pass; cold reader 3,139 checks |
| Liquid rotation | 419 checks / 37 orientations | 419 checks / 37 orientations |
| Surface geometry/flow/roundtrip | 61 checks | 61 checks |
| Preferences including successful cleanup | 35 checks | 35 checks |
| Real TCP transport | 70 checks; five further runs pass | 70 checks |
| Build organization | 18 tests with `PYTHONUTF8=0` | 18 tests |
| Gate registration | Five tests, including Ninja Multi-Config | Four pass; explicit Ninja control skipped (not installed) |

All thirteen selected Linux suites pass. Additional Linux coverage includes
joints (29 checks), motion bounds (470,408), impact robustness (4,412),
conserved reservoirs (67) and actual flat/radial drain/refill (34). No full
production suite is rerun. Native execution uses MSVC v143 Release x64,
Windows 10 Home build 19045, Python 3.13.16 and CMake 3.31.6 in the existing VM.
These timings are not hardware performance qualification or speedup claims.

The matched Linux comparison links the **same final test objects** with either
the preserved original CCD object or the corrected implementation. Complete
observable output is byte-identical, including all 31,404 checks. The corrected
run uses the 1 MiB stack that faults in the original implementation. An earlier
prebuilt binary had six fewer harness assertions; that preliminary comparison
is not used as the final matched oracle.

An intermediate native overlay had matching source hashes but stale test objects:
ZIP timestamps predated the baseline build. The actual compile log proved that
MSVC had not recompiled the two edited test files. That failed attempt is retained;
changed files are then forced to rebuild before the final successful retest.
The final source proof matches all seven changed C++/Python files, and the tested
rotation executable's PE header still reserves **1,048,576 bytes**, not 8 MiB.

## Important distinctions

The native default-stack profiler-initialization control passes: large profiler
buffers were a rejected hypothesis, not the cause of the MK2 failure. GCC reports
a 2,080-byte `ImpactTime` frame in the preserved build. A chain of per-leaf suffix
calls exhausts a small stack; an individual contact primitive appearing at the
fault address does not establish that the primitive owns the excessive stack.

The original liquid diagnostic records a decrease from
`1.2142796644368102e-17` to `1.2142796644368101e-17` at an interval endpoint.
The positive cubic controls are `0, 0, 3.9030417785468886e-18,
1.2142796644368101e-17`. This is an interpolation-rounding defect, not evidence
that the M54 quantity ledger created or lost water. No fluid implementation or
research outside that evaluation path is redesigned.

The Windows VM originally reproduced **nine** transport assertions failing, not
necessarily the same scheduling as the external Windows 11 run. An intermediate
admission-wait-only attempt still failed capacity; it exposed the separate
backlog assumption. The final fixture explicitly proves both raw sockets connect
and that Judas rejects excess admission after its peer table is full.

## Validation record

Final results, source/binary fingerprints, raw logs, reproduction commands and
preserved failures are recorded in
[evidence](evidence/windows-portability-followup/REPORT.json). Consult that record
for actual platform results, skipped checks and any remaining failures; this
report does not infer a full Windows release qualification from selected suites.

The native build uses a new `P:\JudasValidation\WindowsSeven` directory and reuses
installed pinned SDL/GLM dependencies. An initial setup omitted the matching
`VCPKG_INSTALLED_DIR`; runtime-license staging failed despite compilation. Its
failure is retained, then the actual dependency prefix is supplied. This is a
qualification-setup correction, not a change to dependency versions or packaging
policy. The no-Git source snapshot has no fabricated frozen provenance manifest;
archive and individual source hashes establish the tested correspondence.

The operator enabled AMD SVM and loaded `kvm_amd` after the host CPU change.
The old VM's Intel-only Hyper-V `evmcs` option then prevented startup. Its original
XML is preserved locally and only that incompatible option is removed. Guest
disks, memory and other configuration are retained. This does not qualify
physical GPU, controller, audio, Windows 11 or clean-machine distribution.

## Boundaries and human follow-up

- Recheck the eventual Experimental commit on Windows 11 using the ordinary
  native MSVC workflow in [Windows support](WINDOWS.md), without a larger-stack
  executable or UTF-8 environment workaround.
- Check editor theme/layout save, close and reopen, and a representative game's
  physical interactions, sound/input and cold save on actual hardware.
- FLU-001 and the separate half-turn disturbed-surface origin-query limitation
  remain deferred. Their thresholds, protected evidence and independent fluid
  research are unchanged.
- No public JudasJS API changes, automatic main promotion, release-channel
  changes, tags or M100 work are included.
