Settling acceptance-cause investigation, 2026-11-01

Original production gates remain unchanged and failed. No new damping, engine
force correction, fluid unlock or main promotion. See ../../FLUID_SETTLING_CAUSE_REVIEW.md.

FINAL_PROVENANCE.json records source/binary hashes. The normal CMake target
judas_fluid_settling_reference_tests needs no cache or external tooling. It uses
three recorded parameter fixtures and an independent continuous reference, not
the engine hydrostatic sampler. It does not replace the original failing gates.

TRACE_RUNS.json and TRACE_ANALYSIS.json cover original four-second tests.
CONTINUATION_RUNS.json and CONTINUATION_ANALYSIS.json cover observations taken
after every original assertion has already executed. All original statuses are
preserved. ContinuedOriginalGates.cpp.gz documents this; it is not a delayed
acceptance threshold. CalmReference.cpp.gz and calm-reference.csv.gz contain
raw high-resolution and native convergence results.

Gzip members preserve exact original bytes. SHA256SUMS.json excludes only itself.
The baseline sources are pinned to 90a97547. Diagnostic trace hooks exist ONLY
in TraceCoupling.cpp.gz, never in production source. Native class layout stayed
identical in the baseline/current substitution. Traced physical rows were proved
identical to uninstrumented results before drawing conclusions.

Scratch link/run scripts expect repository/.cache/fluid-settling-cause-20261101
and its existing Linux Makefiles Release build; restore that layout if replaying
the exact diagnostic commands. The original baseline object path references the
prior vertical-geometry cache. Baseline source copies are included here, so it
can be rebuilt without relying on that old object. No independent research is
required or was modified. Disposable diagnostic executable hashes were recorded
before removing those rebuildable copies; sources, objects and raw data remain.

The first analyze_trace invocation accidentally compared every baseline case
to the light-body file; it stopped at an assertion. The corrected script and
JSON compare each matching case and all physical rows match. That setup error
is not a numerical failure. No Windows/Android or human qualification claimed.
