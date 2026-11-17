# V2 support boundaries

<!-- judas-release-status:start -->
Source identity is **Judas Engine Alpha v2.0.0** (`2.0.0-alpha.1`).
[Source notes](ALPHA_V2_0_0_NOTES.md) · [Published packages](https://github.com/SuperReaper1999/Project-Judas/releases) ·
[Support boundaries](ALPHA_SUPPORT_MATRIX.md).

Source identity and publication are separate. The
[publication record](RELEASE_STATUS.json) is a dated receipt.
Check the release page for subsequently published packages.
<!-- judas-release-status:end -->

## Qualification versus capability

The authoritative [development-readiness matrix](DEVELOPMENT_READINESS.md) records
exact builds, native application/SDK checks, platform scope and performance limits.
The [baseline receipt](qualification/baseline.json) identifies its evidence.
This page describes the supported product boundaries; it does not extend that
qualification to every device or imply main promotion/publication.

| Target | Development scope | Boundary |
| --- | --- | --- |
| Native Linux x86-64 | Engine, desktop editor and JudasJS SDK; Ubuntu 24.04 reference build and current NVIDIA desktop workflows | Host-native SDKs inherit host ABI; other distributions/devices need their own check |
| Windows x64 MSVC | Native SDK build/tools/selected contracts and loader inspection pass | Current Windows editor/GL/export/hardware behaviour has not been requalified |
| Portable GL / GLES3 | Capability-selected fallback code; portable controls on the tested desktop | Code-path testing is not every GLES/Android device certification |
| Android | Optional game runtime/APK workflow | Complete JDK/NDK/SDK/device required; no current snapshot device qualification; no Android editor |
| Browser | Explicit opt-in Experimental game export through WebAssembly/WebGL2 | Emscripten/isolation-capable hosting required; current snapshot browser runtime unqualified; no browser editor |
| macOS, iOS, consoles | No supported distribution pipeline | Unsupported |

Older recorded human acceptance remains scoped to its original build/device. It
is neither withdrawn nor inherited by this changed source. See [Windows](WINDOWS.md),
[Android](ANDROID.md), [browser](BROWSER_SUPPORT.md) and [Linux baseline](LINUX_BASELINE_BUILD.md).

## Supported feature boundaries

- Production MK2 owns rigid contacts/CCD/joints. Dense/severe contacts, nominal
  elastic energy loss and extreme-spin/arrival limits remain content-dependent.
- CharacterMotor, kinematics and planar physics expose mechanics; game scripts
  supply controllers. Native planar and 3D physical bodies cannot coexist.
- Vehicles/materials/fracture use finite authored representations. Full sheet CCD,
  automatic semantic vehicle/panel discovery and universal industrial mechanics
  are not promised. Natural humanoid get-up remains unavailable.
- Terrain supports native heightfields, local excavation, independent residency,
  geographic charts, navigation, exports and cold persistence. One local physical
  chart and bounded owners/journals are deliberate limits. Mods do not admit
  native Terrain V2 assets; old terrain/liquid formats have no compatibility shim.
- Directional cascades and point/spot shadows are implemented. Local shadow budgets,
  exact invalidation and conservative redraw are explicit. True area/LTC emitters,
  universal GPU residency and arbitrary per-mip streaming are not supplied.
- CPU particle fallback and portable graphics remain valid policies. Required-GPU
  authoring must fail meaningfully when unavailable rather than silently weakening it.
- Basic Water/swimming are default-enabled prescribed effects. Advanced Fluids
  uses one conservative game-water backend, default-off project opt-in in every
  V2 channel. It is not PBF or a hidden microscopic research backend.
- Saves and streaming support explicit participants, current content identities
  and controlled migrations. No automatic topology merge or reconstruction of
  missing historical physical state. Live articulations/unsupported assemblies
  retain documented region pins or reject unsupported suspension.
- Online sessions support explicit authored-root authority/replication. Streamed or
  mod-enabled dedicated roots, hostile public-Internet deployment and arbitrary
  assembly replication are outside the current foundation.
- Editor native detachment is limited to SDL x11/windows backends; native Wayland
  detachment is not advertised. Scene/Game render at logical UI resolution.
  Browser/Android exports do not distribute the desktop editor.

Use the corresponding guide/reference for API phase, format, resource and persistence
rules. The [current limitations register](CURRENT_FINDINGS.md) tracks unresolved
engineering issues; it does not retain closed milestone checklists as future work.
