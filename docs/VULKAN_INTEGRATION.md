# Linux Vulkan backend

The integration uses the current engine renderer and shaders. When Vulkan is
selected, the Vulkan bridge fills the graphics dispatch table and owns device
resources, command recording and presentation. This keeps material overrides,
structural skinning, shadow policy and camera preparation with the current
renderer. The historical explicit mesh and shadow pass is preserved behind
`JUDAS_VULKAN_NATIVE_MESH=1` for migration work. It predates structural
skinning and current shadow and material submission optimizations. Use the
bridge for integration testing.

The Vulkan bridge uses transient uploads by default. Busy ranges are replaced and
retired after their Vulkan submission completes, which avoids waiting on
persistent OpenGL stream pages during draw submission. Static draw buffers
prefer mapped device-local memory when available, with a separate budget of
half the eligible heap. They fall back to host memory when the allocation
does not fit. `JUDAS_VULKAN_TRACE=1` reports peak mapped static-buffer usage
at shutdown.

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
initialize successfully.

## Detached editor windows

On Linux X11, including an SDL X11 session under XWayland, the Vulkan editor
can float a panel or a docked group in its own window. Drag the panel out of
the main editor to detach it, then drag it back to a docking target to
reattach. Scene and Game use the same world and renderer when detached.

This requires `VK_EXT_surface_maintenance1`,
`VK_KHR_get_surface_capabilities2`, and the
`VK_EXT_swapchain_maintenance1` feature on the selected device. The editor
selects single-window docking when those capabilities or SDL X11 are unavailable.
Native Wayland remains single-window. A failure to create or present an
individual secondary surface closes that panel and reports the failure.

The editor presents the main surface first. Each detached window has its own
swapchain and display target on the same graphics device and queue. Busy
secondary surfaces skip presentation. Resize retries after the previous
presentation fences signal. Close waits for that window's fences before
releasing the Vulkan surface and its SDL window. The runtime and the
single-window fallback retain their existing device requirements.

On the GTX 970 under isolated Openbox/Xvfb, the editor viewport fixture
alternated one docked and one floating Game panel with pixel readback and
validation disabled during timing. The six 120-frame samples per mode gave
median run p50/p95/p99 of 18.17/18.37/18.56 ms docked and
20.86/21.11/21.53 ms floating. The floating variant issued one extra
submission and present per frame. This includes Xvfb presentation and costs
from the shared editor fixture. It does not measure KWin or physical scanout.

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

A Vulkan-enabled SDK stages to `build/linux-vulkan-sdk` through
`judas_linux_sdk`. Its Linux binaries link the system loader, `libvulkan.so.1`
(`libvulkan1` on Ubuntu). The default OpenGL backend requires an OpenGL
driver and the loader library. Installing a Vulkan driver is necessary only
for Vulkan selection. An explicit Vulkan request exits with the graphics
initialisation error if no compatible driver can start. The program does not
fall back to OpenGL.

Run `judas --build-info-json` and check `vulkan` to inspect a build's Vulkan
capability. For a running Vulkan application, the graphics startup log
identifies the device, driver and validation state. During an active Vulkan
session a device loss ends the process with an operation-specific diagnostic.
Restart the application to create a new device.
The GTX 970 with NVIDIA 580.178.04 intermittently returned
`VK_ERROR_DEVICE_LOST` during device creation in local trials. The bounded
startup retry recovered and the affected programs completed. The cause on
this host is unresolved. Device-loss recovery during an active session is
outside the supported lifecycle.

The linked glslang and SPIR-V Tools compiler translates engine shaders on the
target system. The SDK and exports made from it carry the compiler notices
and source versions in `third_party/vulkan` (under `engine` in exports).
Shader and Vulkan pipeline caches are generated per user and are excluded
from packages. On a Debian/Ubuntu build host using the installed distribution
compiler packages:

```sh
python3 tools/vulkan/debian_compiler_notices.py build/vulkan-notices
cmake -S . -B build -DJUDAS_VULKAN=ON \
  -DJUDAS_VULKAN_PACKAGE_METADATA="$PWD/build/vulkan-notices"
cmake --build build --target judas_linux_sdk -j2
python3 tools/package_sdk.py verify build/linux-vulkan-sdk
```

For a different compiler distribution, supply a metadata directory containing
`PROVENANCE.json` with schema 1 and `components.glslang` and
`components.spirv-tools`. Each entry names the linked source, version and
relative `licence` file. Include the referenced licence texts.
Distribution qualification uses the Ubuntu 24.04 build and verifies every
public tool against the glibc 2.39 ceiling. SDKs built on other hosts may
require newer system libraries. Windows, Android and Web Vulkan are outside
this integration.

## Parity and qualification

See the [backend parity assessment](VULKAN_PARITY.md) for tested contracts,
known differences, and reproduction commands. Linux CI exercises Vulkan with
Mesa llvmpipe on Ubuntu 24.04 and qualifies relocated OpenGL and Vulkan exports.
Software-driver evidence covers functional behaviour. GPU performance and
human editor acceptance require separate evidence.
