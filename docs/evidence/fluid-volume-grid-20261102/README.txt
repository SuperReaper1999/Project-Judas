Bounded volume-grid water prototype — 2 November 2026
Standalone source/commands: tools/prototypes/fluid_volume_grid/README.txt.
Parent source checkpoint: 35db07d0742ac27138d9f6f9651ff7c384d519ad.
This prototype has no engine dependencies or runtime integration. Main, releases,
the fluid lock, Basic Water, M54/M55 and independent research remain untouched.
Concurrent production contact work is separate and not certified by this archive.

raw-evidence.tar.xz contains source variants, predeclared scope, raw successful
and failed runs, timing rows, compiler/hardware metadata and hashes. Every member
was readback-verified against MEMBERS_SHA256.json. ELF binaries/objects/libraries
are excluded; their exact hashes/provenance remain in the original manifests.

Read .cache/fluid-volume-grid-20261102/README.txt and FINAL_SCOPE.txt first.
Important artifacts:
- GATES.md and first-source.cpp: initial pressure/volume-only experiment.
- pressure-volume-only.cpp: strengthened initial model, before momentum transport.
- momentum-failed.cpp / momentum-failed-line.log / momentum-diagnosis.log:
  a divergent closed-box test input and a rejected-output harness crash, preserved.
- naive-digest.cpp / naive-digest-gates.log: actual nonlinear mass-consistent
  momentum transport, naive pressure operator and complete state digests.
- timed-cached-source.cpp / cached-edge-gates.log: weighted-edge operator cache,
  unchanged equations/tolerance. All57 gate observations/digests are bit-identical.
- bench-r1/r2.csv versus cached-bench-r1/r2.csv:60 consecutive updates per size.
  2304-cell update median9.505 ->3.981 ms;18432-cell156.931 ->65.627 ms.
- COMPARISON.json: matching non-timing observations and complete raw summaries.
- ROOT_PIPELINE_TOTALS.json: per-row update+mesh+1000-query sum, median4.318 ms
  coarse/67.633 ms finer; no GPU rendering or real moving-solid coupling included.
- TIMED_CACHED_PROVENANCE.json: exact timed source/binary; PROVENANCE.json and
  CLI_ONLY_CHANGE.patch distinguish the later delivered CLI validation guard.
- cli-guard-gates.log / CLI_CHECKS.json: delivered source still57/57;12 invalid
  CLI cases reject before allocation and2 positive smokes pass. Refer to the
  actual member inventory for authoritative filenames.

DO NOT interpret this as a PBF speedup. Static aligned masks cannot represent
the original production stairs, moving/rotating bodies, cups or thin gaps.
Hydrostatic traction is a static patch, not general moving-body coupling. The
rotation check transforms output algebraically; actual oblique gravity INPUT is
tested separately. Fine-grid pressure is still too expensive for replacement.
No persistence, origin, streaming, export or other-platform support is claimed.

To inspect all evidence without altering any existing cache:
  mkdir -p /tmp/JudasVolumeGridEvidence
  tar -xf docs/evidence/fluid-volume-grid-20261102/raw-evidence.tar.xz \
    -C /tmp/JudasVolumeGridEvidence
Build/run the tracked standalone source using its README. The archived README
contains naive/cached comparison commands and preserves the failures.
