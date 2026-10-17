# M94 — Judas Learns to Help Developers

Baseline: accepted M93 `3306b5aa66a2e649bad06abc8c5cba452058d3fa`.
Native Linux Release candidate. **Human acceptance pending.** Windows/Android M94
were not validated. Main is not promoted by this checkpoint.

## What changed, and why

Inspection confirmed that `ScriptSystem` already isolated faults, preserved error
stacks, budgeted runaway execution and owned one VM per world. It had no debugger
attachment or deliberate script-session replacement. QuickJS's parser explicitly
ignored `debugger;`; the editor had no practical source/stack/variable workflow.

The implementation retains that interpreter and authoritative callback model:

- A small documented QuickJS extension observes real source-mapped instructions,
  exposes read-only frame bindings, and prepares modules without evaluating them.
  Function names come from safe own-name data, including class methods. Frame-local
  source-map ranges avoid repeated table scans; no collected-function pointer cache
  or rewritten surrogate JavaScript is used.
- `ScriptSystem` owns optional debugger state, source breakpoints, into/over/out,
  cancellation and bounded detached snapshots. Getters/proxies/object coercion do
  not run to display values. Loaded-source snapshots are distinct from disk edits.
- `src/editor/ScriptTools.cpp` supplies the actual stopped SDL/ImGui UI, source
  navigation/edit/save, runtime errors and opt-in console history. Its pause loop
  does not advance the world. Input requests are retired and the editor's clock
  is reset on resume. Stop unwinds before destroying the world; whole-session
  teardown disables stops before calling destroy callbacks.
- Reload is explicit and queued to a callback boundary. The fresh VM's exact
  dependency graph is compiled/linked before old owners retire. Optional bounded
  JSON state is copied; old signal/UI/streaming ownership is retired through the
  existing removal path. Constructors/start run anew, not save-restore callbacks.
- A real streamed-game failure established the need to reject reload during active
  save or queued scene work. A read-only `SceneSession::OperationInProgress()`
  reports that existing ownership boundary; reload does not cancel/steal its work.
- Reloaded worlds cannot certify the old source baseline for saves. Modern capture
  fails, normal legacy UI prechecks fail, and direct native legacy capture throws
  a clear error. Stop/Play establishes a compatible fresh baseline.
- `judas_script_check` and the editor share non-evaluating compile/link diagnostics.
  The source editor stages writes, detects changed disk content and prevents an
  old buffer being saved into a different project's source record.

No public `judas` export/member was added. `judas.d.ts` still describes the same
37 exports / 424 public symbols. Current lifecycle/getting-started/reference and
architecture docs now describe debugging and deliberate reload; the inventory's
identity and its verifier match. No gameplay policy moved into C++.

## Working demonstration

Project: `projects/developer_lab/developer_lab.judasproj`, scene
`Scenes/workshop.judas`. See [the project walkthrough](../../../projects/developer_lab/README.md)
and [tool reference](../../M94_DEVELOPER_TOOLS.md).

The Repair Bench has a moving amber assembly, independent presentation/HUD and a
registered calibration helper. E intentionally calls invalid gain 0. Its worker
faults while the rest of the scene stays alive. A private test copy is edited to
gain 2, freshly reloaded, then exercised with the **same E input**. Actual helper
breakpoints observe both local values and the repaired worker continues. The
original authored project retains the teaching defect; no runtime-only fake fix
or compiler workaround is shipped.

Runtime export: `.cache/m94/delivery/package/judas`. The relocated copy and
unrelated working directory are recorded in `delivery/RESULTS.json`. Runtime
packages contain no editor or script-check executable/debug UI. The runtime
preview, pause/resume and scene reload passed after byte-identical relocation.
Editor tooling is used through `build/judas_editor`, not the exported player.

## Verification

Final-source receipts are consolidated in `RESULTS.json`; original failures are
kept under [development](development/INDEX.md). No full production-suite sweep.

| Scope | Observed result |
|---|---|
| Native M94 VM/reload adversarial tests | 49 checks, 0 failures |
| Native SDL/OpenGL editor tool test | 20 checks, 0 failures |
| Existing games + actual repair cycle | 37 checks, 0 failures |
| Existing script / script application | 35 / 19 checks, passing |
| Existing signal / signal application | 95 / 17 checks, passing |
| Presentation / runtime UI / UI application | 44 / 22 / 16 checks, passing |
| Scene transitions | 23 checks, passing |
| Fresh-process save write / read | 13 / 20 checks, passing |
| Prefab / CharacterMotor | 37 / 32 checks, passing |
| Mods disabled ordinary app control | 5 checks, passing; not enabled-mod debugging certification |
| Executed cookbook | 35 cases, 286 checks, passing |
| Source checker | Eight projects / 83 registered modules, no compile/link failures |
| API/type/live enumeration | 37 exports, 424 symbols, TS 5.9.3; four negative drift controls passing |
| Full editor / export | Undo, save, Play/Stop identical authored scene; relocated closure export passing |

Runtime-debug/reload consumers were **Spring Range, boundary/spacecraft, Packed
Museum and Streamed Spring Range**. These exercised real Application frames and
retained physical bodies. Native UI controls proved old slot handles invalidated
while independent documents survived. Streaming controls proved old slot requests
retired while independent requests survived. A real queued save-list operation
proved rejection without state changes, followed by successful reload after real
completion.

Additional source-only checks covered Core Scramble, Lastlight, Planetfall and
the developer lab. Compilation is not gameplay acceptance. The cookbook exercises
current APIs; it does not exhaustively certify every subsystem/member.

Inspection tests prove actual arguments/locals/closures, into/over/out, each entry
to repeated helpers, immediate breakpoint edits, non-executing getters/proxies,
syntax/missing-export rejection, retained/fresh state, fault recovery, cancellation
and teardown. The editor test compares a moving body's position/velocity across
real stopped UI frames, then advances physics as a positive control. It also
compares all declared script state while stopped and checks clock-reset requests.
The final source-view/breakpoint changes received narrow native/editor reruns.
Final build receipts contain no new warning; earlier broader recompilation retained
the two pre-existing RuntimeWorld misleading-indentation warnings unchanged.

## Performance and iteration

Intel i7-4790 (4 cores/8 threads), GTX 970 4096 MiB, NVIDIA 580.178.04,
GCC 15.2, Linux Release. Raw hardware, commands, hashes and both observations are
under [measurements](measurements).

Paired VM workload: exact accepted QuickJS source vs candidate; same unchanged
helper objects and benchmark source, 15,000 arithmetic/branch iterations per call.
Five alternating paired process rounds; discard each process's first batch.
Statistics below describe 45 **100-call batch means**, not individual-step tails.

| VM, debugger disabled | Median µs/call | p95 batch mean | Worst batch mean |
|---|---:|---:|---:|
| Accepted M93 | 1045.303 | 1063.027 | 1082.909 |
| M94 | 1033.947 | 1070.378 | 1120.076 |

Every checksum trajectory matched exactly. The final median is 1.09% lower; the
preliminary observation was 0.21% higher. Treat that as ordinary timing variation,
**not a speedup claim**. No resolved disabled-hook regression was established.

One live script update plus ordinary queued-signal drain measured median
10.168 µs with tools detached vs 12.606/12.581 µs in two attached rounds. The
benchmark asserts healthy callbacks, so faulted-instance timings are not accepted.
Stopped human-wait time is excluded from these comparisons; profiler script scopes
can naturally include that wall time when a breakpoint is actually held.

| Actual consumer | Startup → first frame, ms | Script-session replacement, ms | Bodies retained at replacement |
|---|---:|---:|---:|
| Spring Range | 242.134 | 3.582 | 52 |
| Boundary / spacecraft | 143.428 | 2.899 | 16 |
| Packed Museum | 206.536 | 2.567 | 0 |
| Streamed Range | 185.969 | 5.842 | 12 |

Startup and script replacement are different operations: first frame is not
all-assets/regions-ready, and reload keeps the world/resources. These timings
show the practical cost of iteration; they are not an equivalent cold-load speedup
certification. No overall GPU/RAM performance claim is made. Optional source
snapshots cap at 8 MiB; the editor source buffer caps at 1 MiB; the existing VM
memory/budget contracts remain. Detached games have no debugger store, socket,
UI or polling service; the dispatch hook-pointer branch remains.

## Limits and readiness

- In-process editor tooling, not DAP/CDP/remote IDE debugging; no watches/eval,
  conditional breakpoints or exception-break mode. External IDE completion still
  uses the existing typings. `debugger;` is still a no-op.
- Line-level source-map stepping: optimized/non-executable lines can remain
  unbound, same-line statements are not individually steppable. Native/internal
  frames and late-attached/snapshot-budget-exceeded source have explicit limits.
- Snapshots are read-only compiled bindings/own data, not a complete lexical or
  prototype/engine-object evaluator. Audio voices may continue during a debug stop.
- Reload is whole-VM/session replacement, not arbitrary live-object migration.
  Fresh reload resets healthy states too. Module globals reset even when JSON
  state is retained. Start/destroy side effects are real, not transactionally
  rolled back; game-owned service objects require deliberate rebind/cleanup policy.
  Syntax/import/state/busy failures reject before teardown, but runtime constructor
  faults after replacement still use ordinary isolation.
- Save compatibility deliberately requires fresh Stop/Play after reload.
  Existing save formats/normal-source captures were not weakened.
- Windows/Android M94 tooling is unvalidated; enabled-mod debug/reload and every
  arbitrary game's custom lifecycle are not certified. No human acceptance claimed.
- Protected history, independent fluid research and existing game content remain
  unchanged. No unrelated deferred physics/fluid defect was reopened.

Linux candidate is ready for an Experimental checkpoint and human workflow review.
Exact files are in `CHANGED_FILES.txt`; source, binary and checkpoint manifests are
separate. Main remains at the accepted M93 checkpoint.

## Human checklist

1. Open the Repair Bench in Editor; View → JudasJS developer tools.
2. Break at worker.js:10 before Play. Inspect `dt`, `phase`, `this.state`; step
   into/over/out, then continue and disable the breakpoint.
3. Focus the scene or hide the tools; E triggers the calibration error. Open its
   source from Runtime errors. Confirm the rest of the scene keeps running.
4. On a project copy, change calibration gain 0 → 2 and explicitly save/check.
   Uncheck Keep JSON state, reload, focus the scene and press E again.
5. Try retained-state reload and a rejected syntax-error reload. Hold a breakpoint
   for several seconds; resume and check movement/input do not jump ahead.
6. Stop/Play, save/reload, and try the same tools on an existing game. Check the
   exported preview from its moved package; editor tools should not ship with it.
