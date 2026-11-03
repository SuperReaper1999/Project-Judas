# Native hydrostatic endpoint reconciliation control

This Linux control uses the exact authored level basin and MK2 rigid body with a separate globally hydrostatic water seed. The original wet seed remains a separate failing gate. The experiment retains exact swept geometry and native rigid settings. It can reject inside a rigid tick after earlier individually verified substeps. No result here qualifies a completed tick.

## Reproduction

From the Judas checkout root, run the adjacent `run.sh`. It builds the prototype plus the current PhysicsWorld, WorldConstraintBatch, WorldMk2Solver and WorldBodies implementations against the configured production fluid test link dependencies. Use `JUDAS_BASIN_CMAKE_BUILD` or `--build` to select a compatible Release GNU Make build whose `judas_production_fluid_tests` dependencies are already built. The script runs the four independent remap variants and retains exit codes and exact logs in a fresh temporary directory. The expected nonzero codes indicate the preserved incomplete tick.

## Accounting

For each accepted native substep, the adapter brackets the unrounded vertical coupled root and recomputes pressure against the held initial water and momentum state for each trial. No pressure or state is published during this search. Native pose and velocity use MK2's actual rounded translation anchor and duration. The bridge checks the per-operation binary32 representation budget, exact capacity equality with independently reconstructed endpoints, positive physical sweep clearance, material remap momentum and kinetic residuals, and closed-domain body plus fluid energy. Its separately reported kinetic mixing loss remains small. Only after the native observer confirms actual endpoint, orientation, velocity and impulse does the adapter publish the precomputed material state.

Independent energy uses rigid translational kinetic energy and potential, plus per-cut-region mixture kinetic energy and the geometric vertical first moment of water and air. The calculation reconstructs the water plane and subtracts the exact moving solid's rectangular intersection in each region. The comparison allows only the calculated pressure force times path roundoff, maximum displaced-fluid potential roundoff and arithmetic residual. The accepted steps lost physical energy. A resolved lateral reaction or torque terminates this translation-only control.

The original whole-domain bookkeeping remap recomputes pressure at zero predictor and zero gravity to obtain paired phase flux, then discards the fictitious pressure impulses and uses the accepted flux to carry actual material momentum. The local pressure variant limits only this bookkeeping step to a one-cell collar and checks every changing capacity and swept face is retained.

The geometry-only collar variant routes the small volume change from regions whose capacity shrinks to those whose capacity grows using physical faces with positive wet aperture on both sides. Each route uses only initial water stock of its ordinary donors, bounded by a wet port depth of one coarse cell. Shared flows then pass existing phase and carried-momentum checks. It is a fully wet collar diagnostic. Mixed-phase routing and production policies are outside its scope. The hybrid diagnostic predeclares whole-domain pressure remap for the first verified substep and geometry-only collar routing afterward. It is not a production fallback.

## Results

- Whole-domain remap verified one actual MK2 substep. Substep 1's continuous root and native endpoint geometry then passed, but its bookkeeping pressure required an infeasible phase split. Remote wall sliver departure deficits reached around 1.5e-21 m³ against local arithmetic allowances around 1.6e-22 m³. Near-body deficits also remained.
- Local pressure remap failed on the first interval. The four leading near-body phase departure residuals were about 1.7–1.8e-18 m³ against 8.54e-19 m³ allowances. This preserves local donor ownership.
- Geometry-only remap verified one native substep. The next physical step rejected an unrooted 8.33e-20 m³ air circulation from initially solid region 7830. The bookkeeping algorithm did not fabricate a donor.
- The hybrid diagnostic verified two actual native substeps. The first physical energy change was −0.04071022699 J and the second −0.01408767783 J. Maximum water drift was 1.77636e-15 m³. Maximum native impulse error was 4.62350e-8 N·s and angular speed remained zero. The first candidate of substep 2 then produced 2.5866798783 N·m of pressure torque against a 3.7659111668e-8 N·m arithmetic allowance. The adapter rejected the candidate before publishing a third impulse.
- The original authored level and rotated wet warmup and direct probes still fail phase closure before any publication. Level dry completes a full native tick with all four substeps verified.

Instrumented raw logs record the exact failed region arithmetic. They do not alter acceptance. All work was confined to the prototype/scratch test path and the Alpha lock was unchanged.

The exact-source inputs were fingerprinted before and after compilation and matched. Binary identity is in `final-binary-sha256.txt`. `final-*.log` names the four uninstrumented exact runs. The instrumented source copies and corresponding diagnostic logs are retained separately.
