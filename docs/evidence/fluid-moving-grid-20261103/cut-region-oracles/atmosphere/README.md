# Atmospheric liquid-only column control

The existing moving-grid experiments evolve incompressible water and air
in a common volume projection. The static/phase geometry already clips
wet storage and wall faces, but the active projection has no atmospheric
Dirichlet free-surface graph. The original mixed-pool failures, source
and exact trace remain in `../static-wall-union/pressure-gate/` unchanged.

This scratch experiment instead admits a constant-area vertical liquid
column with a vented atmosphere, a prescribed bottom piston, and a
kinematic upper water surface. Only wet control volumes have pressure
unknowns. The gas has no pressure solve, momentum or donor inventory.
The pressure gauge is exactly zero at the liquid-air interface. A trapped
gas pocket must be rejected or represented separately. This prototype
explicitly rejects the unvented case and unsupported tensile piston
contact.

## Discrete operator

For pressure link `f`, let `ell_f` be its actual geometric length,
`u*_f` the predicted normal velocity, `A` the column area, and `rho` the
water density. The projection uses

```
u_f = u*_f + dt * (p_left - p_right) / (rho * ell_f)
q_f = dt * A * u_f
q_high - q_low = 0
p(surface) = 0
u(bottom) = u_piston
```

Internal unknowns lie at the centres of the actual wet intervals, including
partial end cells. The free surface uses the true liquid-centre-to-surface
distance, not half a nominal cell. The piston condition fixes velocity,
and its wall pressure is extrapolated from the same normal impulse.
Summing the momentum changes against the projected velocity gives

```
p_wall * q_wall = ΔK_pressure + 0.5 * Σ(m_f * (u_f - u*_f)^2)
m_f = rho * A * ell_f
```

The surface flux advances the boundary `H_new = H_old + dt*u_surface`.
It cannot be discarded merely because pressure is prescribed there.
Clipping the moved lower and upper planes back to the fixed cells gives
their new liquid volumes. The operator checks continuity, reconstructed
water volume and pressure work before publishing. Side wall pressure
queries use the actual wet intersection and piecewise-linear solved trace.

## Result

- Static analytical hydrostatic pressure error `1.82e-12 Pa`. The gauge
  pressure at the atmospheric interface is exactly zero.
- A partially wet probe wall received `1.67873625 N`, matching its analytic
  traction. A probe just `4 mm` above the surface in the same coarse cell
  received exactly zero area and force. A deliberately wrong cell-tag
  ownership control would apply `2.02699125 N` to that dry probe.
- The prescribed moving piston recovers the independent added mass
  `rho*A*(surface-bottom)` and force `-4815.2390625 N`. The work residual is
  `5.33e-15 J`. It advances the interface across a coarse face and places
  `0.005625 m³` in the newly wet cell while preserving total liquid volume.
  Freezing the surface would instead lose `0.0075 m³` in that interval.
- A nonuniform velocity predictor tests the actual pressure solve against
  an independent link-by-link analytical impulse. Force error `9.10e-13 N`.
- Unvented atmosphere and tensile piston separation reject without
  publishing any changed pressure.

The column avoids a **dry-air donor budget** because this is a different
boundary-value model with no advanced gas phase. Its conservativeness
requires actual interface motion, pressure trace and wet geometric
ownership. Omitting air from the original mixture pressure solve without
those changes would not be equivalent. This column contains no lateral
advection, thin *wet* donor pressure topology, wave dynamics, arbitrary
moving rigid bodies, or actual authored-pool trajectory. Its successful
controls do not clear the original mixed-fluid or Alpha gate. A meaningful
next test would use two communicating liquid reservoirs, including a thin
wet aperture, with an atmospheric boundary above each. It should preserve
volume, route new wet storage, and demonstrate matched force/work under
refinement before integrating the actual authored pool.

## Reproduction

From the repository root:

```sh
d=.cache/fluid-moving-grid-20261103/atmospheric-liquid-control
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror "$d/AtmosphericColumn.cpp" -o "$d/column"
"$d/column"
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie \
  "$d/AtmosphericColumn.cpp" -o "$d/column-asan"
ASAN_OPTIONS=detect_leaks=0 "$d/column-asan"
```
