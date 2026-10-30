# Native 2D Stage 0 — bounded architecture decision

30 October 2026. Production baseline:
`e2008d1e6f4ac3229b292724e5718f37e8d2e1ed` (Alpha v1.0.4).

**Select reduced planar mobility with explicit planar joint rows.** The bounded
algebra, camera and native MK2 proofs support this direction. This is not a usable
2D editor/game feature or universal composition certification. Production `src/`,
project APIs, build wiring and main are unchanged. Terra's files, fluids and
independent research are untouched.

## Initial implementation contracts

- One scene, renderer, authoritative MK2 clock, stable handles and export path.
- Default XY plane, +Z angular direction: two linear coordinates and one angular
  coordinate. A fixed rotated plane is tested for covariance, not curved or
  dynamically changing planar gravity frames.
- Metres, kilograms, seconds, radians. Rectangle lamina inertia:
  `m*(width²+height²)/12`; eventual disk inertia: `m*radius²/2`. Art scaling must
  not silently rescale existing physical geometry/mass.
- Render sort/depth never changes the physical plane. Back-to-front order:
  layer ordinal, order-in-layer, optional negative world pivot Y, stable entity
  and subfeature identity. Layer identity survives reorder.
- Straight-alpha artwork; the sprite pass must own its blend/depth state and test
  transparent edges. Existing 3D material defaults remain unchanged.
- Normalized source-image pivots from the top-left, centre by default. Pixel to
  local coordinates: `((x-pivotX*width)/PPU,(pivotY*height-y)/PPU)`. Visual flips
  mirror around the pivot without implicitly mirroring colliders. Suggested
  32 pixels/unit is an editable preset, never an engine constant.
- Viewport coordinates remain top-left. Orthographic rays have parallel
  directions and distinct origins. Pixel snapping is presentation-only.
  Perspective remains the default. Stable sprite/frame identity must not depend
  on mutable filenames or array indices.
- These art/sort rules are design contracts awaiting Stage 1 renderer/import
  tests. They are not implemented or visually qualified features.
- Reject unsupported mixed 2D/3D couplings. This proof does not establish arbitrary
  wheel/ragdoll/deformable/fluid composition.

## Physics decision and failures

For unit plane normal `n`, mass `m` and planar inertia `Iz`:

```
linear response  = (I - n n^T) impulse / m
angular response = n (n . angularImpulse) / Iz
E = m |v|² / 2 + Iz (n . omega)² / 2
```

This is condensed bilateral response, not damping or a post-step clamp. The
prototype retains invertible physical inertia and never inverts its singular
planar response tensor for kinetic energy. Motion leaves carry the descriptor
through CCD; forces, impulses, contacts and pin rows use the same mobility.

The competitor uses three genuine native constraint rows. It passes simple
wall/corner CCD and rotated support, but fails the unchanged off-centre floor
case: normal displacement 0.000169848 m versus 0.0001 m, tilt 0.00102628 versus
0.001. Direct impulses also publish forbidden velocity until the next solve.
No iteration increase, gate relaxation or pose clamp was used to hide this.

A nearly aligned legacy 3D hinge also fails after reduction: a tiny represented
axis error becomes an unwanted constraint on the only allowed spin. The preserved
control loses all spin (5.17241383 rad/s to zero). An explicit planar pin uses two
translational rows and no approximate angular alignment rows in the same native
joint solver. It preserves free spin and energy. Unsupported old hinge coupling
rejects in the reduced prototype; production 3D joints are unchanged.

## Actual verification

- Algebra/camera: 74 checks pass, plus a deliberately missing-row binary that
  must fail at the physical response contract.
- Reduced native MK2: 56 checks pass. Actual point impulses, forces, torques,
  off-centre frictional landing, two-body elastic momentum, simultaneous corner
  CCD at 5/150 m/s, rotated support, sleep/wake and planar pin.
- Prototype versioned mobility saves restore actual impulse response on an
  initially unconfigured body. Invalid normals, newer versions and truncation
  reject without changing previous response; reused slots do not inherit it.
- Existing MK2 suite passes on unmodified and reduced builds. Existing rotation
  suite passes 31,404 checks on each. These are real source builds, not stale
  binaries or a separate toy simulation.
- Unmodified MK2, row prototype and reduced prototype have the same recorded
  300-step 3D trajectory digest: `f67106f8392d2826`. Six bodies, gravity, contacts,
  hinge and a later off-centre impulse; not certification of every 3D workload.
- Camera prototype verifies orthographic framing/aspect, parallel rays,
  viewport roundtrips, invalid inputs and exact old perspective-default matrix.
  Existing clip-w-based `ProjectViewport.behind` cannot identify eye-back points
  under ortho. The private descriptor uses signed view depth; production camera
  consumers are not changed or claimed integrated.

## Native scaling

Release GCC 15.2 on Linux Ryzen 5 5600, shared desktop, no affinity. Same 1/60 s,
32/256/1,024 isolated moving sphere proxies, no gravity/physical contacts. Ten
warmup plus 90 measured steps. These are actual world/constraint costs, not a
production speedup, dense-collision benchmark or predicted game FPS.

| Bodies | Rows median / p95 / worst ms | Reduced median / p95 / worst ms |
|---|---|---|
| 32 | 0.14090 / 0.17608 / 0.20037 | 0.03176 / 0.03741 / 0.03917 |
| 256 | 1.14319 / 1.18064 / 1.22780 | 0.26270 / 0.29515 / 0.34742 |
| 1,024 | 4.63791 / 4.71320 / 4.99276 | 1.09182 / 1.15344 / 1.46162 |

Shapes are existing **3D test proxies**, not newly implemented circles/polygons.
Inline experimental body fields/dispatch are not an approved final optional
storage design. Do not extrapolate these results to finished native 2D games.

## Cold continuation — strict limitation retained

Separate processes capture at step 45 and continue 120 steps using real native
mass/forces, sleep, contacts/observations and applicable joint history. Restored
pose, velocity and sleep bytes match before continuation. All final physical
endpoints rest on actual ground with zero velocity and matching sleep state.

An additional strict byte-identity check fails at the first continued step.
Reduced endpoint position differs by 3.00343e-8 m; orientation quaternion-distance
proxy by 1.05356e-8 rad. This is also present in the ordinary 3D control. The strict
check remains FAILED, not relabeled as a pass.

A separately labeled physical criterion, introduced after that diagnostic,
requires position/orientation errors below 5e-6, both velocity errors below 5e-5
and matching sleep. All three cases pass. This is not bitwise cold determinism.

Cause established by ablation: clearing only the producer's contact warm cache
at the continuation boundary gives byte-identical resumed results in both 3D and
reduced controls. Native persistence stores completed contact observations, not
live solver contact warm history. This reset is diagnostic ONLY, never used by
the normal candidate or proposed as a production fix. No settling delay or blind
cache serialization was added.

## Reproduce

The generator copies source into disposable `.cache` trees and applies guarded
transformations there only. One build uses unmodified production source. Inputs,
generated diffs, binary hashes, raw logs, snapshots and failures are recorded in
`docs/evidence/2d-stage0-20261030/`.

```sh
python3 tools/run_planar2d_stage0.py
python3 tools/prototypes/planar2d_native_rows.py --baseline --output .cache/planar2d-baseline-stage0 --build-only
python3 tools/prototypes/planar2d_native_rows.py --output .cache/planar2d-native-stage0 --build-only
python3 tools/prototypes/planar2d_native_rows.py --reduced --output .cache/planar2d-reduced-native-stage0 --build-only
python3 tools/prototypes/qualify_planar2d_stage0.py
```

Needs GCC/GLM, CMake, Make, Python. Builds are sequential, two compiler jobs on
the 8 GB host. Early missing Ninja/QuickHull include failures are preserved as
setup failures; no dependency was installed. Final qualification records 28
separately classified runs, including rejected candidates and strict diagnostics.

## Boundary / next approval

The bounded decision proof is ready for review. Stage 1 needs separate approval:
native sprites, generic projection integration and the first 2D editor/project
vertical slice. There is no public 2D component, API or playable 2D project yet.

Production reduced mobility still needs real 2D shapes/contact generation,
scene/prefab/stream/save ownership, JS ergonomics, unsupported-combination preflight
and optional storage/dispatch. Whole-game cold saves, editor Play/Stop, render
captures, SDK/export and Windows/Android 2D validation belong to subsequent gates.
Main, published packages, existing games and Advanced Fluid lock are unchanged.
