# M78 reproduction — PARTIAL, evolving execution record

Frozen baseline: `9e77b2226edcf217ab789fe4f0fea42a3415a72d`.
No product fixes, runtime tuning, weakened assertions or baseline updates.

## Baseline Linux campaign

The Release configuration is the existing `build/CMakeCache.txt`; compiler flags
and environment are retained in evidence. CMake discovers 184 test targets.
This build is a single campaign, not 184 independent engine rebuilds.

```sh
cmake --build build --parallel 4 --target $(cat .cache/m78/test-targets.txt)
python3 docs/evidence/m78/probes/run_baseline.py --plan
# Only after the build has succeeded:
python3 docs/evidence/m78/probes/run_baseline.py
```

The owned frozen tree was populated with `git archive HEAD | tar -x -C
.cache/m78/frozen`. Tests execute there, not in original project directories.
Absolute original `__FILE__` paths in some accepted tests still read historical
fixtures; identify these dependencies rather than pretending tests are hermetic.
The runner preserves logs, commands, exit/timeout, binary identity and environment.
Its adapters choose existing output/project arguments, never rewrite assertions.
Cases requiring write/read follow-up remain additional work even when a write test
passes. `exited-zero` deliberately does not mean a whole capability is verified.

Generated audio inputs use `python3 tools/CreateAudioFixtures.py
.cache/m78/audio-fixtures`. Source art stays local; do not redistribute restricted
FBX originals merely because an import check uses them.

## Independent isolated rotation

```sh
c++ -std=c++17 -O3 -DNDEBUG -Isrc \
  docs/evidence/m78/probes/free_rotation.cpp src/RigidBody.cpp \
  -o .cache/m78/free_rotation
.cache/m78/free_rotation
```

`free_rotation_world-receipt.json` records actual compile/link/run commands for the
PhysicsWorld version, reusing the generated baseline link dependencies. The source
is `probes/free_rotation_world.cpp`; the shipping library is unchanged. Analytic
criterion: world angular-momentum **vector** is constant for an isolated torque-free
body. It is not a claim that scalar magnitude or energy changes by the vector's
relative residual. Sphere controls, zero contacts/stationary COM and timestep
refinement establish the observer's declared envelope. Original invalid observer
compile and corrected observer are retained.

## JudasJS static/type coverage

```sh
node scripts/check_judasjs_api.mjs --typescript \
  /home/conner/Documents/GitHub/Project-Judas/.cache/m73/typescript/node_modules/typescript/lib/typescript.js
```

The TypeScript module path must be absolute for this checker. The first relative
path failed module resolution; preserve that observer invocation, not a product
failure. Static checks report 30 exports, 345 public symbols, 239 native operations,
16 callbacks, 39 typed examples and three rejected drift controls. These are not
behavioural capability counts. Actual VM enumeration passed separately; see api-live.log and VM_SURFACE.json.

## Windows

Existing VM settings are unchanged. `windows-environment-source-probe.json` retains
the read-only guest execution receipt. All 853 existing guest source inputs match
both its manifest and the frozen baseline inputs; six focused native executables
are present. This is **not** a fresh whole-engine Windows run. Approximately 158 MB
free space was observed. Heavy Linux/guest work must be serialized; inspect/reclaim
only clearly owned disposable audit artifacts if necessary. Preserve tools, SDK,
source, review packages and the operator's VM configuration. No physical Windows
GPU/input/audio acceptance follows from software rendering or synthetic controls.

## Added independent observers and owned cross-system route

```sh
python3 docs/evidence/m78/probes/compile_probe.py concave_rig
python3 docs/evidence/m78/probes/run_case.py NEW-concave -- .cache/m78/concave_rig OWNED_OUTPUT
python3 docs/evidence/m78/probes/compile_probe.py inert_startup
python3 docs/evidence/m78/probes/run_case.py NEW-startup -- .cache/m78/inert_startup
```

Use fresh observer names/outputs or inspect original build receipts: compile helper
refuses existing logs, and case helper refuses existing receipts. No existing test
assertion/tolerance changes. The rig probe requires the license-restricted cooked
model documented in LOCAL_ONLY_INPUTS.json; no redistribution license is inferred.
900 per-step samples for each box/mesh case are retained under performance/.

`scalability-observational-no-acceptance` deliberately uses the existing
`--measure-baseline` mode to emit timings despite the retained-budget fixture failure.
That mode disables failing assertions; its zero exit is **not** an acceptance pass.
The ordinary mode's first/repeated failure and independent budget controls remain.

Mod/editor commands and exact payload hashes are individually recorded in cases/:
base -> kit -> addition/replacement -> editor terrain sculpt/paint/cook -> scene bake
-> package -> install -> exact hash enable -> owned authoring paths renamed -> moved
runtime. Initial hub/addition-only fixture had wrong expectations; corrected owned
package starts Scenes/landscape.judas. Shared terrain requires rebaking both Scenes/main
and Scenes/region; stale publication rejection is retained. Original projects were
not moved or edited. MOD_PIPELINE_INPUTS.json records the owned configuration delta.

The source-hidden relocated shipped runtime uses probes/mod-runtime.harness, cwd /tmp,
and the same case-data XDG paths as mod install/save tests. Frame22 is paused with zero
fixed steps; frame62 resumed with one. Screenshots are retained, not human acceptance.

Optional notice probe links the existing EditorMilestoneNotice.cpp.o, imgui and SDL.
`milestone-notice-build.json` contains the command with audit-only `-fno-access-control`.
Owned fake gh fixtures return missing/malformed/valid data or a cancellable sleeper.
No network call/authentication or UI click proof follows from this native Query check.

## Windows recovery and limits

User authorized restarting the unresponsive unchanged VM. ACPI reboot receipt:
windows-restart.json. Recovered compact original results: WINDOWS_COMMON.json.
Five native tests exit zero; source identity uses the existing 853-input manifest.
No broad Windows build/run or moved-package/hardware coverage is inferred. An initial
oversized PowerShell JSON transport timed out; the compact receipt preserves native
stdout without expanded PowerShell string metadata. The empty attempted transport
files are failed observers, not valid product receipts. Current low disk and any
remaining agent responsiveness issues are explicit environmental limits.

## Preservation and evidence scope

BASELINE.json hashes all 38,386 tracked inputs. PRESERVATION.json verifies them again,
with HEAD/main/origin-main and tracked/index diff. CASE_INDEX.json retains case command,
identity and statuses. CHANGED_FILES.txt lists only M78 material; POST_M73_REPORT.txt
is an original untracked exception. FINAL_FINGERPRINTS.json hashes new audit artifacts,
excluding itself. Large disposable builds/packages remain ignored and are not staged.
