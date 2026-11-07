# Water V2 Workshop

Editable source-preview game. Open `water_v2_workshop.judasproj` in an
Experimental Judas editor, then Play; or run:

```sh
build/judas projects/water_v2_workshop/water_v2_workshop.judasproj
```

Advanced Fluids must be explicitly enabled in Project Settings. Alpha's fluid
lock is unchanged; published Alpha SDKs cannot run this preview.

- WASD/mouse: walk/look. Space: jump or swim up. Q: dive.
- Aim the centre crosshair at a loose prop within 5 metres; G picks it up or
  releases it. H throws a held prop. Fixed scenery cannot be picked up. The HUD
  confirms the target, held state and failed interactions. Grip releases on load.
- F: toggle water circulation (an external pump and drain adding/removing equal
  quantities). It changes local flow rather than continuously raising the pool.
- B: aim down at nearby water to splash it, up to 12 metres away. Solid walls
  and props block the splash ray. This applies a real local water impulse.
- 1: ordinary pool. 2: tilted pool and gravity. 3: genuinely radial gravity.
- 4: original radial planet/basin geometry, with the new water backend.
- F6/F7: cold-save/load slot. Escape: pause menu. R: restart this scene.

Look for green floating boxes, blue neutrally dense boxes and grey sinking
spheres. Swim down the pool steps. Spray-free physical water quantity is owned
by the grid, with no render particles. The HUD reports inclusive CPU cost,
quantity, displaced storage and failures. External pump/drain receipts belong
to game rules in `Assets/scripts/player.js`.

`Scenes/planet-basin.judas` copies the actual original `fluid_demo` planet,
basin, stairs and props. It replaces the old particle seed with a filled,
0.4-metre grid and the legacy player/HUD with this game's CharacterMotor.
It does not claim identical initial water quantity or microscopic geometry.
The original project is unchanged. Its open bucket is a compound collider;
watertight moving-container pressure/added-mass behaviour is not qualified.

Source assets and regeneration: `tools/create_water_v2_workshop.py`. Regeneration
overwrites only this project's scenes, UI and font assets; the controller is
authored directly in `Assets/scripts/player.js` and is preserved. It never
deletes saves. All assets are original scene geometry or reused Judas example
assets; the included DejaVu font retains its licence.

The domain has closed sides/bottom and atmospheric gas at its top. Water does
not leave the grid automatically when a visual wall is lower than the domain.
Use paired, receipt-accounted sources/drains for external plumbing. Owners are
resident and stationary; surrounding colliders can stream and move. Independent
grids do not exchange water. See `docs/GAME_WATER_PREVIEW.md` in the engine for
the model, support boundaries and qualification evidence.
