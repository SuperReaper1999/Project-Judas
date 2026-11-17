# Maintain current V2 documentation

The [documentation index](README.md) routes users to current engine/editor/SDK
contracts. Source, public interfaces, selected build paths and demonstrated
application behaviour are authoritative. A historical report is not a specification.

## Change one authoritative guide

1. Trace registration, runtime owner, configuration, errors, units, lifecycle,
   persistence and editor/SDK integration. Distinguish selected production code,
   optional extensions, unavailable platforms and unselected experiments.
2. Update the practical feature guide and corresponding JudasJS reference. Add
   current examples with actual component/asset prerequisites. Preserve complete
   useful explanations; replace an obsolete explanation rather than appending a
   correction to an old one.
3. Consolidate duplicate guides. Before deleting, inspect their contents and
   implementation references, move still-current contracts to the owner guide,
   and repair incoming links and SDK inventory. Git retains old versions: do not
   create an archive of progress reports, old patch notes or diagnostic captures.
4. Keep required licences and asset provenance. Retain concise qualification
   evidence only while it supports current scope, a compatibility contract or an
   unresolved decision. Raw outputs belong to one bounded campaign/CI retention.
5. Update the [support boundary](ALPHA_SUPPORT_MATRIX.md), [readiness record](DEVELOPMENT_READINESS.md)
   and current findings only when the evidence changes. Unit/API/type checks,
   real applications, performance, platform qualification and human acceptance
   are separate claims. Never invent missing device coverage.
6. For public API changes, update declarations, module catalogue, reference,
   executable examples and meaningful native regressions together. Documentation
   work must not modify engine behaviour to make an obsolete guide true.

## Checks from the source checkout

```sh
python3 tools/release_docs.py --write
python3 scripts/check_current_docs.py --require-tracked
python3 -m unittest discover -s tests -p test_current_docs.py
python3 -m unittest discover -s tests -p test_release_docs.py
node scripts/check_judasjs_api.mjs
```

`tools/verification/current-docs.json` explicitly registers current owned Markdown,
including project/example entry points and the native physics module map. Add new
current guides there in the same change. The checker rejects missing files/anchors,
stale managed summaries and accidental historical exclusions; it does not prove
runtime semantics. The API checker verifies declarations/wrappers/examples with
negative drift controls; actual native/application tests supply behaviour evidence.

Validate copyable commands with matching tools and isolated output paths. An SDK
can exclude native source or source-only example projects; identify those paths
rather than implying every referenced file is installed. Verify staged SDK docs
and complete inventory/checksums after changing packaged documentation.

## Source, publication and contributor authority

`engine-version.json` owns source identity. `RELEASE_STATUS.json` is a dated
publication receipt and is not updated by documentation cleanup or a development
checkpoint. The single current-version curated notes file is release input;
obsolete notes for an older publication are not required in the active tree.
`CURRENT_FINDINGS.json` owns the generated limitations table. Refresh managed
blocks with `tools/release_docs.py`; never rewrite a receipt as new qualification.

Finish source/document edits before source-aware builds/tests. Record actual
binary identities; a stale-binary refusal is not a pass. Work on Experimental,
preserve unrelated local work and follow root AGENTS.md for commits/pushes.
Main promotion, releases and selected consumer SDK changes require their explicit
operator authority. Ordinary docs cleanup does not grant it.

## Current documentation verification scope

The V2 reconciliation reviewed every owned text-document path and report/capture
inventory, grouped technical guides by current source owner/public interface,
retained useful contracts, removed obsolete reports and repaired navigation/tooling.
Core architecture, animation, physical state, terrain, rendering, build/platform
and support guides were rewritten where successive milestone explanations conflicted.
The shared declarations/examples are independently checked against the binding
catalogue; qualification claims reuse the established exact V2 baseline.

The unregistered legacy `judas_script_presentation_tests` target still references
removed demos. It is a separate implementation finding in [the register](CURRENT_FINDINGS.md),
not repaired or represented as covered by this documentation pass.


## Implementation verification map

Use these source owners when changing the corresponding guide family. Public
arguments/results are independently covered by `docs/judas.d.ts`, the module
catalogue and the API checker; native/application scopes come from the validation
manifest and exact baseline receipts, not this table alone.

| Guide family | Current source / integration boundary |
| --- | --- |
| Application, editor, SDK | EngineHost, Application, InteractivePlay, SceneSession; `src/editor/EditorPanels.*`, workspace/presenter, CMake and `packaging/sdk-inventory.json` |
| Scripts, lifecycle, debugging | ScriptSystem, `src/scripting/Modules.json`, module handlers/library fragments and debugger; one VM |
| Assets, import, named content, exports | AssetDatabase/ResourceManager, ModelImport, NamedAuthoring/AuthoringDocument, `tools/SceneAuthor.cpp`, build/export services |
| Animation, retargeting, IK, physical poses | SkeletalAnimation, WorldAnimation, AnimationRetarget, FullBodyIK/PhysicalAnimation, WorldPersistence, animation binding module |
| Rigid/planar mechanics, queries, events | PhysicsWorld, production `src/physics/WorldMk2*`, geometry/filter/query and event owners; Simulation authoritative schedule |
| Vehicles, structural damage, materials | Wheels/powertrains, JointMaterial, Deformable/PanelBending, `src/deformables/MaterialBatch.h`, finite connections and persistence participants |
| Terrain, planetary charts, streaming | `src/terrain/TerrainCook.cpp`, TerrainPersistence/RenderPages, WorldStreaming, WorldOrigin, navigation publication and terrain binding module |
| Rendering, light, materials, particles | Renderer, `src/render/*`, `src/lighting/*`, particle owners, appearance/material codecs and rendering bindings |
| Water, weather, atmosphere | WorldWater/WorldWeather, `src/fluid/GameWater*`, project policy, current water bindings and explicit unsupported save/region cases |
| Saves, migrations, mods, networking | SaveService/WorldPersistence, SaveEvolution, mod registry/package owners, NetworkSession/NetworkTransport; dedicated/replica authority |
| UI, text, localization, audio/input | RuntimeUI/UIStack/RuntimeUIControls, UIAssetEditor, text/layout/localization, audio backend, InputMap/device/local-player routing |
| Build/platform/debug workflow | CMake identity/feature selection, packaging scripts, Android/browser builders, selected CI workflows, bounded storage and actual tool parsers |

The API pass includes 58 type-checked examples, 353 registered native operations and
524 live VM surface entries; these are surface checks. The cookbook's six actual
VM/world cases have 37 behavioural assertions. Supported native application and
SDK lifecycle/persistence qualification remains tied to its exact V2 code candidate.
This documentation-only assignment does not claim new full-platform qualification.
