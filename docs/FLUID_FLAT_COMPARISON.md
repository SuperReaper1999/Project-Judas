# Terrain planet: flat-ground fluid comparison

This is an Experimental reproduction, not a solver repair or Alpha unlock.
The original `assets/scenes/terrain.judas` remains unchanged.

## Launch

Run the Experimental runtime with `assets/scenes/terrain_flat_comparison.judas`.
It resolves the existing `judas_tech_demo.judasproj`, which explicitly opts into
Advanced Fluids. No second root project is required.
Use `JUDAS_WORLD_STATE=none` for the authored initial state; R resets the world.
Advanced Fluid Dynamics is explicitly enabled in this project. Alpha builds
must continue rejecting it.

## What changes

- Replace the radial terrain with an 80 x 80 m static box ground patch.
- Ground top is world Y=77.353492 m, the original radial surface height beneath
  the lake centre (computed from TerrainDemo's elevation function along world Y).
- Ground normal is +Y; uniform gravity is (0,-9.81,0) within a 200 m half-extent
  box region encompassing this comparison. This is not an infinite gravity field.
- Remove the planet's celestial source and atmosphere.
- Change spacecraft gravity from celestial to local, so it also uses uniform gravity.
- Keep all seven remaining objects' world positions, rotations, dimensions,
  materials, masses, initial velocities and fluid settings unchanged.
- Uses the original project with a distinct scene identity/save filename.
  Use `JUDAS_WORLD_STATE=none` for controlled comparisons.

The authored water remains 125 particles, 0.5 m spacing, original location and
orientation, emitter and limit. No retaining walls are added: flat ground removes
both curvature and the original basin, so eventual spreading differs. Atmosphere
removal also changes atmospheric effects. This is a requested flat-world comparison,
not a single-variable proof isolating gravity from all other physical differences.

## Numerical types in the actual particle path

| Stage | Representation |
|---|---|
| Particle position, velocity, mass, acceleration | float / glm::vec3: binary32 |
| Gravity samples, rigid poses, particle boundary geometry | float vectors/quaternions |
| PBF densities, gradients, corrections and velocity smoothing | binary32 |
| Hydrostatic cached particle field | binary32 |
| Hydrostatic query basis, column intervals and weighted sums | double / glm::dvec3: binary64 |
| Published immersion fraction, sampled velocity and acceleration | converted back to binary32 |
| Buoyancy volume/force/torque accumulation | binary64, starting from float geometry/samples |
| Published buoyancy force/torque and drag path | binary32 |
| MK2 particle contact interface | binary32; optional normal acceleration uses binary64 internally |
| Pending fluid interval / recorded motion timestamps | binary64, converted to float for FluidWorld::Step |

The PBF loop also accumulates double diagnostic values to measure correction
reconstruction error; that diagnostic does not make authoritative particle state
double precision. No binary128, arbitrary-precision fractions or extended-precision
fluid solve is used in these inspected production paths. Integer spatial-cell keys
are indexing, not extra floating-point precision. Promoting floats to doubles
cannot recover information already rounded in particle storage.

`fluid-scale 10` means the default 0.018 m collision radius becomes 0.18 m,
0.105 m smoothing radius becomes 1.05 m, and 0.025 m maximum density correction
becomes 0.25 m per iteration. Particle spacing remains separately authored at 0.5 m.
At default density 1000 kg/m^3, particle mass is 1000*(0.5^3)=125 kg, for a total
15,625 kg represented water mass. This is a coarse physical sample set, not
125 ordinary small droplets.

## Preserved pre-repair smoke evidence

Runtime: `.cache/2d-stage1-build/judas`, Experimental fb77396, engine sources
identical to cafe797a (the intervening checkpoint changes only docs/evidence).
Linux NVIDIA GTX 970; 90 application frames, frame seconds 0.016666667,
no input, no saved state. Process exited successfully; start/end captures written.
Harness output confirms (0,-9.81,0) gravity. After the second fixed step the ship
velocity is approximately (2.841258,5.095192,-19.233284) m/s, magnitude 20.10 m/s.
Thus launch behaviour also occurs on this flat/uniform comparison. This does not
prove numerical precision is its cause or that every failure mode is identical.

Raw local output and captures: `.cache/fluid-flat-comparison-review/`.
This is automated loading/rendering and trajectory observation, not human acceptance.
No engine implementation, original scene or independent research was modified.

The initial ad-hoc flat project was preserved under
`.cache/fluid-flat-comparison-review/original-flat-comparison.judasproj` rather
than leaving two root projects and breaking automatic project discovery.

For current repaired behaviour and remaining limits, see
[the Experimental repair report](FLUID_WET_CLASSIFICATION_REPAIR.md).
