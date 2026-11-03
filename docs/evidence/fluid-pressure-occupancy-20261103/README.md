# Pressure-support and occupied-volume evidence

[Technical summary](../../FLUID_PRESSURE_OCCUPANCY_20261103.md).
The baseline is in `provenance.txt`. All modified solvers were built in
ignored scratch directories against the existing GNU Release build.
No prototype is part of engine runtime.

- `joint/` contains the column phase trace, prescribed box discovery
  implementation and re-link scripts. See its README for exact commands.
  Decompress the archived headers and patches in the scratch directory.
- `dual/` contains alternate solves, linearization and geometry traces,
  tangent smoothing, and the whole-basin volume and kernel measurements.
  Copy `build.py`, `cg.txt`, `step*.txt`, and `ImplicitPressure.h` into a
  new `.cache/<experiment>`. For the warmup-only driver, generate
  `WarmupTest.o` as described in `joint/README.md`. The `--full` mode
  uses the existing unchanged production test object. Examples are
  `python3 .cache/<experiment>/build.py pgs512 half_025` and
  `python3 .cache/<experiment>/build.py moving-prescribed --full half_025`.
  `trace.py` reruns the two PGS budgets passively. It reports substep
  followed by maximum linear violation, nonlinear predictor difference,
  geometry-induced density difference, final violation, the corresponding
  three means, and the count of changed endpoint coordinates.
- `column/` is a separate layered-column fixture with analytic admissible
  reaction multipliers. Copy `build.py` and `column.cpp` into a fresh
  `.cache/<experiment>`, then call the script with no arguments for baseline
  or with a name and the path to a replacement FluidWorld object. The
  rotated trace contains a laterally unstable unconfined column. Compare
  the initial rows and separate no-floor control.
- `geometry/` preserves both pre-release particle inputs as compressed
  original CSVs and contains an independent occupancy calculation and
  analytic clump control. Decompress inputs and use the centres listed in
  its README. The scripts originally read the respective live scratch runs.
- `grid/` contains the before/after deep-pool experiment and exact patch.
  Copy both Python scripts to `.cache/<experiment>`. The first script
  reads the original tracked `tools/prototypes/fluid_volume_grid/VolumeGridPrototype.cpp`
  from an equivalent copy under `.cache/fluid-volume-grid-20261102/volume_grid.cpp`.
  The current tracked source matches that copy byte-for-byte. Run
  `grid-deep-pool.py`, preserve its expected failure, then copy its source
  to `grid-deep-pool-original.cpp` and run `grid-rest-projection.py`.
  For the older gates, replace only the last test driver with
  `int main(){Gates();}` in a copy of the generated candidate and build it.
- `mirrored/` identifies the full rejected mirror archive kept locally.
  Its standalone exact face-reaction controls pass but loaded-pool tests fail.

Raw timing in these logs is diagnostic. Some runs overlapped other local
work, and the candidate grid and engine are different representations.
