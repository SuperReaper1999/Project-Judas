# MK2 rotation correction evidence

See [report](../../PREALPHA_MK2_ROTATION_CORRECTION.md) for mechanisms, limits,
reproduction commands and human checklist. Input revision:
`bded63dfdaefdeec6484d66b3211712c200c2941`; Linux-only automated qualification.

## Inventory and provenance

- `TESTED_FINGERPRINTS.json`: hashes of the 17 changed compiled/build/test inputs,
  30 completed native/application job binary receipts, runtime/engine archive and
  original retained failing executable. Those on-disk binaries were checked again
  when this evidence was assembled. Binaries are not included.
- `tested-source.tar.gz`: complete copies of all 17 changed inputs, including the
  then-untracked helper and two tests. `tested-source.patch` is the tracked diff
  against the input revision. Apply the patch and extract the complete files at
  repository root; the archive includes the new files omitted from the diff.
- `BUILD_INFO_TESTED30.json`: genuine dirty development-build identity. It is not
  rewritten to pretend that tests used a subsequent clean commit binary.
- `qualified30/`: all 22 suite results and raw logs; independent energy traces,
  original native fixture/checker, 128 mixed-spin cases and 60-second controls.
  Original retained envelope rerun reproduces 31.845% exactly. The frozen-archive
  replay differs in the moderate multi-step trace; both remain visible.
- `application30/`: eight real native application/export results and relocation
  receipt. Actual game data/build outputs are excluded. Dummy audio was used.
- `performance-matched30.tar.gz`: complete raw final ABBA benchmark directories,
  profiles, step CSVs, physical endpoints, summaries and environment. Allocation
  interception is disabled in **both** binaries. Readable aggregate copies are
  `performance-results.json`, `performance-environment.json`,
  `PERFORMANCE_SUMMARY.json`. `scaling-baseline-production.json` records actual
  archived-source compile/link provenance; the two drivers are byte-identical.
- `experiments.tar.gz`: available probe sources, scripts, logs, patches, helper
  snapshots and intermediate raw results, including rejected approaches. The
  archive contains no binaries/objects/libraries. Most scripts refer to original
  local scratch paths; they preserve commands, not a packaged build environment.
- `preserved-failures/`: exact failed intermediate M77 snapshot plus failure/query
  diagnostics, and the late-wake failure/corrected control. Original historical
  failures outside this new directory remain untouched.
- `build-final30.log`, validation registration and build-organization logs.
- `CHANGED_FILES.txt`: exact checkpoint inventory, including documentation/evidence.
- `FINAL_FINGERPRINTS.json` / `verify.py`: final intended-file byte verification.
  The manifest excludes itself to avoid a recursive hash. Run
  `python3 docs/evidence/prealpha-mk2-rotation-correction/verify.py` from any cwd.

## Rejected experiments and follow-ups

| Stage | Observed problem | Disposition |
|---|---|---|
| 1 | Replacing rotation alone left a nonphysical stopped elastic response | rejected |
| 3 | Thin-wall crossings remained | rejected |
| 5–6 | Adaptive knots accumulated translation rounding and disturbed rotated stacks | common episode origin; accept verified whole leaves |
| 11 | Energy passed while mesh/timing checks failed | energy alone is not acceptance |
| 12–14 | Geometric projection interpreted as physical momentum; same-instant slot exhaustion | separate channels; chronological arrival accounting |
| 15 | Changed mesh edge policy did not fix the mechanism | reverted; no final CollisionGeometry change |
| 16–19 | Depth allowance no longer restarted, but slider froze with repeated events | rejected despite passing containment; not a timeout |
| 20–21 | Support/timing restored; bounded sphere tangent needed independent obstacle proof | ordinary support retained; certified tangent only |
| 22–23 | Repeated support observations overflowed the unchanged 32-contact query budget | preserve failure; coalesce full reaction, no budget raise |
| 27–28 | CCD-woken body missed final integration interval | analytic asleep/awake/absent controls; join next boundary |
| 28–30 | All positive-restitution contacts deferred even below actual bounce cutoff | conservative effective-restitution bound; same law/threshold |
| performance28 / performance30 | Baseline allocation hooks on, candidate hooks off | **invalid as matched timing comparisons**, raw data retained |
| threshold-control | Long-run e=1 dissipation investigated with diagnostic cutoff zero | counterfactual only; production cutoff unchanged |

Not all intermediate patches contain then-untracked helpers. Available snapshots
are retained; intermediate builds are not all independently reconstructable.
The final source archive **is complete** for the changed inputs.

## Scope

No human acceptance, universal no-energy-error theorem, unbounded CCD guarantee,
Windows/Android test, clean-machine SDK test, or universal stress-lab FPS claim is
inferred. Original accepted research and source projects are unchanged.
