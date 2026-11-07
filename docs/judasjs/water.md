# Basic Water and Grid Water V2

[API index](../JUDASJS.md) · [Types](../judas.d.ts) · [Water status](../GAME_WATER_PREVIEW.md)

Basic Water is inexpensive prescribed water, enabled by default. It renders and
supports swimming without a fluid solver. `entity.basicWater` and
`entity.setBasicWater(patch)` configure that existing system.

Grid Water V2's [continuous presentation](../WATER_V2_PRESENTATION.md) follows
its physical levels at a finer render resolution. It includes depth/refraction
and above/below-water presentation. Its visual waves and optical continuation
are not used by `fluidSample`, buoyancy or quantity accounting. Author colour and
opacity on the Fluid Volume; opacity controls absorption/tint, while interface
reflections remain visible even in clear water.

Grid Water V2 is optional physical water in **Experimental source builds**.
Alpha's Advanced Fluid Dynamics lock remains effective. A project must explicitly
enable **Project Settings → Experimental → Advanced Fluid Dynamics** before Play
or export. Authoring a component does not bypass that policy.

## Authoring a region

Add **Fluid volume** to a stationary object. New editor components select Grid
Water V2; old particle-authored scenes retain their backend. The owner is the
domain's bottom centre, with local +Y pointing away from gravity. Positive
uniform scale is supported. Set cell size, domain cell counts, initial fill
height, density, colour and opacity. Use colliders for walls and other solids.

The exact scene keys, inside an ordinary object block, are:

```text
fluid-volume
fluid-volume.backend grid
fluid-volume.spacing 0.4
fluid-volume.count 30 12 20
fluid-volume.emitter false
fluid-volume.emitter-offset 0 0 0
fluid-volume.max-particles 0
fluid-volume.fill-height 2
fluid-volume.density 1000
fluid-volume.color 0.035 0.3 0.38
fluid-volume.opacity 0.82
```

This is a 12×4.8×8 m domain, initially filled to 2 m. Increasing cell count costs
simulation time and memory; this is not a free ocean system. Preserve real
features at your chosen resolution. Below-resolution geometry and bubbles are
approximate. The water domain itself is stationary; objects can move through it.

## Queries and sources

`world.gridWaterRegions` returns current water-owner entities.
`entity.gridWater` is a `GridWater` handle or null. Its copied `state` includes
quantity, explicit displaced storage, source/drain receipts, accounted air mass,
cell dimensions and last-step diagnostics. `airMass` includes the signed exterior
reservoir receipt; it is a conservation ledger, not the mass of sealed air alone.
`simulationMs` includes staging, gravity/geometry preparation, the solver and body
loads. `solverMs`, `preparationMs`, `couplingMs` and `pressureMs` separate the costs;
`geometryBakes` counts actual static rebakes. `bytes` includes both retained grid
buffers (not every temporary or renderer allocation). Timings refer to the last
completed step. Neither querying nor rendering advances the simulation. A paused
region's `error` is its own failure diagnostic; normal game pause is separate.

```js
import { world } from 'judas';

export default class {
    constructor() { this.state = { supplied: 0 }; }
    fixedUpdate(dt) {
        const owner = world.gridWaterRegions[0];
        if (!owner?.gridWater) return;
        // External pump: account only for the volume the pool can receive.
        const accepted = owner.gridWater.changeVolume(
            { x: 0, y: 2.5, z: 0 }, 0.02 * dt, 0.8);
        this.state.supplied += accepted;
    }
}
```

`changeVolume` uses signed cubic metres and returns the accepted signed amount.
Positive sources require capacity connected to atmospheric air; sealed/full
receivers apply backpressure. Negative amounts remove available water. A source
does not silently discard the unaccepted part or remove hidden displacement
storage. Put that receipt into project-owned inventory/resource rules.

`impulse(point, momentum, radius = 0)` transfers an external momentum impulse to
nearby water. Both mutations run in `fixedUpdate` in an authoritative world.
Invalid/nonfinite inputs throw; this API does not hide an excessive request behind
an arbitrary speed clamp. A pressure/CFL failure is reported explicitly.

The existing `world.fluidSample(point, up, halfHeight, radius, tangent)` queries
actual occupancy for swimming, including Grid Water. Existing Basic Water
swimming code can keep that query. Water quantity comes from the grid, not the
visible mesh or cosmetic particles.

Save slots include a versioned grid-water participant. Legacy world-state delta
saves deliberately reject physical fluids; use the normal slot/save APIs. Missing
grid state must reject loading rather than reseeding the initial fill.

See the current water status for qualified shapes, gravity, streaming and platform
boundaries. This reference does not constitute an Alpha unlock or human acceptance.
