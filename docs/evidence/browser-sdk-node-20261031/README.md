# Desktop browser setup false rejection

Human reproduction: the live editor disabled Build & play and reported setup
needed, although the activated Emscripten SDK and other requirements were present.
Parent: `561596c21fd570032c3e61198fcbeb15f078db10`.

The exact editor check is retained. Its only failure was `which('node')`: the
systemd desktop launch did not inherit the terminal's Node PATH. Emscripten's
activated configuration already pointed to its bundled Node 24.19.0. Earlier
terminal-based checks therefore missed this genuine desktop integration defect.

The exporter now reads literal NODE_JS settings from the selected SDK, resolves
$CFGDIR safely, preserves separate command arguments and uses that executable in
both preflight and the actual compiled-WASM identity query. Build subprocesses
receive that Node directory in their own PATH. The global desktop environment
and system installations are not changed. No SDK configuration is executed for
discovery; broken explicit SDK settings are not masked by an unrelated system
Node. PATH Node remains a fallback when the SDK has no explicit Node setting.

Verification:

- **33 browser tooling tests passed** with PATH=/usr/bin:/bin, including SDK-only
  discovery, paths with spaces/arguments, non-execution of configuration code,
  missing/broken settings, system fallback and existing export boundaries.
- Exact desktop-style preflight with no terminal Node reports **ready=true**.
- Genuine systemd user-service integration, PATH=/usr/bin:/bin, passed actual
  automatic native preparation, compiler cancellation/lock cleanup, retry/build,
  verified local preview/headers and preview shutdown. SDK Node executed the
  real WASM provenance query. The full log is losslessly compressed here.

No checks were disabled and no executable aliases/symlinks were installed.
No rendering, physics, fluid, game, channel or main-branch changes. This remains
Experimental; operator review of the restored workflow is still required.
