# Pre-Alpha Project Settings and water — review handoff

## Isolation and status

Developed on `codex/project-settings-water`, from Claude's latest committed MK2
checkpoint `313f20a82c87cd309968f75344d365f8f0c2428d`, in the separate worktree
`.cache/project-settings-water/worktree`. This is an isolated review checkpoint,
not an integration into Experimental or main. Human acceptance is pending.

No MK2 physics, balance-controller, contact solver, numerical fluid method,
protected historical evidence or independent research implementation was edited.
No release channel, tag, M100 or Alpha release is created. The Local review build
records the base revision plus dirty state; exact source/binary hashes accompany
it. It does not falsely claim to have been built from the later checkpoint commit.

## Delivered

- Organized Project Settings categories reuse existing general/render/input/audio/
  world/save/navigation/localization/export controls. Physics/gravity and networking
  retain their actual scene/component/script ownership; no artificial engine-wide
  switches or automatic network listener were invented.
- Strict persisted project fields for basic water, swimming and advanced fluids,
  in both legacy and named project formats; identical policy captured at initial
  construction, reload, dedicated-server construction and cold-save staging.
- Basic water is enabled by default: finite oriented volumes, shared GPU wave
  surface, approximate Fresnel/sunlight, underwater absorption and public swimming
  queries. Runtime and authored editor preview respect project policy.
- Advanced Fluid Dynamics is off by default and requires explicit UI confirmation.
  Off avoids particle allocation/population/coupling/surface extraction and
  conserved-owner geometry loading/updates. Native owner registration is rejected
  before advanced surface allocation. Existing numerical methods are unchanged.
- Ordinary Stillwater project, editable components, rendering/picking/prefabs,
  streaming publication/retirement, origin covariance and cold persistence.
- A generic cold reconstruction correction: validated saved scene-owned global
  metadata is no longer mistakenly filtered using runtime-spawn restrictions.
  A negative control still rejects runtime creation of scene-global gravity.

The intentional Pre-Alpha compatibility change is explicit opt-in for previously
implicit advanced-fluid projects/native worlds. Eight shipped projects and active
advanced regression fixtures were updated to opt in; inputs, physics forces,
cadence and existing thresholds remain unchanged. See the manual for save limits.

## Final validation actually run

All selected final checks passed on native Linux Release (GCC 15.2, Ryzen 5 5600,
GTX 970/NVIDIA 580.178.04). No full production-suite certification is claimed.

| Check | Result |
| --- | --- |
| Policy/codec/default-off/prefab/region/origin/negative controls | 55 checks, 0 failures (`policy-latest.log`) |
| Actual ImGui settings controls, warning cancel/confirm, save/reopen, Play lock | 22 checks, 0 failures |
| Existing project and scene regressions | Passed |
| Conserved dynamic-surface equations/storage/waves | 61 checks, 0 failures |
| MK2 regression | Passed, 0 failures |
| Balance foundations / controller | 35 / 119 checks, 0 failures |
| Origin/environment and actual cold child processes | 93 + 8 + 8 checks, 0 failures |
| Particle neutral control | 729 checks, 0 failures; 480 steps, 1,152 particles |
| M54 real application transfers/buoyancy/gravity/reset | 41 checks, 0 failures |
| M55 real application surfaces/reload/ledger | 38 checks, 0 failures |
| Basic application, three modes/720 rendered frames, pause and negative swim controls | 48 checks, 0 failures; no GL/script errors |
| Fresh-process save writer / reader | 18 / 16 checks, 0 failures; original save bytes unchanged |
| Actual editor edit/duplicate/undo/Play/Stop/save | Passed; authored scene identical after Play/Stop |
| Read-only relocated standalone from unrelated `/tmp` cwd | Exit 0; 250 frames, swim, pause/resume, reload |
| API/type/documentation checker | 40 exports, 311 operations, 17 callbacks; examples/types/negative drift checks pass |

The API checker used TypeScript 5.9.3; runtime enumeration was not requested.
No new public JS export was added. The existing `world.fluidSample` signature now
also samples basic water; docs/declarations explain the approximation.
Build logs have no own C++ warning/error; dependency configuration emits existing
HarfBuzz/Freetype deprecation notices. Windows/Android were not validated in this
pass; their source paths were preserved, not certified by Linux builds.

## Measurements and scope

Same scene, 960×540, 240 controlled frames per mode. Modes 1/2 have identical dry
player endpoints and inputs. Mode 0 swims and follows a different trajectory.

| Mode | Fixed-step mean / p95 / worst ms | CPU frame mean / p95 / worst ms | Main-view GPU median / p95 ms |
| --- | --- | --- | --- |
| Basic + swimming | .0754 / .0876 / .2167 | .4847 / .4811 / 19.819 | .2876 / .2898 |
| Basic, swimming off | .0821 / .0981 / .1418 | .3917 / .4727 / 2.1717 | .2724 / .2737 |
| Basic off | .0801 / .0923 / .1205 | .3469 / .3984 / 2.0408 | .2049 / .2081 |

These are simulation/CPU submission measurements, not swap-paced interactive FPS.
The initial 19.819 ms frame remains in the evidence; steady GPU samples are the
119 completed timestamp results retained in each 120-frame profiler ring.
Water-surface GPU median is .0232 ms in the matched dry-player water-on mode.
The whole-view delta includes underwater optical work, not only the surface draw.
All profiler drop/truncation counters are zero; no liquid-owner, PBF or legacy
surface work scopes occur in the default application captures. Native checks also
assert no FluidWorld storage, conserved owners or advanced timing accumulation.

Peak RSS is 114,855,936 bytes in the sequential profiled process, including about
70.5 MB of profiler reserve; this is not a standalone water-memory allocation.
The shared mesh is 13,824 vertices, lazily allocated once. Raw profiles are lossless
gzip archives; `PROFILE_ARCHIVES.json` records original hashes and sizes. Nested
GPU passes overlap and must not be summed. See `MEASUREMENTS.json` for raw summary.

## Preserved failures and corrections

- Named project codec initially omitted the new fields: strict named round-trip
  coverage caught it and both project representations now agree.
- Cold snapshot reconstruction skipped valid scene-global metadata: fixed the
  reconstruction context, retaining runtime-spawn rejection.
- Demo cold restore initially lacked a script `restore()` callback: original save,
  script and failed log retained; corrected ordinary UI reacquisition passes.
- The new application oracle initially assumed a floating height above -0.8 m.
  Actual density-consistent centre-line occupancy settles near -0.81 m (observed
  -0.823 m). The new oracle was corrected to that documented approximation; no
  existing physics/controller criterion or fluid test was weakened.
- A header edited during an in-flight large translation-unit compile produced a
  stale object/layout crash. Mtime receipt and backtrace are preserved. A coherent
  dependency rebuild and actual application reruns passed; no solver workaround.
- Two early compilation errors, unavailable sandbox GL display and an accidentally
  reused fresh-output test directory are preserved with successful follow-ups.
  They are development/infrastructure failures, not fabricated numerical defects.

## Reproduce

Configure a fresh Release build, using Local for this isolated task branch:

```sh
cmake -S . -B build-water -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Local
cmake --build build-water -j2 --target judas judas_export judas_editor judas_project_water_tests judas_project_settings_editor_tests judas_basic_water_application_tests judas_scene_tests judas_project_tests judas_liquid_surface_tests judas_mk2_physics_tests judas_physical_balance_foundation_tests judas_physical_balance_controller_tests judas_origin_environment_tests judas_production_fluid_tests judas_liquid_application_tests judas_liquid_surface_application_tests
python3 scripts/verify_project_water.py --build build-water --graphics
```

Run from a native GL desktop. The runner creates fresh per-run output and a
copied editor project; no original scene/preferences/save is overwritten.
The original exact commands, binary hashes and logs are in `NATIVE_CHECKS.json`,
`APPLICATION_CHECKS.json` and `STANDALONE_CHECK.json`; `POLICY_CHECK.json`
supersedes the earlier policy entry. Build/export logs and PNGs are retained.
The existing API checker accepts `--typescript <path-to-typescript-package>`.

## Deliverables and remaining limits

Project: `projects/basic_water_lab/basic_water_lab.judasproj`.
Review export path/runtime hash: `evidence/prealpha-project-settings-water/PACKAGE.json`.
Changed source/assets: `CHANGED_FILES.txt`; final source/evidence/binary hashes:
`FINGERPRINTS.json` in that evidence directory. The manifest excludes itself.

Basic water is prescribed local volume, not real flow/conservation/rigid-body
buoyancy or a single spherical ocean. Its optics use a nearest interval, waves do
not change swimming depth, and reflections are approximate. Existing advanced
numerical, geometry, active-PBF cold-save and regional/origin limits remain.
FLU-001 and independent fluid research are untouched. No broad fluid correctness
or portability claim follows from these selected checks.

Use the single consolidated human checklist in
[Project Settings and water](PROJECT_SETTINGS_AND_WATER.md#human-review).
Integration with any later Claude commits must be reviewed explicitly; this
checkpoint does not overwrite his paused/active branch or worktree.
