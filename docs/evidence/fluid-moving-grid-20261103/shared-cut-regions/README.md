# Original-basin cut-region and native MK2 diagnostic

This fixture compares the bounded shared pressure and transport prototype with the geometry, initial parcel inventory, body, and fixed cadence of the original authored basin. It does not replace the original production settling tests. It runs an explicit experimental prototype and has no connection to the Alpha fluid lock.

## Reproduction

From the Judas checkout root, run the adjacent `run.sh` by its path. It creates a fresh result directory under `/tmp/judas-cut-basin-repro.*` and writes raw logs, exit codes and fingerprints. To choose another compatible configured build, set `JUDAS_BASIN_CMAKE_BUILD` or pass `--build path/to/build`. Pass `--exact` to recompile PhysicsWorld, WorldConstraintBatch, WorldMk2Solver, and WorldBodies from the checkout in addition to linking the production fluid gate's Linux dependencies. The runner needs the already-configured GNU Make build at `.cache/2d-stage1-build` and the link prerequisites for `judas_production_fluid_tests`. Set up that build before use. The independent native controls produced identical raw output with and without `--exact`. Each of the four recompiled production object files is byte-identical to its archived build object. The linked executables have different hashes.

The harness uses `BasinFixture.h`, `BasinMk2.h`, `BasinMk2.cpp`, `BasinSharedStep.cpp`, `NativeControls.cpp`, `NativeWallGeometry.cpp`, `build.py` and `run.sh`. Additional scratch instrumentation is identified in the raw evidence where applicable.

## Fixture scope

The seed maps the original emitter's lattice cubes by literal positive overlap with cut-region boxes. It preserves authored dry cells, wall seams, seed exclusion, body placement, mass and the 60 Hz fixture cadence. In the .25 m case it assigns all 592 parcels and 9.25 m³ exactly, using .125 m base cells. The reference-height gauge uses the previously observed .25 m surface of 1.7094379663467407 m above the floor. The chart has a sealed external boundary, an air collar and a bounded translational release envelope. It does not claim the original production warmup or settling acceptance.

The native bridge builds five authored walls, warms up with the initial static placeholder, then replaces it with the original dynamic MK2 box. Within each actual MK2 substep, the callback stages material and force against a native binary32 pose forecast and requests an impulse only on an exact fixed point. MK2 CCD and solver rows remain enabled. A resolved angular impulse, contact sweep, or unreconciled endpoint rejects the proposal. Material is committed only after inspecting the actual native endpoint, velocity and angular motion. This is a proof boundary for free translation and has no native rollback after a late verification failure. It does not cover angular coupling, contact acceptance, long-range chart remapping or the full production basin trace.

## Initial results

The .25 m case has 20,344 cut regions and 57,348 shared faces. In both the first implementation and the first conservative corridor candidate, the authored 240-step warmup stops at the first step because the phase ledger cannot partition pressure volume flux. With warmup explicitly bypassed, the first native proposal fails the same gate before publication. There are zero committed material steps and zero published impulses.

The pre-corridor region 2566 receives 7.133537747880397e-7 m³ of mandatory water from full region 3543 across a 0.00012500584125518799 m² z-port. Its own capacity is 2.7940982860741315e-12 m³ and it starts dry. An equally sized, opposed z-port has 7.288165018036883e-7 m³ of prescribed mixture outflow but initially allows only air. A perpendicular inlet supplies 1.5462873258760916e-8 m³ of air. Two much smaller side outlets are also present. The measured projected mixture residual is about 1.08e-15 m³, so ordinary pressure conservation is much closer than the phase deficit. See `diagnostic-ports.log`.

The rotated literal .20 m import stops before evolution. 1,152 binary32-spacing parcels nominally hold 9.2160004119873111 m³. Literal overlap assigns 9.2160003404762953 m³. Widening ten authored .20 m intervals ends at 1.0000000298023224 m while the positive walls' interior begins at 1.0000000223517418 m. The positive x and z intrusion explains the measured 7.1511015775627129e-8 m³ difference. An explicitly conservative import policy would be a separate operation.

The air-only .25 m control with the conservative corridor candidate passes one actual native 60 Hz tick, with four accepted MK2 proposals, four impulse publications and four material commits. There is zero water drift. Maximum reported native impulse roundoff is 3.7684065608278861e-7 N·s. This isolates native bridge functionality from the failing wet transport. The independent native control executable also passes 10/10, covering level and rotated no-force/known-force controls, pre-publication material rejection, bad endpoint rejection and MK2 floor contact.

## Native anchor, conservative import and elbow diagnostics

The optional `--conservative-seed` path clips each parcel to the actual initial cut geometry, then places only its excluded volume in the nearest reachable regions with spare capacity, measured by hops on positive-area start faces. It retains the literal import as the default negative control. The level .25 m case assigns the original 9.25 m³ without shifting material. With the chart seeded from the real native binary32 initial pose, the rotated .20 m case conserves 9.2160004119873111 m³. It relocates 7.9018930539788619e-8 m³ over 244 parcel sources with up to eight open-face hops. This includes clipping the moving placeholder. The bridge otherwise rejects a different actual start pose rather than moving initial water implicitly. The translation-only coupling covers rotations of the entire fixture, with angular response unresolved.

A straight corridor handles the leading 2566 transport failure, while region 2567 still needs an orthogonal turn into narrow sliver ports. With the bounded elbow allowance enabled, the original .25 m warmup and direct runs still reject 60 and 204 local deficits respectively before publication. Maximum local deficits are 1.2716202354652706e-9 m³ and 1.0535863661062127e-9 m³. For conservative rotated .20 m they reject 66 and 126 local deficits, with worst 6.6759344088760593e-7 m³ and 6.6455097969438113e-10 m³. The unsealed authored geometry includes a .00100004673 m under-wall seam and some 22.35 nm wall-end slivers. These are geometrically distinct.

The independent level static-start `--timestep-sweep` holds the same geometry and 9.25 m³ initial state for every trial. 1/60 through 1/1920 s reject the phase ledger. Only 1/3840 s accepts, with maximum reported velocity 28.759360373931941 m/s after 0.2604 ms and no publication. This one-shot diagnostic is not an alternate settling clock or an accepted trajectory. Diagnostic compute measurements (~59–85 ms per trial) were on a shared host.

The rotated air-only dry control exposes a separate threshold issue. The original global pressure tolerance of 5.6843418860808028e-17 m³ marks a small region as non-relay despite a real flow 3.49 times its local volume. The source inventory is 7.1054257760316411e-18 m³ and the required outflow is 2.4832405747765362e-17 m³. Incoming/outgoing volume closes to ~6.5e-26 m³. A scratch probe changes only relay eligibility to a relative storage-scale comparison. It passes that check and then rejects the conservative momentum remap's supported cyclic block size. Neither is sufficient for rotated wet flow.

`elbow-source/` freezes the source files needed to distinguish this snapshot from later implementations. `CutRegionStep-elbow-diagnostic.cpp` adds reporting for the actual failed gates and did not change solver acceptance. `rotated-momentum-diagnostic.log` and `threshold-*.log` preserve the separate dry threshold and cycle findings. The frozen solver and instrumented snapshots are separate scratch evidence. The portable runner recompiles the solver from the named checkout and supports a compatible CMake Release GNU Make build.

## Independent native geometry and hydrostatic controls

`NativeWallGeometry.cpp` queries the actual MK2 shapes, native float broadphase bounds and binary64 narrowphase geometry. The level wall gap measures 22.3517418 nm. Both broadphase endpoints round to 1.0, while the public closest-point query still returns the nonzero distance from x=1.0 to the x wall. The tested rotated corner measures 81.1529344 nm from actual rounded world poses. Physical narrowphase therefore retains the gaps. A diagnostic `--rounded-boundary-control` intentionally quantizes the chart walls to float endpoints. The initial variant advanced past phase rejection and failed the pressure-work gate. The variant uses different geometry from MK2.

`--equilibrium-control` fills to one global plane on both sides of the five walls. It passes a stationary material step with exact volume conservation in both orientations. The level direct native control obtains valid fluid trials but never accepts an exact native fixed point in 16 trials. Rotated direct fails the pressure work limit. Both are separate from the original authored inventory and publish no impulses. `--native-wall-pose-control` uses actual rounded rotated wall centers before reconstructing their binary64 chart positions. It conserves the original rotated inventory while relocating 5.3775929604976002e-7 m³ and also reaches the pressure work limit.

The level global-wet endpoint trace shows a return-map slope of about −1.37 at release, so iterating it directly increases the response. The isolated `--bracket-control` solves the one-axis diagnostic against actual binary32 endpoints and accepts a proposal only if its chart displacement is unchanged. The two adjacent representable y values, 0.70007354021072388 m and 0.70007359981536865 m, map back to each other. This bracket independently rejects with no publication. It illustrates an endpoint-quantization limitation, even when a hydrostatic material solve succeeds.

## Final combined Linux rerun

The run in `final/` was compiled from the current shared solver sources and four freshly compiled MK2 implementation objects. Source fingerprints match before and after compilation. The combined binary SHA-256 is `f606548896173105d073382613170774ea2668528432271d1681be9f3e324ccb`.

The .25 m original and .20 m conservative authored seed still reject phase partitioning during both original warmup and the explicitly separate direct proposal. No impulse or material state was published in any of those wet negative runs. Level air-only passes one actual native tick with four commits. The sparse momentum solve allows the rotated dry case to reach its next valid limit, a resolved 0.000853 N·m pressure torque that the translation-only bridge cannot apply.

The stationary global hydrostatic control still passes one exact-volume material step per orientation. The level dynamic global-hydrostatic controls still reject the unstable direct iteration and the bracketed adjacent endpoint pair separately. Rotated dynamic global hydrostatic and the rotated native-wall pose control stop at the pressure work limit. The deliberately rounded level boundary control is physically different from native geometry. The final pressure refinement allows two material and native substeps for that control before the third rejects a donor inventory. It never finishes a rigid tick.

The final fixed-start timestep sweep reproduces six failures. The sole 1/3840 s one-shot acceptance retains 9.25 m³ and reports 28.759360373964011 m/s, with maximum mixture balance residual 4.9604056780850256e-19 m³. The sweep never commits. See `final/results.tsv` and `final/timestep.log`.

## Shared solver checkpoint

The four new qualification suites passed 49 checks in total. The existing
prototype suites and 26 project-policy checks passed. The translating box
completed 12 intervals across cut-region boundaries. Its peak shared face
transfer was 1.56635e-5 m³. Maximum total water drift was 9.99e-16 m³,
pressure-work residual 3.61e-16 J and momentum residual 1.13e-15 kg·m/s.
The level and cyclic-axis 4 mm dry-gap controls give 0.26487 N of gas
buoyancy and no water load. These are prototype controls.

Pressure and water/air transport now use one indexed physical face list
and the same swept control volumes. Transport owns the accepted constituent
fluxes and uses them to remap mass and momentum. Stationary thin regions
can relay material only along a clear, source-connected port path. Ordinary
donor roundoff reconciliation checks both constituent inventories and the
local balance before allowing exhausted carrier to round to zero. Sparse
momentum remapping handles a 326-region strongly connected component
previously rejected by the size of the dense system. The new sparse solve
requires an independently checked contraction and a residual.

The projection now refines measured physical divergence if either volume
balance or the pressure/capacity work pairing requires it. A basin trial
had 2.75e-9 J of discrepancy even though each volume residual was below
its local tolerance. The original wall quadrature agreed with the capacity
work to 5.6e-17 J. Independent pressure refinement moves this particular
trial beyond the work rejection.

### Separate experiments

The rejected well-mixed high-turnover model replaces the geometric relay
allowance in `experiments/CutRegionStep.cpp`. It propagates backward-Euler
phase-concentration bounds along actual fully open paths in stationary
high-turnover cuts. With the original level geometry and seed it completes
one MK2 substep in the direct run with zero measured water drift, then
rejects a donor ledger. The 240-step authored warmup commits one material
step, then rejects a phase circulation with no valid owner. The rotated
direct run continues to fail the phase ledger. It is excluded from the
checkpoint solver.

To rebuild this diagnostic, use `build.py --exact` with the same source
list as `run.sh`, replacing the `CutRegionStep.cpp` argument with
`experiments/CutRegionStep.cpp`. The optional instrumented
`experiments/CutRegionPressure.cpp` preserves the earlier one-refinement
work diagnostic. The original clock, controls and zero-publication
negative results are retained in `final/`.

A separate vented water-only storage derivation and two-region numerical
check are in `experiments/FORMULATION.md` and `experiments/two_storage.py`.
They remove atmospheric momentum while accounting for surface water
and work. They cover a cut-storage breakpoint and moving-wall volume
source. They do not implement a vented 3D basin or qualify MK2 coupling.

The production fluid routes, the Alpha lock and the original production
fixtures were untouched. This is a research checkpoint. The authored wet
basin, resolved angular load coupling, contact, arbitrary cut geometry
and a game-ready runtime remain unqualified.

The preceding native MK2 probes passed 67 checks. The geometric native
player probe passed 28 checks, including the existing swimming
trajectories. Those regressions exercise the preceding solver and are
separate from new cut-region swimming qualification. Trailing spaces
are normalized in the project policy log.
