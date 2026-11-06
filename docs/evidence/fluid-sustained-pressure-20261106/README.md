# Sustained pressure evidence

Parent: `5d412ff196ca8790999625faf3960514977ca340`, Experimental.
The containing commit and source checksum manifest identify the final code.
No native runtime/SDK, main promotion, release or Alpha-lock change.

## Reproduce

Requirements: existing C++17/GLM prerequisites, Bash, Python 3, gzip and GNU time.
The script works without Git and from a different current working directory:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_sustained_pressure.sh /tmp/judas-sustained 120
```

Use a fresh output folder. `1` instead of `120` is a workflow smoke check,
not the sustained qualification. The default 720-second process budget is an
experiment limit, never a timestep/accuracy setting. Increase it explicitly
with `JUDAS_FLUID_QUALIFICATION_SECONDS` if a slower machine needs it.

The script runs all focused controls and the exact-dyadic audit oracle, verifies
the retained input hash, cold-replays the old failed interval, then advances
both original fixtures. `replay.exit` and `sustained.exit` must be zero. The
sustained log must report 120 admitted prefixes for each orientation, original
volume/quantity admission, and the final independent ledgers. Do not infer full
fluid-motion or native integration qualification from those records.

Sanitizers, outside a tracing wrapper for LeakSanitizer:

```sh
ASAN_OPTIONS=detect_leaks=1 JUDAS_FLUID_SANITIZE=1 \
 bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-sustained-sanitized
```

The retained large replay also passed ASan/UBSan/leak checks; its resource/log
records are included. Three thousand checks on small controls alone are not
used as a substitute for that original-geometry test.

## Inputs and references

`third-state.bin.gz` is the admitted legacy third state, produced before the
correction. `CheckpointDriver.cpp` records its producer/replay recipe; native
fixture geometry comes from the unchanged shared `BasinFixture.h`.

The binary is a **diagnostic numerical format**, little-endian Linux:
four binary64 settings (dt, water density, gas density, sound speed), a uint64
geometry stamp, then five uint64-length-prefixed arrays of pairs of binary64
(pressure, water, gas mass, interior integrated flux, gas-port integrated flux).
It is not a native game save, migration fixture or portability claim.
`STATE_SHA256SUMS.txt` checks decompressed files.

`baseline-source.tar.gz` preserves the parent prototype implementation. Its
Fast backend is pinned in `BaselineFastProbe.cpp` regardless of the benchmark
harness's role flag, so the measured parent is not accidentally its older
Reference backend. Build that driver against only extracted baseline headers
and objects. The current `FastAuthoredPressureProbe.cpp` selects `--fast`.
`benchmark_fast_pressure.py` contains the alternating-order command recipe.

The original slow reference is preserved separately at
`../fluid-fast-transport-20261106/slow-reference-source.tar.gz` (`574fa22f`).
`oracle-comparison/` records the fresh first-interval field check against it.
That one sample is a correctness comparison, not the headline timing study;
other qualification work overlapped part of its run.

`matched/` contains three samples per parent/current orientation, raw costs,
resource receipts, determinism and complete field-difference summaries.
Current first-sample field dumps are compressed; all dump hashes are recorded.
Remaining identical repetitions stay in `.cache/fluid-stagnation-20261106/`.
`pressure-pairs.json` is a separate intermediate pressure-accelerator study,
not the final complete-step measurement.

## Failed experiments and unqualified motion

`diagnose.log` reproduces the old stagnation from this exact input.
Mass-residual, log-density and transported-storage candidates timed out without
meeting admission; their logs and diagnostic sources are retained. They were
not adopted. A valid unit-barrier shortcut was slightly slower in its paired
replay and was also rejected. Hardware-specific FMA measurements were diagnostic,
not a controlled portable performance claim, and no such requirement was added.

An earlier long run reached 120 level/98 rotated intervals before its wall-time
budget expired; that is a budget-limited run, not a physical rejection or a
completed two-fixture qualification. `selected-sustained.*` records the completed
120+120 run. Cleanup of an obsolete wrapper and explicit zero-water-port-flux
readback followed it; the final replay matches its output byte for byte, and
all final controls/sanitizers passed. These changes do not alter material state.

`energy-third.log` / `energy-fourth.log` and `EnergyProbe.cpp` expose high-speed
small-face modes. They are end-state dual-face kinetic diagnostics, not an
energy-conservation acceptance test. Conservative momentum transport, full
body coupling and live gameplay remain unqualified. Keep the fluid lock.

Human review checklist:

1. Inspect the old failed replay and final admitted replay of the same state.
2. Reproduce 120 intervals and inspect both prefix/quantity ledgers.
3. Review CPU/RSS costs and the high-speed face diagnostics without treating
   successful volume closure as a full fluid-motion pass.
4. Keep moving bodies/native integration and unlocking pending their own tests.

[Engineering report](../../FLUID_SUSTAINED_PRESSURE.md).
