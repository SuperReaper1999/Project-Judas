# M96 — Judas Learns to Talk to Players

Base: accepted M95 `a488c72463f207473838318b978b5a7f8e1f8afb`.
Delivery is Experimental only; **M96 human acceptance pending**. Main is unchanged.

## Architecture and delivered surface

One retained RuntimeUI path now supports TextInput, Scroll and virtual List, with
existing Canvas/Panel/Text/Image/Button/Slider/Toggle compatibility. The older
UIMenuScreen remains a legacy compatibility path, not a second implementation of
these controls. No project menu, inventory or dialogue concept was added to C++.

- **Text:** InputSystem carries bounded commits/preedit/edit commands. Window is
  the SDL/native clipboard/keyboard adapter; existing LocalPlayers routing retains
  composition owner identity. Native events include a field-session token so old
  queued commits cannot alias a replacement field/world. One OS composition
  session/window coexists with independent document focus. ICU extended grapheme
  boundaries drive selection and insertion/deletion; the existing shared
  HarfBuzz/FreeType bidi/fallback path drives rendering and cluster caret placement.
- **Editing:** selection, pointer drag, Home/End, logical arrows, Ctrl+A/C/X/V,
  undo/redo, submit, Tab and reverse Tab. User insertion rejects malformed UTF-8,
  newlines or capacity overflow atomically. IME preedit remains transient. Keyboard
  bindings are consumed before ordinary gameplay, including fixed observations.
- **Scroll:** implicit clipping, bounded content offsets, wheel/touch/pointer drag,
  focus reveal, visible scrollbar. A dragged list does not activate its release row.
- **Lists:** up to 10000 keyed fixed-height text rows / 2 MiB combined labels/IDs.
  Stable-key selection, disabled rows and data replacement. Only visible rows are
  shaped/drawn; no offscreen widget subtrees. Layout parent lookup uses a per-pass
  index instead of repeatedly scanning the authored vector.
- **Focus/accessibility:** explicit per-document focus, outline indicators, labels,
  descriptions, roles, copied semantic observations, readable scale and live canvas
  reference dimensions. Project JS reflows the same interface into portrait layout.
- **Authoring:** ordinary named UI assets; new optional control fields; version-3
  legacy encoding plus backward version-1/2 reading. Existing editor canvas,
  hierarchy, named source, validation, undo/redo, Save and runtime preview reused.
- **Persistence:** explicit atomic capture/restore of control values, selection,
  scroll, focus and scale. Project data is rebuilt before selected keys restore;
  missing keys clear selection. No hidden save participant. Composition, clipboard,
  undo history, native handles, list data and device-specific layout are excluded.
  The existing 64 KiB script save-state limit is unchanged.
- **Public API:** UIElement scroll/selection/selectedIndex/setItems/configure;
  UIDocument referenceSize/focused/focus/scale/captureState/restoreState/accessibility.
  All use validated generic operations; typings/reference/inventory/live checking
  cover 37 exports, 440 public symbols and 293 native operations.

See [contracts and limitations](../../M96_UI_TOOLKIT.md),
[public reference](../../judasjs/ui.md), [exact file list](CHANGED_FILES.txt).

## Field Notes

`projects/ui_showcase/ui_showcase.judasproj` contains an ordinary single scene,
registered source UI assets, font and two small JS modules. Screens: welcome/main
menu, settings, 3000-row searchable inventory, scrollable dialogue with choices,
Unicode text entry, plus two local-player desks. Controls, semantics, scaling,
responsive reflow, save/load and UI meaning are project JS.

Main menu buttons; mouse/touch; arrows/Enter; Tab/gamepad right shoulder to move
between controls; gamepad D-pad/South; F6/F7 or SAVE/LOAD; F1 leaves local mode.
The local desks use arrows/Enter/mouse versus W/S/Space or a gamepad, with separate
Android touch regions. No hardware controller acceptance is implied by synthetic
SDL controller validation.

Standalone and package path: `.cache/m96/delivery-closure/package/`.
Android arm64 development APK: `.cache/m96/FieldNotes-arm64.apk`.
See [one consolidated human checklist](../../../projects/ui_showcase/README.md).

## Linux validation

Final combined results and raw logs are indexed in `RESULTS.json` and `linux/`.
The frozen final runtime passed all 18 focused driver runs, plus editor/export/
relocated-package checks. Earlier passes and narrow follow-ups remain distinct. Native Release build passed; the broader header rebuild surfaced
existing indentation warnings in unchanged RuntimeWorld.cpp. There is no claim of
an entirely warning-free historical engine or Android build.

| Focused check | Result |
|---|---:|
| New UI controls, native adapters, authoring, lifetime and GL | 60 / 0 failures |
| Actual showcase Application, saves, players, portrait/settings | 28 / 0 |
| Existing runtime UI / application-export | 22 / 16, 0 failures |
| Unicode/localization / logical input / touch | 49 / 40 / 17, 0 failures |
| Local multiplayer / script / native device adapter | 37 / 14 / 14, 0 failures |
| M95 animation workflow | 59 / 0 |
| Existing save write / fresh-process read | 13 / 20, 0 failures |
| Executed controls / old UI / live surface cookbook | 9 / 9 / 9, 0 failures |
| Inventory + live VM + TypeScript + negative drift controls | Pass |
| Editor Play/Stop and relocated standalone | Pass |

Coverage includes:

- New Unicode editing/IME buffer, clipboard, negative insertion limits, bounded
  lists, wheel/drag clipping, focus reveal, scaling, player-addressed commits,
  retired-session rejection, state rollback, native SDL and actual GL drawing.
- Shared editor AuthoringDocument edit/undo/redo/save and external-conflict refusal.
- Ordinary Application startup, native SDL typing → JS filter, dialogue, settings,
  real SaveService write → fresh-world restoration, player creation/departure,
  portrait resizing and actual UI geometry/screenshots.
- Existing runtime UI unit/application/export, text/localization, input/touch,
  local multiplayer/script/device adapter, M95 animation workflow and save checks.
- Executed current UI-controls/UI/surface cookbook cases; TypeScript 5.9.3,
  inventory and live public enumeration; 48 examples statically checked, not 48
  execution claims.
- Editor Play/Stop leaves the authored scene unchanged; explicit source Save;
  normal closure export, byte-identical relocated package launched from unrelated
  working directory and save/load there.

No full production-suite marathon was run. UI work did not alter physics,
gravity, fluid implementations or protected research.

## Ancestor clipping closure

Source review found that a virtual list must derive its row interval from the
intersection of its rectangle and inherited clip, rather than just its own height.
The final implementation does so before any text shaping. The new adversarial GL
check authors a 400000-pixel list under a 240-pixel clipped ancestor and verifies
that exactly six of 10000 rows are drawn. Earlier positive coverage is preserved;
no unexecuted before-fix failure is claimed. Native Linux UI/input/application
checks and both runtime packages were refreshed for this narrow correction.

## Android integration finding

The initial arm64 build failed to link M93 meshopt codec symbols. Accepted M95's
CMake linked judas_meshoptimizer below Android's early return, while AssetCook
was already part of the common engine. This is an existing packaging integration
defect, not a text-editing numerical/runtime failure. The correction moves that
same dependency into the common engine link list; desktop semantics are unchanged.
The original failure and corrected build are preserved. The final arm64 APK passed
native compilation/link, signature verification and 16 KiB ZIP alignment; exact
artifact/native library hashes are in `android/BUILD.json`. Existing Android
compiler warnings are retained; unrelated physics/fluid sources were not edited. No phone or emulator was
connected; build/package checks are distinct from keyboard/IME/touch acceptance.
Windows M96 was not run and remains informational/outstanding.

## Performance method

Native Linux Release; i7-4790 / GTX 970 desktop. Focused CPU observations cover
44 authored widgets plus 100/1000/10000 list rows, fixed viewport and six visible
rows. Steady input samples exclude data creation/validation and GL rendering;
draw_cpu_us is one real GL submission pass, not GPU elapsed time. Estimated row
storage counts item structs plus string lengths, excluding allocator capacity,
font caches and driver memory. No historical speedup or whole-game FPS promise.
Final measurements are retained verbatim in the production test log.

| Rows | Steady input median / p95 / worst (ms) | Visible rows | Estimated row bytes |
|---:|---:|---:|---:|
| 100 | 0.029999 / 0.041746 / 0.058004 | 6 | 10380 |
| 1000 | 0.031123 / 0.056245 / 0.072957 | 6 | 105780 |
| 10000 | 0.033468 / 0.058701 / 0.065076 | 6 | 1077780 |

The separate 300-sample 10000-row pass measured 0.030498 ms median, 0.042124 ms
p95, 0.067025 ms worst. One-pass CPU draw observations ranged 0.192–0.372 ms;
glyph-cache warmup and run order affect these, so the lower later number is not
claimed as a speedup from more rows. No GPU or Android timing was measured.

## Limits

- Single-line inputs; logical grapheme navigation, not a code editor. No multiline
  editor, word-navigation shortcut, rich text or native screen-reader adapter.
- Existing outline-font coverage applies; Unicode buffer support does not install
  CJK/emoji glyphs. Colour emoji remains unsupported. Mixed-bidi caret affinity and
  precise internal GDEF ligature stops are approximated from shaped clusters.
- IME/clipboard use SDL/platform services. Preedit underline is provided; the OS
  owns candidate UI. Real input methods and Android keyboards still need human
  testing. One native text session/window; no independent multi-mouse hardware.
- Fixed-height text list rows, not arbitrary widget templates or variable heights.
  New list/scroll actions must be bound in older custom project input maps.
- Scale/referenceSize/setLayout support reflow; fixed authored layouts do not
  automatically become responsive. Text raster size is capped at 512 pixels.
- Explicit presentation snapshots do not bypass the normal bounded game-state,
  save ownership or resource lifetime rules. Authors persist game data separately.

Final source (457 files), checkpoint (128 intended files) and local
binary (21 artifacts) hashes are recorded separately. The package runtime
byte-matches the validated native build; all manifests were verified before
staging. APK SHA256: `d1ccfa85c0df075f855b7582f09006a00e542d0b4249563ff5cf799c4d42767c`.
`CHANGED_FILES.txt` is the exact checkpoint inventory; manifest self-hashing is
excluded only for CHECKPOINT_FILES_SHA256SUMS. Original
failures and corrected follow-ups remain under `development/`. No human or platform
acceptance is invented. No post-M96 work was begun.
