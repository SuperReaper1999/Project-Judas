# Bending Lab

Ordinary editable Judas project for M80. Open `bending_lab.judasproj` in the
editor and Play, or run its normal exported package.

## Stations

- **A, green:** a left-clamped elastic tetrahedral beam. Load and unload it.
- **B, orange:** the same beam with authored plastic flow. Compare residual
  shape after unloading. Gravity remains present when the additional load stops.
- **C, blue:** a flexible partition with M79 section-strength connections. Loads
  can exceed actual section strength; detached geometry keeps its physical state.
- **D, purple:** an edge-clamped yielding panel and a finite-mass rigid press.
  Pick up the orange press to unload the panel, then lower or throw it back.
- **E, cyan:** an independently rotated elastic specimen and gravity region.

The flat and radial scenes share the same physical mechanisms. The radial scene
uses a real RadicalGravity field; the oblique specimen has its own ordinary
higher-precedence uniform field.

## Controls

| Control | Action |
|---|---|
| WASD / mouse / Space | Move, look, launch |
| B | Toggle equal additional end loads on A and B |
| H | Cycle increasing additional end loads on C |
| Click | Apply an impulse at the actual deformed surface hit |
| G / T | Pick up or drop / throw a tagged rigid prop |
| P | Spawn independent elastic and plastic beam prefabs, alternating |
| F3 | Request / release the normal streamed annex |
| 1 / 2 | Load uniform / radial scene |
| F5 / F8 | Save / load `bending-current` |
| F9 | Reload authored scene |
| Escape | Pause menu |

`Assets/scripts/lab.js` contains the game-side loads and UI policy. It never
sets node positions, changes rest shape, requests interface damage, or scripts
collapse. `controller.js` separately owns input, camera and prop manipulation.
The immutable cooks, per-instance deformable state, collision and material
mechanisms belong to Judas.

Regenerate the assets/scenes with `tools/CreateBendingLab.py` and a current
`judas_deformable_bake`. The script reuses scene-format helpers without executing
or modifying historical lab generators. It preserves the authored lab script.
Font licensing is in `Assets/fonts/LICENSE.txt`.
