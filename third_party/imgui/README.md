# Dear ImGui vendor note

Judas vendors **Dear ImGui v1.91.9b** (upstream tag `v1.91.9b`, source
commit `f5befd2d29e66809cd1110a152e375a7f1981f06` in
[`ocornut/imgui`](https://github.com/ocornut/imgui)), licensed under the MIT
License — `LICENSE.txt` is copied intact from that checkout.

Only the files the Judas editor needs are included:

- the core library: `imgui.cpp`, `imgui.h`, `imgui_draw.cpp`,
  `imgui_internal.h`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imconfig.h`,
  and the three `imstb_*.h` headers it depends on;
- the SDL2 platform backend (`backends/imgui_impl_sdl2.*`);
- the OpenGL 3 renderer backend (`backends/imgui_impl_opengl3.*` and its
  self-contained `imgui_impl_opengl3_loader.h`).

`imgui_demo.cpp`, `examples/`, `docs/` and the other platform/renderer
backends are deliberately not vendored. `imconfig.h` is unmodified.

Dear ImGui is used **only** by the `judas_editor` executable
(`src/editor/`). No engine or runtime source file includes it, and the
`judas` runtime does not link it — see `docs/ARCHITECTURE.md`,
"Milestone 28," for the engine/editor dependency direction.
