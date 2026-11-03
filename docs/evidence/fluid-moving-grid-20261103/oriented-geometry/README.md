# Oriented grid geometry controls

The static test clips independently rotated boxes against cells, shared
faces and a fixed planar water reference. The swept test compares signed
solid transport and pressure work against endpoint capacity. An independent
quaternion pose and 1,024 midpoint samples check the angular aperture
integral. A frozen start-wall load is the negative control.

The benchmark invokes the swept geometry operator 11 times for a single
box on the same 125-cell grid for each motion. It records the median and
maximum; individual timings were not emitted. The first binary built
unneeded volume caps and patch second moments at each intermediate
sample. The second only clips the wall polygons and measures the face
apertures, with tighter conservative spatial bounds. Both retain the
same endpoint volume and time-integration checks. These are geometry
measurements on one Linux host. They exclude the pressure solver,
transport, engine integration and rendering.

Run from the repository root. The original before and after builds used
the same command and benchmark source, with the implementation present
at each stage. The earlier executable remains in the local cache and is
identified in `provenance.txt`; its intermediate implementation source
was not captured separately. The commands below reproduce the optimized
benchmark and both correctness gates from their checkpoint.

```sh
mkdir -p .cache/moving-grid-geometry
src=tools/prototypes/fluid_volume_grid
ev=docs/evidence/fluid-moving-grid-20261103/oriented-geometry
g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror -I. \
  "$ev/sweep_benchmark.cpp" "$src/GridOrientedGeometry.cpp" "$src/GridOrientedSweep.cpp" \
  -o .cache/moving-grid-geometry/sweep_benchmark_repro
.cache/moving-grid-geometry/sweep_benchmark_repro

g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
  "$src/GridOrientedGeometry.cpp" "$src/GridOrientedGeometryTests.cpp" \
  -o .cache/moving-grid-geometry/oriented_tests
.cache/moving-grid-geometry/oriented_tests

g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
  "$src/GridOrientedGeometry.cpp" "$src/GridOrientedSweep.cpp" "$src/GridOrientedSweepTests.cpp" \
  -o .cache/moving-grid-geometry/sweep_tests
.cache/moving-grid-geometry/sweep_tests
```
