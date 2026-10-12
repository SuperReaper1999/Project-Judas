# Final archive: current physical standing witness

This read-only observer links the existing original BalanceLab diagnostic copy against the **actual final Linux archive** `39973b306e1d251e4422b8afc5048e0d2f5e8ccf037f35a8a0d6e72c13890449`. No production source, test, threshold, controller gain or cadence is changed. The old optional command-replay branch was removed from this observer. Its only extra behavior is recording physical state and continuing the exact saved live world for the original 720-step interval.

The original `Standing` helper can inspect a controller observation that remains cached after failure. This witness therefore separately reads **current** History.progress and actual mapped rigid-body pose/velocities, samples gravity through ordinary world APIs, and reads current completed contact samples. It reconstructs the same physical support acceptance and standing limits from those data, requiring current `Succeeded` status. It never calls the original Standing helper for its derived verdict.

## Linux result

Original cold-write: **24/0**. Original fresh-process cold-read: **13/0**. Uninterrupted and fresh-restored endpoint records are **exactly equal** by durable entity ID, including seven physical bodies and actual accepted contact data.

| Current physical metric | Both endpoints |
| --- | --- |
| Progress | 2 — Succeeded |
| Reason | measured supported physical standing |
| Root position | (3.08264065, 1.36034322, 0.34009087) m |
| Root linear velocity | (2.68256e-6, -1.20906e-6, -1.92983e-7) m/s |
| Root angular velocity | (3.19043e-8, -6.27887e-8, -1.47357e-7) rad/s |
| Actual gravity-relative upright | 0.99858439 |
| Actual root height above current support | 1.36158717 m |
| Actual COM speed relative to support | 2.13776e-6 m/s |
| Current accepted support contacts | 4 / 4 |
| Landings | 1 |
| Derived current physical standing | true |

[Full comparison](CURRENT_PHYSICAL_ENDPOINT_COMPARISON.json), [build/run/hash receipt](RECEIPT.json), and both `write-physical-final.json` / `read-physical-final.json` preserve the actual data. The save identity is recorded for both processes. `cold-read-state.json` script output is not used as physical evidence.

The observer compilation retains one harmless unused-parameter warning after removing its replay branch. This is not a production-source warning. A native recipe/source bundle is provided; native results must be recorded separately, rather than inferred from this Linux witness. Exact endpoint equality here is within this paired Linux run, not a guarantee of universal cross-platform replay.


## Native Windows result

The separately compiled native observer links final archive `71a722dded64cfcc634d679e55ddbd257058da888ada914f7f7150b8d88c2d62`. Original cold-write/read pass **24/0 and 13/0**. [The native comparison](WINDOWS_ACTUAL_ENDPOINT_PARITY.json) records exact uninterrupted/restored endpoint equality within that native pair, current **Succeeded** status and genuine standing: upright **0.99860215**, support height **1.36187017 m**, both feet supported, relative COM speed **7.13618e-7 m/s**, and one landing. Native snapshot and observer source identities are recorded separately; no Linux state is imported as a native result. The VM establishes native functional behavior, not physical GPU/input or host performance acceptance.
