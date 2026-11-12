# Advanced Fluids V2 — current architecture

The playable game-water implementation passed operator gameplay and visual review
on Experimental. **Advanced Fluids V2 is available by project opt-in in every V2 build channel.**
The obsolete Alpha-channel restriction is removed; no release or main promotion is implied. Basic Water and swimming
remain available independently, with no advanced solver allocation by default.

## Authoritative implementation

`src/fluid/GameWater*` owns physical grid state, geometry preparation, transport,
pressure, air reservoirs and MK2 coupling. RuntimeWorld owns each authored water
region through GameWaterRuntime. `WorldWater.cpp` supplies shared current water
queries/swimming. Renderer V2 and the water shaders own presentation; visual
particles are not authoritative physical water.

The physical model deliberately uses game-scale approximation:

- Fixed regular 3D cells own water quantity in binary64; bounded paired transfers
  respect inventory and available capacity. Velocities are binary32 with a bounded
  binary64 pressure projection. No multiprecision runtime is required.
- Sampled solid occupancy and face apertures supply consistent coarse capacity and
  connectivity. Below-resolution disconnected free spaces can seal conservatively;
  microscopic geometric fidelity is not claimed.
- Moving supported rigid shapes change capacity. Unplaceable displaced water stays
  in explicit measured storage and returns when capacity becomes available; it is
  not silently deleted.
- Resolved trapped-air reservoirs retain mass and finite compressible storage.
  Open boundaries use atmospheric pressure; openings have finite venting. Unresolved
  bubbles/foam use the documented coarse free-surface approximation and air ledger.
- MK2 bodies receive hydrostatic buoyancy and implicit drag, with reaction applied
  to nearby water. This is not a monolithic added-mass solve or exact global energy/
  angular-momentum conservation. Numerical transport diffusion is declared.
- Physical quantity and presentation resolution are separate. The fine surface,
  waves, refraction and underwater attenuation follow the physical levels without
  adding physical water or forcing simulation at visual resolution.

## Lifecycle and authoring

Add a Fluid Volume to a stationary owner, configure grid size/fill/density and opt
in through Project Settings on an eligible build. There is no backend selector,
PBF emitter, reservoir/container component or liquid-bake dependency.
[JudasJS water reference](judasjs/water.md) gives exact keys and controls.

Current saves use the existing modern archive/service architecture. Existing region
and origin admission rules still reject unsupported combinations explicitly.
A physics region cannot be evicted merely because the camera looks away; current
simulation ownership and support determine residency.

## Demonstration and qualification

Open [Water V2 Workshop](../projects/water_v2_workshop/README.md). It demonstrates
filled pools, flow, moving props, floating/sinking, finite air behaviour, original
basin geometry, rotated/radial cases, swimming and cold saves. See
[physical model and measured limits](GAME_WATER_PREVIEW.md),
[presentation](WATER_V2_PRESENTATION.md), and the
[purge qualification](ARCHITECTURAL_PURGE.md) for checkpoint-specific results.

The old PBF and microscopic conservative-pressure prototypes are retired from the
current engine. Their unsuccessful experiments and reference results remain in
Git history; they are not production backends or pending implementation stages.

## Boundaries

This is finite-resolution game water, not scientific two-phase flow, arbitrary
planet-wide oceans or full atmospheric simulation. Domain movement, changing
resolution and universal streaming/replication are not implied. Sub-resolution
channels, bubbles and solid details are approximate. Use the explicit stored-volume,
air-ledger, preparation-time and error diagnostics when setting content budgets.
