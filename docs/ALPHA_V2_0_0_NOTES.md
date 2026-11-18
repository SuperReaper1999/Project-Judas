# V2 source scope and release notes

- One runtime/lifecycle connects engine, desktop editor, JudasJS and matching SDK tools.
- Current terrain, particles, renderer/lighting, skeletal/physical animation,
  production MK2, vehicles/materials, UI, audio and persistence retain their explicit
  ownership, resources and supported integration boundaries.
- Native Linux has the [development-ready scope](DEVELOPMENT_READINESS.md).
  Ubuntu and historical Windows build qualification, device gaps and measured performance
  limits are stated there rather than inferred from version/maturity.
- Basic Water/swimming are independent default-enabled effects. Advanced Fluids
  uses one game-water backend and explicit project opt-in in every V2 channel.
- [Intentional removals](V2_BREAKING_CHANGES.md) have no automatic old-project/save
  conversion. Use the [SDK consumer workflow](V2_CONSUMER_GUIDE.md) and the matching
  declarations/docs instead of instructions from an older SDK.

Current guides, rather than earlier patch notes, document navigation/tilemap edits,
shape overlaps, force/filter controls, camera targets and effect readback. Public
binding/type/shape checks and actual native workflows have different evidence scopes.
Preparing a future release still requires the exact-source pipeline and explicit
operator approval in [RELEASING.md](RELEASING.md).
