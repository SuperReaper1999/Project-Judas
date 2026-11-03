# Pressure compatibility in nearly disconnected cells

The volume-grid pressure projection checks that each sealed component's
source sums to zero within binary64 representation error. Previously
it divided the admitted roundoff by the number of cells and assigned
it to every row. A weak branch with no physical source then received
an artificial volume flux. The pressure error scales inversely with
that branch's conductance.

The new projection balances each accepted component on an existing
source row. It chooses the largest absolute source, breaking ties by
the pressure diagonal. This keeps the correction out of a physically
balanced row and retains the existing rejection test and solve tolerance.

The new regression exercises the public volume-step entry point. It
preserves a 3e-5 m³ transfer between two cells with a tiny representational
imbalance. The third cell connects only through a weak throat and has
zero prescribed volume change. The prior implementation publishes
3.16e-20 m³ of spurious flux into that cell and fails the new check.
The corrected implementation passes both assertions.

An independent pressure-graph diagnostic sets the weak link conductance
to 1e-29 in its test units. Before the change, the artificial source
produces a 3.388e9 Pa pressure jump and 1.15e-10 J of spurious work.
After source-row balancing, its right-hand side is zero and measured
work matches the independent result. Plain CG still permits a roughly
15 Pa leaf pressure difference whose flow lies below tolerance. Exact
leaf pressure recovery is a separate refinement for the new local
cut-region operator.

## Captured mixed displacement

The paired local pressure tests use the identical
[native tick-28 snapshot](../remap-native-continuation/failed/native-root-input.bin)
and import its water volumes and shared face velocities onto fixed cut
regions. The local pressure geometry and its mass model are documented
in the [cut-region evidence](../cut-region-oracles/README.md).

With the prior compatibility rule the rigid displacement solve stalls
after 37 evaluations, with a 5.40e-11 m residual against 1.25e-12 m.
With the new rule it converges in 13 evaluations to 6.98e-14 m. The
separate fixed-displacement pressure gate passes all 11 assertions.
This verifies a pressure forecast. A shared material/velocity publication
and a bounded remap to the actual native endpoint are separate gates.

## Combined prototype and native checks

- 876 prototype checks, 67 native MK2 checks, 56 native player checks,
  and 26 current Alpha fluid-policy checks passed.
- Both 0.25 m sealed pool proxies complete all 720 release steps and
  pass the mandatory settling, immersion and conservation checks.
  The cyclic Z-axis run previously rejected at 10.7333 s. The current
  level and cyclic volume drifts are 3.78e-13 and 5.10e-13 m³.
  Their late RMS vertical speeds are 0.01063 and 0.01293 m/s.
- Both pool preparations still measure about 0.12811 m/s against the
  original 0.05 m/s diagnostic. The 0.20 m proxy still rejects the
  unsupported half-cell phase transfer at 0.5333 s.
- The proposed native pressure sweep can be rejected before impulse
  publication by the actual broadphase, even when the initial
  constraint batch has no contact rows. The four fixtures cover a
  clear path, a swept wall, another moving body and a sensor.
  This is a conservative first-substep admission probe. The longer
  native motion path and a world-level rollback are separate.

The captured tests qualify the numerical correction in the isolated
backend. The authored basin, general contact and CCD composition,
and engine-side backend selection remain open.

## Reproduction

Run the prototype qualification script as in the
[geometric continuation evidence](../geometric-continuation/README.md).
The new failure control is in `MovingGridTests`. Link the new test
against the previous checkpoint's objects to reproduce the failure.

The two captured pressure drivers use the same pressure source and
state. Build either driver from the root with the prototype and prior
cut-region include directories. `MovingVolumeGrid.cpp` is included
by the disposable drivers to reach the private pressure entry point.

```sh
s=tools/prototypes/fluid_volume_grid
e=docs/evidence/fluid-moving-grid-20261103
o=/tmp/judas-pressure-compatibility
bash "$s/qualify_moving_grid.sh" "$o"
for mode in Imported Root; do
  c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
    -I"$s" -I"$e/cut-region-oracles/pressure" \
    "$e/pressure-compatibility/mixed/LocalRectCutPressure$mode.cpp" \
    "$o"/{MovingVolumeMomentum,HalfDualVolumeMomentum,GridHalfCellGeometry,GridGeometricMomentum,GridConservativeRemap,GridPhaseFluxRepair}.o \
    -o "$o/$mode"
  "$o/$mode" "$e/remap-native-continuation/failed/native-root-input.bin"
done
```

Run the native admission control using the same GNU Make engine
archive as the prior native probes.

```sh
python3 "$e/pressure-compatibility/build_native_probe.py" --exact
```
