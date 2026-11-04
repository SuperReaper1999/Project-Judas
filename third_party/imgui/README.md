Editor V2 uses pinned `v1.91.9b-docking`; exact source provenance is in JUDAS_VENDOR.json.

# Dear ImGui vendor note

Judas vendors **Dear ImGui v1.91.9b-docking**, source commit
`4806a1924ff6181180bf5e4b8b79ab4394118875` in
[`ocornut/imgui`](https://github.com/ocornut/imgui), licensed under MIT.
`LICENSE.txt` is copied intact; exact archive provenance is in JUDAS_VENDOR.json.
The previous master-branch vendor was v1.91.9b (`f5befd2d…`).

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
