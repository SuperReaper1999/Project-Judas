# Judas V2 architectural purge

The operator approved removal of superseded implementations, historical projects
and compatibility paths on 2026-11-08. This is development on Experimental, not a
release or human acceptance of the resulting engine.

## Preserved baseline

- Engine: `2ee4203a0053cc545eab4a184c627f4f5507c4bd`.
- Protected main: `1094d64515604fe9d03fdab479efc7f880b112a8`.
- Before: 74,367 tracked files, 4,130,630,845 apparent bytes; 120,051
  first-party source code lines under `src` (C/C++/Python/CMake/shell extensions).
- Existing Linux generated build: 540 Judas target registrations, including 451
  test executables, 17 SDK tools, 23 performance targets and 49 other targets.

The six consumer games were copied, including their local ignored content, to
`/home/conner/Projects/Judas-Extracted-Games/2026-11-08-2ee4203a/` before removal.
Each sibling `NAME.preservation.json` records the engine snapshot, last project
commit and SHA-256/size of every source file. Copy verification compared all
file contents and permission modes. These are historical editable projects,
not projects promised compatibility with the current engine.

The two locally modified historical water screenshots were copied and hash
verified in that folder's `preserved-local-water-screenshots` directory.
The RPG project, its SDK and existing HDD archives are outside this operation.

## Execution register

| Bundle | Status | Validation |
| --- | --- | --- |
| A: projects, examples, historical repository content | Pending | Pending |
| B: engine-owned gameplay and legacy delta saves | Pending | Pending |
| C: PBF and retired reservoir/container liquids | Pending | Pending |
| D: Terrain V2 consolidation | Pending | Pending |
| E: obsolete infrastructure | In progress: unused ASCII rasterizer and empty editor compatibility source removed | Architecture checks: 5 passed; editor build passed; dock/workspace 24 checks, Unicode text 49 checks, native text rendering passed |

No compatibility shim or conversion path is required for deliberately retired
systems. Current contracts and their unique regression coverage must survive.
The Advanced Fluids Alpha lock remains a required current contract.

## Qualification boundary

Builds and automated tests do not establish human acceptance. Final SDK,
integration, platform and CI results will be recorded after the corresponding
work has actually completed. Main remains unchanged throughout the purge.
