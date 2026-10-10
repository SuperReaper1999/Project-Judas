# M76 candidate evidence — local mods and terrain-capable kits

Base: `38d345883a541d4eb2faa43c321e940d223fce09` (accepted M75).
**NOT HUMAN VALIDATED.** Operator authorized a provisional checkpoint after additional
automated UX checks; human review remains pending. No M77 work. See [UX review](UX_REVIEW.md).
[M76 delivery/controls](../../M76.md) · [modding reference](../../MODDING.md) ·
[development failures/follow-ups](DEVELOPMENT.md) · [results](RESULTS.json) ·
[measurements](MEASUREMENTS.json).

## What is implemented

A developer-owned optional contract, bounded immutable local packages, deterministic
selected dependency resolution, curated author kits and per-game user installation.
One effective AssetDatabase supplies normal resource jobs, scripts, scenes, prefabs,
physics, navigation, streaming and modern saves. Qualification precedes cooking;
base identities and empty/unmodded save archive v2 remain unchanged. Nonempty
loadouts use checked v3 metadata and exact contract/resolution/payload preflight.

No second loader/VM/world, native game semantics, registry merger, online service,
compression dependency, runtime terrain editor or hostile-script sandbox was added.
Inspection/install validation compiles but does not evaluate scripts. Executable
packages need both game permission and explicit exact-payload trust. Shared caches
and installed versions use scoped OS leases; the existing ResourceManager drains
old jobs before retiring a content view. The whole requested set fails together.

The support matrix, bounds, script imports, qualified IDs, replacement requirements,
platform storage and transaction qualifications are documented in MODDING.md.
The prototype is not filesystem-wide power-loss certification. Trusted JS still
has normal project powers and runtime slot fault isolation.

## Ordinary demonstration and authoring

`projects/modding_lab/modding_lab.judasproj` is a neutral game-owned hub, protected
monument, permitted landscape and streamed region. It publishes a controller,
physical drop prefab, references, behavioral property schemas and extension points.
Game JS consumes detached `mods` metadata and chooses maps/prefabs. No scene names
or mod meanings occur in engine C++.

Ignored `.cache/m76/artifacts/` contains the exported Release game, `FinalKit/`,
separate `Hill Mod/` (A) and `Landscape Fork/` (B) author projects, their installable
packages, an actual editor-produced A variant, dependency examples and a 100-small-
asset stress package. See M76.md for exact paths and runnable commands.

- A uses newly created/sculpted/painted M75 terrain, matched cooked mesh/collision/
  material and Detour bake, an ordinary scripted prefab, signal and saved fact.
- B forks published terrain, recooks/rebakes it and replaces only the expressly
  permitted landscape and region scenes. The protected hub remains unchanged.
- Two dependency mods execute a published ordinary ES module whose saved value is
  7. A and dependencies deliberately share `Assets/scripts/visit.js` and a local
  AssetId; qualified runtime identities and ownership remain distinct.
- Kits copy only published closure, explicit editable sources/notices, exact registry
  settings and matching offline reference/types/example manifest. `kit.json` reports
  inclusion reasons and SHA-256 for assets, metadata, scenes, sources and docs.
  Generated descriptor/README are also shipped; they are not self-hashed entries.
- The final lab explicitly includes its font license and content notice. A/B package
  payloads did not change for that content-only contract/kit correction.

Author projects are ordinary Judas projects. The existing terrain panel performs
viewport sculpt/paint, Undo/Redo, cancel/focus-loss, Save/Cook and navigation bake.
The manifest draft uses normal guarded Save/Undo/Redo. Package validation is shared
with CLI/editor/install/runtime rather than a tool-only approximation.

Original source was renamed out of reach during kit-based create/fork/cook/package
proofs. Original, kit and mod authoring trees were unavailable during runtime proof;
only exported base content, immutable installed packages/cache and user saves were
available. Actual exported executables were launched, not just a repository override
or a hand-constructed RuntimeWorld. Paths with spaces/Unicode, unrelated working
directories and read-only base packages were exercised.

## Linux focused verification

Release runtime/editor/tools and focused tests built. Incremental affected rebuilds
followed demonstrated defects; the initial signature/link mismatch was not counted
as final-source evidence. Existing/inherited warnings are recorded, not described
as a zero-warning build. No full production or unrelated research suite was rerun.

| Check | Final result |
| --- | --- |
| Package/policy/identity/storage | 127 checks, 0 failures |
| Existing save-storage harness | 28 checks, 0 failures |
| Real Application with A/B + declared dependencies | 20 checks, 0 failures |
| Editor-produced package through real game | 20 checks, 0 failures |
| Fresh save / fresh process restore | 9 / 11 checks, 0 failures |
| Changed loadout refuses before live-world swap | 7 checks, 0 failures |
| Zero / empty / stress startup cases | Passed, separately measured |
| CLI negative publication fixtures | 9 rejected, last-good output preserved |
| Actual runtime admission negative fixtures | 3 rejected before world readiness |
| Editor viewport + normal Play/Stop | failures 0; authored bytes IDENTICAL |
| Shipped export, sources unavailable | EXPECT_SCENE passed; screenshot captured |
| JudasJS drift/types/live VM | 30 exports, 340 public symbols, 234 native operations, 16 callbacks; 38 examples; 3 negative drift checks; TypeScript 5.9.3; passed |

The real application route proves walking on edited collision, a thrown ordinary
body contacting that collider, ray queries, Detour routing, prefab creation,
scripted signals/facts, published dependency modules, region unload/revisit with
safe remapped body identity, and scene reload cleanup. Save checks use the normal
asynchronous save service and reconstruction, not snapshot-only serialization.

Core checks include bad UTF-8/NFC/case aliases/Windows names/traversal, parser and
archive bounds, dependencies/cycles/conflicts, exact trust, payload tamper/truncation,
immutable publication, cancellation/interruption/retry, in-use removal and shared
cache retention. Three POSIX-specific link cases are excluded on native Windows.
CLI negatives include protected replacement, missing dependency, hidden executable
content, undeclared references, altered kit identity, stale terrain, corrupt/stale
navigation and incompatible game. Actual launch negatives independently corrupt an
installed fixture to prove missing published references, wrong asset type and bad
data-only property schema are rejected before world activation.

## Native Windows scope

Native MSVC compilation, source hashes and refreshed executables are recorded in
the Windows receipts. Tests run as the normal Conner desktop user in the established
Windows 10 VM. A matching Windows SDK is generated with the mod tool, offline docs,
reference/types/example manifest, runtime dependencies and notices.

Final Windows results are recorded in RESULTS.json. The VM uses explicit
`GALLIUM_DRIVER=llvmpipe` and app-local Mesa only in loose/disposable test copies.
Pristine SDK and exports contain no Mesa DLLs. This is native execution, **not**
physical Windows GPU/input/audio acceptance; hardware validation remains pending.
Existing Windows short-root/long-path limitations remain.

Native package/storage checks: **124 passed**. Actual editor sculpt/paint/Save/Cook/
bake/manifest-package/Play-Stop: **failures 0, authored bytes IDENTICAL**. Its package
passes **19** actual application checks. The identical Linux A plus B and declared
dependencies passes **20**; final notice-complete fresh save/restore passes **9/11**.
Earlier changed-loadout refusal passes **7**, and zero-use/empty/trust denial/removal
were exercised independently. The actual moved shipped executable exits 0 after
the expected mod-scene transition; all source trees were unavailable and every
read-only package hash remained unchanged.

Native artifacts remain under `C:\Users\Conner\J76`: `FinalKitLatest`, independent
author projects, `Packages`, `editor-native.judasmod` and `Moved été Game` (the
software-rendered test copy). Pristine SDK: `C:\JudasValidation\Build\windows-sdk`.
The duplicate unmoved disposable export was removed after proof to recover space
on the fixed VM disk; compiler, SDK, source, authored projects and tested moved copy
were retained. No VM configuration or global path/permission policy was changed.

Preserved setup failures include an old Linux-only SaveStorageTests include, stale
incremental guest objects caused by source timestamp skew, missing loose-tool UI
font, ordinary stderr misclassified as failure, incorrect file ACL inheritance,
and an unspecified Mesa backend failing before readiness. None was hidden behind
Linux/Wine substitution. The editor-produced map also exposed an incorrect test
assumption about a rolling prop's final height; the corrected assertion observes
real non-sensor body/terrain contacts. No physics correction or quality reduction
was made for that assertion.

## Performance and sizes

These are actual uncapped hidden Release application runs on the development Linux
host, using M56 inclusive scopes. Frame and fixed-step timings overlap; do not sum
scopes. The workload changes between hub and terrain, so these are **not** before/
after speedup claims. Resource bytes are the existing budget counter, not exact
allocator accounting. Sampling occurred while native VM work was active; observable
outliers are retained.

| Loadout | Ready ms | Frame mean / p95 / max ms | Fixed mean / p95 / max ms | Bodies | Resource bytes |
| --- | ---: | --- | --- | ---: | ---: |
| Zero-use (`--no-mods`) | 631.84 | .701 / 1.217 / 19.552 | .238 / .310 / 2.722 | 3 | 759720 |
| Enabled-empty | 639.36 | .689 / 1.239 / 11.367 | .221 / .314 / .397 | 3 | 759720 |
| A/B terrain/script | 827.86 | .703 / 1.221 / 6.107 | .318 / .413 / .558 | 2 | 1475661 |
| A/B + dependency pair | 827.32 | .678 / 1.292 / 10.893 | .304 / .436 / .980 | 2 | 1475661 |
| A/B + 100 small assets | 805.49 | .751 / 1.262 / **66.087** | .375 / .435 / 6.353 | 2 | 1475661 |

The 66 ms frame is an observed stall, not erased by the mean or attributed to a
specific cause without profiling proof. No physics cadence/quality/resource budgets
were reduced. Admission is synchronous bounded tool/startup work; after resolution
there is no directory discovery, whole-package rehash, recook or package mount work
in the steady update path. Ordinary resources still load through existing jobs.
The small-assets case stresses admission/catalog routing of 100 registered small
scripts; it does not pretend those are 100 simultaneously executing script instances
or resident meshes. The unchanged resource counter makes that distinction explicit.

Separate measured tool operations: new 100-asset install/preflight 236.72 ms;
selection changes approximately 14–28 ms; removal/cache ownership inspection
515.61 ms. Detailed process timings and payload hashes are in MEASUREMENTS.json.
Packages: A 377578 B, B 366297 B, Linux editor A 462540 B, dependency pair 4337 B,
100-assets 32813 B. All are below declared bounds. Final kit is about 1.59 MB;
base Linux export about 54.14 MB. Installed/cache sizes depend on retained immutable
versions and are explicitly distinct from active resident resources.

## Current limitations and handoff

No hot mount, automatic dependency version choice, save migration, arbitrary
registry merging, hostile-JS isolation, online distribution, native plugins or
filesystem-wide crash transaction. Cold selection is eager and can cost more than
steady play. Structural/reference/module/nav admission is eager; ordinary decoding
and script execution can still fail later through existing resource/slot diagnostics.
Owned world/liquid/deformable/environment/audio-effect/catalog assets are excluded
in v1. M75 finite-patch/four-layer/unit-collision/offline-navigation limits remain.

Human review is still required for editor usability, visible terrain/collision/
audio/scripted interactions and moved-game behavior. Use the single checklist in
M76.md. M74 deferred motion review and physical Windows acceptance remain separate.

CHANGED_FILES.txt lists the exact candidate scope; FINAL_FINGERPRINTS.json records
source/doc/tool/fixture inputs and selected generated artifacts with exclusions.
At initial candidate delivery, nothing was staged, committed, pushed or tagged and
HEAD remained the accepted M75 checkpoint. The later operator-authorized provisional
checkpoint includes only that implementation plus UX evidence and validation-status
notes. Protected historical research/evidence, accepted fluid and consumer game
projects remain unchanged; untracked POST_M73_REPORT.txt is not absorbed.
