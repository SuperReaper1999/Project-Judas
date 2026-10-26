# Pre-Alpha developer workflow pass

Parent Experimental: `fa0a9d21b145325b54fc340780828ca6f6ebac21`.
Main remains `c2cdd376f1c20a9d6a8a75d8257f9ac44327ec09`.
Linux automation and screenshot review only; operator acceptance is pending.
No M100, release/channel change, main promotion, physics redesign or fluid work.

## Delivered

- **Runtime Inspector**, under View → Diagnostics and extensions, is optional
  editor-only UI. It selects live entities including streamed content, displays
  owner and stable save reference, region/native visual state, demand/retention
  reasons, body/query-proxy and CharacterMotor publication, script phases and
  faults. Ordinary selected-object Inspector links to it under Ownership & readiness.
- Its save section reads only an already-initialized service: installed identity,
  content fingerprint, saved compatibility/version/content and actual refusals.
  It never loads, migrates, deletes, creates demands or serializes script JSON.
- `ScriptSystem::InspectPhase` is a read-only native developer observation, not a
  JudasJS binding or universal ready flag. It distinguishes absent, awaiting start,
  awaiting restore, running callbacks and faulted. Disabled slots are labelled by
  their authored flag. UI explicitly explains that project readiness is separate.
- Live picker uses lightweight IDs and clipped rows rather than copying full
  SceneObject records. No new per-frame work runs in exported games; no measured
  game-speedup claim is made. Tool-visible costs are not exhaustively benchmarked.
- New window visibility participates in existing saved layouts/preferences. No
  docking architecture or component editor was replaced. Existing selection,
  transform, script, creation and authoring controls remain.
- Save listings and refused loads share `DescribeSaveCompatibility`: exact saved
  and installed fingerprints, compatibility/version changes and existing mod
  differences, plus safe recovery advice. Malformed mod metadata produces a
  diagnostic rather than throwing out of listing. Native payload validation and
  explicit allowlisted migration rules remain in force; no hashes are relaxed.
- World regions' editor Request now holds one lease per row. Repeated clicks no
  longer overwrite a token while leaking another demand. Release removes only
  that editor lease; independent gameplay demands remain valid. Old-world tokens
  are rejected by the existing process-unique token contract.

## Shipped editable lessons and guides

`docs/DEVELOPER_WORKFLOWS.md` is linked from editor menus, help, Runtime Inspector,
API index, first-project guide and the current lifecycle/streaming/save pages.
The architecture/editor guides describe the observation boundary.

The self-contained project `docs/judasjs/examples/developer_workflows/` teaches:

1. Ordinary project JS movement and a presentation camera.
2. A physical four-wheel/powertrain vehicle with an automatically inherited
   body-free visual; Space brakes. No car-specific engine system is introduced.
3. A root crate and an independently requested yard crate. Real ray/impulse
   interaction increments their own saved counters. A transient per-VM registry
   binds in start/restore and clears in destroy; it is not serialized.
4. Native region preparation/publication versus project registration, followed
   by actual unload/reconstruction and fresh-process saves.

The example ships through the existing docs/judasjs SDK directory and engine
source-archive rules, not a new packaging scheme. Fixture SDK staging checks every
sample file/metadata byte, relocation and missing-metadata detection. This is not
new full native 17-tool SDK or clean-machine certification. Full games/research
remain outside that SDK payload. DejaVu Sans and its licence are included; original
example code/assets use the repository MIT notice.

The example is deliberately flat/uniform-gravity and small, with primitive shapes,
a basic camera without collision boom, assisted boarding/exit, no audio, no campaign
and no controller/mobile qualification. It is a learning project, not a framework.
Larger source-only games/labs are supplementary references, not SDK dependencies.

## Actual failures, corrections and boundaries

- Initial C++ build used a raw-pointer deduction for the existing shared SceneSession
  accessor; corrected to its actual shared type. The original build log is retained.
- An initial readiness test dereferenced a lazy, not-yet-created ScriptSystem and
  crashed. The test now checks absence without initializing it, then observes phases
  after callbacks. No engine crash path was introduced by the inspector, which
  already guarded the optional pointer.
- The new panel adds a preference-table row. The old mouse fixture clicked a text
  field at y=582 instead of Save layout at y=609. The native click assertions remain;
  the coordinate was updated and actual saved visibility verified. That older test
  still has coordinate-sensitive input; it is not claimed universally DPI-independent.
- The first teaching scene had invalid hand-authored render/required component
  fields, then a leftover audio-emitter marker. Scene validation rejected these;
  corrected source passed. No parser strictness was weakened.
- Initial lesson test waited for active installation while paused. Actual native
  policy allows worker preparation but holds installation until resume. Corrected
  test proves prepared has no published entity, then resumes and checks active.
- Screenshot review caught an inherited bottom-centre UI alignment: text existed
  in state but rendered off-screen. The source anchor was corrected. A real layout
  bounds assertion was added to the application fixture and final screenshots are
  inspected; original blank screenshot and logs are retained.
- The unchanged streaming regression initially failed two retention assertions.
  The original 16,384-byte fixture budget is smaller than today's 16,508-byte
  **empty** retained record. SceneObject is 5,328 bytes; the unchanged production
  accounting includes three copies plus other state. The shrink actually succeeded
  (JSON became 26 bytes), but could never fit that old budget. Source hashes prove
  Scene.h and WorldStreaming.cpp match the parent checkpoint.
  An isolated 64 KiB diagnostic, with its initial pin assertion deliberately omitted,
  measures that cost; its 81 passing checks are **not** counted as acceptance.
  The real test now budgets `3*sizeof(SceneObject)+4096` = 20,080 bytes here. It keeps
  the same 8 KiB oversized payload, over-budget pin, unchanged-state preservation,
  shrink, bounded retirement and restored-state assertions. Final suite passes all
  85 checks, including three new editor-lease controls. **Engine budgets/accounting
  are unchanged.** Original failures, diagnostic source/logs and correction remain.

A speculative warning on every presentation transform write was deliberately not
added: detached visual animation can legitimately write transforms. The inspector
instead shows context-specific explanations for body-free children and physical
bodies; the public example demonstrates correct timing.

## Validation

- Release native build of affected engine/editor/tools and application fixture.
  Incremental builds and affected reruns, not another full production-suite marathon.
- Script regression: **39 checks, zero failures**; native save evolution:
  **48 checks, zero failures**; streaming: **85 checks, zero failures**.
- Editor preferences: **42 checks, zero failures**, including actual native mouse
  interaction, both themes and persistence of the new panel's visibility.
- New lesson: real movement, root/regional impulses and counters, paused preparation,
  actual unloading, native wheel/powertrain movement and saving; fresh-process
  restoration checks actual vehicle endpoint and driver motor state, resumes driving
  and reconstructs the regional counter/collision.
- Independent disabled-script fixture publishes regional collision but has no script
  instance or registration. Independent changed-content fixture rejects the load,
  names the fingerprints/recovery and leaves the current world intact. The original
  save bytes remain unchanged.
- A registered focused CTest/CLI gate runs a shell-free bounded child-process write/
  read cycle. A separate path-with-spaces cycle validates argv/save-path handling.
  See the final registered result and raw child logs; no source-count coverage claim.
- API drift: **475 public symbols, 40 exports, 313 native operations, 17 callbacks**,
  unchanged. Four example modules compile/link. No new JudasJS binding/type drift.
- Packaging fixtures: **29 cases**, zero failures. Build organization: **18 cases**,
  zero failures. Gate registration: **4 executed, 1 skipped** because Ninja was not
  installed for the explicit multi-config control. No Windows/Android execution
  was inferred from these platform-independent fixture checks.
- Isolated editor Play/Stop/duplicate/undo/save preserved the authored scene
  IDENTICALLY, with zero failed resources/jobs. Native screenshots cover dark and
  light/125% workspaces. Readability review is not operator game-building acceptance.
- Existing Lunar Island Stage 6 in-flight cold save: **20 checks, zero failures**
  on the rebuilt engine, including actual body/motor/shaft/fuel and continued flight.
  Its source and historical evidence were not edited.
- Ordinary standalone export, complete relocation and launch from unrelated /tmp
  working directory, followed by a real cold load. Final counts/hashes are in
  VALIDATION.json and adjacent manifests. Native provenance is recorded honestly:
  a Release development build from the parent plus this working-tree patch, not
  a clean Alpha release or a new main-channel package.

## Human acceptance and remaining work

Use HUMAN_CHECKLIST.md. Judge whether owner/readiness/recovery information is clear
while creating your own tiny game. We did not certify universal asynchronous
readiness, every physical assembly's suspension/adoption, all save migrations,
all camera configurations or all display scales. Existing documented engine and
fluid limitations remain untouched; Windows/Android qualification remains separate.
Main stays unchanged pending explicit approval. No tag, channel change or M100.


## Final artifacts

- Editable project: `docs/judasjs/examples/developer_workflows/developer_workflows.judasproj`.
- Final standalone: `.cache/developer-workflows/Checkpoint Learning Project/`.
- Export: 7 assets, 2 scenes, 57,518,783 bytes, 0.245 s; complete moved package
  launched from /tmp and cold-loaded its genuine saved counter/driver state.
- Final registered gate: checkpoint-gate/RESULTS.json. Earlier gate runs retain
  their own source identities; they are not silently relabelled as final results.
- The final application fixture also checks actual viewport bounds. The deliberate
  original bad-alignment copy fails exactly that check (expected exit 1); normal
  write/read pass. Independent final missing-script and incompatible-update
  controls pass 11 and 6 checks. This expected negative is not an unresolved defect.
- Source/binary/package/evidence hashes and exact changed inventory accompany this
  report. Native tools share the same actual development build identity; this is
  not a clean release rebuild or a claim of full SDK/platform qualification.
