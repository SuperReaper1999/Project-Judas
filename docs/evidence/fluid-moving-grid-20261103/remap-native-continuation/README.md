# Staggered momentum cost and repeated MK2 endpoints

This work follows Experimental `8fd9a2de1156b262782d8e252f9426d4320a6e91`.
It changes the isolated volume-grid prototype and its tests. Runtime
fluid routing and the Alpha lock retain their existing behaviour.

## Momentum transport

Each staggered half-cell owns one velocity component. Previously the
prototype padded its extensive momentum into a vector and evaluated the
same carrier plan for two identically zero components. The new axis
operation calls the same carrier and kinetic-work checks once. The
ordinary vector operation remains available to callers with vector data.

The first 20 release frames were measured twice per implementation in
alternating order on the Ryzen 5 5600. Median went from 62.47 to 52.97 ms,
p95 from 66.43 to 57.47 ms and worst from 379.04 to 352.68 ms. Shared
host activity makes the worst measurements less precise. The captured
2,623,840 state bytes per implementation were identical, including
pressure, water and mixture flux, body and staggered fluid velocities,
and impulse. Both state files had SHA-256
`430f3f640570dc5f218d5ac6f77fb5c3c62e2572ab7ba20d65fa1abce600a60b`.
The independent axis controls passed 532 of 532 checks, including
non-finite, overflow, size and unowned-momentum rejection.

The full sealed 0.25 m pool ran all 720 release frames with exact
agreement in every logged non-timing field against the first geometric
continuation checkpoint. Median was 76.49 ms, p95 153.54 ms and worst
322.06 ms on the retained run. Its strict preparation diagnostic still
fails at 0.12811 m/s against 0.05 m/s. The original production
`half_025` PBF comparison also fails this strict diagnostic at
0.08685 m/s while passing mandatory checks. The unchanged PBF
`rotated_half` comparison passes all 2,176 checks in strict mode.
Those are legacy runtime results, separate from the new backend.
The current standalone prototype passed 874 checks. The three native
MK2 suites passed 67 checks.

A scratch nonlinear search experiment reused the tangent across accepted
Newton iterations. It cut recorded final-substep geometry evaluations
from 7,223 to 5,063 and passed the sealed proxy's settling gates, but
changed the number of retries on 135 frames. Maximum trajectory
differences reached 5.41 mm and 0.0290 m/s. The existing search is retained.
The candidate patch and its entire run are in `failed/`.

## Native publication

The repeated MK2 probe originally rejected at tick 18 because the
continuous box translated to the native center and the box rebuilt
from that center differed by at most 8.67e-19 m³ in each overfilled
cell. The probe now compares both cut capacities before mapping only
their binary64 representation difference. It also checks the actual
native pose, velocity, impulse and fluid inventories after publication.
Twenty-four ticks now pass, covering 96 MK2 substeps and the reversal of
vertical motion. Cumulative phase representation correction is
5.55e-17 m³, and the retained longer trace measured water-mass drift
of 1.78e-12 kg. The exploratory 60-tick run still rejects at tick 28
in the coupled pressure/displacement root. It remains a separate
negative result. The probe continues to cover a free translating box.

The retained tick-28 snapshot isolates the later failure. With fixed
vertical displacement of −3.634914 mm and lateral displacement
−1e-14 m, one initially blocked lateral face becomes fully open and the
solver returns +1,036.40 N in the lateral axis. The +1e-14 m probe
instead opens the opposite face and returns −1,036.90 N. Exact zero
lateral displacement keeps both faces closed and returns −0.241 N.
Both off-axis candidates are accepted by the pressure and phase solve.
This discontinuity defeats the rigid root before contact or a native
rounding remap. Zeroing old fluid velocities still gives a 5.69 N
one-sided limit. Suppressing the predictor on initially closed faces
still gives a roughly 797 N jump. Those diagnostic variants were
discarded. They preserve the moving geometry to separate momentum
history from the discontinuous aperture and traction ownership.
The newly opened side also carries about 2.765e-5 m³ of mixture on
either perturbation, essentially unchanged from 1e-8 to 1e-14 m.
Vertical pressure force changes from 2,030.80 N to about 1,141.8 N.
This is a change in the pressure path and the wall reaction together.

Two additional native probes exercise the public coupling hook with
real contacts. A 100 N·s impulse has the same empty initial row set
and acknowledged velocity in a clear scene and with a wall. With the
wall, CCD shortens travel from 0.4167 m to 0.1950 m and stops the body.
A supported resting contact with acknowledged zero physical velocity
still moves the body 90 µm through the position solver. The probes
confirm that the velocity-stage callback cannot qualify the final
pressure path. A rejected later substep also leaves already committed
native motion. The engine needs a clear swept-path admission boundary
for a bounded contact-free runtime and a separate native composition
contract for actual contacts.

## Reproduction

Run the prototype and native commands from the
[geometric continuation checkpoint](../geometric-continuation/README.md).
The new repeated probe runs as part of `MovingGridMk2MixedTests`.
For the independent remap controls, compile
`controls/GridAxisMomentumControls.cpp` with `GridConservativeRemap.cpp`
and the prototype include directory. The paired timing driver is
`bench/pool-first-20.cpp`. It uses the same link inputs as the full
pool driver. The two original production runs used
`judas_production_fluid_tests --strict-diagnostics --case <name> --output <dir>`
with `half_025` and `rotated_half`.

Build `failed/root-scan.cpp` with the prototype include path and the
full pool driver objects except `CoupledVolumeBox.o` and
`GridParcelSeed.o`. Pass `failed/native-root-input.bin` as the only
argument. The snapshot contains 5,326 little-endian binary64 values,
ordered as box bounds, body velocity, mass, timestep, gravity, water
inventories, shared velocities and half-cell velocities.
`failed/root-scan-flux.cpp` uses the same command and also records
the shared lateral fluxes and vertical pressure force.

The native path controls use the same Linux engine archive as the
grid probes and rebuild the relevant native modules from source.

```sh
probe=docs/evidence/fluid-moving-grid-20261103/remap-native-continuation/controls/mk2-path
python3 "$probe/build.py" --exact
python3 "$probe/build.py" --exact --contact
```

The recorded path log has trailing spaces normalized.
