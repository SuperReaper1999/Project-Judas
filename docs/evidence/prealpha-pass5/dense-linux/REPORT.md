# Production candidate-list qualification

This is a verified **partial Pass 5 checkpoint**, not Pass 5 completion or human
acceptance. Main remains at `4c141e9bb6c68a50c63dec263dcec398200328ad`.
Curved-mesh policy and fluid implementation are unchanged.

## Source and build

The narrow production change adds a private, bounded candidate-list helper and
two event-loop call sites. Normal response filtering, native geometry, primitive
expansion, event ordering and solver settings remain in use. The new 17-check
fixture is registered in the ordinary collision validation group.

The native Linux Release build passes without new warnings. Comparing all 170
engine archive members against the frozen accepted archive finds exactly one
changed member: `WorldImpactEvents.cpp.o`. The candidate archive is SHA256
`0e0dc7973b7d2b24510743d5297e9ae002cdb7b4dbdc943d55a4bdc4aab78514`.
See [archive-member proof](dense-linux/ARCHIVE_MEMBER_PROOF.json) and the exact
source, compiler, command and binary receipts in [RESULTS.json](dense-linux/RESULTS.json).
The build retains its actual dirty-development identity and disclosed pre-existing
untracked source-like inputs; this is not clean-release qualification.

## Focused physical checks

All eleven fixtures pass: candidate cache (17), impact timing (3197), contact
cache (13327), kinematic (96), native continuation (112), contact lifecycle
(1076), CharacterMotor (32), joints (29), finite particle contacts (77), rigid
motion (10239), and impact robustness (4412). These are 32614 assertions, not
32614 independently certified engine capabilities. Deliberately withholding a
global tree revision fails the candidate test, as intended.

The unchanged original **unfiltered six-scenario** mesh fixture records
**372 checks / four failures** on both the frozen baseline and production cache.
The initial-arrival variant passes all 372. The separate curved-only research
invocation uses 193 checks; those counts are not interchangeable. Neither known
failure is counted as an acceptance pass. [Original and follow-up commands](dense-production/RESULTS.json).

The actual production build repeats the original 270-step room and four scaling
controls. Every recorded non-timing physical field, digest and endpoint agrees
with the frozen baseline. The 1024-loose-body endpoint retains 2475 contacts,
7.2171085 mm deepest penetration, zero uncertain/unsupported pairs and digest
`192f0f4bed728953`. [Exact comparison](PRODUCTION_COMPARISON.json).

## Performance scope

The earlier [matched evidence](../dense/REPORT.md) retains three baseline/candidate
pairs, including the candidate's 1957.994 ms worst step. It establishes lower
p95 cost, **not** lower worst-case latency or a real-time guarantee.

This additional final-production-source replay reports median **24.794418 ms**,
p95 **344.380387 ms**, maximum **1679.343544 ms**, and **50.516240 s** wall time.
It is a source qualification replay, not another matched speedup comparison.
All original workloads, object counts, timestep, profiler settings and quality
checks are retained. The severe dense-contact tail remains open.

## Reproduction and preservation

The two scripts in `reproduction/` are the exact runners executed from
`.cache/prealpha-pass5/root-plan/`, using the repository as their working
directory. `check_dense.py` configures/builds and runs the focused fixtures;
`run_production.py` runs preserved failing/arrival fixtures and the five original
workloads. They refuse existing output directories. Reproduce the baseline from
the accepted 4c checkpoint and preserve its executables before building the
candidate. Baseline binary receipts and unchanged inputs are in the sibling
[dense evidence](../dense/REPORT.md).

Large profiler JSON payloads are losslessly compressed. Each `.gz.identity.json`
records the original decoded byte count and SHA256; decompress to recover the
exact original output. No executable, build archive, generated project, SDK or
temporary export is included here. Windows and combined application qualification
remain separate follow-up work at this checkpoint.
