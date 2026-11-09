# Intentional V2 architectural removals

These are deliberate retirements, not deprecated APIs with forwarding shims.
Use the historical engine revision for historical projects. Modern SaveService
rejects incompatible content/participant identities rather than manufacturing
missing state. No old-save conversion is provided.

| Removed | Current ownership |
| --- | --- |
| GameSession, native player/pilot/spacecraft, pickup, door/light-switch rules, native HUD/pause/reset | RuntimeWorld + InteractivePlay + SceneSession; project scripts implement gameplay using CharacterMotor, input, joints, queries, runtime UI and cameras |
| `legacyGameplay` / `legacy-gameplay`, player-start/vehicle/door/lightSwitch and body.pickable demonstration metadata | Project-owned scripts and data; no replacement engine gameplay flag |
| `.judasstate`, pose-delta overlay, native save/delete-state input actions | SaveService slots, WorldPersistence and developer-controlled save evolution |
| LiquidSystem, reservoirs/containers, transfers, liquid assets and `judas_liquid_bake` | Retired capability; Basic Water and accepted Advanced Fluids V2 remain distinct current systems |
| `liquid`, `LiquidVolume`, `Entity.liquid`, PBF FluidWorld/ProductionFluidCoupling and backend/emitter switches | No compatibility API. Current `GridWater` APIs use the sole game-water backend; Alpha lock still applies |
| V1 bounded terrain, VolumeTerrainSource, `terrain.create`, volume author CLI, v2-import and full-mesh v2-cook bootstrap | Terrain V2 native asset/source, `createLandscape(serializedSource)`, heightfield surface edits and sparse volume excavation |
| Fixed-panel editor fallback, empty panel compatibility files, old ASCII atlas/stb rasterizer | Docked Editor V2 panels and current Unicode text renderer |
| `world.sweepCapsule` fixed native-player wrapper, no-op debugOverlayVisible | Explicit `physics.capsuleCast`; project UI and real current diagnostics |

Mod authoring kits now use `Judas-ModKit-1`; milestone-labelled kits are retired.
Use `judas_mod registries PROJECT` when authoring an explicit new game contract.

Current portability and CPU particle fallbacks, MK2, flat/radial gravity,
planetary precision, input, audio, joints, deformables, animation and modern
persistence are legitimate capabilities and remain. No V2 release, main promotion
or fluid unlock is implied by this purge.
