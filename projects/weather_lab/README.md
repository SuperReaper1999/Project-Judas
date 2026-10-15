# Judas Weather Lab — M86

Open `weather_lab.judasproj` in the editor, or run its exported standalone.
The startup scene is a flat meadow with a walkable, physically solid pavilion.
**F9** loads the Earth-sized planetary scene. Both are ordinary project content.

## Controls

| Key | Action |
|---|---|
| WASD / mouse / Space | Walk, look, jump |
| 1 / 2 / 3 / 4 | Sunshine / gathering clouds / rain / thunderstorm |
| 5 / 6 / 7 / 8 | Moving tornado / snowfall / blizzard / fog |
| F5 | Start/stop the watch tour; direct weather selection stops the tour |
| Q | Quarter / half resolution weather rendering |
| 0 | Toggle weather visuals only; sampled wind and physical forces continue |
| F9 | Switch flat / planetary scenes through ordinary M43 loading |
| F1 / F2 | Ascend / land; the planetary scene can travel through clouds to orbit |
| F3 | Planetary cube-face boundary visit |
| E | Throw an ordinary physical probe |
| F6 / F7 | Save / load the current scene's weather slot |
| R / Escape | Reload / pause |

Weather changes smoothly over three seconds. Thunder follows a lightning strike
with a distance delay. Keep sound enabled. The moving tornado is an authored
track with a bounded prescribed wind field, rather than simulated storm air.

## Things to inspect

1. Start the watch tour or select each weather type directly. Look upward and
   around: clouds occupy a volume, rather than a sky texture.
2. Walk into and out of the pavilion. Its ordinary roof collision shelters local
   rain/snow. The roof stays real even with weather visuals disabled.
3. Compare light standing panels, ordinary freight and the 300 kg heavy crate.
   Only objects with **Wind response** opted in receive additional weather drag.
4. Select a tornado and watch the funnel move across the field. Wind and debris
   are bounded approximations; visual debris is not individual rigid bodies.
5. F9 loads the planetary scene. F1 ascends continuously through the cloud layer
   to orbit; F2 descends. Ground colliders and the observatory stream out before
   leaving the local physics chart. F3 visits a different planetary surface frame.
6. Save mid-transition or during a storm, alter the weather, then load. Native
   weather time, transitions, moving funnels and pending thunder are restored.
7. Try Q and 0 while a light prop is moving: graphics settings must not change
   the authoritative wind field or opt-in physics forces.
8. Repeat in the relocated exported package.

## Editable project layout

- `Scenes/flat.judas`: meadow, solid shelter, ordinary gravity and physical props.
- `Scenes/planet.judas`: M85 double-precision terrain, RadicalGravity, weather shell.
- `Scenes/station.judas`: streamed physical observatory for the planetary scene.
- `Assets/scripts/lab.js`: project-owned exploration, camera, UI and watch tour.
- `Assets/scripts/profiles.js`: ordinary editable weather parameter recipes.
- `Assets/world/weather.judasworld`: M59 region registration and residency budget.
- `Assets/prefabs/`: independent CharacterMotor traveller and physical probe.
- `Assets/ui/weather.judasui`: ordinary M41 HUD and pause menu.

`tools/CreateWeatherLab.py` regenerates authored scenes, UI, project registration
and asset metadata. It preserves the manually maintained JavaScript. The licensed
DejaVu font and small movement helpers are reused from the current planetary lab.

## Scope

The first scene uses flat weather framing; the second uses an absolute spherical
weather shell. The lab's free flight is project-owned camera travel, and the
motor is recreated when landing after streaming the distant local props out.
Weather does not alter gravity or turn precipitation into a conserved liquid
reservoir. There is no flooding, physical snowpack, climate model or simulated
storm-scale fluid. See the current M86 implementation/reference documents for
rendering quality, shelter and camera limitations. Human visual acceptance is
pending until the operator reviews this candidate.
