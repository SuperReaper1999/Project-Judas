# Scripting V2 accepted-source baseline

Native source revision: `1094d64515604fe9d03fdab479efc7f880b112a8`.
Source archive SHA-256: `ceb60999dc78206525058778cc361fe7497a77ef20db1c76b7369b96cbd58793`.
Source fingerprint: `281aec8a6d86a3bef5df04a3a69bc4f55dbd418202f8cba0713993303731d9ff`.
Archive profile: engine; the six consumer fixture projects were restored from the exact same committed tree with `git archive`.
Native code and existing test source were not edited in the baseline extraction.

Configuration: native Linux x86_64, GNU 15.2.0, Release, validation enabled,
Experimental channel. `results/BUILD_INFO.json` records archive-clean provenance.
Rendering integration used SDL offscreen with Mesa llvmpipe; audio used the dummy
backend. This establishes automated developer workflow behaviour, not human
visual acceptance, hardware rendering performance or real audio validation.

## Runs

All seven focused executables passed: **291 checks, zero failures**.
Exact commands, binary hashes, elapsed times and emitted samples are in
`results/RESULTS.json`; per-suite raw logs are alongside it.

| Suite | Checks | Failures |
|---|---:|---:|
| Core scripting | 59 | 0 |
| Developer debugger/reload | 49 | 0 |
| Physical/presentation integration | 44 | 0 |
| Editor source/debug tools | 23 | 0 |
| Editor script creation/attachment | 70 | 0 |
| Existing consumer applications | 37 | 0 |
| Live JudasJS surface | 9 | 0 |

Run `python3 .cache/scripting-v2/baseline/run.py` to repeat the frozen baseline.
The one-pass timings are diagnostic baseline observations, not statistically
matched before/after performance claims.

The consumer tests run real applications, including shooter_game, boundary_demo,
asset_lab, streamed_range and developer_lab. They cover debugger source locations,
JSON-state reload, slot-owned versus independent UI/streaming lifetimes,
busy asynchronous save rejection, retained physical bodies and a deliberate
edit-debug-fix-reload cycle.

`STATE` and `POSE` hashes are emitted-observation digests. They are not complete
physical trajectory digests. The presentation test independently verifies that
presentation callbacks preserve actual authoritative pose and velocity.

## Local Audio acceptance negative control

`tests/ScriptModuleAcceptanceTests.cpp` exercises the normal public ScriptSystem
path, then reads actual native emitter/camera state. Compiling that test against
the frozen baseline libraries gives **8 checks, exactly 1 expected failure**:
unknown Audio options currently succeed and mutate fields instead of rejecting
the patch atomically. The valid-setting, invalid-known-value, nested cross-module
accessor and teardown controls all pass.

`negative_local_audio.py` reproduces the compile/link/run without altering frozen
native source. `results/local-audio-negative-commands.json` and
`results/local-audio-negative.log` retain exact commands and raw outcome.

Only this negative control is intentionally failing. Its expected result must
not be counted as a passing implementation test.
