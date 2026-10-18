# M94 — JudasJS developer tools

[API index](JUDASJS.md) · [Lifecycle](judasjs/lifecycle.md) · [State/lifetime](judasjs/scenes-state.md) · [Lab](../projects/developer_lab/README.md)

Current repository tooling through M94. Judas remains JavaScript; neither a
TypeScript runtime nor an external debugger service is required.

## Start debugging

1. Open any ordinary project in `judas_editor`.
2. Choose **View → JudasJS developer tools**. Choose a registered project script.
3. Set the one-based **Line**, then **Toggle breakpoint**. Enable **Attach debugger**
   before Play to retain exact loaded source snapshots.
4. Play. A source breakpoint suspends the current JS/C call stack and the world.
5. Use **Continue (F5)**, **Step into (F11)**, **Step over (F10)**, **Step out**, or
   **Stop Play**. Select a stack frame to inspect its arguments, compiled locals,
   captured bindings and object data.

This is an in-process editor debugger over the existing QuickJS VM, not a second
interpreter. A line breakpoint stops before an instruction mapped to that source
line. It does not run source-rewritten surrogate code. Internal `judas` wrapper
locations are skipped. Imported project modules can have their own breakpoints.
Breakpoint edits made while stopped apply before execution resumes. Breakpoints
are editor-session settings, not saved game/project content.
Break next stops at the next observable project source boundary, including module
initialization or construction when applicable.

Optimized/non-executable lines may have no matching instruction. Use an executable
statement on its own line; the source pane shows the actual stopped line/column.
There is no automatic breakpoint relocation or claim that every selected line is
bound. Multiple statements and loops on one line are not independently steppable.
`debugger;` remains a no-op; use the tool's breakpoints.

### What inspection means

Snapshots expose at most 32 bytecode frames, 256 compiled bindings per frame,
512 value nodes overall, three object levels, 32 own properties per object and
truncated string previews. Compiled lexical bindings may include shadowed or
out-of-scope/uninitialized entries; these are not a reconstructed lexical evaluator.
Closures are included, but native stack frames and private object internals are not.

Getters, setters, proxies and object coercion are not executed to render values.
Object prototypes are not traversed. A function is displayed as a function, not
called. There is deliberately no arbitrary expression evaluation or variable
mutation. The inspector is a trusted-development tool, not a security sandbox.

Loaded source is retained only while debugging is attached during compilation,
with an 8 MiB source-snapshot budget per session.
If attached after module loading, the tool warns that the exact compiled snapshot
is unavailable. Reload with debugging attached to obtain it. If disk edits differ
from the loaded source, the stopped line pane uses the retained VM snapshot and
warns about the difference. Editing still operates on the disk buffer.

### Pause correctness

The stopped UI pumps only window/input and its own ImGui rendering. It does not
advance physics, scripts, signals, scene replacement, saves or streaming publication.
Pending input is discarded on resume and the editor resets its wall clock so a
long stop cannot create fixed-step catch-up. Cancellation unwinds the current
callback through the existing fault boundary, then Stop discards the runtime.
Whole-session Stop/shutdown disables debug stops before destroy callbacks.
Already-issued game side effects are not rolled back. Existing audio voices may
continue playing; pausing a debugger is not a game-authored audio-pause policy.

An online session also stops polling transport while its owning script stack is
suspended. The stopped editor shows a warning: remote peers can hit the normal
receive-idle timeout. Continue, then use ordinary reconnect if needed. A brief
source stop can retain the connection, but there is no distributed breakpoint
pause or heartbeat thread. Ordinary gameplay pause is different: the application
can keep polling networking without advancing physics or callbacks. The
[Pass 3 checks](evidence/prealpha-pass3/integration/REPORT.md) exercise actual
QuickJS stops, real loopback endpoints, timeout and reconnect with an injected
monotonic clock; this is not a claim of transparent online debugging.

## Deliberate edit and reload

The source pane can **Edit source**, explicitly **Save source**, discard edits,
or read fresh disk content. Saving uses an owned staging file and the existing
platform replacement primitive. A changed disk buffer is rejected before replacing
it; read-only/mod-owned records cannot be edited here. Back up important source
or use version control; this is not a full external IDE or a source-history system.
Source files remain ordinary `.js` files; an external editor works equally well.

**Check sources** compiles/links the current world's enabled slots and transitive
imports without executing project top-level code. **Check project scripts** does
that for every registered script, including helpers/unattached modules, with up to
128 failures reported. Neither check proves runtime logic, schema values or
constructor behaviour. Existing inspector metadata inspection separately executes
modules in its bounded no-world VM; keep top-level native game work in callbacks.

**Reload scripts** is queued until the next callback boundary. An active save or
queued scene replacement rejects reload before teardown: let that operation finish,
then retry. Reload does not cancel or steal the session's pending work. It:

1. Builds a fresh VM and compiles/links its replacement module graph. These exact
   compiled modules are then used; it does not reopen files after validation.
2. Optionally validates/copies each live instance's existing bounded JSON `state`.
3. Runs the old nonfaulted `destroy` callbacks and retires slot-owned signals,
   streaming requests and UI through ordinary lifecycle cleanup.
4. Replaces the VM/module cache. Constructors and `start` execute anew, with the
   optional JSON state assigned before `start`. `restored` is false; this is not
   save restoration and does not call `restore` instead of `start`.

Syntax, import and requested state-preservation failures reject before teardown.
Runtime module/constructor/start failures after the boundary use normal isolated
fault diagnostics; arbitrary game side effects cannot be transactionally undone.
Faulted instances cannot preserve their state: fix the source and uncheck **Keep
JSON state** for an explicit fresh reload. Fresh reload resets all instance states,
including healthy ones, while retaining the physical world. Normal faulted-instance destruction
policy is unchanged. There is no migration of closure values, module globals,
subscriptions, UI handles or arbitrary object graphs. Rebind them in `start`.
One-shot prefab `initialState` payloads are not replayed: constructors must tolerate
reconstruction, or use full scene reload for a clean construction pass.
The world, physical bodies and session service are retained; `destroy`/`start` may
intentionally change them. Reload does not reconstruct destroyed/spawned entities.
Use normal scene reload when a full clean game state is wanted.

After a script-session reload, modern and legacy save publication is rejected with
an instruction to **Stop and Play**. The original world's baseline fingerprint
must not certify a different script version. Existing saves, snapshots and
ordinary game/scene loading otherwise retain their contracts.

## Actionable diagnostics

The developer panel presents runtime errors with asset/source stack, entity ID,
slot and lifecycle callback, plus an **Open** action. It also shows the latest 256
console messages (each bounded to 4096 bytes) while tools are attached. Messages
still go to normal stdout. Runtime exceptions continue to fault only their own
instance. Infinite loops retain the existing interrupt budget; debugging does not
increase it. Compiler locations and stack columns are QuickJS's source positions,
not invented engine line numbers.

For CI or an external editor:

```sh
build/judas_script_check projects/developer_lab/developer_lab.judasproj
```

Exit 0 means registered modules compile/link and the scanned catalog has no
reported problems. Exit 1 reports source/catalog problems; exit 2 is invalid
invocation/project. No project code runs, no sidecars are written, and this does
not resolve an enabled mod session: check the actual effective project separately.

## Costs, portability and limits

Without attachment, the VM performs a disabled hook-pointer branch at instruction
dispatch and allocates no debugger/source-snapshot store. No debugger socket,
network service, editor UI or polling thread is installed in standalone games.
Attached debugging adds source-map/line bookkeeping and inspection cost; pauses
are intentionally not representative game timings. See [measured evidence](evidence/m94/REPORT.md).

This first implementation is integrated with Judas Editor, not DAP, CDP or an IDE
remote-debug protocol. IDE completion still comes from [judas.d.ts](judas.d.ts).
There are no conditional breakpoints, exception-break mode, expression watches,
automatic hot reload, live-object migration or native C++ stepping. Use ordinary
runtime diagnostics and native tools for native failures. Linux automated checks
and human acceptance are recorded separately; Windows/Android M94 are unvalidated.
