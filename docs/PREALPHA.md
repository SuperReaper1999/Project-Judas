# Current Pre-Alpha status

Judas remains in unnumbered Pre-Alpha stabilization. **M100 and Alpha release work
have not begun.** Main is the human-accepted release line; Experimental may contain
verified corrections awaiting their own human acceptance.

## Accepted baseline

The current human-accepted engine checkpoint is
`c76e92dc20b5765f61d6077bf618a8b94ac22462`, including the rotation/contact correction.
MK2 is the sole production solver, including coupled work. Main and Experimental
were aligned at that acceptance; subsequent qualification/documentation work is
Experimental until separately accepted. See [the current contract](MK2_PHYSICS_ARCHITECTURE.md).

## Current correction work

The [final Pre-Alpha corrections](PREALPHA_FINAL_CORRECTIONS.md) address disabled-
fluid admission, streaming/export consistency, project-document protection,
explicit saved-content export, current validation and editor onboarding.
The follow-up [Linux readiness report](PREALPHA_LINUX_READINESS.md) records MK2
diagnostics, fresh SDK/tool qualification and cross-system tests. Experimental
pushes are automated qualification, not a new human acceptance.
The tracked [finding register](PREALPHA_FINDINGS.md) distinguishes remaining limits.
The [final bounded qualification](PREALPHA_FINAL_QUALIFICATION.md) supersedes old
pending status, with the [intended Alpha support matrix](ALPHA_SUPPORT_MATRIX.md).
Prior reports retain their checkpoint-time failures and platform claims.
Final qualification found and corrected a free-body external-kick timing regression
that disturbed wheel suspension. This new Experimental correction has automated
Linux coverage and awaits human acceptance; it is not part of accepted main yet.

Start with [your first project](FIRST_PROJECT.md), [the editor guide](EDITOR_GUIDE.md),
[Project Settings/water](PROJECT_SETTINGS_AND_WATER.md), [JudasJS](JUDASJS.md),
[architecture](ARCHITECTURE.md) and [distribution](DISTRIBUTION.md).

## Remaining readiness concerns

- Selected Linux native/editor/game checks do not certify every engine feature.
- Windows/Android qualification is explicitly deferred by the operator;
  historical platform receipts apply to their recorded source. Windows VM timing
  remains diagnostic, not a physical-host performance gate.
- Advanced fluid dynamics remain default-off and experimental. FLU-001 and the
  retained half-turn sampling issue are not silently reclassified as passes.
- Replication, physical charts, regional suspension, terrain edits and migrations
  retain explicit supported subsets. No universal compatibility promise is made.
- The reproduced elastic energy-gain defect is corrected and human accepted.
  Long-run dissipation, extreme-spin phase error and bounded arrivals remain
  documented in the [correction report](PREALPHA_MK2_ROTATION_CORRECTION.md).
- Dense MK2 performance is workload-dependent. Do not apply the retired MK1
  event-cascade diagnosis or old pending benchmark status to current MK2.

## Historical evidence

[Original assessment](evidence/prealpha-pass2/ORIGINAL_ASSESSMENT.md),
[Pass 1](evidence/prealpha/REPORT.md), [Pass 2](evidence/prealpha-pass2/REPORT.md),
[Pass 3](evidence/prealpha-pass3/REPORT.md), [Pass 4](evidence/prealpha-pass4/REPORT.md)
and [Pass 5](evidence/prealpha-pass5/REPORT.md) retain exact checkpoint-time claims.
The [MK2 development archive](MK2_PHYSICS_DEVELOPMENT.md) retains its original bytes.
The [MK1 event investigation](PREALPHA_EVENT_DEPENDENCIES.md) is historical and is
outside the current-reference link gate. Removed source remains available in Git
at those recorded revisions; no retired implementation is reinstated for links.

## Focused validation from a clean checkout

Configure with `-DJUDAS_ENABLE_VALIDATION=ON`; normal runtime builds need no
validation tooling. Build the declared prerequisites before running CTest:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_prealpha_tests -j2
ctest --test-dir build -L prealpha --output-on-failure
```

Application cases need a working native GL display or `xvfb-run -a` on headless
Linux. The manifest's known-failure case is opt-in, not counted as a pass. Every
run records provenance and fresh output; the gate never implicitly builds stale
binaries. See `scripts/prealpha_validate.py --help` for selecting bounded cases.

`prealpha.build-organization` checks required build relationships rather than
requiring the current dependency graph to equal a historical snapshot. The
preserved extraction fixture supplies minimum sources, owners, definitions and
platform boundaries. Additional sources/libraries/targets and normal link-call
reordering are allowed. Direct ImGui includes also require an implementation
link to `judas_imgui`; this requirement is derived from the compiled sources.
Negative controls deliberately break these contracts. Run this fast check after
changing test/build wiring:

```sh
python3 -m unittest discover -s tests -p test_cmake_test_targets.py -v
```

This declaration check does not replace real configuration, compilation and
linking. Its simulated Windows/Android guards are not native platform passes.
Removing a retained requirement intentionally still needs a reviewed test change;
do not regenerate historical fixtures merely to silence a difference.

## Ownership and integration contracts

MK2 is the sole rigid solver. Public components own their physical representation;
project JS owns gameplay policy. Save/load and streaming stage replacements before
publication, and reject unsupported composition rather than discard state. Basic
water is independent of the default-off advanced fluid policy. Origin shifting
uses one bounded physical chart and participant preflight. Networking supports an
explicit authored-root subset; it is not universal physics replication. Read the
[current architecture](ARCHITECTURE.md) and subsystem references for exact limits.
