# Shadow receiver-plane correction and browser endurance — 31 October 2026

Experimental follow-up from `0cddd4a4`. Main and published releases are unchanged.
Browser support still requires `-ExpBrowserSupport=1`; Advanced Fluids remain locked.

## Confirmed cause

The repeated triangles were shadow acne, not extra mesh triangles or a Firefox-only
failure. Both Firefox/NVIDIA and Chromium/SwiftShader reproduce them. The old
nine-tap PCF compares the fragment-center depth with neighboring shadow texels.
On a sloped receiver, each neighbor represents a different surface depth. A fixed
normal-based bias cannot consistently absorb that geometric difference across
shadow resolutions, projection scales and viewing phases.

Diagnostics retained: bypassing shadow evaluation removes the pattern; explicit
fragment-depth output and integer depth fetching alone leave it unchanged.
The receiver-plane prototype removes the flat-face pattern while preserving
occlusion. The production correction was then compiled through the normal native
and browser build/export paths; diagnostic JavaScript packages were not adopted.

## Generic correction

`src/Renderer.cpp` captures world-position derivatives once, at the start of the
fragment shader. Each selected light projects those plane directions with the
projective quotient rule. Solving the resulting 2x2 system gives depth change per
shadow UV coordinate. Each of the same nine depth texels is compared against the
receiver plane at that texel's actual center:

`receiverDepth = fragmentDepth + dot(depthGradient, texelCenterUV - fragmentUV)`.

The numerical bias, map resolutions, map count, lighting, geometry and physics
settings are unchanged. Integer fetches make tap centers and edge clamping
explicit. Degenerate/nonfinite gradients fall back to the existing bias.

Derivatives are outside material and tiled-light branches; applying the selected
matrix analytically avoids differentiating between different lights in adjacent
fragments. This also respects the nonuniform-control-flow derivative restrictions
in the [Khronos GLSL ES specification](https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.html).

## Correctness checks

- The actual production shadow function is compiled by a small WebGL2 oracle.
  Independently populated depth planes cover flat/oblique/opposite slopes,
  projective lights, real occluders and adjacent tiles selecting different lights.
  **6/6 pass in Firefox, 6/6 in Chromium**, with zero bad pixels and GL errors.
  The original function fails **5/6** on the unchanged oracle.
- Full source-built player captures in both browsers: the selected wall region
  changes from per-channel ranges **8/10/12** to **0/0/0**; the crate region changes
  from **4/15/8** to **0/0/0**. Floor shadow `(13,17,19)` remains distinct from
  the lit floor `(78,97,94)`. These checks would reject disabling shadows.
- Native renderer architecture: **44/44**, including authored spotlight
  occlusion, independent cameras, array layers, instancing and target teardown.
- Native sprites: **22/22**, including pixel-perfect motion and UI layout.
- Browser export/workflow Python tests: **34/34**.

## Longer memory check

Installed Firefox 159 on the NVIDIA host completed **600 seconds of continuous
play**, then **three additional 30-second play/close/reopen cycles**. The complete
run lasted about 12.8 minutes, with 529 real WebDriver keyboard/mouse action
batches and 766 memory samples. Profiles were isolated; motion and first/third
person switching exercised the real WASM game.

The predefined gates passed: total cgroup peak **1331.7 MiB (1.30 GiB)**;
median private/shared memory ended **29.0 MiB lower** than the early steady-state
window. After leaving the game, memory fell instead of surging. After full browser
exit it returned to **53.9, 55.1, 56.3 and 57.5 MiB** across the four sessions
(controller/server included). No GPU Xid or OOM event was recorded in the kernel
query covering the run.

Gates were recorded before launch: 2500 MiB hard cgroup limit, zero swap,
2200 MiB proactive stop, 128 MiB maximum plateau growth, 64 MiB permitted growth
10 seconds after navigation, and at most 256 MiB private/shared memory five
seconds after browser exit. These were not relaxed after measurement. Cgroup
memory includes browser descendants, automation, server and charged cache;
private/shared plateau and cleanup comparisons use `anon + shmem`.

The test uses the installed Firefox binary in a fresh automated profile, not the
operator's personal browser profile. The GPU path is the host's NVIDIA stack;
privacy-masked WebGL renderer strings are not independent GPU identification.
No isolated CPU/GPU throughput or universal browser-memory claim is made.

## Scope and limitations

This fixes the tested false self-shadow pattern. Finite-resolution PCF still has
ordinary jagged/soft shadow boundaries and a local-plane approximation on curved
surfaces; it is not a cascaded or physically exact shadow system. Windows,
Android, mobile browsers and Safari were not qualified by this change.

The test package retains its parent/dirty-source build identity. It is not
relabelled as a clean post-commit export. Evidence pins the exact compiled source
and binaries, old failures, screenshots, gates and raw results under
`docs/evidence/browser-shadows-20261031/`.

## Human check

Use the fresh browser player package. Inspect the walls/crates while moving and
in third person, confirm genuine cast shadows, then close and reopen. No human
acceptance of this shader candidate is inferred from the automated checks.

Reproduce the focused operator tests with:

```sh
PLAYWRIGHT_MODULE=/absolute/path/node_modules/playwright \
  node tests/browser/shadow_plane.cjs /fresh/results firefox
# Repeat with chromium. Optional third argument selects an archived Renderer.cpp.
```
