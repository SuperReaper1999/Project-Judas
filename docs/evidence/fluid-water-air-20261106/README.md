# Water / finite-air checkpoint evidence

Source parent: `ba5fefeddfd95a2115caf63a40117aba993eb902`, Experimental.
`SOURCE_SHA256SUMS.txt` identifies the actual final tested sources, scripts and
unaltered authored-basin fixture. `verification.json` records platform, compiler,
flags, controls and pressure timings. This is not runtime qualification.

## Results and reproduction

From the repository root, with the existing Linux C++/GLM prerequisites:

```sh
sha256sum -c docs/evidence/fluid-water-air-20261106/SOURCE_SHA256SUMS.txt
bash tools/prototypes/fluid_volume_grid/qualify_water_volume.sh /tmp/judas-water-air-check
```

**Expected overall exit: 1.** The 32 control suites pass; the two basin pressure
proposals pass; the complete authored-basin advances reject; the 240-interval
flow/energy control passes. The script runs all three phases and preserves each
`.exit`, `.log` and `.resource`. Do not replace the whole-advance gate with the
passing pressure-proposal test. Fields on rejected unpublished results remain
default values; they are not measured zero error or zero execution cost.

```sh
JUDAS_FLUID_SANITIZE=1 bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-water-air-sanitized
```

Expected exit 0: the same **3,506 checks in 32 suites**, under address/undefined
behaviour sanitizers with leak detection. Leak detection was run natively outside
the tracing wrapper. This is focused prototype coverage, not a new whole-engine
or platform release qualification.

The 50-check reference-mode invocation is separate:

```sh
/tmp/judas-water-air-check/controls/CutMassMomentumTests --reference
```

Independent filtered-residual dyadic oracle (842 checks):

```sh
mapfile -t objects < /tmp/judas-water-air-check/controls/objects.list
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -Itools/prototypes/fluid_volume_grid tools/prototypes/fluid_volume_grid/CutFastResidualOracle.cpp "${objects[@]}" -o /tmp/judas-water-air-check/oracle
python3 tools/prototypes/fluid_volume_grid/qualify_fast_residual.py /tmp/judas-water-air-check/oracle
```

Six prior pressure-only states, three for each orientation, remain byte-identical
(`legacy-pressure-comparison.json`). This does not claim identical corrected
momentum trajectories: geometric half-face routing intentionally removes the
old product-partition mixing. Its negative control records the changed profile
and mixing energy independently.

## Sustained physical control

`sustained-flow.log` is a closed two-column fixture, **240 intervals at 1/60 s**.
Every complete step must publish. Water and air mass are compared with cold
initial quantities; the energy calculation independently includes mean water
kinetic energy, analytic column gravitational potential and isothermal air free
energy. Every adjacent energy increase is checked, not just the last state.

Initial column quantities `(0.6, 0.4) m³` become approximately
`(0.503672184160266, 0.496327815839734) m³`; energy decreases from `2550.6 J`
to `2452.637754125 J`. There is no damping knob or energy clamp. This control
was declared after the old continuation failed at step 71, before inspection of
later sustained states (`sustained-water-gates.txt`). A second attempt failed at
step 81; both failures are retained. Correcting the accumulating geometric
partition error is necessary to complete the final run.

This does not prove arbitrary free-surface transport, moving solids, native MK2
trajectories, cold saves, radial fields or runtime streaming/export.

## Measured cost and deterministic repetition

`pressure-repeat.log`: three repeats per orientation on Ryzen 5 5600,
GNU C++ 15.2, Linux x86-64, O3, no fast-math. Compiler jobs had finished;
the desktop remained a shared host. Each repeat uses the same immutable source
and freshly prepares the liquid/gas operators. Fixture cooking is outside the
reported pressure solve. The bit digest covers pressure, gas mass/volume and
liquid flux arrays; it is a deterministic comparison, not an integrity checksum.

| Pressure proposal | Mean | Median | p95 / worst | Linear applications |
| --- | ---: | ---: | ---: | ---: |
| Level | 2060.23 ms | 2063.38 ms | 2147.25 ms | 198 |
| Rotated | 5493.01 ms | 5491.58 ms | 5558.91 ms | 285 |

All three state digests match within each orientation. Neither uses a fallback
in this final sample. With only three samples, nearest-rank p95 is the maximum;
these are diagnostic distributions, not a statistically strong tail estimate.
Peak process RSS is **286,604 KiB**, including retained original, conforming and
compacted fixture fields. These are **not complete fluid steps and are not
real-time costs**. Do not advertise a production speedup versus the older
full-mixture timestep: the physical model and amount of work differ.

## Remaining acceptance failures

- Level: initially fully wet cell 5770 receives different air fronts (owners
  5409 and 5418); another incoming parcel is about `3.697e-5 m³`. They remain
  separated by water. One lower-water prism cannot represent that state.
- Rotated: after compatible local inverse refinement clears the original
  momentum/work rejection, material departure crosses the represented phase
  front. It requires a geometry event/re-cut rather than negative-water clipping.
- The stricter minority-relative pressure diagnostic remains outside its gate:
  about 3.69 normalized for level and `5.77e18` for rotated. Only the original
  containing-cell residual criterion passes at the pressure-proposal stage.

The Alpha lock is unchanged. There is no new native playable backend to accept.

## Retained attempts

`prior-attempts/` includes bounded old-model direct, warm-start, timestep
continuation/substep failures; local mass/projection variants; and the two
sustained-flow rejections. The old restarted full-mixture rotated run actually
completed one original interval in **7:49.02**, after 16 continuation attempts;
it is positive reference evidence, not sustained viability.

Complete scoped source snapshots and individual diagnostic variants are both
retained. Individual variants and early timing logs do not all contain a fully
frozen dependency tree, and are **not** presented as final qualification or
isolated performance comparisons. `qualification.log.gz` records an invalid early
shell run after its active script was edited; it is excluded from all counts.
The final frozen-source runs above completed independently of that harness error.

The three long control/harness logs are compressed without altering their bytes
(`controls-normal.log.gz`, `controls-sanitized.log.gz` and the prior harness log).
Use `gzip -dc` to inspect them; `COMPRESSED_RAW_IDENTITIES.json` records hashes
of their original uncompressed bytes.
