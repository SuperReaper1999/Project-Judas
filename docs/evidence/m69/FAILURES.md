# M69 — original single-gate failure

The sole build invocation exited 2 after 102.782 seconds. Runtime (`judas`),
editor (`judas_editor`) and authoring (`judas_scene_author`) targets built, then:

```text
gmake: *** No rule to make target 'judas_paired_input_tests'.  Stop.
```

The existing Unix Makefiles configuration initially lacked the new test targets.
Its running top-level Make process parsed those old rules before the first
existing target triggered CMake regeneration. Regeneration wrote all three new
M69 target rules into `build/Makefile`, `CMakeFiles/Makefile2` and
`TargetDirectories.txt`; the already-running parent Make did not reread them
when it reached the new target name. Source/CMake target definitions exist.
This is gate orchestration, not evidence of an input/query implementation failure.

No focused target, test, API checker, example generator or application smoke ran.
There was no retry or candidate-source edit after this failure. The original log
and false gate result remain preserved. A new invocation against the regenerated
rules can build the three new test targets and cookbook target, then run the
focused pass. It requires operator instruction; no second cycle was started.
A future gate introducing new targets should ensure generation precedes the
multi-target Make invocation. That tooling correction was not applied here.

## Operator-authorized follow-up

The operator explicitly authorized a new invocation after the stale-Make rule
failure. The complete incremental Release build passed in 30.579 seconds, with
no compiler warnings/errors. Paired native input passed 43 checks; paired
input through the VM/play boundary passed 21 checks. No passed check was rerun.
The batch fixture stopped during `start` at `sphere independent witness/order/miss`.
Original commands/logs are preserved in `authorized-followup/`.

Static investigation identified a faulty miss fixture: the common miss ray is
at Y=4, while a dynamic box is centred at (4,3,0) with half-extents (.5,.5,.5).
The .5-radius sphere cast touches its top at Y=3.5, and the .75-half-height,
.25-radius capsule spans down to Y=3, overlapping it. A ray miss at Y=4 is not
necessarily a shape-cast miss. The preceding scalar/batch snapshot comparison did
not throw. The smallest correction is to place the intended miss safely clear
of every collider (for example Y=8), retaining the independent witness/distance
assertions and exact scalar comparison. No geometry/runtime change is indicated.
Actual hit details were not printed by this assertion; corrected execution is
still required to confirm the entire fixture. No test tolerance was weakened.

No correction or further execution was performed after the failure. Remaining
batch checks/measurement, cookbook/API checks and example generation/startup did
not run. All 29 frozen source hashes still match. Resume, if authorized, should
build only the corrected batch target and run that fixture plus pending checks;
there is no reason to repeat the input fixtures or production suite.

## Corrected, operator-authorized completion

The operator authorized all necessary checks. The shared miss origin changed
from Y=4 to Y=8, clear of all swept shapes. Only the batch target rebuilt
(8.281 s); its fixture passed 80 JS correctness assertions and 19 C++ checks.
No runtime/query geometry changed and no assertion tolerance was weakened.
The three pending cookbook cases passed (paired-stick 10, ray-fan 10, surface 9).
TypeScript 5.9.3/source/live enumeration passed: 304 public symbols, 27 exports,
207 internal operations, 33 type-checked examples and 3 negative drift controls.
The ordinary example was generated and its normal application startup passed;
the captured HUD reported 100 rays, 66 hits/34 misses and scalar comparison YES.
No real controller interaction is claimed.

The gate now explicitly generates rules with `cmake -S <repo> -B <existing build>`
before its multi-target Make request. This reuses the cached Release setup.
A configure-only check passed; no runtime build or successful input test was
repeated for this tooling correction. Original source fingerprints are retained
in ORIGINAL_FINGERPRINTS.json; CORRECTIONS.json identifies the two changed files.
Completed logs, commands, screenshot and measurement are in completed-checks/.
