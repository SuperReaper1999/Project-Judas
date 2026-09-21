# Architecture — Milestone 1

This document explains the technical decisions behind Milestone 1 and how
they relate to the engine's long-term purpose. It is meant to be readable by
someone with no prior context on this project.

## What exists right now

Open a window. Render a visible 2D box. Move that box with the keyboard.
Nothing else. See the root `README.md` for build/run instructions.

## Language: C++

The long-term engine needs tight control over large-world coordinate math,
and will eventually integrate physics middleware (rigidbody dynamics,
collision detection) and possibly a low-level graphics API like Vulkan. The
mature options in that space (Jolt Physics, PhysX, Vulkan itself) are C or
C++ libraries with the most direct, lowest-overhead integration path in C++.
Choosing C++ now avoids introducing an interop layer later for problems we
already know we'll have.

C# was considered and is workable (e.g. via Silk.NET), but it buys nothing
for Milestone 1 and adds friction for the physics/graphics integration work
that's explicitly called out as a future requirement. There was no
technical reason to prefer it here.

## Dependencies

**SDL2** — provides window creation, OS event handling, keyboard state, and
OpenGL context creation. This is a solved, commodity problem (cross-platform
windowing/input) that every mature engine either uses a library for or
reinvents badly. SDL2 is mature, widely used in shipped games, and its
scope (window/input/context, plus audio and controller support we aren't
using yet but will eventually want) matches this engine's needs without
pulling in anything unnecessary. GLFW would have served the window/input/
context role equally well; SDL2 was chosen because its broader scope
(controller input, audio) is directly useful to this engine later at no
cost now, and it was already present on this development machine.

**Nothing else.** No math library (GLM, etc.) yet — Milestone 1 only needs a
handful of manually-computed floats for a 2D orthographic projection.
GLM should be introduced when real 3D vector/matrix math shows up (see
"Deliberately Not Implemented" below); pulling it in now would be
speculative.

## Windowing & Graphics API

**SDL2 + OpenGL 3.3 core profile.**

Vulkan was considered, since it's the more plausible long-term target for
large open-world/planetary rendering. It was rejected *for this milestone*
because its setup cost (instance, physical/logical device selection,
swapchain, pipeline objects, command buffers — typically several hundred
lines before a single triangle appears) buys nothing at the "prove we can
render a box" stage. The brief for this milestone explicitly prioritizes
owning the render loop over rendering sophistication.

This choice is deliberately *not* treated as locking us out of Vulkan
later. The `Renderer` class (see `src/Renderer.h`) is the engine's own
boundary: "initialize graphics / begin a frame / draw a rectangle / end a
frame" is a concept this engine owns, independent of which low-level API
implements it. `Application`, `Window`, and `Box` never call an OpenGL
function directly — only `Renderer` does. Replacing OpenGL with Vulkan
later means rewriting `Renderer`'s internals, not the engine's structure.

### OpenGL function loading

OpenGL 3.3 core functions (buffers, vertex arrays, shaders — anything
past the ancient fixed-function pipeline) aren't declared by the system
headers and have to be resolved at runtime from the driver. The standard
answer is a generated loader like glad or GLEW.

For Milestone 1 we call roughly 30 GL functions total. `src/gl_core33.h`
and `src/gl_core33.cpp` hand-declare exactly those functions and resolve
them via `SDL_GL_GetProcAddress`. This was chosen over vendoring a
generated loader because:

- glad's typical generation path is a web service or a local generator
  tool — an extra build-time dependency for ~30 functions.
- The alternative (checking in a pre-generated glad bundle) means carrying
  a large file we use a tiny fraction of.
- A ~100-line hand-written loader for this exact, small, known set of
  functions is fully inspectable and has no build-time or network
  dependency.

**This is a deliberate, explicitly temporary decision.** The moment a
future milestone needs a significantly larger GL surface (textures,
framebuffers, compute shaders, etc.), hand-maintaining this file stops
being the smaller option and it should be replaced with a real generated
loader (glad2) instead of growing indefinitely by hand.

## Main loop structure

`Application::Run()` (`src/Application.cpp`) owns the loop:

```
Init Window (SDL2 window + GL context)
Load GL functions
Init Renderer (compile shaders, upload quad geometry)
Create Box

while (!window.ShouldClose()):
    window.PollEvents()          // pump OS events, detect close request
    deltaTime = measured elapsed time since last frame
    box.Update(window, deltaTime)
    renderer.BeginFrame(...)     // clear, set viewport/projection
    box.Draw(renderer)
    renderer.EndFrame()
    window.SwapBuffers()

renderer.Shutdown()
// Window's destructor tears down the GL context, the window, and SDL itself.
```

Responsibilities map directly to the brief's four conceptual systems
(Application, Window/Input, Renderer, Demo) as four small classes — not
because four classes are inherently correct, but because that was the
simplest split that kept each piece independently understandable:

- `Window` (`src/Window.h/.cpp`) — window/input, combined because
  Milestone 1's only input source *is* "keyboard state of this window."
- `Renderer` (`src/Renderer.h/.cpp`) — graphics.
- `Box` (`src/Box.h/.cpp`) — the demo content itself.
- `Application` (`src/Application.h/.cpp`) — wires the above together and
  owns the loop.

## Input handling

`Window::IsActionActive(Action)` exposes an **action** (`MoveUp`,
`MoveDown`, `MoveLeft`, `MoveRight`), not a raw key code. Internally it
checks SDL scancodes (`W`/`Up`, `S`/`Down`, `A`/`Left`, `D`/`Right`).

This indirection exists specifically so that a later input source (mouse,
controller) can drive the same actions without `Box` or any future gameplay
code changing. It's the smallest possible version of that boundary — an
enum and a switch statement — not a general input-remapping system, which
would be speculative for what Milestone 1 needs.

Keyboard state comes from `SDL_GetKeyboardState`, queried live each frame
after `SDL_PollEvent` has pumped the event queue. Nothing in the demo layer
fakes or simulates input.

## Frame timing

`Application::Run()` measures real elapsed time each frame using
`SDL_GetPerformanceCounter()` / `SDL_GetPerformanceFrequency()` (a
monotonic, high-resolution timer), not a frame counter or an assumed
fixed timestep:

```cpp
deltaTime = (currentCounter - previousCounter) / frequency;
```

`Box::Update` moves the box by `direction * speed * deltaTime`, so movement
speed is independent of frame rate. Diagonal input is normalized so
pressing two keys at once doesn't move faster than one. `deltaTime` is
clamped to a maximum of 0.25s purely to stop a stall (e.g. dragging the
window) from producing one huge, visible jump — it does not impose or
assume a fixed rate during normal operation.

Vsync is enabled (`SDL_GL_SetSwapInterval(1)`) as a sensible default to
avoid an uncapped busy-loop; it caps the frame rate but the timing code
does not depend on it.

## FUTURE CONSTRAINTS PRESERVED

This section explains only how the current design avoids *unnecessarily*
blocking known future requirements. None of these are implemented yet.

- **3D rendering** — `Renderer` is already the sole owner of GL state and
  draw calls; nothing outside it assumes 2D. Moving to 3D means extending
  `Renderer` (real projection/view matrices, depth testing, a real vertex
  format) without restructuring `Application`, `Window`, or the
  input/timing boundaries.
- **Arbitrary gravity / reference frames / large planetary coordinates** —
  `Box` currently owns a plain `(x, y)` in window-pixel space, and nothing
  elsewhere in the engine treats that as a universal coordinate system.
  There is no camera, no "world space," and no assumption that positions
  are small or that "up" means anything beyond "toward the top of this
  window." Introducing authoritative large-world coordinates and local
  rendering coordinates as separate concepts later does not require
  undoing anything here, because Milestone 1 never conflated them.
- **Spherical terrain** — nothing here assumes an infinite flat plane;
  there is no terrain of any kind yet, so there's nothing to unwind.
- **Physics middleware** — the render loop and the update loop are already
  separate steps (`box.Update(...)` then `renderer.BeginFrame(...)`), so a
  future physics step slots into the loop as its own stage rather than
  requiring the loop to be restructured.

## DELIBERATELY NOT IMPLEMENTED

Explicitly deferred, not forgotten:

- 3D rendering, cameras, depth/perspective projection
- Gravity fields, reference frames, world rebasing, large-world coordinates
- Spherical or any other terrain generation (including Terrain-ML)
- Physics middleware integration (collision, rigidbodies, constraints)
- Entity/component systems or any scene graph
- Asset pipelines, serialization, save/load
- Audio
- Networking
- Editors or in-engine tooling
- Scripting
- Mouse or controller input (the `Action` boundary exists for this, but
  only keyboard is wired up)
- A generated OpenGL loader (glad/GLEW) — see the note under "OpenGL
  function loading" above; the current hand-written loader is scoped to
  today's ~30 functions and is expected to be replaced, not grown.
- A math library (GLM or similar) — not needed until real 3D transforms
  exist.
