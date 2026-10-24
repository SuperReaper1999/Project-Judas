# Current Pre-Alpha status

Judas remains in unnumbered Pre-Alpha stabilization. **M100 and Alpha release work
have not begun.** Main is the human-accepted release line; Experimental may contain
verified corrections awaiting their own human acceptance.

## Accepted baseline

The operator accepted MK2, including MK1 removal, at
`c859901cc9a52cf5b7104887f56b5e7ad4269b1d`. Project Settings/basic water were also
human-tested before integration. MK2 is the sole production solver, including
coupled work. See [the current contract](MK2_PHYSICS_ARCHITECTURE.md).

## Current correction work

The [final Pre-Alpha corrections](PREALPHA_FINAL_CORRECTIONS.md) address disabled-
fluid admission, streaming/export consistency, project-document protection,
explicit saved-content export, current validation and editor onboarding.
Experimental pushes are automated qualification, not a new human acceptance.
The tracked [finding register](PREALPHA_FINDINGS.md) distinguishes remaining limits.

Start with [your first project](FIRST_PROJECT.md), [the editor guide](EDITOR_GUIDE.md),
[Project Settings/water](PROJECT_SETTINGS_AND_WATER.md), [JudasJS](JUDASJS.md),
[architecture](ARCHITECTURE.md) and [distribution](DISTRIBUTION.md).

## Remaining readiness concerns

- Selected Linux native/editor/game checks do not certify every engine feature.
- Current MK2 Windows/Android/hardware combinations require their own qualification;
  historical platform receipts apply to their recorded source. Windows VM timing
  remains diagnostic, not a physical-host performance gate.
- Advanced fluid dynamics remain default-off and experimental. FLU-001 and the
  retained half-turn sampling issue are not silently reclassified as passes.
- Replication, physical charts, regional suspension, terrain edits and migrations
  retain explicit supported subsets. No universal compatibility promise is made.
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

## Ownership and integration contracts

MK2 is the sole rigid solver. Public components own their physical representation;
project JS owns gameplay policy. Save/load and streaming stage replacements before
publication, and reject unsupported composition rather than discard state. Basic
water is independent of the default-off advanced fluid policy. Origin shifting
uses one bounded physical chart and participant preflight. Networking supports an
explicit authored-root subset; it is not universal physics replication. Read the
[current architecture](ARCHITECTURE.md) and subsystem references for exact limits.
