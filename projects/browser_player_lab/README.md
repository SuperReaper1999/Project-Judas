# Browser Player Lab

An ordinary editable Judas 3D project, shared by desktop and the experimental
browser runtime. MK2 owns the five pushable crates; the ordinary CharacterMotor
and first-person template own player movement and the camera. No HTML game logic.

- WASD / arrows: move; mouse: look; Space: jump; V: third-person view.
- Escape: release/capture mouse. Click the game to permit browser mouse capture.
- K / L: save/load through the ordinary durable save service.
- Page Fullscreen button: enter/leave fullscreen. Escape also leaves fullscreen.
- Release capture to click **Test click**. Its counter is persistent.

Resize the browser, try portrait/wide windows and fullscreen, and check all four
coloured UI markers. UI uses the existing centred reference-resolution layout;
portrait does not automatically redesign a project's authored HUD.

Desktop: `judas Browser_Player_Lab.judasproj`.
Browser export uses the opt-in workflow in `docs/BROWSER_SUPPORT.md`. Source
assets are deliberately small primitives to isolate presentation/input/lifecycle
behaviour; this is not a production art or complete 3D-effects qualification.
Regenerate a **fresh** project with `judas_browser_player_lab REPOSITORY OUTPUT`;
its generator refuses to overwrite edited content.
