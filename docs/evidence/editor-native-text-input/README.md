# Editor native text-input ownership — 2026-10-25

Operator failure: directory/new-folder, project-name and scene-save fields would
not accept typing. This affects the editor's shared text-input path, rather than
the content of any individual field.

Root cause at `577c4575`: Window initialization explicitly calls SDL_StopTextInput.
Runtime UI requests a native session through Window::SetTextInput, but the editor
never requested one for ImGui. The bundled ImGui SDL backend intentionally does
not call SDL_StartTextInput (see its 2023-04-06 note). In addition, gameplay
keyboard claiming and runtime frames without a UI text target stop game text
sessions. Treating editor text as game-owned would therefore be insufficient.

Correction: Window has a separate native-host text request. Native SDL activation
is the union of host/game requests, gated by application/window focus. The host
has priority; its text reaches the existing event hook and cannot enter the
gameplay text queue. Editor polling requests it from ImGui::WantTextInput.
The existing backend retains ownership of the editor IME candidate rectangle.
Standalone runtime UI keeps its player/token routing and has no host request.
There are no new JudasJS bindings or project settings.

Validation on native Linux:

- Release editor/UI/input targets built successfully.
- UI production suite: **76 checks, zero failures**. Includes SDL native-session
  activation, runtime/host ownership, focus loss/recovery, clipboard and existing
  per-player text routing. New representative folder/name/save fields consume
  Unicode SDL_TEXTINPUT through the real ImGui SDL backend, while gameplay sees
  no text. These input events are synthetic; this is not human acceptance or
  physical IME/platform certification.
- Window/gameplay input suite: **40 checks, zero failures**.
- Folder-picker regression: **15 checks, zero failures**.
- Corrected editor launched without closing existing, potentially unsaved sessions.

The first combined test reported one activation-timing failure. It checked SDL
before the event-poll boundary following ImGui's published focus request; the
retained test advances that boundary before checking activation. The initial raw
log is retained as `initial-test-timing.log`. No production threshold was changed.

Reproduce from repository root with a working desktop graphics connection:

```sh
cmake --build build --target judas_editor judas_ui_production_tests judas_input_tests judas_editor_file_browser_tests -j 2
SDL_AUDIODRIVER=dummy build/judas_ui_production_tests /tmp/judas-editor-text-validation
SDL_AUDIODRIVER=dummy build/judas_input_tests
build/judas_editor_file_browser_tests
sha256sum -c docs/evidence/editor-native-text-input/SOURCE.sha256
```

Human follow-up: in the new editor window, type a project name and directory,
browse into nested folders, enter a new folder name, create the project, then
type a scene filename in Save As. Verify clipboard/editing and that typing during
Play does not move the player. Main remains unchanged pending operator acceptance.
