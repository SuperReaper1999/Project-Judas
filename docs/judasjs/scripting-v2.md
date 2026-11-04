# Scripting V2: binding architecture

This is the current native implementation guide. Games still import one `judas`
module and the existing editor still authors the same script slots. V2 changes
implementation ownership, not the scripting language, editor format or physics.

## Where to work

`src/scripting/Modules.json` is the compiled catalogue. CMake generates registration
and embedded-library lists from it; the API checker reads the same catalogue.
Native handlers live in `src/scripting/modules/`, while their JavaScript facades
live in `src/scripting/library/`. These are compile-time fragments of one module,
not separately loaded runtimes or project assets.

| Native module | Responsibility |
|---|---|
| World | entities, prefabs, transforms, scenes, saves, streaming and origins |
| Terrain | planetary and volumetric terrain |
| Environment | weather, water and experimental fluid queries |
| Rendering | cameras, lights, materials, sprites and tilemaps |
| PhysicsBodies / PhysicsQueries | body operations / collision queries |
| Characters / Animation | CharacterMotor / clips, IK and ragdolls |
| Mechanics / Joints | wheels, powertrains, fracture, deformation / joints |
| Navigation | agents, routes, obstacles, links and runtime configuration |
| Input / Network | input and local players / network sessions |
| Audio / Particles / UI | subsystem configuration and interaction |
| AI | utility decisions, sensing, reservations and slot-owned native state |
| Runtime | delegation of runtime-owned profiling, signals and script state |

Existing specialised `ScriptBindingsUI`, `ScriptBindingsJoint` and AI helpers
remain implementation helpers behind their module adapters. There is no reason
to rewrite their working internals merely to rename files.

## Shared runtime and ownership

`ScriptSystem` owns QuickJS, module loading, budgets, callback order, debugger,
reload, JSON state and diagnostics. Every played world has one shared session;
metadata parsing uses the existing isolated no-world context. No new VM is
created per binding module. Projects without scripts still need no script VM.

`BindingContext` borrows world/services and **references** to invocation identity
and phase. Do not cache it or retain world/body pointers. JavaScript getters can
make nested native calls, so copied owner/slot/phase values would be unsafe.

The immutable `BindingRegistry` routes named operations to their registered
module. It rejects duplicate registrations and world-dependent calls from a
metadata context. Ordinary dispatch does not construct session state.

Only a module retaining native or JavaScript session resources needs a
`BindingModuleState`. Its lifecycle hooks release claims, retire a slot and
invalidate origin-sensitive state. AI retains its prior cleanup order. States
are destroyed before their QuickJS context. RuntimeWorld continues owning actual
entities, physics and subsystem simulation. Bindings do not create parallel
simulation systems.

## Add or change a binding locally

1. Implement the operation and its validation in the owning module. Register its
   operation name in that file's `Register…Bindings` function.
2. Add its facade in the corresponding `library/` fragment and its public type in
   `docs/judas.d.ts`. Document behavior in the subsystem reference.
3. Add positive and negative public-path tests that inspect native results.
4. Run `node scripts/check_judasjs_api.mjs`; use `--write-inventory` when the API
   inventory intentionally changes. Run the live surface/type checks too.

An existing module change needs no edit to `ScriptSystem::Impl::Native`, the
registry implementation, or CMake. A genuinely new subsystem adds one catalogue
entry (native source/registration, and optionally a facade fragment); CMake then
compiles/registers it automatically. Keep related native validation local; use
shared value readers only for genuinely common representations.

Do not bypass existing world mutation, phase, ownership or readiness rules.
Validate a complete settings patch before publishing it. A JS exception must
remain an exception, not be converted into a successful default value.

## Compatibility and deliberate differences

The public import path, exports, methods, callback order, editor property schema,
slot IDs, state format and debugger interface are retained. No conversion tool
is needed or provided. Entity prototype property **enumeration order** now groups
members by subsystem; member names, bodies, arities and property descriptors are
preserved. Code must not assign gameplay meaning to native member ordering.


The focused behavior correction is documented separately with its negative
control: Audio settings reject unknown enumerable option names before changing
native state. Use the documented `AudioSettings` keys; misspelled/foreign fields
are errors rather than silently ignored properties. Native C++ handler failures are converted to contextual JavaScript errors at the
registry boundary; existing JavaScript error types pass through. No general API redesign is
part of this pass.

## Qualification

See [implementation evidence](../evidence/scripting-v2/README.md). Baseline
observations, real fixed-step endpoint comparisons, editor debugger/reload
checks and relocated exports are distinguished from manual human acceptance.
Native Linux qualification does not by itself establish Windows, Android or
browser qualification for this refactor.
