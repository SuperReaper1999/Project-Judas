# Internal flux and phase-front transport: bounded prototype

Status: **incomplete, original basin intervals still rejected**. This is a
continuation of the connected-owner work at `b0b2abc852ebd9544a6b55cd0341d4083367155a`.
It is outside the engine runtime, SDK and native build targets. Advanced Fluids
remain locked. Basic Water, main and independent research are unchanged.

## Implemented pieces

- `CutOwnedFlux`: recovers internal face fluxes by a minimum-change kinetic
  projection inside each connected owner. It can turn through a nonrectangular
  connected path, without filling a bounding box or bridging a wall. A separate
  graph projection enforces incompressibility in initially gas-free owners.
- `CutPhaseFronts`: evolves phase geometry in time rather than freezing the
  initial donor face material for an entire interval. It handles finite front
  arrival and outlet changes in straight channels, paired conservative owner
  ledgers, geometric inventories, gas-only ports and transactional rejection.
- A pressure trial keeps its external field and owner divergence sources fixed
  during transport. Merely acquiring a tiny gas sliver must not release a
  pressure constraint midway through that trial.
- `ProjectCutOwnedMaterial` can return an explicitly diagnostic pressure trial
  separately from its physical output. Rejection still preserves physical
  output; a diagnostic trial is not an admitted timestep.

The graph, geometry, flux arrays and primary quantities use binary64. The small
local dense kinetic solve uses native `long double` to reduce cancellation in
thin-fragment systems. This is not a 113-bit runtime solver; native extended
precision and its portability have **not** been qualified beyond Linux here.

## What the new controls establish

One- and three-fragment channels fill in both directions. Gas leaves first;
after the front arrives, water leaves. The final material inventories and
inlet/outlet ledgers agree, repeat runs are deterministic, and internal flow
also follows a right-angle connected path. Invalid inputs, incompatible sealed
water sources, exhausted budgets and water reaching gas-only ports reject
without publishing partial results.

These are scoped transport controls, not moving-body or native fluid acceptance.
The new field correction is not yet integrated with published pressure/wrench,
momentum evolution or a complete EOS-coupled fluid timestep.

## Original basins and the actual remaining mechanism

Both preserved authored fixtures were exercised using their existing geometry,
water import, owner mapping, gravity, native tick and top vents. The body is
frozen, as in the previous admission reproduction. Neither advances a complete
interval. Existing coarse pressure closure still succeeds; admission rejects.
The new transport also rejects rather than publishing negative phase inventory.

A three-region counterexample isolates why subdividing time is insufficient:

1. A thin region has two **disjoint** apertures on its upper face. Water enters
   one; material leaves the other. Both connect through real, open geometry.
2. The current front representation chooses an axis-aligned plane from the
   incoming flow normal. Any positive water produces a film across both ports.
3. In this case the initial water rate is 1; the endpoint rate becomes 0.
   One Heun step gives `h/2`, whereas two half steps give `h/4` in the thin region.
4. Measured central error divided by `h` is exactly `0.25` for `h` from `1e-8`
   to `1e-14`; the owner L1 error divided by `h` approaches `0.5`.
   The normalized error therefore does not converge as time is subdivided.

This is a representational/transport closure failure, not a reason to relax the
accuracy gate. It does not prove every possible interface reconstruction fails.
It specifically rejects this normal-selected single-plane, fragment-wise
explicit evolution as sufficient for the authored basins.

Exploratory swept-prism and conservative outgoing-flux-budget variants were also
rejected: they did not complete the basin interval and encountered vanishing
time increments or repeated budget exhaustion. Their source and measurements
are retained. They are not selected as the implementation or described as fixes.

## Required next engineering change

Keep the connected physical owners and immutable boundary fragments. Replace
independently evolved fragment stocks with conservative **unsplit space-time
transport within those owners**, retaining inlet/outlet footprint and path
ownership until phase volume has actually crossed the region. Reconstruct a
spatial interface from that transported material, rather than immediately
spreading a new film over every coplanar aperture. Pressure must be re-solved
when a whole physical owner changes its gas/constraint state; treating geometric
fragment gas birth as that event is wrong.

The first acceptance gate is the coface counterexample, then the original
basins with their existing EOS, phase bounds and conservative ledgers. Do not
optimize or integrate this prototype into MK2 while those remain rejected.
No damping, quantity clamps, wall closure, tiny-phase deletion, vent inflation,
or weakened tolerances were adopted to get a basin pass.

## Evidence and scope

[Raw results and reproduction](evidence/fluid-internal-transport-20261106/README.md).
The older evidence folders remain unchanged. No native gameplay, rendering,
performance improvement, Windows/Android, moving-body, cold-save or streaming
acceptance is claimed by this checkpoint. There is no new playable fluid build
to human-test yet.
