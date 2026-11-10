# Deformables V2: real-model presentation follow-up

This follow-up supersedes the material-fidelity claims and intact-model images at
`aacdfc2d`. Automated crash qualification did not establish that the private imported
models visually matched their source references. The operator subsequently human-accepted Deformables V2 Stages 1–4 at
69677c8c, including these material corrections. Ural window/door authoring and
the demonstration follow camera are accepted asset/script limitations, not
requirements for further private-asset polishing.

## Confirmed causes and corrections

- Interior geometry was present, but visibility was wrong. The Challenger source
  contains 9,347 interior triangles. The Blazer's Details material, which includes
  cabin geometry, had imported mean alpha about 0.497/255. Source texture alpha and
  legacy transparency connections were inappropriate for its opaque surfaces.
  Private material recipes now explicitly distinguish opaque body/interior/rubber,
  masked decals/grilles and translucent glass. Original downloads are unchanged.
- The Shelby source uses a white shading/specular texture as its tyre diffuse
  input. Explicit private rubber authoring uses dark nonmetallic colour and the
  supplied normal/shading maps in their intended roles. Tyre alpha channels are
  not presumed to mean transparent rubber.
- The legacy sources connect several tangent normal images through FBX Bump
  slots. Their intent is explicitly authored in private material recipes; the
  importer does not guess that every scalar bump texture is a tangent normal.
  Supplied normal, roughness, metallic and occlusion maps are used where their
  roles are established. Original shader graphs are not automatically reproduced.
- The diagnostic assembler could leave brake discs and calipers on the chassis
  when their names did not contain “wheel”. It now recognizes these roles and
  accepts an exhaustive source-part role map. Unknown, missing or absent-source
  entries fail rather than silently dropping geometry. Rotors spin; calipers
  follow the actual steered/sprung mount with wheel spin removed.
- Explicit FBX dependency remaps previously lost to embedded image data, and FBX
  dependency discovery ignored the supplied remap settings. Both now respect
  explicit replacements and record them for cooking/publication invalidation.
  The image allocation limit is unchanged. A resized image can replace only that
  source dependency without a Blender geometry round trip.
- The cruiser is now imported directly from its original private FBX, retaining
  all 84,233 source triangles, including all 9,612 internal/engine triangles.
  The previous Blender derivative contained 83,945 triangles. It is preserved as
  earlier evidence, not treated as an exact copy of the original geometry.
- Intact comparison renders use the ordinary scene renderer, including shadows
  and an original, baked studio environment. The diagnostic crash tool's manual
  draw path is not a substitute for that visual comparison. Generated normal
  projects retain the caller's lighting/environment rather than overwriting it
  with high flat ambient light.

## Reusable authoring controls

The private proof preparer accepts:

```
--part-roles=/absolute/path/roles.json
--environment=/absolute/path/studio.judasenv
```

A role file uses exact imported part identities and must cover the whole source:

```json
{"schema":1,"parts":{"Example/Body":"body","Example/Rotor":"wheel","Example/Caliper":"caliper"}}
```

This is local proof preparation, not native game-specific gameplay. The resulting
projects use normal scene meshes, Vehicles V2 mounts, native deformables and
project-owned scripts. A preparation receipt reports source and assembled triangle
counts. No source triangles may disappear during the assembly split.

For a project-owned FBX texture replacement, the existing import recipe accepts:

```json
{"settings":{"dependencyRemaps":{"embedded.png":"Sources/Textures/replacement.png"}}}
```

The key is the source texture's normalized relative filename; the replacement is
inside the project. An explicit missing replacement fails instead of falling back
silently to embedded content. Cooking revision 12 invalidates old recipe caches.

## Qualification and limits

Original Judas-owned procedural tests cover explicit roles, disc/caliper distinction,
geometry-preserving wheel fitting and invalid/incomplete authoring. A tiny CC0 FBX
fixture covers embedded-image replacement, dependency discovery, image-only recook,
stale-publication rejection and preserving the accepted model on failure. Running
these new tests against the previous FBX importer produces five expected failures;
the corrected reimport suite passes all 48 checks.

Private recipes, source/derivative hashes, geometry inventories, rendered comparisons
and playable projects remain outside Git and public SDKs. No redistribution licence
is inferred. The glass is ordinary translucent PBR, not a promise of exact source
renderer refraction or custom paint clearcoat. Lighting differs from the supplied
reference viewer. Visual adequacy still requires operator review.

The existing severe-impact performance limitations remain; material corrections do
not claim to fix MK2 CCD spikes. Main, the Advanced Fluids lock, the RPG project/SDK
and the accepted Stage 1 build are not changed by this follow-up.
