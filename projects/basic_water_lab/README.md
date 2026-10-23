# Stillwater — Basic Water Lab

Ordinary Judas project demonstrating inexpensive water and script-owned swimming.
Advanced Fluid Dynamics is explicitly disabled. No particle or conserved-flow
components are authored here.

Open `basic_water_lab.judasproj` in the editor. Use WASD and mouse to explore;
Space jumps / swims up, Q dives, Escape pauses / resumes and R reloads the scene.
Walk down the pool steps or jump off the jetty. The HUD shows swimming immersion.

`Assets/scripts/player.js` owns movement, buoyancy, drag, camera and UI behaviour
through public JudasJS APIs. The water component owns only prescribed volume,
current and visual parameters. World gravity continues to come from its field.

Try Project Settings → Water: disabling swimming leaves the water visible while
the character sinks. Disabling Basic water also removes its surface/query volume.
Restart Play after policy changes. Use the Inspector to change colour, waves,
depth, density and local current. Save scene and project separately.

See `docs/PROJECT_SETTINGS_AND_WATER.md` in the engine repository for the complete
manual, limitations and human checklist. The water is an oriented finite region,
not a conserved fluid solver or spherical planetary ocean.
