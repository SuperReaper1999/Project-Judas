# Dense scalability evidence — 2026-10-19

## Qualification

This directory preserves the new scenario-3 investigation. The current combined **fast-path** production candidate is a physically unchanged, Linux-focused improvement. **The requested 24–30 FPS goal remains failed.** Continuous-motion CCD A1/A3/A4 were evaluated and rejected; no numerical retiming correction from them is merged.

The report is [PREALPHA_DENSE_SCALABILITY.md](../../PREALPHA_DENSE_SCALABILITY.md). No historical report has been edited to convert an old failure into a pass.

## Read first

- `FINAL_FASTPATH_COMPARE_A.json`: exact 270-step baseline/candidate physical and work comparison.
- `FINAL_FASTPATH_UNPROFILED_COMPARE_A.json` and `runs/{baseline-unprofiled-A1,combined-fastpaths-unprofiled-A1}`: separate native 270-step pair with profiling explicitly disabled. Physical/work/endpoints match exactly. These single-pair observations are not a statistical significance or FPS claim.
- `runs/baseline-A2` and `runs/combined-fastpaths-A1`: raw step/frame/begin CSVs, endpoint geometry, commands, exits, resource usage and result summaries.
- `runs/baseline-realtime-A1` and `runs/combined-fastpaths-realtime-A1`: real application cadence. Different physical horizons and wall overshoot prohibit an aggregate FPS speedup claim.
- `combined-fastpath-checks/RESULTS.json`: all 27 invocations, actual/expected exits, logs and durations. Three expected-failure controls are included.
- `snapshots/baseline-e30` and `snapshots/candidate-fastpaths-final`: exact source archives, complete recorded manifests, build identities/configuration. Executables and static libraries are deliberately excluded; original hashes remain in the manifests.
- `sphere-adoption/ADOPTION.json`: equivalence of the production certificate/helper/sphere functions to the tested isolated prototype, excluding comments/whitespace. It records an intermediate source identity, not a future final-source freeze.
- `MEASUREMENT_HELPER_CHECKS.json`: ten synthetic measurement-accounting checks. These are not engine physics or FPS results.
- `PORTABLE_HELPER_TEST_RESULTS.json` and `PORTABLE_HELPER_TESTS.log`: eleven newly executed tests of the portable helper. They include changed-binary/fixture rejection and verified relocated-source archives. The original passing run's resource warnings are retained separately; CSV handles are now explicitly closed.
- `raw-compressed/contact-system-research/event-3470.json.gz`: full exact-bit 969-body / 2,379-row contact event, sufficient to replay the actual coupled solver input.
- `raw-compressed/event-topology-observer-v2/pair-trace.jsonl.gz`: full recorded pair/time observations used to investigate temporal query continuity.
- `RESIDUAL_ALL_ROWS_SUMMARY.jsonl.gz`: numerical summary for every one of the 3,520 impact observations. `raw-compressed/residual-observations-selected.jsonl.gz` preserves unchanged original lines for all 535 large-contact observations plus early controls (567 records total). Selection and original full-file identity are explicit in `COMPRESSED_RECORDS.json`.
- `raw-compressed/runs`: complete retained profiler output for both matched exact runs and the candidate real-time run. The profiler itself retains bounded history; this is not every application frame.

## Numerical observations and rejected approaches

`experiments` preserves small source files, build receipts, proof notes and outcomes for predicate filtering, support classification, residual observation, floor/energy observation, contact-system replay, manifold blocks, Schur/Anderson trials, solver ordering, retained support guesses, isotropic inertia and parallel queries. Raw run records for rejected experiments remain under `runs`.

Prototype `PROTOTYPE.json`/`PREPARED.json` files describe their as-created state. Later build/run receipts describe execution. A successful compile or direct control is not a successful live experiment.

`experiments/continuous-toi/snapshot/isolated-overrides.tar.gz` preserves A1's exact override bytes; later work is not substituted for that failure. `A1_LIMITATIONS.txt` records why the observed A1 result cannot certify CCD. `experiments/continuous-toi-A3` and `continuous-toi-A4` preserve separate sources, replay outputs, helper controls and exact override archives. A3 reaches the original eight-small-advance heuristic; A4 reaches the existing 64-advance cap. Both retry the literal native search and leave all 18 returned query times unchanged. They are rejected, with no full-world A4 run or production adoption. Further coherent geometry research remains isolated and unadopted.

Large essential observations and representative profiler captures are stored losslessly compressed and verified byte-for-byte after decompression. Some larger experiment tables have a `.json.gz` suffix. Full 240,026-row response-output equivalence is retained by its recorded size/hash and generating corpus, avoiding two identical large hexadecimal transcripts. `COMPRESSED_RECORDS.json` describes the exact retained scope; original cache observations remain untouched. Binaries, object files, unrelated assets and downloaded dependencies are excluded. `FINAL_FINGERPRINTS.json` records the verified fast-path checkpoint source and binary identities. `CHECKPOINT_FILES.json` enumerates its intended changed-file payload. Neither certifies the isolated, unadopted next geometry investigation.

## Reproduce the matched workload

Start with the exact source checkpoint/overrides identified by the chosen manifest. Use the existing Release configuration and build only the benchmark/required focused targets:

```sh
cmake --build build --target judas_physics_stress_live_benchmark judas_physics_scaling_diagnostics --parallel 2
python3 scripts/physics_dense_measure.py freeze --build build --output .cache/my-dense-snapshot \
  --extra-source src/ContactSatFilter.h \
  --extra-source src/SpherePredicateFilter.h \
  --extra-source src/physics/ImpactSupportSets.h \
  --extra-source tests/ContactSatFilterTests.cpp \
  --extra-source tests/ContactSphereFilterTests.cpp \
  --extra-source tests/ImpactSupportSetsTests.cpp
python3 scripts/physics_dense_measure.py run --snapshot .cache/my-dense-snapshot \
  --output .cache/my-dense-exact270 --steps 270 --seconds 100 --timeout 130 --profile 1
python3 scripts/physics_dense_measure.py run --snapshot .cache/my-dense-snapshot \
  --output .cache/my-dense-realtime --mode realtime --seconds 35 --timeout 60 --profile 1
python3 scripts/physics_dense_measure.py compare --baseline BASELINE_RUN \
  --candidate CANDIDATE_RUN --output .cache/my-dense-comparison.json
```

Use a native desktop display and confirm the actual graphics driver in `run.log`; sandbox software rendering is not a matched hardware observation. Run only one CPU/GPU workload at a time. The exact mode requests 270 actual 60 Hz steps and deliberately does not measure FPS. Real-time mode records frame cadence, simulation progress and catch-up-cap observations. A completed capture is not a target pass.

The commands above reproduce the explicitly profiled pair. Use `--profile 0` on **both** baseline and candidate runs to reproduce the separate unprofiled comparison. Never mix settings or equate either synthetic cadence with playable frame rate.

Source archives can be relocated with their manifests; the portable helper verifies the sibling archive's recorded hash. Snapshot binaries are not committed here: rebuild the exact variant or retrieve the preserved local snapshot, then verify its recorded identity before comparing. Historical absolute paths in raw receipts are provenance, not a promise that those paths exist elsewhere.
