# Linux follow-up — portable JudasJS module literal

The Windows compiler exposed a per-token string-literal size limit in the
embedded JudasJS module. The accepted narrow correction splits one C++ raw string
into adjacent literals at the `EntityGravity` class boundary. Their concatenated
JavaScript bytes are unchanged; this is not a binding or gameplay change.

This receipt qualifies `src/ScriptSystem.cpp` SHA-256
`e91aa35e7076cda032d00525ff237a09d7917fc5725c8e8fe3844588f891b173`,
based on checkpoint `60a92176adbdfb8ccf90cc1fe3a40505416b1559` plus that source edit.
`BUILD_INFO.json` correctly records the then-uncommitted source as dirty and
preserves the 16 pre-existing untracked source inputs. The separate Windows
literal proof records exact concatenated-source preservation.

## Actual additional validation

One incremental Release `-j2` batch rebuilt the affected scripting unit, all
**17 public Linux SDK tools**, and the selected existing script/example fixtures
in 60.96 s with **zero warnings**. All **nine** recorded commands exited 0:

- Ordinary script runtime: **35 assertions**, including independent instances,
  input/physics, persistence, exceptions, malformed modules, stale handles and
  fault isolation.
- Executed gravity-selection cookbook: **9 assertions**. Real VM output confirms
  spatial/uniform/field selection, independent spatial query and clear-to-default.
- Executed live public-surface cookbook: **9 assertions**.
- TypeScript 5.9.3 declarations, actual live enumeration and API drift: **473 public
  symbols, 40 exports, 311 native operations, 17 callbacks**, with all **four**
  deliberate negative drift checks passing.
- Current SDK checksum/identity verification and all **17** native ELF loader
  checks pass on this Linux host.
- Current SDK exporter produces the ordinary Physics Stress Lab: **6 assets,
  4 scenes**, including its public script-owned control policy.
- Fresh package copied to `/tmp/Judas-PreAlphaPass4-Physics-portability` and launched
  from unrelated working directory `/tmp`: actual startup, 90 harness frames,
  project watch action, screenshot and normal shutdown pass.

No full suite or second physics benchmark was run. After the actual recompilation,
the **entire GCC engine archive remains byte-for-byte identical** to the already
qualified archive:
`6e77a9f799ade7f6ce3ad741687689b8cae6d5efa6fd82e472923749ee21b441`.
`ENGINE_ARCHIVE_EQUIVALENCE.json` records the actual byte comparison. The separate
build-identity library and tool binaries correctly changed to report the new
source provenance. The original 383 assertions, cold physical endpoints and
four-ragdoll measurements remain preserved in `../linux/`.

## Scope and reproduction

Native application execution uses the same Mesa llvmpipe software GL environment
as the earlier qualification. This does not certify physical GPU performance,
interactive pointer capture, older Linux distributions or human acceptance.
The SDK ABI requires glibc 2.43 on the measured host. Known curved-mesh CCD,
dense-contact tail, legacy-save and deferred fluid findings are unaffected.

Exact commands, environment, timestamps, exits, executable hashes and logs are in
`RESULTS.json`. Source and binary fingerprints, actual surface/gravity output,
SDK manifest, native dependency receipt and exported provenance are retained.
`reproduction/run_portability_as_executed.py` is the exact executed follow-up;
it imports the frozen qualification helper, so copy both runners back under
`.cache/prealpha-pass4/linux/` in a matching checkout and use fresh owned output
directories. No engine/game fixtures, numerical settings, thresholds, protected
historical evidence or research were changed by this verification.
