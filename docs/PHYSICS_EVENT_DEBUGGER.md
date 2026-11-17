# Physics-event debugger

## Launch and controls

From an existing source build, launch an ordinary offline project:

```sh
JUDAS_PHYSICS_DEBUG=1 \
JUDAS_PHYSICS_DEBUG_REPORT=/tmp/judas-physics-debug.json \
/home/conner/Documents/GitHub/Project-Judas/build/judas /absolute/path/Game.judasproj
```

`JUDAS_PHYSICS_DEBUG=1` enables the runtime inspector and initially pauses at a
completed fixed boundary. `JUDAS_PHYSICS_DEBUG_REPORT` selects the JSON report;
its default is `physics-debug.json` in the working directory. The matching
existing-profiler export is written beside it with `.profiler.json` appended.
The debugger report is replaced atomically at observations and final export.
Use a writable report location outside project source assets.

Click the game window to give it keyboard focus. The buttons have the same
commands as these keys:

| Control | Action |
| --- | --- |
| F10 — Fixed | Advance one ordinary fixed interval, or finish the current in-flight interval, then pause. |
| F11 — Event | Continue to the next accepted CCD response immediately before its authoritative publication. It may cross fixed intervals that contain no accepted response. |
| F12 — Phase | Continue to the next safe processing boundary. |
| F7 — Arm/play | Resume real-time project controls and stop before the next accepted CCD response. |
| F8 — Live | Resume real-time gameplay with diagnostic tracing but no automatic collision stop. |
| F9 — Pause | Pause at a completed fixed boundary. It cannot abandon a partially executed step. |
| C — Save | Request the normal project SaveService checkpoint in slot `physics-debugger`. |
| L — Restore | Request restoration of that checkpoint through SaveService. |
| Esc — Detach | Leave diagnostic stepping; finish the current fixed interval safely before ordinary gameplay continues. |
| Right mouse drag | Orbit the diagnostic camera. |
| `+` / `-` | Zoom the diagnostic camera. |

Save, Restore and boundary Pause are disabled during an in-flight stop. While a
checkpoint service operation is busy, root stepping/resume controls are disabled;
the status text reports service progress or failure. Ordinary project controls
apply while Arm/Live is running. Manual Fixed/Event/Phase stepping freezes the
latched gameplay input: moving a control while paused does not change that
in-flight input. Return to Arm/Live to sample current gameplay controls again.

Paused wall time does not enter the fixed accumulator or cause catch-up when
resuming. Existing accumulator time from before the stop remains accounted for.
The ordinary gameplay catch-up policy is unchanged when the debugger is absent.

## Safe phases and accepted events

The observer runs synchronously on the simulation owner thread after relevant
workers have joined. A stop retains the existing native solver stack. Continuing
returns to that stack; it does not rerun the collision search or reconstruct a
partially executed solver from a saved state.

| Phase | Meaning |
| --- | --- |
| `FixedBegin` | Whole-world boundary before the next fixed interval starts. Repeated paused UI polls do not add repeated trace records. |
| `Candidates` | Native candidate/contact generation is available, before the MK2 substep solve. |
| `MaterialBegin` / `MaterialEnd` | Joined boundaries around the coupled local-material interval. |
| `SearchCompleted` | A real serial query or joined worker query result. Recorded for inspection; phase stepping does not stop on every query. |
| `PredictionDiscarded` | A queued prediction failed the existing version/trajectory checks. Recorded as discarded, never promoted to an accepted impact. |
| `ToiSelected` | The scheduler selected a current prediction; response preparation can still reject it. |
| `ResponsePrepared` | Existing contact rows/private response structures are ready. This is not yet an accepted response. |
| `BeforeImpact` | The existing response solve and energy validation succeeded; authoritative body response has not yet been published. Event/Arm breakpoints stop here. |
| `TrajectoriesPublished` | Response processing, motion retiming, proxy updates and affected candidate discovery have completed, before the next search flush. `applied` distinguishes a committed response from an unsuccessful selected prediction. |
| `DeformableBegin` / `DeformableEnd` | Boundaries around post-physics deformable publication/update. These are distinct from the coupled physical-material interval. |
| `FixedEnd` | The normal whole-world fixed interval completed, including scripts, integration and event publication. |

`BeforeImpact` therefore means **before committing the accepted response**, not
before the CPU has searched or prepared that response. It exposes the incoming
motion ledger at the selected local time and the actual prepared manifold.
Immediate/resting contact solving is not relabelled as an accepted CCD event.
Use phase stepping to inspect work without a CCD arrival.

Same-time accepted responses retain native ordering and share a time-group ID
when their macro times are exactly equal. A recurrent coupled response is one
atomic batch: all participating bodies are inspected before publication, then
published together. There is no stop halfway through writing that batch.
`event` and `time_group` are scoped to the physics macro step; `sequence`, the
completed fixed-step count and `world_epoch` disambiguate records. Restoring or
replacing a world rebinds the observer and clears its old prediction trace.

## What the inspector shows

The runtime view draws diagnostic body enclosures, actual contact-normal arrows
and sampled physical-material nodes. It is not the full lit project scene and
its enclosures are not replacement collision geometry. Pausing does not invoke
live world presentation scripts or pump resource publication inside the solver.
The inspected bodies are sampled from the existing motion ledger; capture does
not issue additional collision queries.

The UI/report includes body/part handles, trajectory revisions, prediction
version, candidate and response-row counts, accepted/invalidated event counts,
CCD/sampling work, geometry-query reuse, completed CPU categories, material
constraint counts, strain, plastic work, broken connections, released
attachments and sleeping state. Material counters describe the most recently
completed physical work; a boundary before that work cannot report its future
result.

Capture is deliberately bounded:

- Up to 8,192 trace records for the retained fixed-step trace; `omitted_records`
  reports dropped oldest records. A new fixed interval starts a new trace.
- Up to 256 manifold points in each retained trace record; actual contact counts
  remain available even when a stored manifold is shorter.
- Up to 8,192 material-node positions across the snapshot; per-material
  `positions_truncated` identifies reduced position capture.
- Up to 2,048 diagnostic bodies by default. The view also limits drawing to its
  local inspection range. This is a diagnostic display limit, not a simulation
  or collision-coverage limit.

## Checkpoints and replay

Use C at a completed boundary before the impact, wait for a completed SaveService
status, then Arm/play or Event-step into the impact. Finish an in-flight interval
with Fixed before using L. Restoration uses the project's ordinary save identity,
content checks, participants, resource preparation and world-publication path.
It can be used across process exit when those normal project save contracts are
satisfied; a failed or still-pending save is not a replay checkpoint.

A checkpoint does not serialize the native call stack, queued partial solver
state or an arbitrary `BeforeImpact` boundary. The replay starts from a completed
fixed boundary and runs the collision again. Project-owned script state must be
persisted through its normal save contract. Ephemeral collision caches are
rebuilt as they are for ordinary cold restoration; exact replay of every complex
scene is not implied by the debugger interface alone.

The debugger is an offline-authority tool. Launch network options and active
network sessions are rejected. It cannot pause a replicated session while other
peers keep simulating.

## Timing and profiler interpretation

`PhysicsDebugClock` excludes synchronous observation, report export and UI wait
from owner-thread CPU-work timers. Physical `dt` does not change. Existing raw
profiler and GPU timestamps retain real elapsed time, including the explicit
`Physics debugger inspection/wait` scope. Each fixed profiler record adds
`debugger_wait_ns`; active fixed-step CPU cost is
`end_ns - start_ns - debugger_wait_ns`, bounded at zero for display.

The inspector suppresses its own repeated paint scopes on the current profiler
thread without resetting the live frame or invalidating its enclosing scopes.
The normal profiler export contains completed frames; an open collision scope
is not presented as already completed. Debugger boundary data supplies the live
native counters and elapsed active gap. Completed interval CPU categories may
still be pending inside their enclosing phase, and search/sampling/solve scopes
can overlap or nest. Do not sum them as whole-frame time.

Debugger-enabled runs are not throughput benchmarks: inspection, JSON output
and pause/resume deliberately add diagnostic overhead. The normal whole-engine
capture driver rejects `JUDAS_PHYSICS_DEBUG=1`. Compare normal gameplay with the
debugger disabled using [whole-engine performance qualification](WHOLE_ENGINE_PERFORMANCE.md),
including presented-frame latency, hitch sequences and simulation backlog.

## Application test harness

The existing [application harness](TEST_HARNESS.md) can drive the same controller
without an interactive inspector. It uses these additional directives:

| Directive | Meaning |
| --- | --- |
| `PHYSICS_DEBUG REPORT.json` | Attach the debugger and export its diagnostic report. |
| `DEBUG_ACTION ACTION` | Queue a command consumed at the next debugger stop, in declaration order. |
| `DEBUG_EXPECT PHASE MINIMUM` | At harness completion, require at least this many observations of the named native phase. |

Actions are `fixed`, `event`, `phase`, `arm`, `live`, `pause`, `checkpoint`,
`restore` and `detach`. Phase names are case-sensitive names from the table
above. These command queues are not frame-indexed key bindings. A queued save or
restore is valid only at a completed boundary; the harness waits for the normal
save service to become idle before consuming the next root command. Use a
workload that actually generates an accepted impact for `BeforeImpact`
expectations; merely finding a candidate does not satisfy one.

For example, this script advances two fixed intervals and then remains paused:

```text
FRAMES 30
PHYSICS_DEBUG /tmp/physics-debug-harness.json
DEBUG_ACTION fixed
DEBUG_ACTION fixed
DEBUG_EXPECT FixedEnd 2
```

Run it through the ordinary application:

```sh
JUDAS_TEST_SCRIPT=/absolute/path/step-two.txt \
/home/conner/Documents/GitHub/Project-Judas/build/judas /absolute/path/Game.judasproj
```

The harness rejects unconsumed queued commands and unmet phase expectations.
If commands run out inside a live interval, it finishes that interval safely;
at a root boundary it holds. Assertions and checks in the workload must still
verify the intended physical outcome. Phase counts alone do not prove fidelity,
cold restoration, visual quality or platform qualification.

## Native ownership

`PhysicsWorld::SetDebugObserver` installs a read-only synchronous observer only
at a completed boundary. Re-entry into `Step` is rejected. Native callbacks must
not mutate the world, save, replace it, pump resources or install another
observer. Public whole-world phase entry points cannot forge native accepted
collision events.

`PhysicsEventDebugger` owns diagnostic state and borrows its `RuntimeWorld`.
Keep the world alive until `Unbind()` has completed, or explicitly detach it via
`InteractivePlay::End` / `SetPhysicsDebugger` before destroying the world.
The runtime lifecycle rebinds through normal scene/save replacement. A custom
native handler must explicitly resume an in-flight stop; it cannot return Hold
or Pause and discard the live call stack.
