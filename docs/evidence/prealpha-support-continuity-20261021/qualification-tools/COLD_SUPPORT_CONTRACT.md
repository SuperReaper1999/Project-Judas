# Generic cold support: existing promises and prepared probe

Prepared only. No new serialization is proposed or implemented.

## Source-backed contract

- `docs/judasjs/saves.md:117–121`: native handles and most solver caches rebuild;
  specific guarded joint continuation is an explicit modern-slot exception.
- `docs/judasjs/saves.md:174–179`: explicitly **not** universal bitwise trajectories
  or persistence of every cache.
- `src/PhysicsWorld.h:358–360`: the bounded joint continuation is native save state,
  explicitly **not** a contact warm cache.
- `src/WorldPersistence.cpp:193`: physical settings are retained separately from
  disposable warm history.
- `src/physics/WorldPersistence.cpp:233–354`: only actual completed observations
  (identity/anchor/normal/normal impulse/separation/publication epoch and timestep)
  persist. Restoration sets `completedContactObservations`, never `contactCache`.
- `tests/WorldPersistenceTests.cpp`: existing resting-body/loaded-hinge continuation
  asserts <.01 m/<.01 rad over the first three restored steps, not bitwise equality
  for arbitrary piles. `tests/PhysicsContinuationTests.cpp` separately proves
  completed-observation restoration does not replay impulses or invent callbacks.
- M70/M77 actual endpoint checks remain narrower demonstrated contracts. Their
  passing results do not certify a large free-primitive pile's warm continuation.

Thus a difference between uninterrupted and cold 10-pass support solves is not,
by itself, a broken documented universal exact contract. Quantify whether it is
bounded and physically acceptable; separately preserve the existing gates. If
the new long-lived guess becomes essential durable state, establish that with a
failed meaningful supported contract before changing formats or serializing data.

## Prepared genuine public-save probe

`ColdSupportSnapshot.cpp` calls public `WorldPersistence::Capture`, actual
`SaveStorage::Write/Read` of a `GameSnapshot`, and public SceneFrom/Build/Prepare/
Restore/Published in **fresh processes**. It does not read, populate or copy a
private cache. Five ordinary boxes and a static support are warmed at 60 Hz,
unchanged ten sweeps; calm and small-impulse snapshots are separate cases.

Positive controls:

1. Compare exact physical entity/mass/pose/velocity/sleep state at the captured
   boundary, before any resumed step.
2. Two independent cold-reader processes must reproduce each other exactly.
3. Record ten uninterrupted versus restored physical endpoints: position,
   velocity, spin and orientation deltas; do not declare nonzero deltas a defect
   or silently introduce a new arbitrary acceptance threshold.
4. Run the identical fresh test driver against frozen baseline and control core;
   preserve all four continuation paths and all command/binary/source identities.

Real negative controls copy loaded chunks **in memory** and remove or truncate
the required contact participant. Restoration must reject both. Original slot
bytes are never changed. These test persistence integrity, not a fictional promise
that every discarded warm guess will be reconstructed.

Prepared commands (root must grant sole workload slot):

```sh
python3 .cache/persistent-contact-20261021/measure/cold_support_protocol.py \
  --baseline .cache/persistent-contact-20261021/measure/baseline/frozen/libjudas_engine.a \
  --control build/libjudas_engine.a \
  --output .cache/persistent-contact-20261021/cold-public-paired
# Inspect the plan; append --execute only when authorized to compile/run.
```

The protocol builds only one fresh private driver and two private executables.
It does not build/modify an engine, project, test, save format or expected gate.

## Fresh-driver focused qualification

The original reused bending object segfaulted against **both** original/control
archives while a newly compiled BendingTests driver passed both. Therefore final
qualification must compile fresh test TUs; a successful relink is insufficient.

`link_focused.py --fresh-drivers` reads each target's actual CMake build.make
compile recipe, expands its recorded flags, retains main-renaming/ValidationMain
wrappers and any per-source options, and redirects outputs/dependency files to
private directories. Both ordinary test TUs and `tests/ValidationMain.cpp` become
fresh objects. Direct `src/physics/WorldImpactEvents.cpp.o` paths are explicitly
substituted; all repeated engine archives are replaced without changing order.

```sh
python3 .cache/persistent-contact-20261021/measure/link_focused.py \
  --archive build/libjudas_engine.a \
  --event-object build/CMakeFiles/judas_engine.dir/src/physics/WorldImpactEvents.cpp.o \
  --output .cache/persistent-contact-20261021/focused-fresh \
  --fresh-drivers
# Inspect plan. --execute performs serial private compilation/linking/testing.
```

This refreshes test drivers, **not** every directly compiled implementation object
or the engine. The root's eventual final ordinary clean build is still required
to qualify final headers/implementations together. A frozen-object provisional
run must never be reported as that final clean-source qualification.
