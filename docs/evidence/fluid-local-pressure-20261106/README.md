# Locally constrained pressure reference evidence

Source parent `6930344983fc3c8df7bc5257da7fac062060d3bd`, Experimental.
The containing commit and source checksum manifest identify the selected code.

**Both original authored, frozen-body basins pass one coupled rest interval.**
The new solver is still a slow reference outside native runtime/SDK integration.
Main and the fluid lock remain unchanged. No human validation is claimed.

```sh
bash docs/evidence/fluid-local-pressure-20261106/reproduce.sh /tmp/judas-local-pressure
sha256sum -c docs/evidence/fluid-local-pressure-20261106/SOURCE_SHA256SUMS.txt
```

Requirements: existing GCC/GLM dependencies, Bash and GNU time. The script builds
and runs 299 scoped controls, then the original level/rotated cases at their
unchanged authored timestep, geometry, physical quantities and gas ports. A
failed admission or quantity check returns nonzero. The 180-second process
limit is an experiment budget, not a simulation timestep or quality setting.

`final-basins.log` records all nonlinear iterates, final admissions, local
residuals, independent quantity/initial-remap checks, and phase costs. Both cases
start with zero pressure guesses; neither uses physical substeps or continuation
for its final result. `final-resource.log` includes process RSS and exit status.

Reference cells follow actual aperture splits without optional extra midplanes,
retaining the authored grid spacing. That option remains available to older
prototype callers. No real aperture is closed, no stored gas quantity is
removed, and no original final volume tolerance is loosened.

`linear-exact.log` separately reproduces the earlier failing *linear* systems
on the expanded 150,360/264,800-node fixtures. The condensed solver passes every
original row with outward residual audits. Its wrapper deliberately refuses to
cast the compensated state into old physical output: this is a diagnostic
linear-stage result, not a native fluid step. The diagnostic routing threshold
exists only in the archived probe, not in the selected solver.

All eight control logs are retained. Totals: 14 pressure, 9 directed transport,
6 residual-enclosure, 16 local-coupling, 22 spatial, 50 front, 33 connected-owner
and 149 dynamic checks, with zero failures. Sanitized copies of all logs record
the same 299 checks with address/undefined-behaviour/leak checks enabled.

To reproduce sanitizer qualification:

```sh
ASAN_OPTIONS=detect_leaks=1 JUDAS_FLUID_SANITIZE=1 \
  bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-local-sanitized
```

LeakSanitizer must run outside a tracing wrapper; the initial wrapper-specific
failure is preserved among exploratory logs and was not counted as an engine
pass or failure. Linux GCC uses round-to-nearest, gradual underflow and no
fast-math. Other compiler/platform/floating-point-mode qualification is pending.

`attempts/` preserves raw exploratory logs and rejected/diagnostic source
snapshots. They include failed approximate pressure updates, poor scaling,
zero-flow linearization choices, transport error bounds, capped continuation
and unsuccessful preconditioners. These are historical experiments, not the
selected implementation or additional accepted physical intervals. Some
snapshots predate the final interfaces; no claim is made that every exploratory
binary reproduces identically against the final headers. Final source and raw
selected results have separate hashes.

A pressure precision stress case with conductance `1e10`, source `1e-15` and
row tolerance `1e-29` is retained as explicit numerical rejection coverage.
It needs pressure differences beyond this two-component representation. An
early test driver indexed absent output after that rejection and crashed; the
driver was corrected, and the numerical rejection remains covered. No native
engine crash or physical success is inferred from that experiment.

The two end-to-end timings are single samples, not a performance distribution:
34.96 and 35.99 seconds per reference interval, peak RSS 154,788 KiB. Most cost
is transport and its numerical checks. These costs are unsuitable for games;
this checkpoint is a correctness oracle for a faster candidate.

[Engineering report and limitations](../../FLUID_LOCAL_PRESSURE.md).
