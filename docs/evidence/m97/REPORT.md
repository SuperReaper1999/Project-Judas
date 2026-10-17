# M97 — Judas Learns to Change Worlds

Linux-validated Experimental candidate. **Human acceptance pending.** Baseline:
`94e4113a52af4ae66b942e8089e3e4e0e4daf1d4` (accepted M96). Main is not promoted.

## Delivered

- Optional `RuntimeTerrain` instances use M84's exact shared field brush/extraction
  implementation. `entity.terrain.create`, `brush`, `state` provide bounded
  add/remove/smooth operations through the normal JudasJS module.
- Immutable job products contain one scalar field revision, indexed mesh, cooked
  collision and optional local Recast navigation. Last-good products remain active
  on rejection/failure. The owner thread stages native navigation and updates static
  collision in place; nearby sleepers wake and obsolete geometric history retires.
- Existing WorldStreaming retains committed fields/settings and root primitive
  runtime construction through normal region-local recipes, tombstones, physical
  poses/velocities and staged publication. Retained geometry reconstruction is async.
  No inactive region bodies continue simulating.
- Existing cold saves have a conditional terrain participant; streaming archive v9
  carries terrain and constructed-member provenance. New ordinary-joint saves use
  articulation v5 for exact native physical settings before strict continuation.
- The ordinary Living World Lab has two flat quarries, a rotated quarry, excavation,
  physical block construction, a releasable joint-supported cantilever, position
  interests, region travel, an Earth-radius absolute offset, origin shifts and slots.
  There is no engine scene-name/object-name policy or mining/reward system.
- Existing M84 editor source editing/cooking is retained. `EmbedVolumeSource.py`
  packages a source as an ordinary ES module so exports need no source filesystem.
  Author regional content in `Scenes/quarry*.judas`; Play the composed workshop.

## Generic defects found during end-to-end validation

1. **Non-dynamic runtime metadata publication:** CreateEntity's extra-owner path
   did not advance EntityVersion. Streaming's cached identity list could miss a
   newly created root joint owner and unload its participant region. The path now
   publishes its version; a real external joint positively pins the region until
   removed. Metadata, prefab and classification regressions pass.
2. **Moving ordinary joint owner precision:** only a world-frame quaternion differed
   after float inverse/forward owner-frame reconstruction (first byte difference
   offset 48). New saves retain exact native physical settings, mapped by existing
   entity IDs, before restoring existing numerical continuation. Equality is still
   strict; no tolerances, controller settings or solver budgets were changed. The
   independent fixture checks bit-exact joint settings and the next physical endpoint.
   See [original contexts and derivation](development/joint-cold-save/CAUSE.md).
3. **Private staging accumulated motion:** disabling collision did not stop hidden
   dynamics while an asynchronous terrain restore prepared. A fast moved-package
   tour exposed crates at Y=-39.57m instead of their captured Y=0.50m. A distinct
   internal simulation-suspension flag now preserves staged rigid/kinematic motion.
   Ordinary collider-disabled gameplay actors still move. The original trace and
   [cause](development/staged-motion/CAUSE.md) are retained. Final frame 650 and 860
   checks keep the crate between 0.4 and 1.0m; final trace is approximately 0.50m.

Development failures also include private-owner lookup/publication, a normal
single-lane busy restore, the new save participant's initial omission from the
schema whitelist, malformed authored joint field types, and a game script writing
motor velocity outside fixedUpdate. All corrected follow-ups use the normal paths.
The new rounded-material probe rolled off its spherical support; its incorrect final
height assertion was replaced by observation of actual added-material contacts above
an unchanged cave floor. This was a new fixture correction, not a weakened historical
physics test. A rotated fixture's expected sign was corrected: +90 degrees about Z
maps local down to world +X. Original logs remain in `development/`.

## Focused final Linux validation

[Raw results](RESULTS.json), [commands/logs](linux/RESULTS.json), [delivery](delivery/RESULTS.json).
No full production-suite marathon was run.

| Scope | Result |
|---|---|
| New terrain/physics/persistence core | 58 checks, pass |
| New retained construction/terrain | 35 checks, pass; repeated cycles, actual joint recreation, zero unloaded bodies |
| Real lab application | write 13 / fresh-process read 8 / rotated 6 checks, pass |
| Cold physical proof | exact density bytes and actual cave-ray endpoint; returned crate supported for 120 physical steps |
| Existing streaming | 82 checks, pass |
| CharacterMotor | 32 checks, pass |
| M84 volume authoring/geometry | 69 checks, pass |
| Navigation | 77 checks, pass |
| Live-origin assembly streaming | 23 checks, pass |
| Metadata / prefab / classification | 32 / 37 / 39 checks, pass |
| Lifecycle | pass |
| Physics continuation / excluded saves | 112 / 77 checks, pass |
| Existing streamed-range cold saves | write 11 / read 19 checks, pass |
| Animation cold save | write 8 / read 8 checks, including first resumed physical endpoint |
| API / TypeScript / live enumeration | 38 exports, 447 symbols, 296 native ops; 49 static examples; 11 structured shapes; pass |
| Executed cookbook surface | 9 checks, pass |
| Editor | Play/Stop authored scene identical; save succeeds |
| Export / relocation | normal exporter; byte-identical package launched from unrelated /tmp cwd; return/save/load/origin tour and prop support checks pass |

There are 24 final focused driver cases. `linux/RESULTS.json` preserves the failed
rotated expectation invocation and its passing follow-up; latest result per case is
zero. Earlier invocation mistakes (save-exclusion missing output argument, using the
non-streamed shooter fixture for a streaming-only test, an unsupported `STEP`
harness directive) are preserved as tooling failures, not claimed engine defects.
Native compilation used Release. Existing warnings in untouched render-material
lines of RuntimeWorld remain; no global zero-warning claim is made.

## Measurements

Intel i7-4790, GTX 970 4GB, NVIDIA 580.178.04. [Host/build identity](BASELINE.json).
No CPU-intensive build or other Judas game ran during the final performance case.

| Metric | Final observation |
|---|---:|
| One remove dab, input to committed publication | 38.2264 ms |
| Worker preparation for that dab | 34.7137 ms |
| Native publication for that dab | 0.191723 ms |
| Frame CPU/wall interval median / p95 / worst | 0.895279 / 1.18758 / 2.65779 ms |
| Physics step median / p95 / worst | 0.017773 / 0.148936 / 0.198377 ms |
| Whole-process peak RSS, including GL | 145336 KiB (about 142 MiB) |
| Committed edited geometry estimate | 497940 bytes, excludes cooked BVH/native nav/GPU/allocator overhead |
| Peak bodies / active regions in travel test | 29 / 2; one-quarry steady inventory 15 |
| Peak retained snapshot estimate | 768276 bytes |
| Worst observed streaming outer integration | 3.59974 ms |

Frame measurements use 600 post-ready frames in an **uncapped hidden 1024x768 GL
functional harness with forced 60Hz simulation**, not a human FPS certification.
There is no asserted historical speedup. Whole-process RSS is not incremental terrain
memory or GPU memory. CSVs are retained in `linux/`; raw M56 captures are compressed
in `profiles/` (`gzip -dc file.json.gz`). Preparation is asynchronous; bounded native
navigation activation/mesh upload can still consume owner-thread time.

## Boundaries and remaining limitations

- At most 8 local volumes/world, 32768 samples/volume, 262144 total, 64m/axis,
  16384 extracted triangles/product, one preparation lane. Busy returns false;
  scripts retry. Empty extraction/over-budget products retain last-good geometry.
- Finite dense M84 fields and immutable outer air boundary. No seamless volume
  tiling, adaptive volume LOD, planetary terrain excavation, soil simulation, material
  mass/debris ledger or guaranteed sub-cell features. Visible faceting is expected.
- Root unit-scale static volume owners only. Navigation covers the volume's own
  triangles in its explicitly oriented local +Y frame; adjacent external geometry
  is not rebaked/unioned. Use existing links between separate surfaces.
- Newly adopted complex hierarchies/resource-owned subsystems remain restricted;
  existing active ragdoll/liquid/navigation-intent/attachment dependencies still pin.
  This is selected regional suspension, not universal assembly certification.
- Regional reconstruction preserves physical state/configuration but does not
  promise bit-identical contact warm-start trajectories across unloading. Full
  cold saves retain their existing strict continuation contract separately.
- Legacy saves retain their existing limitations. No missing historical state is
  invented. Unrelated fluid/physics research or deferred defects were not reopened.
- Pending edits require an unpaused publication boundary. Play/Stop discards runtime
  changes unless saved. Source edits require the documented cook/embed content step.
- Windows and Android M97 functional validation were not run and remain outstanding,
  nonblocking. Human acceptance is pending.

## Reproduce / handoff

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# Build the targets named by scripts/m97_validation.py, plus judas/editor/export.
python3 scripts/m97_validation.py --output .cache/m97/reproduce
python3 scripts/m97_delivery.py --output .cache/m97/reproduce-delivery
sha256sum -c docs/evidence/m97/FINAL_SOURCE_SHA256SUMS
sha256sum -c docs/evidence/m97/BINARY_SHA256SUMS
```

The scripts' `--resume` is only for the same frozen source after a tooling failure;
use a fresh output after changing engine/project code.

- [Project and controls](../../../projects/living_world_lab/README.md)
- [Workflow, limitations and the human checklist](../../M97_LIVING_WORLD.md)
- [JudasJS terrain reference](../../judasjs/terrain.md)
- Built package: `.cache/m97/final-delivery/package/judas`
- Relocated tested package: [location receipt](delivery/LOCATIONS.json)
- Exact changes: `CHANGED_FILES.txt`; source, checkpoint and binary hashes alongside.

Experimental is eligible to checkpoint on the scoped Linux evidence. Main remains
at accepted M96. No human acceptance, Windows pass or universal suspension/excavation
support is inferred from these checks. Protected historical evidence, existing games
and independent fluid research remain unchanged.
