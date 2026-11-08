# Terrain material detail — bounded qualification

This checkpoint adds authorable metre tiling and PBR roughness multipliers to all
four terrain layers, plus one optional shared normal-detail texture. It does not
claim completion of the whole Terrain V2 programme. Main, releases, the Alpha fluid
lock, existing game assets/saves and the installed RPG SDK remain unchanged.

`TerrainMaterial.h` owns the shared presentation settings and
`TerrainMaterialShader.h` owns terrain-only GLSL. Renderer, native terrain owner
and editor use the same descriptor. No second geometry or physical representation
is introduced. The existing renderer overload remains available. Native consumers
must relink with their matching Experimental headers and library.

Defaults omit the new metadata entirely. New values are validated, serialized in
native headers and reference saves, and included in terrain source identity. The
normal texture is a registered asset/export dependency. One shared RGB normal map
uses unit12 within the portable16-unit binding scheme; albedo, world height,
shadows, environment, weather, light-grid and skinning retain their own bindings.
The normal sampler branch is skipped when absent or strength is zero. Existing
source assets are never upgraded automatically. The compatibility preview atlas
remains albedo-only. Per-layer normal maps, virtual textures, fine physical
interest and native data demand paging remain unimplemented.

## Correctness and workflow

- New material controls:86 checks on actual NVIDIA desktop4.6 and portable3.3.
  Twelve signed/rotated face cases compare lighting with independently calculated
  ordinary-mesh normals: maximum RGBA8 difference **zero**. Neutral detail,
  disabled detail and zero strength preserve exact pixels. Four independent
  ordinary-UV meshes verify each metre scale. Independently weighted ordinary PBR
  material verifies the roughness blend, including a nonzero highlight control.
  Invalid ranges, nonfinite data, unknown keys and malformed vectors are rejected
  without replacing the previously parsed source.
- Actual new-project editor workflow:46 checks each modern/portable. The test
  types tile size2 and roughness0.4 into the real fields, selects the normal map,
  then applies, undoes, redoes, publishes, saves and starts gameplay.
- Existing editor15 and geographic editor23 checks pass each graphics path.
- Original rendered height/shadow controls978 and LOD/history controls98822 pass
  each path. Existing runtime renderer54 and physical world93 pass, including
  300 supported seam steps with **zero** monolithic trajectory difference.
- Native asset33 and component38 checks pass, including deterministic cooking,
  material dependency collection and exact metadata after world restoration.
- Shader47 controls pass. The preserved original-pin run fails only the expected
  mesh-fragment identity. Only that identity advances after the independent GPU
  controls; other shader pins, tolerances and negative controls are unchanged.
- Build organization22 and current-doc/release-packaging36 Python tests pass.
  All17 SDK tools were rebuilt; a new post-commit SDK qualification is separate.

The ordinary flat and geographic Workshops were copied into fresh outside-checkout
consumer projects. `--material-detail` adds an original, reproducible normal
texture/settings only to those copies. Both cook, export, relocate, excavate a
real eight-chunk tunnel, traverse it, save, quit and restore in separate processes.
Exported normal bytes are independently checked. Both restore two surface dabs,
ready navigation and an empty edit queue. The flat copy also persists a real
reliable particle contact and consumed droplet. The geographic run retains its
origin change. See both `*-game-final/RESULTS.json`, screenshots and raw commands.
These are automated results and inspected captures, not operator acceptance.

## Performance

No compiler/game jobs ran during the final timing batch. The frozen previous
binary is retained in `.cache/terrain-v2/material-detail/before-render-benchmark`
with its SHA256. Three alternating-order invocations per variant each contain
30 measured two-camera frames of the same1km,513x513 landscape at1280x800,
2px LOD target. Draws56 and triangles28672 are unchanged.

| Variant | CPU submission median range | GPU elapsed median range |
| --- | --- | --- |
| Frozen previous binary |0.365–0.410ms|0.493–0.605ms|
| New default, normal absent |0.388–0.398ms|0.570–0.575ms|
| New normal enabled |0.389–0.403ms|0.651–0.653ms|

The last two paired repetitions show approximately0.08ms default GPU overhead and
0.02–0.03ms CPU overhead against the warmed old binary. Enabled detail adds another
approximately0.08ms GPU elapsed over the new default. This is a feature cost, **not
a speedup**. GPU elapsed includes submission bubbles; these scopes are not whole-game
FPS. Full p95/worst rows, transition workloads, order and commands are retained in
`benchmark-summary.json` and raw logs. One128x128 RGBA8 normal map with mip chain
uses87380bytes; there is no additional per-chunk normal texture or physical mesh.
No whole-process-memory improvement or new platform certification is claimed.

## Retained setup failures and limitations

- First build requested the new target before reconfiguration; then visual test
  linkage omitted the existing authoring library. Both are corrected build wiring.
- One follow-up build used a nonexistent guessed runtime-render target name;
  the actual `judas_runtime_render_tests` target was rebuilt and passed.
- The first portable editor invocation correctly refused a nonfresh default test
  directory. A fresh explicit directory passes15 unchanged checks; the failure is
  retained rather than deleting prior test data or weakening the guard.
- An extra GPU invocation used an unrecognized environment variable and selected
  modern rendering. Its log is explicitly named `wrong-env`; the actual portable
  qualification uses `JUDAS_GL_PROFILE=portable` and reportsOpenGL3.3.
- The first authored demonstration normal had conspicuous periodic stripes. Its
  screenshots/logs remain in the temporary first consumer directories. The final
  driver uses subtler periodic multiscale noise. No engine test was relaxed.
- Default geometry and quantities are untouched. All collision chunks remain
  resident while their source region is active. Fine physical interest/data paging
  is still a separate outstanding architectural boundary.

## Reproduction / human check

Build `judas_terrain_v2_material_tests`, run it on desktop and with
`JUDAS_GL_PROFILE=portable`. The test accepts an optional capture directory.
`judas_terrain_v2_render_benchmark near detail` enables the measured normal path;
omit `detail` for the default path. The frozen binary provides the old baseline.

Run the documented `qualify_terrain_v2.py --material-detail` workflow with a fresh
outside-checkout output, adding `--planet` for the second game. Walk around the
hill, inspect surfaces and tunnel ceilings, paint, save/restart/load, and check
World→Terrain tools→Surface materials for scale, roughness and normal authoring.
Choose a PBR material on the terrain owner to judge roughness. Check both absent
and enabled normal detail. Human visual/gameplay acceptance remains pending.
