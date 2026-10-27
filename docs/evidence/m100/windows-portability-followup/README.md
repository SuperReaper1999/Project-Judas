# Native Windows qualification and portable-control follow-up

The exact 2108f085 native MSVC SDK built successfully: 18 tools including platform
readiness, 294 inventoried files. All 45 native cases, seven additional physical
suites, 21 Windows readiness checks and the SDK editor/tool/relocated-export
workflow passed. The coordinated driver then correctly stopped on failing Python
controls; it is not recorded as a fully passing combined run.

The controls established and corrected:

- A provenance fixture generated all Visual Studio configurations into one
  identity.json. Outputs now include $<CONFIG>, and each generated configuration
  is checked. The fixed fixture passes both single- and multi-config generators.
- The same fixture invoked a hardcoded python3 command, which selected Windows'
  Store alias. It now uses the running interpreter's sys.executable.
- A source-archive test compared Git's canonical blobs with CRLF working-tree
  bytes. It now compares exact HEAD blobs, preserving the actual archive contract
  without normalizing or weakening the byte comparison.
- A genuine SDK path guard defect accepted drive-less rooted Windows paths,
  because pathlib.is_absolute correctly distinguishes these from drive-qualified
  absolute paths. Portable inventory paths now explicitly reject leading slash
  roots and non-string values. Positive relative paths remain supported. A
  PureWindowsPath control exercises the defect even on Linux.

Native preflight proves the corrected provenance and packaging controls; the
latter retains its explicit skipped Linux-shebang probe fixture. Actual native
SDK probe execution and real editor/export work supply separate Windows evidence.
Final-source metadata refresh and combined results accompany the final artifacts.
No physics, fluid, game, controller, numerical threshold or solver budget changed.
