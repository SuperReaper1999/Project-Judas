# Project Settings and water

Project Settings and basic water are integrated with the human-accepted MK2
engine baseline `c859901cc9a52cf5b7104887f56b5e7ad4269b1d`. MK2 is the
authoritative physics engine. The later Experimental corrections described in
[Final Pre-Alpha corrections](PREALPHA_FINAL_CORRECTIONS.md) strengthen policy,
persistence and editing workflows without changing fluid numerical methods or
the MK2/balance solvers. Those corrections and the later rotation repair are
in the human-accepted `c76e92dc` baseline.

## Alpha availability

**Advanced Fluid Dynamics is disabled in Alpha builds due to contact stability
issues.** The Experimental page explains the restriction and cannot enable it.
Projects with an existing `advanced-fluid-dynamics "true"` request can still be
opened and edited, but Play, cold-world construction and export fail before
advanced simulation starts. Use **Disable advanced fluids for this project**, then
save. Components and assets are preserved; advanced saves are not converted into
basic water. Basic Water rendering and swimming remain available independently.
The restriction is compiled build-channel policy, not a project/environment toggle.

The opt-in workflow below describes **Experimental/local builds only**, retained
for investigation. It does not imply Alpha support. Re-enabling Alpha is deferred
to a validated patch; no date or completed repair is promised. See
[contact-stability investigation](M100_FLUID_CONTACT_STABILITY.md).

## Configure a project

Open **Project Settings** from the editor's File menu. The category list groups
existing project controls rather than adding global switches for every component.

| Category | Actual ownership / controls |
| --- | --- |
| General | Project name, startup scene, game icon |
| Rendering | Existing cooked visual residency budget; links to scene lighting/exposure/background |
| Physics and gravity | Stable tags/collision/render registries; links to scene settings. Gravity remains authored fields/regions and optional scripted selection |
| Input | Existing logical actions, axes, vectors, bindings and deadzones |
| Audio | Existing sound groups, gain, mute and pause routing |
| World and persistence | World-manifest selection, directory information, save evolution/migration policy |
| Navigation | Existing stable profiles and area registry |
| Localization | Existing locale/catalog selection and font fallbacks |
| Water | Basic water and swimming-query switches; ordinary water component creation |
| Networking | Explains existing explicit JS-owned session policy; no automatic listener or invented network configuration |
| Experimental | Advanced Fluid Dynamics availability/restriction; explicit opt-in warning only in Experimental/local builds; legacy gameplay compatibility |
| Export | Existing scene inclusion/exclusion, asset closure/runtime roots and package destination |

**Save project** persists configuration and resets its independent dirty indicator.
**Undo settings / Redo settings** affect configuration, separately from scene
undo. Closing/replacing a project protects unsaved configuration with
Save/Discard/Cancel. Run/Export offer an explicit save-all or saved-version choice.
Save scene edits separately during ordinary editing. Stop/restart
Play for runtime policy changes. Runtime construction, scene replacement and cold
save staging receive the same captured project policy. Configuration controls are
unavailable during Play; the world also rejects native reconfiguration after Build.
Changes to the existing visual resource budget still require reopening the project.

Project settings do not override every scene's physics, gravity or environmental
parameters. Fixed timestep/solver configuration was deliberately not redesigned
alongside the concurrent MK2 work. Networking remains explicit game policy; there
is no automatic server, account service, or project-wide port chosen by this panel.

## Default water: inexpensive prescribed volumes

New and old projects with no new fields get these defaults:

```text
basic-water "true"
water-swimming "true"
advanced-fluid-dynamics "false"
```

The named project representation uses `basicWater`, `waterSwimming` and
`advancedFluidDynamics`. Both codecs preserve the same strict booleans. The project
serializer/exporter carries them; they are not editor preferences or local-only
runtime flags. Enabling support does not automatically fill a scene with water.

Use **Create → Basic water**, **Project Settings → Water → Create basic water**,
or add **Basic water** through an entity's component menu. The Inspector provides:

- Explicit half width, depth, half length; entity scale stays one.
- Colour and opacity.
- Visual wave amplitude, wavelength and speed.
- Prescribed local swimming current and density.

The entity's local +Y points out of the water. Its local occupancy is
`[-halfWidth,+halfWidth] × [-depth,0] × [-halfLength,+halfLength]`, transformed by its
ordinary scene pose. Rotate the entity for rotated gravity. Scene hierarchy,
prefabs, streaming publication, origin shifts and authored save data use existing
entity ownership; no independent absolute water coordinate system is introduced.

### Rendering

A lazily allocated shared 48×48 grid animates two directional waves in the existing
GPU vertex shader. Analytic slope normals feed approximate Fresnel sky reflection
and directional sun glints. Transparent surfaces are double-sided and depth-tested.
No per-frame CPU remeshing, particle reconstruction, pressure solve or flow field
is required. The grid is reused by all volumes; absent water does not allocate it.
Editor previews draw water after opaque terrain and props, including neighbouring
world regions, so authoring order cannot hide it. Normal depth testing still
occludes water behind nearer geometry. See the
[preview correction and evidence](M100_BASIC_WATER_PREVIEW.md).

The existing underwater optical shader receives inexpensive ray/box intervals.
Opaque geometry is attenuated by water depth. Each view recomputes its own paths;
it does not reuse a primary camera's history. The normal renderer handles colour,
lighting, HDR and viewport state. Ordinary visibility/render layers control surface
submission.

These are deliberate approximations:

- Reflection is a procedural sky approximation, not screen-space or planar reflection.
- No refraction, caustics, shoreline wetting, foam simulation or fluid displacement.
- Water optics attenuate rendered geometry; they do not physically refract the sky.
- Visual waves do not change the mean swimming volume or carry conserved mass.
- One nearest water interval is represented per optical sample; separated volumes
  on one ray do not accumulate exact disjoint absorption.
- Finite oriented boxes model lakes, pools and authored river/ocean regions. One
  box is not a planet-wide spherical ocean. Use local oriented regions; choose
  wavelengths appropriate to the bounded grid, rather than tiny waves on one
  enormous tile.
- This system supplies no rigid-body buoyancy, water pressure, flow, flooding,
  scooping or transfer. These require advanced fluids, which are unavailable in Alpha.

### Swimming

`world.fluidSample(point, up, halfHeight, radius, tangent)` now works without PBF.
It returns the existing `{immersion,density,velocity,acceleration}` shape. Basic
water uses bounded 17-sample capsule-centre-line occupancy, a prescribed current
rotated to world space, and zero fluid acceleration. This is a swimming envelope
approximation, not exact displaced volume. It supports arbitrary supplied up.
Overlapping volumes do not add immersion beyond one; greatest immersion wins
(ties retain stable owner order). When an advanced particle field also exists,
the greater-immersion sample wins rather than adding two buoyancy sources.

**Water swimming queries** can be disabled without hiding water surfaces. Disabling
**Basic water** disables its volumes and visuals together. The historical native
player also receives basic-water samples. Modern CharacterMotor games own swim
policy in JS: cancel the appropriate fraction of gravity through buoyancy, relax
velocity toward the current, and add directional propulsion. The reusable motor
has not gained a hardcoded swimming game controller.

See the complete ordinary project at `projects/basic_water_lab`. Its script uses
public logical input, CharacterMotor, presented camera poses, runtime UI and the
existing query. No private native dispatch is used.

## Experimental Advanced Fluid Dynamics (non-Alpha builds)

**Project Settings → Experimental → Advanced Fluid Dynamics** is off by default.
Checking it opens a warning; **Cancel** does not enable it. Confirmation explains
that the feature is experimental, potentially performance-intensive, and subject
to geometry/numerical/streaming/persistence limitations. The warning remains
visible in the category while enabled. An explicitly authored project field is
also an opt-in; command-line runtime does not prompt interactively.

When disabled:

- RuntimeWorld does not allocate/populate a particle FluidWorld, particle volumes,
  particle coupling or a particle surface mesh.
- Conserved liquid owners are not synchronized or advanced. No dynamic-surface,
  flow, parcel, buoyancy or fluid-body drag update runs.
- Advanced baked resources are not requested by normal runtime construction.
- Native attempts to register conserved owners on that world are rejected before
  surface allocation. Existing JS owners remain null until enabled and registered.
- Authored components/assets are preserved; disabling does not convert or delete
  conserved quantities or save data.
- Basic-water drawing and JS/native swimming remain independent.

When enabled, existing supported particle/PBF and conserved reservoir/container
paths retain their established algorithms, tolerances, cadence and ownership.
This work does not alter any fluid numerical method, independent Solbert research,
protected FTFT/P1 experiments, or deferred fluid defect acceptance criteria.
Standalone mathematical LiquidSystem fixtures remain usable without a project;
the project gate applies to RuntimeWorld-owned instances.

### Compatibility and persistence boundaries

This intentionally changes Pre-Alpha defaults: a project or native RuntimeWorld
that previously relied on implicit advanced simulation must opt in explicitly.
The shipped advanced-fluid examples are given explicit opt-in fields. Active native
regression fixtures opt in without changing forces, inputs or thresholds.

Basic water persists as ordinary component data. Native cold snapshots preserve
its authored parameters, entity state, motor and script state through the existing
save participants. Disabled legacy particle components are inert authoring, so they
do not invoke the active-PBF cold-save restriction. A saved scene-owned global
entity is reconstructed as scene-owned metadata, not rejected as an attempted
runtime-spawn of a global component.

Active PBF still has its existing separate legacy save contract; enabling the new
switch does not create a modern cold-save particle participant. Conserved-liquid
saves still require their exact supported geometry/topology and enabled project
policy. Loading any nonempty advanced owner, detached-parcel, ledger or connection state into a disabled world fails atomically with
an actionable opt-in diagnostic; it must never silently discard that state.
Switching policy is a fresh-world operation, not a live conversion of an existing
ledger. Original save bytes are not rewritten merely by changing the setting.

Existing unsupported geometry, origin, regional suspension and solver limits
remain. Consult [conserved liquid](judasjs/liquid.md),
[dynamic surfaces](LIQUID_SURFACES.md), and the current
fluid limitation records. No claim of climate/continuum fluid accuracy is made.

## Human review

Open `projects/basic_water_lab/basic_water_lab.judasproj` in this branch's editor.

1. In Project Settings, visit each category; inspect existing controls and the
   explanation of scene-owned physics/gravity and script-owned networking.
2. Confirm Advanced Fluid Dynamics is off. Check it, cancel the warning, and
   confirm it remains off. Confirm again, save, reopen; then turn it back off.
3. In Stillwater, move/look with WASD/mouse. Walk down the steps or jump in.
   Space swims upward; Q dives. Watch the immersion display and underwater tint.
4. Pause/resume with Escape. Confirm no look/movement leaks through the menu.
5. Edit the pool's wave amplitude/colour/current, save and restart Play.
6. Disable swimming only: water stays visible but the character sinks.
7. Disable basic water: no basic surface or swimming volume remains.
8. Open a shipped advanced-fluid example, inspect its explicit opt-in, and try
   the existing supported controls. Numerical limitations remain documented.
9. Run the relocated Stillwater export and repeat the basic swim/pause checks.

Automation results, measurements and failure/follow-up records are maintained in
`docs/evidence/prealpha-project-settings-water`. Project settings/basic water were human accepted and integrated before the accepted
`c76e92dc` baseline; later platform qualification remains separate.

### Dormant authored content

With Advanced Fluid Dynamics disabled, authored advanced components remain in
scenes/prefabs/exports, but their liquid bakes are not required or loaded for
regional admission and do not create conserved-liquid suspension pins. Export
dependency closure omits these inactive references; conservative all-assets
export still copies registered assets. Enabling the feature restores normal
geometry/bake validation. Other active components keep their ordinary checks.
