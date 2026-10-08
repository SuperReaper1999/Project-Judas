# Retained legacy particle-fluid demonstration

For current conserved reservoirs/containers and dynamic surfaces, see
[Liquid reservoirs](LIQUID_RESERVOIRS.md) and [Liquid surfaces](LIQUID_SURFACES.md).
This project preserves the accepted legacy PBF/cavity repair; its 20 Hz cadence
and deferred quality limitations do not define the M54/M55 path.

Open `projects/fluid_demo/fluid_demo.judasproj` in the editor, or run:

```sh
./build/judas projects/fluid_demo/fluid_demo.judasproj
```

This ordinary project registers `Scenes/pool.judas` and `Scenes/planet.judas`.
F1 loads the flat pool; F2 loads the planet. Escape opens the project-authored
pause menu. R reconstructs the current scene. No historical evidence is loaded.

## Controls and content

WASD and mouse move/look. Space jumps on support and supplies swimming propulsion
when immersed. Walk down the seven steps into the pool; walk back up to leave.
G picks up/drops the targeted body; H throws it. Orange and purple blocks have
respective densities 500 and 2,000 kg/m³. Water's reference density is 1,000 kg/m³.

The clear-sided open bucket starts **empty**. Pick it up from the right-hand
side of the deck. Move to the unobstructed right-hand part of the basin, lower
and tip the opening into the water, then raise the view to bring it upright
before lifting it out. Carry it away; tip it again to pour. Do not lower it onto
the blocks you threw into the pool: those are real solid obstructions. There is
no fill state, water inventory script or particle creation during this process.

Both scenes use the same 6.5 × 6.5 m initial water footprint, about 2.6 m deep,
380 particles at 0.65 m spacing, 20 Hz liquid / 60 Hz rigid physics, 16 rigid bodies and one empty cavity. This is a
coarse production demonstration, not a fine-detail water simulation. The bucket
is a resolved 1.9 m-wide vessel, mass 1,500 kg; each fluid particle represents
274.625 kg. Smaller cups require finer configurable particle spacing and cost
more CPU. Analytic exterior buoyancy does not require increasing particle count.
All particles continue to simulate; none are frozen or replaced by decoration.
The project explicitly chooses the existing lower-rate fluid setting to retain
interaction headroom while scooping. The engine default remains 30 Hz.

The planetary scene uses a real 30 m-radius sphere and radial gravity. Its basin
lies on an oblique surface patch; the spherical surface is the basin bottom.
Gravity direction varies across the water. The pool scene uses a flat floor and
uniform gravity. Neither implementation knows which demonstration it is running.

## Scope

The accepted approximate production PBF/hydrostatic/cavity/controller model is
unchanged. Exterior fluid/body momentum is not exactly conserved. Water surface
extraction and transparency are visual approximations. This is a local planetary
basin, not a planet-wide ocean. No new character controller, fluid research solver
or M49 capability is included.

The measured regression investigation, automated checks, performance and raw
scoop/carry/pour particle counts live in `docs/evidence/fluid_demo_refresh/`.
Human visual and interactive acceptance is still required.

## Deferred production-fluid improvements

The October 2026 performance repair substantially improved the production demo
and is accepted for checkpointing, not final fluid-quality acceptance. The model
remains coarse/approximate; the current demo requires 20 Hz liquid cadence for
acceptable interactive performance, while the engine default remains 30 Hz.
Planetary/container interactions can still show numerical roughness, including
transient velocity spikes. Overall fluid performance, stability and
visual/interaction quality are not considered final. A dedicated future
production-fluid improvement pass is intentionally deferred rather than expanded
now.

## Operator checklist

### Flat pool

1. Walk into the pool and swim.
2. Leave the pool normally using the steps.
3. Throw both blocks in and watch their different responses.
4. Pick up the bucket.
5. Dip/tilt it to scoop actual water out of the pool.
6. Carry the captured water away.
7. Pour it out.
8. Confirm interaction remains comfortable throughout.

### Spheroid planet

9. Move around the curved surface.
10. Enter and swim in the basin.
11. Exit using the steps.
12. Throw an object into the water.
13. Scoop planetary water into the bucket.
14. Carry and pour it.
15. Check that motion follows local gravity rather than world -Y.
16. Confirm interaction remains comfortable throughout.

Export with the editor's normal Export Project command and repeat in the moved
standalone package. Automated checks do not substitute for this review.
