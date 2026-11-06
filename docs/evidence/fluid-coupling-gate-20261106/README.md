# Conservative carrier / pressure-flux feedback control

Dependency: the checked `CutMassMomentum` implementation at Experimental
`fc90aa1e` (full hash in `qualification.json`). No native solver or fluid lock
change. This is a bounded reduced experiment, not an integrated basin fix.

A stipulated transverse inlet carries zero velocity in the tested component.
An equal axial outflow has mass carrier `k*u`, with `k=rho*dt*A`. The body of
fluid retains mass `m`; an imposed component impulse is `J`. The declared
backward-Euler conservative momentum row is `(m+k*u)*u=J`.

The normal and ASan/UBSan/leak-checked runs each pass 24 controls. They compare
the existing carrier operator with an independent analytic quadratic root,
exercise both fast and reference backends, close the discrete work identity,
and demonstrate failure of the frozen-inertia/postprocessing shortcut for
these coupled equations. The old numerical answers remain as negative controls.

The input mass, area and impulse magnitudes match a fast-face diagnostic,
but the boundary policy is prescribed. There is no full channel geometry,
unknown pressure-field solve, moving body or original-basin trajectory here.
A different operator split is not globally disproved by this test; it would
require its own complete numerical model and qualification.

Reproduce from the repository root:

```sh
bash tools/prototypes/fluid_volume_grid/qualify_internal_transport.sh /tmp/judas-coupling-objects
mapfile -t objects < /tmp/judas-coupling-objects/objects.list
c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror \
 -I tools/prototypes/fluid_volume_grid \
 docs/evidence/fluid-coupling-gate-20261106/CoupledCarrierGate.cpp \
 "${objects[@]}" -o /tmp/judas-coupling-gate
/tmp/judas-coupling-gate
```

For sanitizers, compile both the objects and this executable with
`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, and run with
`ASAN_OPTIONS=detect_leaks=1` outside an incompatible tracing wrapper.
There are no performance claims from this one-cell test.

The result requires compatible velocity-dual mass carriers and feedback into
the pressure/flux solve before accepting an implicit integrated correction.
The shortcut was not adopted. Original basins still have extreme face speeds.

[Full investigation](../../FLUID_MOMENTUM_GEOMETRY.md).
