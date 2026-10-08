# Scalability and content-pipeline contracts (accepted M68)

M68 is accepted at `c042797c755df68b45e36aa917c4ca72818b9bcd`; its original
starting checkpoint was `52828779993843d9c8b0060f5e30e49e61d0466f`.
M67 human acceptance remains provisional/deferred. Automated checks here do not
supply that acceptance. Measurements and exact commands are in
[evidence/m68](evidence/m68/REPORT.md).

## Runtime work

RuntimeWorld keeps its existing entity index, ordered script membership and
component ownership sets. Normal motion does not invalidate structural identity.
Script synchronization copies active slot metadata; callback dispatch snapshots
entity/slot keys. Inert scenery does not cause unrelated full definition copies.
Physics events resolve observed generation-aware body handles and freeze deliveries
before callbacks; active contact ownership retains exit identity. Explicit
`world.entities`/full-world native enumeration still enumerates the world.

Character discovery uses authored component membership. Necessary coarse evolution,
physics, animation, presentation and rendering remain real per-entity work. This is
not an ECS rewrite or a claim that arbitrary worlds take constant time. Destruction,
slot/property replacement, prefab construction, streaming and restore retain their
existing lifecycle and reference contracts.

## Streaming and resource readiness

Existing WorldStreaming preparation, budgets, pins, retention and coherent region
publication remain authoritative. Live ID snapshots follow entity membership
changes. Typed script references still read current slot properties; relationship
pins read current definitions. The measured streaming units fit their existing
2 ms target; no speculative replacement state machine was introduced.

ResourceManager bounds admitted CPU/private-GPU work to three requests and an
estimated decoded envelope (file bytes × 8 against the existing cache budget).
Completed CPU work retains admission until publication/discard. One oversized
request is admitted alone to guarantee progress. This is backpressure, **not a
hard memory guarantee**: decode expansion/transients and safety-pinned content may
exceed estimates. Pending requests retain metadata, not decoded payloads. A large
request can delay later requests; actual latency and bytes must be inspected.

Normal Pump shares one 2 ms **soft elapsed budget**, with at most two completed
asset installations. Skinned/static meshes upload geometry then individual material
maps privately. `Ready`, skeletal data and mesh handles publish together after all
required uploads. Cancellation/project changes destroy private GPU state on its
owner thread. Workers never call OpenGL. The existing blocking tool/required
physical-geometry boundaries remain explicit; normal async rendering does not wait.

Driver buffer/texture calls, mipmap generation, filesystem calls and script callbacks
are indivisible. They can exceed the soft target. M56 separates mesh geometry,
texture texel upload and mipmap work. Streaming plus upload plus publication is the
aggregate frame cost; separate targets must not be added up and called one budget.
Resident cache eviction still preserves references/pins; retained world state is
not discarded to make byte counters smaller.

## Import and disposable caches

M66 recipes, settings, identity, normalization and full cooked-asset validation remain
unchanged. Unchanged CLI/build import verifies source/dependency contents, recipe,
cooker revision and accepted output hash. A hash-paired receipt supplies validated
provenance without decoding the product again. Missing/corrupt legacy receipts fall
back to the embedded validated product and recreate the receipt. Editor preview
still decodes the required mesh once. Publication compares verified inode/size/
mtime/**ctime** stamps only as race guards; changed stamps require hashing. Stamps
are never cache keys, and same-mtime edits cannot pass unnoticed.

Base and motion caches carry payload hashes and are reconstructible from author-owned
sources/recipes. Bad/missing caches rebuild; failed/stale/cancelled generations retain
last-good output. Cache writes and accepted staging use unique owned temporary files.
Source content, settings, cooker revision and compatible motion dependencies control
invalidation. Unchanged source art, vertices, parts, bones, influences, textures,
clips and archive version are retained. Runtime packages still contain cooked data,
not an FBX importer, source art or editor caches.

## Export policy and inclusion report

Project settings support:

- `exportAssetPolicy: "all"` (legacy/default): all registered project assets.
  Existing dynamic JS/ID lookups keep working without new configuration.
- `exportAssetPolicy: "closure"`: selected scenes, project icon/world/localization,
  explicit `runtimeAssets` IDs, then transitive prefab/material/model/collision/
  deformable/UI dependencies. Runtime glTF/GLB compatibility remains self-contained;
  approved external source buffers/images must be cooked by M66 into `.judasmodel`
  first. World-required region scenes must follow the existing scene policy.
- `runtimeAssets`: explicit stable-ID roots for dynamic requests and save/retained-only
  resources. Their dependencies are gathered normally. The exporter does not guess
  JS code paths or scan strings as a proof of completeness.

All registered scripts/modules, UI, fonts and catalogs remain conservative roots
because current save fingerprints include these sets and script/UI demand is dynamic.
Other dynamic assets require explicit roots when closure is selected. If requirements
are unknown, keep `all`. Project Settings → Export exposes the policy and root list.
The named schema stays 1, project format stays 1 and fingerprint schema stays 5.

Every package includes `DEPENDENCIES.json`: selected scenes, reasons, asset IDs,
relative paths, types, byte counts, SHA-256, shared payloads, excluded assets and
auxiliary files. Byte-identical same-type immutable payloads use hard links within
staging where supported, with copy fallback. Logical IDs/metadata and runtime mutable
instances remain independent. `du` physical bytes differ from a logical sum of file
sizes; no lossy compression or archive representation change was introduced.

The M61 project save-content fingerprint normalizes the new packaging-only policy
and root fields to their legacy defaults. Actual packaged files are still hashed.
For excluded assets, the same dependency report retains ID/type/content hashes so
pruning unused bytes does not change the original project identity. Those records
are **not loadable assets**: dynamic/save-only resources still need explicit roots.
Malformed/duplicate exclusion records fail clearly; an actual packaged file edit
still changes its hash. Existing packages without this report retain their prior
identity path. No save or canonical fingerprint schema changes were made.

Strict asset-database integrity, required/missing/stale validation, licences, runtime
libraries, text/localization and transactional previous-package preservation remain.
Do not remove resources merely because a package exceeds 200 MiB. Inspect the report.

## Bounded authoring follow-through

M67's shared editor/source/CLI model remains. Typed named loaders validate once at
the actual parse boundary; format conversion no longer repeats preceding validation.
Accepted editor candidates move their already-owned prior scene into history instead
of copying it again. One action/one undo, drag cancellation, 200 complete revisions,
prefab/reference/ID validation and external conflict handling remain. Identical
accepted AuthoringDocument text is a no-op; different text is still normalized and
validated. Whole-scene history still consumes memory proportional to retained content.

## Reproduction and human review

Run from the repository with a Release build and fresh output paths:

```sh
python3 scripts/m68_measure.py --output .cache/my-m68-capture --bin build --tracks runtime import export authoring resource
build/judas_scalability_tests .cache/my-m68-structure
build/judas_scalability_pipeline_tests .cache/my-m68-pipeline build/judas
build/judas_resource_handoff_tests .cache/my-m68-gpu
python3 scripts/m68_integration.py --output .cache/my-m68-integration
```

The professional import fixture follows its existing local-only policy. Missing
original content is a missing fixture, not a synthetic substitute or a passing test.
Heavy captures run serially; OS caches are not purged. Do not interpret a new process
as a cold disk cache. Human GUI/visual/audio acceptance remains the operator's job.
