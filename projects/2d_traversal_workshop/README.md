# 2D Traversal Workshop

An editable ordinary project demonstrating native one-way platforms and a grid
RPG movement preset. No hidden demo controller or alternate physics world.

- **1:** platform scene. A/D or arrows move; **Space** jumps. Gold platforms are
  straight static segments: walk underneath, jump through and land above.
- **2:** grid scene. WASD/arrows step one cell. Tap, hold or buffer a turn. Green
  solid cells block movement. The kinematic controller uses real shape casts.
- **R:** reset scene. **K / L:** save / load. Desktop F6/F7 bindings remain available; use K/L in browsers to avoid reserved shortcuts. Saves retain a pending grid step.

Open `2D_Traversal_Workshop.judasproj` in the editor. `Scenes/main.judas` and
`Scenes/grid.judas` are ordinary editable scenes. `Assets/platform-player.js` and
`Assets/game.js` are editable movement policies; `Assets/controls.js` owns scene
and save buttons. Gold platform collision lives in its Body component, not its
sprite. Use Body → Preview collision to see the blocking-side arrow.

Create menu → 2D one-way platform / 2D grid RPG player. New Project also provides
New 2D game - grid RPG. Keep only one script-owned camera per view. New grid
players expose followCamera so additional actors need not fight over the camera.

For a palette/tilemap workflow, use the separate Tilemap Workshop. These one-way
segments are authored independently of solid tilemap rectangles. No automatic
moving/curved one-way surfaces, drop-through input, pathfinding or turn-based
battle framework is implied. See docs/2D_TRAVERSAL.md and docs/judasjs/physics-2d.md.

Content reproduction: build judas_2d_traversal_workshop, then invoke it with a
fresh output directory. It refuses to overwrite an existing project. Generated
starter art and source templates are included locally; no external assets used.
