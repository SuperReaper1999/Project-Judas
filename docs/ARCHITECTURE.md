# Architecture

This document explains the technical decisions behind Project Judas and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project. It is updated in place as
milestones land, rather than kept as a per-milestone snapshot — see
"Milestone history" below for how to recover an earlier milestone exactly.

## What exists right now (Milestone 2)

Open a window. Render three cubes in 3D space with correct perspective and
depth testing. Fly a free camera through the scene with keyboard + mouse.
Nothing else. See the root `README.md` for build/run instructions and
controls.

## Milestone history

Each milestone is tagged in git so it can be recovered exactly, rather than
preserved as parallel runtime code:

- `milestone-1` — window creation, a single 2D box, keyboard movement. The
  demo content this document originally described has since been replaced
  (not extended) by Milestone 2's 3D scene; check out the tag to see or run
  it as it was.

## Language: C++

The long-term engine needs tight control over large-world coordinate math,
and will eventually integrate physics middleware (rigidbody dynamics,
collision detection) and possibly a low-level graphics API like Vulkan. The
mature options in that space (Jolt Physics, PhysX, Vulkan itself) are C or
C++ libraries with the most direct, lowest-overhead integration path in C++.
Choosing C++ now avoids introducing an interop layer later for problems we
already know we'll have.

C# was considered and is workable (e.g. via Silk.NET), but it buys nothing
here and adds friction for the physics/graphics integration work that's
explicitly called out as a future requirement. There was no technical
reason to prefer it.

## Dependencies

**SDL2** — provides window creation, OS event handling, keyboard/mouse
state, and OpenGL context creation. This is a solved, commodity problem
(cross-platform windowing/input) that every mature engine either uses a
library for or reinvents badly. SDL2 is mature, widely used in shipped
games, and its scope (window/input/context, plus audio and controller
support we aren't using yet but will eventually want) matches this engine's
needs without pulling in anything unnecessary. GLFW would have served the
window/input/context role equally well; SDL2 was chosen because its
broader scope (controller input, audio) is directly useful to this engine
later at no cost now, and it was already present on this development
machine.

**GLM** (added in Milestone 2) — a header-only C++ library providing
`vec3`/`mat4` types and the standard operations 3D rendering needs
(`lookAt`, `perspective`, `translate`, `cross`, `normalize`, ...). Real
model/view/projection matrix math is a hard requirement starting this
milestone. Vector/matrix math with correct, fast, well-tested
implementations is a completely solved problem — GLM is the de facto
standard in the OpenGL/Vulkan world, mirrors GLSL's own syntax and
conventions (so shader code and C++ code read the same way), and is
header-only, so it adds no linking complexity. Writing our own would mean
re-deriving well-known linear algebra for no advantage. It's MIT-licensed,
compatible with this repository's own MIT license. Installed via the
system package manager (`libglm-dev`) rather than vendored, consistent with
preferring package-manager integration over vendoring when it's sufficient.

Nothing else. No physics, no asset loading, no image libraries yet — see
"Deliberately Not Implemented" below.

## Windowing & Graphics API

**SDL2 + OpenGL 3.3 core profile.**

Vulkan was considered again for this milestone, since 3D rendering is
where its explicitness starts to look more appealing. It was rejected
again for the same reason as Milestone 1: its setup cost (instance,
physical/logical device selection, swapchain, pipeline objects, command
buffers) buys nothing at the "prove we can render and navigate a 3D scene"
stage, and the milestone brief explicitly excludes it.

This is still not treated as locking out Vulkan later. `Renderer` (see
`src/Renderer.h`) remains the sole owner of graphics state and draw calls:
`Application` and `Camera` describe *what* to render (a camera, a cube at a
position with a color) and never issue an OpenGL call themselves. Moving to
Vulkan later means rewriting `Renderer`'s internals, not the call sites
that use it.

### OpenGL function loading

Milestone 1 introduced a small hand-written loader (`src/gl_core33.h/.cpp`)
instead of a generated one (glad/GLEW), on the grounds that the ~30
functions needed were too small a surface to justify a generator's
build-time or vendoring cost — with an explicit note to reconsider once the
surface grew.

Milestone 2 was the point to re-examine that. The actual new functions
needed for 3D rendering with depth testing turned out to be exactly two:
`glEnable` and `glDepthFunc` (everything else — buffers, shaders, uniform
matrices — was already loaded for the 2D renderer). That's a small enough
addition that hand-extending the existing loader remains the smaller,
simpler option; switching to glad2 now would be a larger change than the
actual requirement justifies. This decision should be revisited again the
next time the GL surface needs to grow — likely when textures or
framebuffers show up.

## 3D rendering pipeline

`Renderer::DrawCube` implements the conventional model → world → view →
clip-space pipeline entirely via matrices, computed with GLM and uploaded
as uniforms to a single, simple shader (`src/Renderer.cpp`):

```
local (unit cube, [-0.5, 0.5] per axis)
    --(uModel: glm::translate to the cube's world position)-->
world coordinates
    --(uView: Camera::GetViewMatrix(), a glm::lookAt)-->
camera/view coordinates
    --(uProjection: Camera::GetProjectionMatrix(), a glm::perspective)-->
clip space / screen
```

The vertex shader is exactly:

```glsl
gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
```

There is no per-object rotation or scale yet — `DrawCube` only translates —
because nothing in this milestone needs it. The model matrix is still a
full 4x4 transform, not a raw position add, so introducing rotation/scale
later is a local change inside `DrawCube`, not a pipeline change.

### Coordinate conventions (current, local to this renderer)

- World space is a conventional right-handed 3D space in engine-defined
  units ("world units"); nothing currently maps a world unit to a physical
  size, since there's no terrain or physical scale reference yet.
- `+Y` is used as a reference "up" direction by the camera and by nothing
  else. **This is explicitly not an engine-wide law** — see "Camera
  orientation" below and "Future constraints preserved."
  `-Z` is the camera's initial forward direction, matching OpenGL's
  conventional default view direction.
- These are the only coordinates that exist right now. There is no
  distinction yet between authoritative large-world coordinates and local
  rendering coordinates (see "Future constraints preserved") — introducing
  that distinction is future work, not something this milestone had reason
  to build.

## Depth handling

`Renderer::Init` enables depth testing once, at startup, and leaves it on
for the lifetime of the program:

```cpp
glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LESS);  // the GL default; set explicitly for clarity
```

`Renderer::BeginFrame` clears both the color and depth buffers each frame
(`GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT`). A 24-bit depth buffer is
requested at context creation via
`SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)` in `Window::Init` — without
this, the GL context has no depth buffer to test against regardless of
`glEnable(GL_DEPTH_TEST)`. Draw order of the three demo cubes in
`Application.cpp` is deliberately not sorted by depth, to make it obvious
that correct occlusion comes from the depth buffer and not from paint
order.

## Projection handling

`Camera::GetProjectionMatrix(aspectRatio)` builds a perspective projection
via `glm::perspective(fov, aspectRatio, near, far)` fresh every frame,
using the window's current width/height. `Application::Run` computes
`aspectRatio` from `window.Width() / window.Height()` each iteration of the
loop rather than caching it or reacting to a resize event specifically —
the same "just recompute it every frame" approach Milestone 1 used for its
2D projection. This means a window resize is handled automatically and
correctly with no dedicated resize-handling code path to get wrong; window
height is clamped to at least 1 to avoid a divide-by-zero if the window is
minimized.

## Main loop structure

`Application::Run()` (`src/Application.cpp`) owns the loop:

```
Init Window (SDL2 window + GL context + depth buffer request)
Load GL functions
Init Renderer (compile shaders, upload cube geometry, enable depth testing)
Create Camera

while (!window.ShouldClose()):
    window.PollEvents()          // pump OS events, close request, Escape toggle
    deltaTime = measured elapsed time since last frame
    camera.Update(window, deltaTime)   // keyboard movement + mouse look
    aspectRatio = window.Width() / window.Height()
    renderer.BeginFrame(...)     // clear color+depth, set viewport
    renderer.SetCamera(camera.GetViewMatrix(), camera.GetProjectionMatrix(aspectRatio))
    for each demo cube: renderer.DrawCube(position, color)
    renderer.EndFrame()
    window.SwapBuffers()

renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

The engine-level split from Milestone 1 is unchanged:

- `Window` (`src/Window.h/.cpp`) — window/input, combined because input is
  just "keyboard/mouse state of this window."
- `Renderer` (`src/Renderer.h/.cpp`) — graphics.
- `Camera` (`src/Camera.h/.cpp`) — the demo content/game-state layer for
  this milestone (Milestone 1's equivalent was `Box`).
- `Application` (`src/Application.h/.cpp`) — wires the above together and
  owns the loop.

## Input handling

`Window::IsActionActive(Action)` still exposes actions, not raw key codes,
for the same reason as Milestone 1 — so a future input source (controller)
can drive the same actions without callers changing. The action set
changed to match a free-flight camera instead of a 2D box:

`MoveForward` / `MoveBackward` / `StrafeLeft` / `StrafeRight` / `Ascend` /
`Descend`, bound to `W`/`Up`, `S`/`Down`, `A`/`Left`, `D`/`Right`,
`Space`, `Left Ctrl` respectively. "Forward" and "strafe" are relative to
the camera's current look direction, not to any fixed world direction.

**Mouse look** is new: `Window::GetMouseDelta` wraps
`SDL_GetRelativeMouseState`, and the mouse is captured
(`SDL_SetRelativeMouseMode`) on startup so it drives yaw/pitch immediately,
as is conventional for a free-camera/first-person demo. `Escape` toggles
capture on/off (handled as a discrete key-down event in
`Window::PollEvents`, not a polled action) — this is the "sensible way to
release/restore mouse control" called for by the brief, not a general
input-remapping system. While released, `GetMouseDelta` reports zero
motion (but still drains SDL's internal accumulator), so releasing the
mouse also stops camera look, and re-capturing doesn't produce a jump from
motion that happened while released.

## Camera orientation

`Camera` (`src/Camera.h/.cpp`) is a conventional yaw/pitch free-fly camera.
It keeps a fixed reference axis, world `(0, 1, 0)`, to build a stable local
right/up/forward frame for mouse look and to give `Space`/`Left Ctrl` a
consistent vertical direction.

**This is a convention scoped to this one class for this one demo scene —
explicitly not an engine-wide definition of "up."** Nothing outside
`Camera.cpp` references it, and no other system in the engine treats
world `+Y` as meaningful. The engine's long-term requirements include
worlds with no universal up (arbitrary gravity, spherical planets, moving
spacecraft frames), and this is called out here specifically so that fact
isn't lost when this file is next extended: when reference frames exist,
camera orientation will need to derive from whichever frame the camera is
currently in, not from a hardcoded world axis. See `Camera.h`'s own comment
for the same note at the point of use.

## Frame timing

Unchanged from Milestone 1: `Application::Run()` measures real elapsed
time each frame via `SDL_GetPerformanceCounter()` /
`SDL_GetPerformanceFrequency()`, clamped to a maximum of 0.25s to stop a
stall (e.g. window drag) from producing one large visible jump.

`Camera::Update` uses this `deltaTime` for **keyboard movement**
(`position += direction * speed * deltaTime`), exactly like Milestone 1's
box. Movement direction across all six keyboard actions (including
vertical) is combined into one vector and normalized once, so pressing
multiple movement keys at once doesn't move faster than one.

**Mouse look deliberately does not use `deltaTime`.** `SDL_GetRelativeMouseState`
already returns the pixels the mouse moved *since the last call*, i.e. it's
already an amount of motion, not a rate — multiplying it by `deltaTime`
would double-apply time and make sensitivity vary with frame rate instead
of being independent of it. As long as the mouse delta is polled exactly
once per frame (which it is, from `Camera::Update`), applying it directly
is what makes look input frame-rate independent. This is explained at the
point of use in `Camera.cpp` since it's the kind of thing that looks like a
bug (a moving value with no `deltaTime` next to it) if you don't know why.

## FUTURE CONSTRAINTS PRESERVED

This section explains only how the current design avoids *unnecessarily*
blocking known future requirements. None of these are implemented yet.

- **Arbitrary gravity / reference frames / no universal up** — the only
  place world `+Y` is treated as "up" is inside `Camera`, documented
  explicitly as a local, temporary convention (see "Camera orientation").
  No renderer, math, or engine-level code depends on it. Introducing real
  reference frames later means changing how `Camera` (and later, other
  objects) derive their orientation, not undoing an assumption baked into
  the rendering pipeline.
- **Large planetary coordinates** — `Renderer` takes plain `glm::vec3`
  world positions and has no idea what scale they represent. There is
  still no distinction between authoritative large-world coordinates and
  local rendering coordinates (see "Deliberately Not Implemented"), but
  nothing here actively assumes positions are small or that
  render-space and world-space are the same representation forever —
  that separation simply hasn't been needed yet.
- **Spherical terrain** — still nothing here assumes an infinite flat
  plane; there is no terrain of any kind, so there's nothing to unwind.
- **Physics middleware** — the update step (`camera.Update`) and the
  render step (`renderer.BeginFrame`/`DrawCube`/`EndFrame`) remain
  separate stages of the loop, so a future physics step slots in as its
  own stage.
- **Moving spacecraft reference frames** — `Camera` computing its own view
  matrix from position + orientation, independent of how the scene's
  objects are drawn, is the same shape a moving-frame camera will
  eventually need (a camera attached to a frame, not to fixed world axes).
  Nothing here prevents that; it just isn't built yet.

## DELIBERATELY NOT IMPLEMENTED

Explicitly deferred, not forgotten:

- Physics, collision detection, gravity
- Planets, terrain (including Terrain-ML), a player capsule/controller
- Lighting, shadows, textures, materials, model loading
- Audio, networking
- Entity/component systems, scene graphs
- Editors, scripting, UI frameworks
- Reference-frame systems, floating origin, astronomical coordinates
- Vulkan (see "Windowing & Graphics API" above)
- Gameplay of any kind
- Controller input (the `Action` boundary exists for this, but only
  keyboard + mouse are wired up)
- A generated OpenGL loader (glad/GLEW) — see "OpenGL function loading"
  above; the hand-written loader was re-evaluated this milestone and kept
  deliberately, not by default.
- Per-object rotation/scale, indexed drawing, vertex colors/normals, or
  any vertex format beyond bare position — the demo cubes don't need them.
- Face culling — not enabled; irrelevant for solid opaque cubes viewed
  with depth testing, and not worth the winding-order bookkeeping it would
  require in `kCubeVertices` for this milestone.
