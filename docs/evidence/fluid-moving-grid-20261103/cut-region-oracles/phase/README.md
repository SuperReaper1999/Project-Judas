# Fine-grid paired phase feasibility

Disposable research on the exact 0.20 m authored seed and prescribed warmup in `qualification-pool-fine-local.cpp`. This is the 10 × 15 × 10 flat-grid proxy at 60 Hz. The tracked prototype was left alone. These runs use isolated snapshots and keep the original fixture, thresholds and step cadence.

| Snapshot | Accepted release frames | Last accepted time | Failure |
| --- | ---: | ---: | --- |
| Baseline | 32 | 0.533333 s | Phase-specific midpoint support |
| Paired transverse split | 43 | 0.716667 s | Infeasible air split at cell 676 |
| Paired split with axial exclusion | 57 | 0.950000 s | New paired support cut |
| Paired split with quantitative axial receiver capacity | 57 | 0.950000 s | New coupled transverse/endpoint cut |
| Quantitative axial variant with storage diagnostic | 57 | 0.950000 s | Same result. Diagnostic does not change publication. |

All five snapshots pass the existing 40 geometric momentum controls. All accepted CSV columns except timing exactly match across all variants for the 32 baseline frames. The warmup diagnostic still reports mean water speed 0.106739 m/s against its existing 0.05 m/s check. Exit status 3 preserves the release rejection.

## Paired transverse solve

For each phase and split axis, a shared transverse phase transfer has fixed total (q). Its lower share has signed bounds (0\ldots q) in the transfer direction when both geometric halves have support, or one fixed value when either half is unsupported. The upper share is exactly (q-q_L). The low balance is

```
div(q_L) + midpoint = low_start − low_end + fixed_axial_in.
```

The midpoint directional bounds use observed phase openings or the existing verified swept/incoming witness. These bounds are additionally limited by the donor's available phase and the receiver's possible phase storage plus outgoing traffic. The trial retains causal remap and rechecks actual incoming/swept witnesses after changing splits.

At the 0.716667 s failure, cell 676 (6, 7, 4), x split, has no low-half air before or after. The midpoint opening of approximately 1.164692e−4 m² is entirely wet. The smallest recorded trial requires 1.477366e−12 m³ lower-half net air outflow after fixed axial inflow, while the supported interval ends at zero. Its numerical tolerance is 7.315317e−15 m³.

The axial variants send such a cut back to the coarse phase solve when there is no supported midpoint or transverse exit and the current axial receipt exceeds measured half-phase storage gain. The zero-gain exclusion and quantitative capacity variant both preserve total mixture flow and intersect the original donor bounds. They repair previously rejected frames, but later cuts survive. Cell 545 in the last quantitative trial requires z-split air divergence 3.7728053502539657e−8 m³ against a minimum supported value of 3.7733919642816278e−8 m³. Other trial motions include an x-split water cut at cell 876 with supply 1.2432605678317239e−6 m³ and supported divergence exactly zero.

## Endpoint storage degrees of freedom

For each cell and momentum component, allow a distinct lower endpoint water amount (w_L), with the upper receiving the complement. Given coarse endpoint water (W) and half capacities (C_L,C_H), its bounds are (\max(0,W-C_H)\leq w_L\leq\min(W,C_L)). The water and air equations share the same storage edge.

```
div(qL_water) + midpoint_water + wL = old_low_water + fixed_axial_water_in
div(qL_air)   + midpoint_air   − wL = old_low_air   + fixed_axial_air_in − CL
```

The analytic fixture uses a stationary solid covering the entire cell midpoint and two equal same-height lanes. With no outer boundary transfers and 0.4375 m³ of water, a single horizontal plane puts 0.21875 m³ on each side. This violates the isolated lanes' balances when the left is dry and right full. Independent endpoint storage preserves (0, 0.4375) m³ exactly, and the equal-fill control accepts the single plane. All four analytic assertions pass.

All 36 sampled active fine-grid failing-component systems have a feasible circulation when endpoint volumes vary in this range while every coarse phase transfer and its transverse and midpoint support bounds stay fixed. Cell 545's z-lower water amount changes from 0.0039851490067298635 to 0.003985154597011634 m³ in the final recorded trial. Cell 876's x-lower water amount changes from zero to 1.2381958822257514e−6 m³ in one failing trial. The first diagnostic rejected due to cancellation in the dummy reservoir supply sum, even though measured cut deficits were below tolerance. A compensated binary64 sum changed only that diagnostic's result. The diagnostic does not publish or preserve a relaxed phase state. See the next section for cross-axis consistency.

## Shared endpoint and restricted joint oracle

The common storage diagnostic integrates eight geometric octant capacities per cell. Four constraints fix total water and the x-, y- and z-low water totals. It checks zonotope support planes and, where feasible, constructs a bounded eight-volume witness by enumerating nonsingular four-column bases. The blocked low-low-low control proves separate bounds are insufficient: for water total W, a missing low-low-low corner implies xLow + yLow + zLow <= 2W. The independent half checks each permit a low total of W, yet their combination demands 3W. The adjacent-corner positive control passes.

Of the 36 diagnostic captures, 30 had independently solved marginals on all three axes. In 29 of those 30, every cell had a common allocation. One real capture had two separated cells, 544 and 545, out of 1,500. The other six captures did not produce all three independent assignments because a separate axis supplied an invalid signed edge. They are unqualified. At cell 545, the separating normal (-1, 0, 1, 1) gives normalized deficit 2.9269493112949507e-9 m³ versus balance tolerance 8.4546209009467316e-15 m³. The raw sample is preserved.

The joint HiGHS diagnostic couples the same physical octant inventories to all six phase/axis layers. Both halves must balance. Each pair of transverse shares sums to its fixed accepted primal phase flow. Midpoint directions and transverse halves retain their original supported bounds. Ordinary half donor limits are verified. The first exact-equality run rejected, including measured endpoint representation error. The final oracle admits the original per-row balance tolerance with a binary64 arithmetic margin and independently rechecks the raw physical residual after clipping only representational bound overrun. It never treats the numerical slacks as material edges.

| Solve | Result | Operation size |
| --- | --- | --- |
| Freeze independently chosen marginals | Infeasible, matching the two separating certificates | 62,714 columns, 24,000 equality rows, 209,354 coefficients |
| Joint on all cells, fixed primal flow | Feasible, raw residual 8.4075541667560927e-15 m³ and zero donor or bound excess | 58,214 columns, 19,500 equality rows, 4,380 donor rows, 186,854 coefficients |
| Freeze valid exterior, allow only 544 or only 545 | Rejected because the other cell has no common octant state | No solve attempted |
| Freeze all 1,498 valid exterior cells, allow 544 and 545 | Feasible, raw residual 8.3986637089417115e-15 m³ and zero donor or bound excess | 32 physical variables, 26 equality slacks, 154 coefficients |

The two-cell witness moves 12 octant variables, two internal transverse shares and 18 midpoint directions. It holds each boundary share and every exterior octant fixed. The full check finds a maximum marginal change of 1.6784577022092156e-8 m³ and maximum source-unreachable numerical trace of 6.9864071582922743e-18 m³. The reduced solve takes 19 iterations and about 2.3 ms solver-only. Two runs produced byte-identical witness files. This is the smallest patch under the frozen-exterior contract for this sample.

This is an existential one-trial storage result. It does not establish one shared spatial flux at the intersection of a face's transverse half splits, chronology of transfer through swept phase geometry, persistent octant state, or subsequent pressure and rigid-body behaviour. The trace remains stopped at 0.95 s because no diagnostic endpoint is published.

## Reproduce

Run from the Project Judas checkout. Exact snapshots are in `baseline/`, `trial/`, `axial/`, `axial-bound/` `storage/`, `octants/`, `joint/` and `local/`. The driver, sources and logs are fingerprinted. The intermediate Python patches are scratch provenance. Compile from the snapshots.

```sh
d=.cache/fluid-paired-half-feasibility-20261103
bash "$d/build.sh" storage
"$d/bin/storage-geometric-momentum"
"$d/bin/storage-fine" 60
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$d/storage" "$d/storage/GridEndpointStorageProbeTests.cpp" "$d/bin/storage-GridHalfCellGeometry.o" "$d/bin/storage-GridPhaseFluxRepair.o" -o "$d/bin/storage-probe-tests"
"$d/bin/storage-probe-tests"
```

The raw diagnostic runs were bounded to 90–120 seconds each. Timing columns are retained for diagnosis and are not matched performance measurements.

The joint sample was exported by the `local` snapshot with `JUDAS_JOINT_SAMPLE_PATH` set. The offline Python oracle uses CPython 3.14, SciPy 1.18.1 and NumPy 2.5.4. Cached wheel hashes are recorded in `JOINT_LP_WHEEL_SHA256SUMS.txt`. With those packages in an isolated Python path:

```sh
OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python3 "$d/joint_octant_feasibility.py" "$d/octant-separation-local-sample.txt" independent-picks
OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python3 "$d/joint_octant_feasibility.py" "$d/octant-separation-local-sample.txt" joint --witness "$d/joint-witness.npz"
OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 python3 "$d/joint_octant_feasibility.py" "$d/octant-separation-local-sample.txt" joint --patch 544 545 --eliminate-fixed --witness "$d/two-cell-witness.npz"
```
