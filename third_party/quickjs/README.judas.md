QuickJS-NG v0.17.0
https://github.com/quickjs-ng/quickjs/releases/tag/v0.17.0
Vendored core only; no libc/OS modules or CLI. MIT license in LICENSE.
Credits: Fabrice Bellard, Charlie Gordon and QuickJS-NG contributors.
Source archive SHA-256: 559bc4c420475e55c7ab4510adbc562f55d7524d75e8e89d79ce4bb02f5687d9

M94 local Judas extension: optional JS_SetExecutionHook / JS_GetDebugFrame / JS_PrepareModule in
quickjs.h/c. Adds source-boundary instruction observation and read-only live
frame bindings for editor tools, plus non-evaluating module linking; disabled by default. Frame-local source-map
range caching has no allocation or collected-function identity cache. Upstream
version/license remain unchanged. See docs/M94_DEVELOPER_TOOLS.md and M94 evidence.
