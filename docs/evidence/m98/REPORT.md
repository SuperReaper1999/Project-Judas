# M98 — Save Evolution candidate

Baseline: accepted M97 `4c26a3bb23b8e3d1a7047283da0dcd2afdcc5e9c`, on
Experimental. Linux validated; **human review pending**. Main is not promoted.

## Implemented

- Existing SaveService/SaveStorage/WorldPersistence remain authoritative. No second
  save system or migration VM. Unconfigured projects retain exact-content policy.
- Format 4 adds explicit project compatibility and content version, with combined
  identity/payload integrity. Native schema and participant versions stay separate.
  Formats 1–3 retain their readers; an actual accepted M97 save restores unchanged.
- Installed, project-owned policy pins exact old content hashes. Ordered forward
  steps transactionally patch script/session JSON and a restricted scene/prefab
  appearance/script/provenance surface. Stable entity and prefab-asset selectors;
  script assets can select independent runtime instances without hardcoded spawned IDs.
- Opaque native/unknown state is retained; strict native readers still reject an
  unsupported schema or physical context. No geometry, body ID, topology, force,
  cache or controller reconstruction is invented by a policy.
- Existing save worker performs conversion. Existing private world validates all
  required resources/participants and publishes only after restoration succeeds.
- Migration opt-in is `saves.load(slot,{migrate:true})`. Status/list observations,
  declarations, references and API drift coverage are synchronized. No new native
  gameplay operation or top-level module export.
- Changed/missing mod contexts reject with exact saved/current requirements. Exact
  author-approved old/new context pairs are possible, but do not install/enable
  packages, override trust or bypass dependency/native-resource checks.
- Disk sources remain untouched. Successfully migrated source IDs are protected
  from save overwrite for the service lifetime; upgraded progress uses another slot.
  Backups preserve exact original bytes. Unsupported future containers refuse
  overwrite. `judas_save` provides identity/inspection/check/separate-slot migration,
  recovery and explicit previous-good selection; destinations are locked/unused.
- Project settings provides a policy editor; ordinary project serialization and
  export preserve it. Five ordinary versioned projects form Save Evolution Lab.

## Demonstrated integration defect and correction

A real constructed/adopted prefab was present after region return, but its saved
reference could not resolve. Region reconstruction replaced its preserved root
identity with a fresh region-qualified identity. The generic registration handoff
now carries the saved identity to the newly allocated runtime ID. This does not
reuse native handles, attach objects to transforms or change physics.

Original log/save and pre-correction source: `development/adopted-reference/`.
Controls now pass both exact-content load and content migration: unload the edited
quarry, cold save, load, return, resolve the construction and check its real dynamic
body. Multiple migrations retain that reference. No test was weakened.

## Linux results

Release configured/incrementally built; no full production-suite rerun.

- **40** new conversion/codec/policy/storage checks. Includes independent multi-instance
  state, unknown-data retention, failed guards, no missing steps/downgrades, future
  container preservation, exact legacy backups and a >64 KiB scene conversion.
- **38** existing storage-protocol checks unchanged.
- **125** real application checks across older-game creation, same-content control,
  default refusal, two-step and direct upgrades, current-format reload, injected
  patch/native failures, incompatible content and missing-mod diagnostics.
- Native physics continuation **112**, CharacterMotor **32**, save exclusion **77**,
  existing streaming, live terrain **58**, retained terrain/construction **35**,
  streamed cold saves **11/19**, animation cold continuation **8/8** pass.
- Actual M97 fixture backward read **8**: exact field and actual collision endpoint.
  Portable checked-in fixture repeats the result. Original archive bytes preserved.
- Existing mod dependency/version/trust/storage tests **127** pass. Real four-package
  modded game write/read **9/11** pass, including actual motor endpoint. Removing
  the selected landscape-fork package yields expected load refusal **7** checks.
  Explicit allowed changed-version pairs are converter tests, not a certification
  of arbitrary real mod upgrades.
- New cookbook UI callback **10** checks. API live enumeration/types: **38 exports,
  447 symbols, 296 bridge operations, 17 callbacks, 50 statically checked examples,
  13 structured result shapes, 4 negative drift controls**, TypeScript 5.9.3.
- Editor Play/Stop preserves authored scene; five exports succeed. Byte-verified
  v1/v3 packages run from unrelated `/tmp` paths. A genuine v1 game collects seven
  salvage, saves, then v3 migrates to inventory/chapter and writes another slot;
  original source bytes unchanged. Screenshots show the restored seven items.

`RESULTS.json` joins scoped runs and follow-ups. Earlier API/JSDoc and bad UI fixture
failures remain recorded; final `linux/api-final.log` and `cookbook-evolution.log`
supersede those failures. No aggregate full-engine certification is claimed.

## Costs

Host: i7-4790 / GTX 970, Linux Release. See ENVIRONMENT.json. Twelve separate
processes upgrade identical v1 progress directly to v3; ~230 KiB source snapshot.
These are development-host timings, not historical speedup or frame-rate claims.
Nearest-rank p95 with 12 samples equals the maximum; raw rows/logs are retained.

| Phase | Median ms | p95 / worst ms |
|---|---:|---:|
| Worker conversion only | 12.344 | 15.429 |
| Worker read/conversion/listing | 39.439 | 44.906 |
| Private preparation/restoration (elapsed, includes async waits) | 56.178 | 58.805 |

Costs are separate: conversion does not run on the render thread; preparation has
existing owner-thread publication work. Exact unconfigured control takes the
comparison path without patches. No migration latency guarantee for all bound-sized
policies, no FPS speedup claim, no Windows/Android performance claim.

## Limitations / honest compatibility boundaries

- Migration is explicit forward conversion, not universal compatibility. Physics
  topology/geometry changes, unknown native codecs and missing historical continuation
  cannot be fixed by a project schema number. FLU-001 and independent research untouched.
- Only active entity/script records can be patched. Inactive regional records,
  construction recipes and terrain payloads remain byte-exact; changed inactive-region
  script schemas/layouts/resources need a future dedicated conversion mechanism.
- Loaded worlds are physical snapshots, not automatically rebuilt from new prefabs
  or authored scenes. Only the documented safe component/provenance subset is patchable.
- No arbitrary JS migration callbacks/arithmetic interpreter. Explicit JSON Patch
  replacement/removal is developer policy; careless policies can still destroy game
  meaning. Preserve old content, use guards and test actual restores.
- Offline conversion is unmodded only and is not a native-restore certificate.
- Older engines cannot read format 4. Windows/Android M98 validation not run and
  remain informational/nonblocking. No human acceptance inferred.
- Existing unrelated RuntimeWorld indentation and ModContentTests temporary-pointer
  warnings remain visible in build records; no zero-warning clean-build claim.

## Reproduction / deliverables

- Projects: `projects/save_evolution_lab/{v1,v2,v3,failure,incompatible}/save_evolution.judasproj`.
- Local export roots/relocated locations: `delivery/LOCATIONS.json`.
- Policy/tool documentation and single human checklist: `docs/M98_SAVE_EVOLUTION.md`.
- `python3 scripts/m98_validation.py --output <fresh>`.
- `python3 scripts/m98_regressions.py --output <fresh>` (build listed targets first).
- `python3 scripts/m98_backward.py --output <fresh>`.
- `python3 scripts/m98_mods.py --output <fresh>` (real archived selected packages).
- `python3 scripts/m98_delivery.py --output <fresh>`.
- `python3 scripts/m98_measure.py --output <fresh> --fixtures <validation-output>`.

Exact changed paths: CHANGED_FILES.txt. Source, binary and checkpoint fingerprints
are separate and checked before checkpointing. Existing games, protected historical
records, accepted fluid work and independent research remain unchanged. No M99 work.

## Development records

Preserved compile signature errors; a prefab test fixture initially omitted its
required root provenance; the first large-scene fixture was below the byte boundary;
a cookbook UI initially lacked its required root canvas; a JSDoc parameter caused a
TypeScript error; and the first existing mod application run lacked its required
selected packages. These were fixture/tooling corrections, not engine failures.
The adopted-reference failure above was a genuine runtime defect and is separately
preserved with the generic fix and positive/negative controls.
