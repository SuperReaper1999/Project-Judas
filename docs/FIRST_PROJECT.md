# Your first visible Judas project

[API index](JUDASJS.md) · [Full authoring guide](judasjs/getting-started.md) ·
[Debugger and reload](M94_DEVELOPER_TOOLS.md) · [Distribution](DISTRIBUTION.md)

Start with one visibly rotating box and an explicit camera. This small project
needs no downloaded art, CharacterMotor, input bindings, gamepad, npm or C++ game
code. Add your own gameplay after this loop works. The engine does not silently
provide a character controller or camera policy for new projects.

## 1. Open the editor and make a project

From the root of an **unpacked SDK**, start:

```sh
./judas_editor
```

On Windows use `.\judas_editor.exe`. From a **configured source checkout**,
use `./build/Experimental/judas_editor` instead. Below, terminal examples use
SDK-root paths; source-build users substitute `./build/Experimental/` before each
tool name. Keep the whole SDK together; it contains the fonts, types and guides.
The engine source archive and SDK omit repository game projects, so create your
own project as below rather than trying to launch `projects/tiny_game`.

Choose **File → New project…**, use **Browse…** to choose a new directory and
enter a name such as **My First Game**. The default **Visible script starter**
creates a rotating box, tracked `Assets/game.js` and a script-owned camera.
Press Play immediately; edit the speed property or script, then check/reload.
This uses the same public example described below; no hidden native controller
or C++ compilation is involved. Existing scripts are never overwritten.

Choose **Empty scene** if you want to assemble everything below yourself. That
scene has a floor, authored gravity, light and historical start marker. The marker
does not create a modern player: new projects use `legacy-gameplay false`.

Create a box and name it **My rotating box**. Set its position to `(0, 2, 0)`.
Give it a **Render** component with box shape. Remove its **Body** component if
the creation command added one. This example animates a visual object; assigning
a physical body's transform would teleport it rather than simulate motion.
Keep the floor and light.

## 2. Attach the complete starter script

**Visible script starter users:** this step is already done for `Assets/game.js`;
continue to step 3. The manual instructions below are for the Empty scene option.

Copy [first-project.js](judasjs/examples/first-project.js) to your project's
`Assets/first-project.js`. SDK users can find the same file under
`docs/judasjs/examples/`. It uses only the current public `judas` module:

- A numeric inspector property controls spin speed.
- `update(dt)` changes the render-only entity's rotation.
- `presentationUpdate()` supplies the main camera at `(0, 2, 6)`, looking along
  its local `-Z` axis. This example chooses a Y spin; that is content, not a
  universal engine up direction.
- `destroy()` releases the camera. Its JSON state belongs to this instance.

Open **View → Asset Browser**, **Rescan**, then **Track** the script. Select your
box, choose **Add component… → Scripts**, and select the tracked asset in an
enabled behaviour slot. Save the scene. Asset IDs and `.judasmeta` files are
part of the project: retain them with the source.

Press **Play scene / F5**. You should see the box turning in front of the camera.
The startup log says `My first Judas project is running.` Stop returns to the
authored scene. The code needs no input capture; this first exercise does not
provide walking or mouse look.

The runtime currently shows its historical diagnostic HUD when a scene has no
authored UI. Its player/spacecraft labels are compatibility diagnostics, not a
controller supplied by this example. Use **Escape → Options → Show HUD: Off**
to hide it. Build your own interface through [runtime UI](judasjs/ui.md).

## 3. Edit, debug and deliberately reload

Open **View → JudasJS developer tools** and select your script. Enable **Attach
debugger (Play)** before Play, then put a breakpoint on `this.state.frames++;`. Play,
inspect `this.state`, step once and continue. A line must contain an executable
statement to stop there.

Change the inspector's `rotationSpeed`, or use **Edit source → Save source**
(or your external editor) to change the script's default from `1.0`
to `2.0`. Explicit inspector values override defaults. Save the source; changes
on disk do not execute automatically. **Check sources** compiles/links, and
**Reload scripts** deliberately replaces the script session at a safe boundary.
**Keep JSON state** retains the elapsed counter; it does not retain closures,
objects or arbitrary VM state.

Reload changes the running content identity. Use Stop/Play before cold saves or
online sessions after reloading; it is not a migration of an existing saved game.
Modern cold loads call `restore()` instead of `start()`; see the
[load-aware lifecycle](judasjs/saves.md#load-aware-lifecycle) before adding saves
to a project that spawns content in `start()`.

Try a syntax error and check/reload again. The diagnostic should identify the
script; rejected compile/link preflight keeps the running session. Correct the
source before reloading. Runtime exceptions remain separately reported failures;
successful compilation does not prove game logic.

You can also check all registered project modules from a terminal:

```sh
./judas_script_check '/absolute/My First Game/My_First_Game.judasproj'
```

## 4. Save and export

Project settings have their own dirty indicator and Undo/Redo. Closing or replacing
a project offers Save/Discard/Cancel. Save source, scene and project settings; choose `Scenes/main.judas` as startup
scene. **Run project** checks ordinary startup. **Export project (Release)**
uses saved disk content. With unsaved scene/project changes, explicitly choose
**Save all and continue**, **Use saved version**, or **Cancel**. Choose a fresh directory
outside the project. The equivalent command is:

```sh
./judas_export '/absolute/My First Game/My_First_Game.judasproj' \
  '/absolute/My First Game Package' ./judas
```

Use runtime/exporter binaries from the same build and channel. Exports reject
mismatched provenance with an actionable message. A development export can
record dirty source; release SDK/signing presets impose stricter clean-input
requirements. On Windows use the matching `.exe` tools. Move the exported
directory and launch its `judas`/`judas.exe`; do not move only the executable.

## Common first-project problems

| Symptom | Check |
|---|---|
| Blank scene or editor view during Play | Script tracked, enabled and attached? Correct box position? Read script diagnostics; the historical start marker is not a script camera. |
| Script is absent in the picker | File is inside `Assets/`; Rescan and Track it. Track relative helper imports too. |
| Speed edit has no effect | Save source, reload deliberately; clear/change an explicit inspector override. |
| Exits or faults on import | The VM has no Node/browser globals. Top-level native world operations belong in lifecycle callbacks. |
| Walking keys do nothing | This starter supplies no movement. Author logical controls and a CharacterMotor separately. |
| Export shows old content | Save the scene/project/source first. Export reads saved assets. |
| Export rejects the runtime | Use matching version/channel/source tools; rebuild the pair rather than editing provenance. |
| IDE shows no suggestions | Copy current `docs/judas.d.ts` to `Types/` and follow [IDE setup](judasjs/practices.md). This is optional tooling. |

Next, follow [Making a game by hand](judasjs/getting-started.md) for input,
CharacterMotor, physics, UI, saves and inspectable complete game projects.

## Verification scope

`judas_first_project_tests` creates an ordinary project from scratch, registers
this exact script, saves/reopens its scene and exercises real VM callbacks. It
checks actual rotation/camera endpoints, syntax-failure isolation, deliberate
reload and state continuation. It needs no private content, renderer or compiler
at execution time. It does **not** certify editor clicks, visual appearance,
export, platform hardware or human acceptance; test receipts record execution
status separately in the [Pre-Alpha evidence](evidence/prealpha-pass2/).
