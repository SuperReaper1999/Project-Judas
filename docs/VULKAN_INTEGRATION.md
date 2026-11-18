# Linux Vulkan integration

`Vulkan-V2-Integration` starts at Experimental `6765e9fb`. It ports the
backend from `Vulkan-Exp` at `0cf1c0fe`, with common ancestor `65a0400b`.
The original branch retains its history and measurements.

The integration uses the current engine renderer and shaders. When Vulkan is
selected, the Vulkan bridge fills the graphics dispatch table and owns device
resources, command recording and presentation. This keeps material overrides,
structural skinning, shadow policy and camera preparation with the current
renderer. The historical explicit mesh and shadow pass is preserved behind
`JUDAS_VULKAN_NATIVE_MESH=1` for migration work. It predates structural
skinning and current shadow and material submission optimizations. Use the
bridge for integration testing.

## Build and run

Build on native Linux with a Vulkan 1.3 loader and driver, the Vulkan
development headers, glslang development files and SPIR-V Tools. Distribution
package names can vary. On Ubuntu the dependencies are `libvulkan-dev`,
`glslang-dev`, `glslang-tools` and `spirv-tools`. Ubuntu Resolute separates
the SPIR-V libraries into `spirv-tools-dev`. Install the Khronos validation
layers for validation runs.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_VULKAN=ON
cmake --build build --target judas judas_editor -j2
JUDAS_GRAPHICS=vulkan JUDAS_VULKAN_VALIDATION=1 \
  build/judas projects/renderer_v2_lab/renderer_v2_lab.judasproj
JUDAS_GRAPHICS=vulkan JUDAS_VULKAN_VALIDATION=1 \
  build/judas_editor projects/renderer_v2_lab/renderer_v2_lab.judasproj
```

OpenGL is the default runtime backend. A build configured with
`JUDAS_VULKAN=OFF` (the build default) rejects an explicit Vulkan request.
`AUTO` includes Vulkan if its dependencies are present. Use
`JUDAS_VULKAN_DEVICE` to require a particular device name substring.
A Vulkan validation request requires the Khronos layer and callback to
initialize successfully. Vulkan editor presentation uses one main window.

```sh
python3 tools/vulkan/compare_backends.py --no-xvfb --device 'GTX 970' \
  --output build/vulkan-parity --frames 40 --shot 15 --shot 30 \
  projects/renderer_v2_lab/renderer_v2_lab.judasproj
```

The comparison reports the OpenGL repeatability control, Vulkan image
differences, exit status and validation messages. Read image differences
against the feature's rendering contract. An exit status of zero does not
guarantee exact equality.

## SDK and distribution

A Vulkan-enabled development SDK stages to `build/linux-vulkan-sdk` through
`judas_linux_sdk`. It requires the system Vulkan loader and driver even though
OpenGL remains the default renderer. The backend translates the shared shaders
at runtime using its linked compiler. Exported Linux games carry the runtime
binary chosen by the exporter. The existing published SDK and Linux support
policy apply to the qualified OpenGL baseline. The Vulkan branch needs its
own toolchain, runtime dependency, licence and relocation qualification
before distribution. Windows, Android and Web Vulkan are outside this increment.

## Initial qualification

These are local Linux desktop results on the GTX 970 with NVIDIA
580.178.04 and Vulkan 1.4.312. The shared renderer ran in its modern tier
on each backend. A Khronos validation run ended with zero callback
messages in the sample applications and the focused tests below.

| V2 contract | Evidence in this increment | Remaining scope |
| --- | --- | --- |
| Editor and game viewport | Built-in editor automation rendered Edit and Play and found the authored scene identical after Stop. Native viewport and context lifecycle suites passed on both backends. | Detached Vulkan editor windows and sustained human authoring |
| Geometry, PBR, materials and cameras | Renderer lab, native submission, render controls, material overrides and multi-camera shadow tests passed. The lab exercised save/load, scene switches, split views and camera monitors. | Broader project and material combinations |
| Lucifer | Workshop captures differed at most one byte level. Native directional and local light contract checks passed. | More hardware and scenes |
| Skinning and animation | Animation studio images matched at two fixed ticks and the renderer submission suite passed. | Full structural-fracture visibility and animation/IK stress qualification |
| Terrain | Native fade and coverage tests passed. Workshop samples contain small residual image differences. | Planetary and long-streaming comparisons |
| Particles | GPU-resident compute and draw gates passed. The sample lab rendered on both backends with small localized pixel differences. | Stress performance and long-running memory |
| Advanced water and UI | Water workshop and UI showcase launched and rendered. UI samples matched exactly; water samples have small image differences. | Dynamic water conservation and rendering under extended backend workloads |
| Standalone export | Renderer lab exported with Vulkan capability in provenance, then ran both backends after relocation without an engine source path. | Full native SDK inventory, build dependency provenance, licences and Ubuntu release baseline |

The branch has scoped hardware evidence. OpenGL continues to own the published
support baseline. Cross-backend output can differ by rasterization, render
scheduling and scene timing. Review the actual captures and use the numerical
contracts for every feature separately.
