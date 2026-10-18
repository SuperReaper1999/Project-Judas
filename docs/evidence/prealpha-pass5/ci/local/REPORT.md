# Pass 5 — shared SDK inventory CI correction

## Cause and scope

The original check scanned only literal `add_executable(...)` declarations in the
root `CMakeLists.txt`. Pass 3 (`7da56c2c`) extracted four public-tool declarations
into reachable `cmake/targets/*.cmake` modules and converted the Windows readiness
target to the existing `judas_add_engine_test(...)` helper. The old regular
expression consequently reported five missing tools although all five were still
authored. This is a stale verification assumption, not evidence of absent binaries
or five distinct runtime failures.

The exact original local result is preserved in `before/`: 24 tests, one failure,
exit 1, naming `judas_cook`, `judas_script_check`, `judas_save`, `judas_server` and
`judas_windows_readiness_tests`. Its exact script and relevant declarations are
included. Parent-owned remote CI receipts establish which remote jobs share this
cause; this local report makes no independent GitHub rerun claim.

## Correction

Only `scripts/prealpha_packaging_checks.py` changes in this local workstream.
Its inventory scanner now follows reachable repository-local CMake includes from
the root and recognizes literal `add_executable` and `judas_add_engine_test`
targets. It does not accept dynamic `${target}` names, comments or quoted/bracket
text as declarations. It detects missing local `.cmake` includes, handles shared
includes/cycles, and retains the original required-tool and SDK payload checks.

This remains a cross-platform source-declaration check, not a CMake interpreter or
proof that every platform's binary builds. Native configure, build, SDK staging
and executable checks remain independent gates. New CMake declaration helpers or
variable-generated target/include schemes would need deliberate scanner support;
unknown variable targets cannot silently satisfy this inventory.

Two permanent tests add:

- Removal of the real Windows helper declaration must hide that target.
- Removal of the real RenderingAssets include must hide `judas_cook`, even though
  its module still exists on disk.
- A small include fixture covers multiline/quoted arguments, comments, quoted and
  bracket text, dynamic helper names, include cycles and missing local includes.

The parent separately changes CI to execute the packaged SDK inspector rather
than its source-tree copy, retaining all host/glibc/OS checks and the subsequent
strict SDK verification from `/tmp`. That workflow diff was reviewed; no gate is
removed or skipped. Workflow source and remote-job evidence are parent-owned.

## Results

| Check | Result |
| --- | --- |
| Original unchanged packaging suite | 24 tests; 1 preserved failure; exit 1 |
| Corrected packaging suite | 26 tests; 0 failures/errors; exit 0 |
| Existing actual CMake helper/module contracts | 12 tests; 0 failures/errors; exit 0 |
| Real inventory assertion, missing helper | Exactly 1 expected failure naming the removed Windows tool |
| Real inventory assertion, module made unreachable | Exactly 1 expected failure naming `judas_cook` |

The two adversarial executions invoke the actual production inventory assertion
with read-only source overlays, not a replacement assertion. Their outer runner
succeeds only when each inner assertion fails for the expected missing tool. No
repository CMake file is modified, and a stale orphan module cannot satisfy the
check. `negative-controls/` preserves each inner failure verbatim.

No compiler, native engine build, physics test, benchmark, VM workload or full
production suite was run for this correction. Existing passing checks are not
claimed as new platform qualification. No commit or push was performed by this
subagent; the parent reviews and checkpoints the combined CI change.

## Reproduction

From the source checkout, with Python 3:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 scripts/prealpha_packaging_checks.py --output /tmp/judas-packaging-result.json
PYTHONDONTWRITEBYTECODE=1 python3 tests/test_cmake_test_targets.py
PYTHONDONTWRITEBYTECODE=1 python3 docs/evidence/prealpha-pass5/ci/local/reproduction/negative_controls.py --source . --output /tmp/judas-inventory-negative-controls
```

The first two commands require exit 0. The final command also requires exit 0,
while its two preserved inner test logs must each contain one expected failure.
The CMake contract test invokes `cmake -P` with mock target commands, without
configuring or compiling native targets. `after/RECEIPTS.json` records actual
commands, times and exit codes. `SOURCE_FINGERPRINTS.json` ties the result to the
final verification source and unchanged inventory/build declarations.
