# Judas Engine Alpha v2.0.0

Alpha V2 uses `2.0.0-alpha.1`. Official Linux x86-64 SDK candidates
target Ubuntu 24.04 and include OpenGL and Vulkan. OpenGL is the default.

- One runtime/lifecycle connects engine, desktop editor, JudasJS and matching SDK tools.
- Current terrain, particles, renderer/lighting, skeletal/physical animation,
  production MK2, vehicles/materials, UI, audio and persistence retain their explicit
  ownership, resources and supported integration boundaries.
- Vulkan uses the shared renderer through the GLAD dispatch bridge. Supported
  standalone Linux games can [switch OpenGL and Vulkan live](judasjs/settings.md)
  while retaining the running world. Graphics quality includes render scale,
  independent directional and local shadow settings, and explicit visual
  density controls. [Vulkan setup and limits](VULKAN_INTEGRATION.md)
- [Hair & Fur V2](HAIR_AND_FUR.md) adds editor-authored groom patches,
  fixed-step guides attached to animated sockets, and shared lighting and
  shadows. Human visual acceptance is pending. On the tested GTX 970 crowd
  workload, ribbon preparation dominated CPU cost and the 120 FPS p99 target
  was missed. Close-up strands can still look like ribbons.
- Native Linux has the [development-ready scope](DEVELOPMENT_READINESS.md).
  Windows remains unsupported and its packages are historical.
- Navigation avoids repeated full-scene bookkeeping and keeps off-surface
  obstacles out of Detour's slots. Single-joint queries use the same matrix
  order as a full pose. Visible terrain material assets retain residency under
  cache pressure.
- Basic Water/swimming are independent default-enabled effects. Advanced Fluids
  uses one game-water backend and explicit project opt-in in every V2 channel.
- [Intentional removals](V2_BREAKING_CHANGES.md) have no automatic old-project/save
  conversion. Use the [SDK consumer workflow](V2_CONSUMER_GUIDE.md) and the matching
  declarations/docs instead of instructions from an older SDK.

Current guides, rather than earlier patch notes, document navigation/tilemap edits,
shape overlaps, force/filter controls, camera targets and effect readback. Public
binding/type/shape checks and actual native workflows have different evidence scopes.
Exact-source qualification, operator acceptance for `main`, and public
publication are separate steps. [Release procedure](RELEASING.md)
