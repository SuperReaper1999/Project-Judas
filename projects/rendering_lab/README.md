# The Light Factory — Rendering Lab (M92)

An ordinary editable project, not an engine demo mode. The default gallery has
1,024 repeated modules, 128 independently scripted coloured sources, live monitor
cameras and shadow-requesting spots. Its watch tour starts automatically.

```sh
build/judas projects/rendering_lab/rendering_lab.judasproj
build/judas_editor projects/rendering_lab/rendering_lab.judasproj
build/judas_export projects/rendering_lab/rendering_lab.judasproj /tmp/judas-rendering-lab build/judas
```

| Control | Action |
|---|---|
| F5 | Watch tour / free flight |
| WASD, Q/E, mouse | Free camera movement and look |
| L | All sources on/off (individual Entity.setLight calls) |
| H | Authored spotlight shadow requests on/off |
| V | Two independent split views / ordinary main camera |
| 1 / 2 / 3 | 256 / 1,024 / 4,096 repeated modules |
| F6 / F7 | Save / load the scene and script/light state |
| R | Reload authored scene |
| Esc | Pause/resume and release/capture pointer |

The dark graphite modules, warm housings and coloured light pools make source
coverage visible; two live screens use ordinary authored render cameras. Small
per-light intensity changes are project JavaScript. No game-specific engine
branch, simulation, model-spawn loop or hidden draw path is used.

## Inspect in the editor

Open `Scenes/gallery.judas`. Select an `Independent luminaire` to change kind,
colour, intensity, range, shadow request and priority. A spot's local -Z is its
beam direction. Edit repeated module geometry or material normally. Different
material states deliberately split batches. Scene density changes draw/CPU work,
not physics fidelity. Dense scenes can still expose ordinary world traversal and
shadow-caster submission cost.

See [current rendering limits](../../docs/judasjs/rendering.md) and
[M92 evidence](../../docs/evidence/m92/REPORT.md). Linux automated validation is
separate from operator visual acceptance; Windows and Android M92 are unvalidated.
