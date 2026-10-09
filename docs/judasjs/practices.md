# Practical JudasJS conventions

[Index](../JUDASJS.md) · [Lifecycle](lifecycle.md) · [Examples](cookbook.md)

- Keep top-level modules declarative. Store per-instance behaviour on `this`, not
  mutable module globals unless sharing is intentional.
- Use `fixedUpdate` for authoritative intent/forces, `update` for frame-relative
  look input, and `presentationUpdate` for interpolated camera/cosmetic placement.
  Use `uiUpdate` for menus that must run while gameplay pauses.
- Treat transforms/info as snapshots. Assign a modified transform back explicitly;
  use CharacterMotor for collision-aware motion instead of teleporting a prop.
- Cache handles where useful, but validate Entity/Joint before later use and
  reacquire UI/scene handles after replacement. Don't serialize wrapper objects.
- Store bounded game facts in `this.state`; use session for bounded cross-scene
  facts. Neither automatically writes to disk or preserves arbitrary VM memory.
  Explicit [save slots](saves.md) capture supported state and the session map.
- Avoid redundant per-frame tag scans/queries/logs and needless object churn in
  large loops. Prefer correct small scripts over premature pooling/frameworks.
- Use project IDs/names and logical input. Display names and global world-Y are
  not physics policies. Separate gravity, support, attachment and frame concepts.
- Order is defined at callback boundaries, not a network/replay guarantee. JS math,
  input timing and native approximations aren't a deterministic lockstep protocol.
- Errors fault slots, so use null/valid guards and narrow expected exception handling;
  don't hide every exception with an empty catch. Teardown must tolerate stale owners.
- Judas supplies engine primitives; JS supplies game meaning. No engine classes for
  inventory/quests/weapons, animation gameplay states or character locomotion modes.

For the complete create/track/attach/Play/export workflow, start with
[making a game by hand](getting-started.md). For VS Code or another TS-aware JS editor, copy `docs/judas.d.ts` into a project
`Types/` directory and create a development-only `jsconfig.json`:

```json
{"compilerOptions":{"checkJs":true,"target":"ES2020","lib":["ES2020"],"module":"ESNext","moduleResolution":"Bundler"},
 "include":["Assets/**/*.js","Types/judas.d.ts"]}
```

Use JSDoc `@param {import('judas').ScriptContext} context` on constructors, and
`@type {import('judas').Entity|null}` where a cached optional handle benefits.
Import `console` from `judas` for its declared logging type; the runtime global
alias has the same methods. The ES-only library setting avoids suggesting browser
globals that Judas does not supply.
Declarations are editor tooling, not executable exports: interfaces such as
ScriptBehaviour/Vec3/QueryFilter do not exist at runtime. TS cannot enforce finite
numbers, registry membership, JSON budgets, script phases or setter-only reads.

Get the offline reference/declarations from this checkout's `docs/` at the same
engine checkpoint. M38 packages runtime assets/scripts, not the engine docs tree.
No package-manager/internet service or TS runtime is required by games; don't import
`.d.ts` at runtime. No exporter change or whole-doc-tree shipping is needed.
