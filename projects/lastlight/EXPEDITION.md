# Lastlight: Wayfarer expedition

Current project: `lastlight.judasproj`, startup `Scenes/town.judas`.
Standalone package: repository `.cache/Lastlight-Expedition-Package/judas`.
Human boarding/flight/gameplay acceptance is pending.

## Boarding and easy controls

Wayfarer is parked at the southern end of the town's main street, roughly 30 m
behind the initial player position. Walk around to its **orange rear door**.

1. Approach the door and press **E** to physically open it.
2. Walk through the doorway and along the cabin aisle normally.
3. Approach the captain's chair/teal command console and press **E** to sit.
4. Press **N** to cruise to **Cedar** and land automatically. **T** selects the
   next destination: Cedar, Ember, Frost, or Lastlight/Home.
5. After stopping, **E** leaves the chair. Walk to the orange door, **E** opens it,
   and walk onto the landing plaza. The same player, inventory and weapons remain.
6. Return to the chair, choose another destination with **T**, then **N**.

Manual flight: **W/S** forward/back, **A/D** strafe, **mouse** steers,
**Space** rises, **Ctrl** descends, **Shift** boosts. Release movement to brake
and hover. **R** levels against the current sampled gravity; it has no reference
in zero gravity. **V** toggles cockpit/chase camera. **Esc** pauses normally.
Manual movement cancels cruise. **N** cancels/restarts it. Standing is allowed only
below 2 m/s; the HUD explains when the ship must slow first. Walking **E** retains
mantling except when a nearby door/chair interaction consumes it.

HUD shows contextual controls, destination, distance, speed and cruise stage.
Pause field notes switch to flight controls while seated.

## Small planet villages

- **Cedar / Greenhaven:** green island world, broadleaf trees, two refuges.
- **Ember / Ash Village:** ochre world, two buildings and supply crates.
- **Frost / Pine Watch:** pale world, pine trees and two watch houses.

Each has a landing plaza and six zombies spawned on first approach. Buildings
have actual open doorways, floors, walls and interiors. Ordinary sphere collision
and authored RadicalGravity fields provide the curved ground and local gravity.
House and tree placement follows the spherical normal. Existing rifle, launcher,
melee, explosions, Framewalk pursuit and death ragdolls remain available.

Outpost zombies do not count toward the town's five-wave completion. Town spawning
waits while piloting/visiting a planet and resumes on return; the original town
round, score, inventory and supply vendor remain shared. Scene restart reconstructs
the entire expedition, including ship, island population and round state.

## Ownership and limitations

All additions are ordinary project content and JudasJS. No engine C++ or bindings
changed. One dynamic compound hull provides the real walkable floor and cabin
collision, with a separate dynamic door and ordinary hinge spring. Chairs and
benches belong to the same hull. Force/torque thrust, gravity compensation,
braking, attitude stabilization and cruise/landing are project policy, not
teleports or prescribed ship transforms.

While walking aboard, the motor uses ship support motion and a temporary
ship-local uniform gravity selection. Leaving the cabin restores its prior
selection. Seating disables the motor and explicitly transfers control; standing
restores it with ship velocity. Cameras use presented/interpolated ship poses,
while flight and gameplay use authoritative fixed-step state.

The seated figure is an original `PilotSeat` clip in `pilot_seated.glb`, with
editable `ArtSource/pilot_seated.blend`. It is not a full interactive hand/console
animation. Cabin gravity currently affects the player, not loose cargo. Cruise
uses a clear-space rise/travel/descent route and finite physical forces; it is not
an obstacle-avoidance pilot. Manual flight can collide with buildings and planets.
The three villages are modest blockouts, not streamed worlds. Island AI uses the
existing local collision-aware pursuit rather than pretending the town navmesh
covers spherical terrain; it can stall against complicated obstacles. Six guards
are cleared once per visit/session; restart repopulates them. Town victory/death
retains the original end-of-round menu.

## Authoring and review

- `Assets/scripts/ship.js`: boarding, pilot transfer, flight and cruise.
- `Assets/scripts/expedition.js`: destination data.
- `Assets/scripts/player.js`: current player integration and island population.
- `Assets/scripts/enemy.js`: island guard participation in existing combat.
- `Tools/build_expedition.py`: append/rebuild ship and spherical village content.
- `Tools/build_pilot.py`: Blender authoring for the seated pilot.

`Tools/build_town.py` now appends the expedition automatically. After regenerating
content, bake `Scenes/town.judas` with `judas_navigation_bake`, then export through
`judas_export`. Ordinary scene/editor changes remain possible; generators overwrite
their own authored output, so preserve hand edits before regeneration.

Human review: open the door, walk the complete aisle, sit/stand, manually fly and
brake, toggle views, cruise/land on all three worlds, disembark, enter buildings,
fight their zombies, return home, and pause/restart. Check that cabin walking and
camera following remain smooth and that the ship stays parked when unoccupied.

Content was parsed/baked and exported with the existing Release tools. An ordinary
Release desktop startup reached the player script without a script exception.
No production suite or automated gameplay tests were run for this content task.
No claim of completed human boarding/flight validation is made.
