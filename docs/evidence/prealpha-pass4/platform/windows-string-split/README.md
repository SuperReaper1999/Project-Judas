# MSVC C2026: virtual JudasJS literal boundary

The native Windows build exposed an oversized individual C++ raw literal in
`src/ScriptSystem.cpp`. The first fragment was 16866 UTF-8 bytes; the second was
13439. Actual MSVC compilation of the exact original module fails with C2026.

The correction adds one adjacent raw-literal boundary before
`export class EntityGravity`. The fragments become **13783 / 3083 / 13439 bytes**.
C++ concatenation emits the identical **30305 JavaScript bytes**, SHA256
`b9d26c0405d234a31dd9a5f95754a5dd38ad69d46966dad76a915344ee838da2`.
The existing preceding newline remains inside the earlier literal; no newline,
export, binding or behavior changes. `PRODUCTION_DIFF.patch` is the entire change.

## Evidence

- `BYTE_PROOF.json` records exact before/after source and payload hashes,
  fragment lengths and three detecting negative controls.
- `literal-original.cpp` and `literal-split.cpp` preserve the exact native
  negative/positive fixtures, without engine dependencies.
- `LITERAL_CONTROLS.json` is the native Windows receipt, preserved as produced:
  original compile exit 2/C2026; split compile exit 0; emitted bytes match the hash.
- `literal-controls.ps1` records the actual MSVC commands and guest paths. Adapt
  only task-local paths and the installed Visual Studio location to reproduce.
  No binary, object, private configuration or SDK credentials are included.

Portable byte/negative-control reproduction from the repository root:

```sh
python3 docs/evidence/prealpha-pass4/platform/windows-string-split/verify_literal_split.py --source src/ScriptSystem.cpp
```

The verifier detects merging the oversized literal again, changing an export
byte or inserting a JavaScript newline. The native compiler receipt establishes
actual MSVC behavior; this portable check does not replace a full engine build.
Linux API and final native Windows engine/SDK validation are coordinated
separately. No human or hardware acceptance follows from this literal control.
