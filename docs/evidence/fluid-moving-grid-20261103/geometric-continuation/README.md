# Coupled water, air and momentum through changing geometry

This isolated Experimental prototype follows
`57502315dba253ab0b0b33b5cd04529cafec7df8`. No production
runtime routing, original fixture, test tolerance, or Alpha policy
changed. The untouched production tests still exercise PBF.
Measured on Linux 7.0.0-42-generic with GCC 15.2.0 and an AMD Ryzen 5
5600, 2026-11-03 UTC.

## What changed

- The pressure metric starts with the actual water and air masses
  of its two geometric half-cells. Newly exposed cells route
  phase from existing inlet volume. A finite-mass inlet retains
  its own pressure difference and impulse. Only a selected inlet
  with zero old inertia uses a reduced pressure row and recovers
  its flow from local continuity.
- The volume solve still enforces its original balance before it
  publishes flux. A separate bounded circulation repairs the
  water/air split when the preferred geometric face partition
  cannot satisfy the actual ending cell capacities. Face support,
  old donor budgets, receiver capacity and reachable phase
  sources are checked. An admitted shrinking cell can relay
  incoming material through physical openings instead of
  borrowing from a fictitious old phase.
- Momentum uses the measured water and air half-storage, real
  phase traces and an explicit phase transfer ledger. Each phase
  is transported conservatively and their velocities are mixed
  by mass. Endpoint-representation changes and interface/face
  mixing are measured separately. A dry transverse opening
  cannot carry water merely because the coarse volume balances.
- The pressure-active share of each face's old inertia is its
  mean aperture divided by the largest endpoint/mean aperture.
  For a step-open aperture this is its open-time fraction. For
  an arbitrary area variation it is a bounded approximation.
  Pressure applies the corresponding impulse to the owning
  material and records the energy lost when the active share is
  mixed back into the rest of the owner.
- The coupled interval only publishes a complete external step.
  A geometrically limited transport or opening balance can retry
  using shorter internal steps, preserving the external cadence.

The fixed inlet oracle uses a solid with its base at .49 m and
tests displacement .01 m ± 1 µm. The open path must carry exactly
`A*1e-6` m³. The predicted finite pressure difference is
`rho*occupiedLength*(.01+1e-6)/dt²`, 1890.189 Pa. The full-interval
inertia probe applied 18.90 MPa and produced a 7.89 N·s impulse
jump. Open-duration inertia produces 1890.189 Pa and a 0.0131
N·s difference across the two paths. This is an independent
activation control for the prescribed cut, not a universal added
mass oracle.

The earlier release failed before the first opening. Source logs
here preserve the zero-source phase, donor budget, receiver
capacity, moving return-stroke and ill-conditioned opening
failures. An optimization that deferred the entire momentum
check until after the rigid pressure root changed which trial
paths were accepted and failed at 3.57 seconds. It was rejected.
Its recorded source delta and trace are under `failed/`.
The discarded signed bounded-flow helper was independently
checked and remains archived in the ignored experiment workspace.
It has no runtime or prototype consumer.

## Measured results

| Check | Result |
|---|---|
| Standalone prototype | 866 passed |
| Native MK2 | 65 passed across represented, reconciled and mixed probes |
| Native PlayerController | 28 existing checks and 28 geometric checks passed |
| Existing Alpha project policy | 26 passed |
| Sealed 0.25 m pool, 720 release steps | Completed with 3.36e-13 m³ maximum volume drift |
| 0.25 m pool settling | Early/late RMS vertical speed 0.36488/0.01053 m/s, late peak 0.02083 m/s |
| 0.25 m pool immersion | Late side/exterior fractions 0.45724/0.45631. Original 3–4 s window 0.46347/0.48968, with speed 0.14328 m/s at 4 s |
| 0.25 m pool preparation | 0.12811 m/s mean water speed against the original 0.05 m/s diagnostic. Failed |
| 0.25 m pool release cost | 88.75 ms median, 176.89 ms p95, 383.55 ms worst. 490 of 720 steps split, up to 4 accepted substeps |
| 0.20 m pool | 32 accepted steps, then rejected unsupported midpoint phase transfer at 0.5333 s |

The 0.25 m pool's peak pressure-work and rigid-impulse errors
were 2.63e-8 J and 4.48e-8 N·s. Timing was collected during the
qualification run with other work on the host and is diagnostic.
The native harness linked a pre-existing engine archive with SHA-256
`170a5306ef4a8187555452fca01aa804366c8fa8a380ef3856fba32a8af36823`.
This checkpoint adds no engine runtime selection or release unlock.

## Reproduction

```sh
src=tools/prototypes/fluid_volume_grid
evidence=docs/evidence/fluid-moving-grid-20261103/geometric-continuation
out=/tmp/judas-geometric-continuation
bash "$src/qualify_moving_grid.sh" "$out"
python3 "$src/run_grid_native_player_tests.py" .cache/2d-stage1-build
python3 "$src/run_grid_native_player_tests.py" .cache/2d-stage1-build --geometric
python3 "$src/run_grid_native_mk2_tests.py" .cache/2d-stage1-build "$out/mk2"
JUDAS_ENGINE_ROOT="$PWD" .cache/2d-stage1-build/judas_project_fluid_policy_tests

for driver in qualification-pool qualification-pool-fine-local; do
  c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$src" -Itests \
      "$evidence/$driver.cpp" "$out/MovingVolumeGrid.o" \
      "$out/MovingVolumeMomentum.o" "$out/HalfDualVolumeMomentum.o" \
      "$out/CoupledVolumeBox.o" "$out/GridHalfCellGeometry.o" \
      "$out/GridGeometricMomentum.o" "$out/GridConservativeRemap.o" \
      "$out/GridPhaseFluxRepair.o" "$out/GridParcelSeed.o" -o "$out/$driver"
done
"$out/qualification-pool" 60
# Expected exit 3 at the unsupported midpoint.
"$out/qualification-pool-fine-local" 60
```

The 0.25 m grid pool matches the preserved source parcel omission,
water quantity, box dimensions and mass, preparation time, 60 Hz
external release clock, and original observation windows. It uses
a sealed rectangular chart and a translation-only box. The source
scene has an open basin and authored wall gaps. Its complete native
geometry and its original particle observation are distinct
qualification requirements. The grid run measures wet area on the
actual side patches and independently computes a volume-equivalent
exterior water level from columns separated from the body. The
maximum traced exterior height is retained as another diagnostic.
`--strict-warmup` checks the preserved 0.05 m/s preparation
diagnostic as an exit gate.

The nominal 0.20 m chart uses the original binary32 parcel omission
and retains the exact sum of the assigned chart parcel volumes.
The original corner placement makes several occupied cells
overfull by a combined 8.58e-9 m³. The seed importer routes this
amount over at most two open-face steps without losing water.
Radii 0 and 1 reject. This chart still approximates the actual
binary32 parcel support and original authored wall geometry.

The native mixed probe drives one real MK2 tick with pressure,
qualifies an exactly represented endpoint or a bounded conservative
binary32 remap, and compares a 4 mm dry gap with the same air-only
geometry. The exact-only attempt encountered adjacent binary32
endpoint cycling, preserved with its source. No production motion,
contact/CCD, joint, terrain, save or renderer integration is
implied by the native probe.
