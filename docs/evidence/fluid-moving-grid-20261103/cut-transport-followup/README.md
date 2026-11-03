# Cut-region inventory and free-surface follow-up

The shared cut-region checkpoint is `38f5d677dc3abfb5d352da25969e3f649b6b7d8e`.
The code change here tightens only the cut-region phase solver. Production
routing, Alpha's Advanced Fluids restriction and the original basin
fixtures are outside this change.

## Local donor inventory

The ordinary donor has separate departure and arrival vertices. Water
must leave before the node can spend receipts. A deficit at the
departure vertex overspends water. A surplus of water from its source
arc permits too little water on outgoing faces and can overspend air.
Previously the chart-wide residual bound could accept either mismatch,
even when the local air amount was much smaller.

The cut-region caller supplies the existing momentum model's density
ratio and local roundoff allowance to the phase solver. The circulation
now closes its ordinary departure vertices within these directional
limits and independently audits the actual water and air face totals.
The capacity, pressure flux and geometric face limits are unchanged.
Legacy generic callers use their existing bounds.

The continuous hydrostatic endpoint diagnostic previously sent
2.42652e-21 m³ of air out of region 2524 with only 1.60308e-21 m³ on
hand. The revised circulation finds a valid partition at the same
1.43525e-12 m³ total face transfer. It sends zero air and retains the
trace. The pressure root, requested force and native forecast are
unchanged. Water remains 15.829885793519965 m³ after the rounding
remap. The remap reports 2.31e-17 kg·m/s of momentum residual and
−7.43e-17 J of kinetic work residual. This diagnostic previews the
remap without publishing an impulse.

The four shared cut suites pass 52 checks. Generic phase repair retains
17 checks. The exact level and rotated wet basin direct runs still
reject the geometric phase ledger before applying a fluid impulse.
The separate hydrostatic control uses one equilibrium plane across
the entire chart rather than the original parcel inventory.

## Free-surface experiments

These disposable sources are archived separately from the shared solver.
The trial keeps the same rectilinear cuts and one signed water transfer
per shared physical face. The end water inventory is a bounded storage
curve in the hydraulic head. Port inertia uses one-sided wet support
and the trial solves the nonlinear storage and shared flux simultaneously.
A second trial computes one compressible pressure per closed gas region.
The same gas pressure shifts the storage curve of every participating
interface. The scheme retains the previous vented-only run as a control.

The vented-only model advances 12 stationary level intervals on the
original geometry and 9.25 m³ seed, then rejects. Peak face speed reaches
55.33 m/s. The diagnostic energy decreases while the short local face
speeds rise. The trial carries face speed across changing apertures
without a validated face momentum remap. The original seed contains a
0.534 m³ trapped air pocket, so treating it as exterior changes the
physical problem. The rotated parcel case still rejects before evolving.

With a separate gas pressure, the first stationary level interval solves
with a 1,307 Pa mean gas pressure, 0.744 m/s peak face speed, and
1.06e-14 m³ water drift. The trial then rejects topology ownership. A
horizontal liquid plane in every cut places newly arrived air at the
top of its region even when the air enters from below. The reconstructed
gas appears as a separate 0.001205 m³ pocket.

The matched geometry diagnostic keeps every final region volume and
moves the newly arrived dry material to its connected inlet side.
The two apparent pockets then form one 0.524303 m³ pocket, with a
6.94e-18 m³ maximum reconstruction mismatch. The same distinction
appears at a 1/240 s diagnostic interval. At 1/960 s the main pocket
rejoins and four genuinely unowned traces remain. This diagnoses the
limitations of a gravity-aligned plane per region. It leaves
advection unqualified. The inlet reorientation is a diagnostic and no
fluid or MK2 state was published.

The gas topology controls distinguish the exact 22.35 nm wall clearance
from true wall contact. A dry inventory below the coordinate precision
remains accounted for but opens no pressure aperture. The rotated
original seed retains 483 such isolated remnants totaling 1.03e-16 m³.
Its main trapped gas volume is 0.168 m³. The resolved rotated
liquid/gas trial remains unconverged.

## Reproduction

Run from the repository root. The prototype suite uses the following command.

```sh
bash tools/prototypes/fluid_volume_grid/qualify_moving_grid.sh /tmp/judas-cut-local-qualification
```

For the native donor diagnostic, make a scratch copy of
`../shared-cut-regions/build.py` alongside the source files in
`local-donor/` ending in `.h` or `.cpp`. Reuse the CMake Release GNU
Make tree used for the shared basin. Pass `--exact`, the normal sources
from `../shared-cut-regions/run.sh`, and `ContinuousEndpointDonor.cpp`
instead of the basin driver. Replace the normal `CutRegionStep.cpp`
with `CutRegionStep-preview.cpp` to expose the diagnostic phase remap.

The stationary free-surface experiments use the following commands.

```sh
e=docs/evidence/fluid-moving-grid-20261103/cut-transport-followup
s=tools/prototypes/fluid_volume_grid
b=docs/evidence/fluid-moving-grid-20261103/shared-cut-regions
d=$(mktemp -d /tmp/judas-cut-free-surface.XXXXXX)
tar -xzf "$e/free-surface/scratch-sources.tar.gz" -C "$d"
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$s" -I"$b" \
  "$d"/{CutLiquidGas,InitialGasAnchor,VentTopology,AnchoredDry}.cpp \
  "$s/CutRegionGeometry.cpp" -o "$d/gas"
"$d/gas"
"$d/gas" --divisor 4
"$d/gas" --rotated
```

The authored cases above are retained as expected rejections.
Use `CutLiquid-trust.cpp` and `InitialBasin.cpp` as the two scratch
translation units for the separate vented-only experiment. The archive
also contains standalone vent-topology and mean gas-pressure controls.

The native hydrostatic control and its full reproduction are in
[native-hydrostatic](native-hydrostatic/README.md). The matched hybrid
diagnostic accepted two native MK2 substeps with 1.78e-15 m³ water
drift and a decrease in independently measured mechanical energy at
both endpoints. It rejected at the third substep when the force
calculation produced 2.59 N·m of torque outside the translation-only
control. No 60 Hz wet rigid tick completed. The actual authored wet
cases remain separate rejections before publication.

The final source suite passed 931 checks across 23 executables. The
four-phase physical face partition in the inlet-anchor diagnostic
sums to the same aperture within 8.67e-19 m². The partition measures
shared wet, shared dry and each one-sided interface separately
without subtracting tiny areas from a full face.
