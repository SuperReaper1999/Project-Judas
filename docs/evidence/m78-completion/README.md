# M78 final audit evidence

Subject: `f20cb40cc4ac3f9eb19357eabf4920578b594b6a`.
Current engine source equals the Stable split baseline `3102bf6c…`; only branch
policy differed at the subject commit. Final summary: [M78_COMPLETION.md](../../M78_COMPLETION.md).

## Provenance and selected execution paths

| Probe | Implementation trace | Independent observation / rejection |
| --- | --- | --- |
| `ComposedPhysicalCold.cpp` | `WorldPersistence::{Capture,SceneFrom,Prepare,Restore,Published}` → articulation/contact continuation, `PhysicsWorld`, `MotorPersistence`; `WorldStreaming::{Persist,Request,Advance}` | Exact saved pose/momentum/coordinate and uninterrupted first-three-step endpoints in a fresh process; actual regional motor velocity; absent excluded owner; private truncated restore preserves live world |
| `ModImports.cpp` | Actual `ScriptSystem::{Normalize,Module,Evaluate}` → `AssetDatabase::{PublishedModule,ModuleImportAllowed}` with an effective owned catalog and normal `ResourceManager` | Returned imported values versus denied private/undeclared imports; healthy independent script proves error isolation |
| `SkeletalDependencies.cpp` | Actual ResourceManager jobs/owner publication → `RuntimeAnimation`, `UpdateRagdolls`, `SetSocket/UpdateSockets`, shared IK identity mapping | Real GPU generation, joint-key replacement, old body retirement, failed mappings, malformed source and recovery; deterministic gate only holds old decode before publication |

These are native tests of the actual named boundaries. They do not claim every JS
wrapper was executed, package admission was repeated, OS power-loss durability was
proved, or editor gestures were automatically certified. Previous actual Application,
mod-package and moved-export routes remain at their original evidence paths.

## Reproduction

From repository root, use a configured Release `build` containing the ordinary
engine/dependency targets. No permanent new build dependency is required.

```sh
python3 docs/evidence/m78-completion/probes/run.py
python3 docs/evidence/m78-completion/probes/run_composed.py
python3 docs/evidence/m78-completion/probes/run_final.py
```

The recorded scripts intentionally refuse reused log/output names. For independent
reproduction, copy this evidence and runner to a new owned root and replace the
`E`/`C` roots; preserve the original receipts. All three final probes compile against
`build/libjudas_engine.a` with the existing target's compile/link configuration.
Fresh-process composed read uses the exact preceding write directory. Headless GL
uses `SDL_VIDEODRIVER=offscreen`, audio uses `SDL_AUDIODRIVER=dummy`. Runs are serial;
no Windows performance comparison is inferred.

`RESULTS.json` records commands/identities for final runs. `RUNS_MANIFEST.json`
accounts for every retained log, including earlier observer versions. Exact
participants and expected physical endpoints are under `artifacts/composed/`.
`PERFORMANCE.json` and the four CSV/compressed profiler pairs preserve the unchanged workload.
Final fingerprints cover engine sources, declaration/inventory inputs, probes,
records and current binary/archive identities. Binary/build products stay ignored.

## Preserved probe failures, not product findings

- Initial mod observer constructed scripted worlds without a ResourceManager:
  construction failed and downstream checks were invalid. The corrected ordinary
  resource-backed world passes; rejected imports are independently exercised.
- Initial skeletal observer queried PhysicsWorld *after* world shutdown. A debugger
  placed the fault in that invalid post-shutdown observation. The final observer
  checks retired animation/articulation ownership without touching shutdown physics.
- Initial composed observer omitted the streaming JobSystem; debugger traced that
  setup error. A later fixture used a legacy `managed` body in a region and received
  the documented `session-owned/unsupported component` rejection. The valid fixture
  uses an ordinary unmanaged modern body. Neither is a new product defect.
- One composed observer compile failed due to an initializer brace; corrected source
  compiles cleanly. No engine code was changed to accommodate any observer mistake.

Original M78/FLU/legacy-save failures and all prior correction receipts remain
unchanged in their original directories. Windows preflight: `win10` powered off;
new completion probes unexecuted there. Linux passes are not Windows or human passes.

Profiler payloads use lossless `.json.gz` compression (standard gzip). The preserved
performance fixture requires the existing registered cooked Skate rig in
`projects/post_m65_consumers`; original private/restricted FBX sources are not
redistributed by this audit. Missing prerequisite assets must be reported, not
replaced with a different rig while claiming the same workload.
