# Latest combined midpoint performance

Read-only analysis of the root's completed `midpoint-final-concave` run. This supersedes the earlier predictor-only combined timing for the current candidate. The original failing evidence and isolated optimization result remain preserved.

## Unchanged workload and observed result

The same observer loads the original thirteen-body articulation and duplicates it four times: **52 articulated bodies plus one floor, 48 joints, 8,192 actual floor triangles, 900 fixed steps at 60 Hz**. No geometry reduction, forced sleep, extra damping, timestep change or contact-fidelity reduction was introduced for this measurement.

| Four-rig mesh workload | Median (ms) | p95 (ms) | Maximum (ms) | Steps over 16.67 ms |
| --- | ---: | ---: | ---: | ---: |
| Original failed candidate | 1.040693 | 18.875046 | 33.793832 | 78 / 900 |
| Boolean-work optimization in isolation | 1.032763 | 14.963051 | 23.999817 | 30 / 900 |
| Latest combined implicit-midpoint candidate | 1.058026 | **5.498837** | **26.653463** | **11 / 900** |

The latest combined p95 meets the unchanged **16.67 ms** gate. Its maximum remains above that gate; high-cost collision steps have not disappeared. Different legitimate physical trajectories result from corrected event state and retained implicit midpoint. This comparison is not per-step trajectory parity, and the combined speedup must not be attributed solely to the Boolean optimization. Its paired isolation experiment establishes that narrower attribution separately.

Latest aggregate observations: **56,576 contact rows, 19,163 CCD search iterations, seven bounded search limits, minimum bone-center Y 0.0447216108 m, 26/52 sleeping at the end**. The unchanged finite/penetration sanity guard passed. Mesh-1 ends with **0/13 sleeping**; box-1 and box-4 end with all 13/52 sleeping. Do not claim all mesh articulations settle, or infer that motion was forcibly removed.

## Remaining expensive work

- Worst step **85**: 26.6535 ms from the original CSV precision; **25.5307 ms solver**, 0.902546 ms contact/narrowphase, 0.054326 ms broadphase; **285 CCD queries, 830 search iterations, one sampling fallback/limit**.
- Slowest p95 tail: 46 steps, mean 12.9208 ms, with **90.78% of summed wall time in the contact/joint/continuous-impact solver timer**. The 11 steps above the gate average 20.2881 ms, **93.81% solver**.
- Wall time correlates with CCD iterations **0.97256**, CCD queries **0.95891**, solver timer **0.99635**. Remaining costly collision steps are associated with repeated CCD work. This supports the original measured attribution but is not an invented new subphase decomposition.
- Final 300 steps average **0.799234 ms**; contact/narrowphase averages 0.490958 ms, solver 0.145101 ms, with 22.1 sleeping bodies on average. Once repeated CCD work subsides, ordinary cooked contact generation is the largest remaining timer.

The final profiler retained its last 120 frames and **no early spike**. Its scoped history therefore cannot directly decompose the step-85 maximum. The largest retained late frame is 0.837047 ms: contacts 0.557114 ms, continuous impacts 0.011294 ms, total contact/joint solver 0.123526 ms; it has zero impact events/queries. `RETAINED_SCOPES.json` preserves these scopes/counters. Capture diagnostics report zero dropped/exhausted/incomplete/truncated records; seven open scopes are the enclosing export-time scopes. The CSV's `geometry_predicates = 0` is the separate primitive diagnostics counter, not proof that cooked geometry did no work.

## Exact inputs and reproduction

- Root run/build receipts: `../midpoint-final-concave.json`, `../midpoint-final-concave-build.json`, and `../midpoint-final-concave.log`.
- `raw/` preserves byte-identical copies of all four CSV/profile pairs.
- `ANALYSIS.json` contains groups, correlations, maxima and exact root stdout aggregates.
- `RAW_FINGERPRINTS.json` hashes copied measurements, receipts and the analysis recipe; it is **not** the final candidate source manifest.
- `probes/review_final_ragdoll_trace.py` only copies/analyzes existing output and refuses to overwrite this folder. No simulation or build was run for this analysis.

Native Windows performance/combined correctness is reported by the root validation, not inferred from these Linux results. No further optimization is proposed from this read-only analysis.
