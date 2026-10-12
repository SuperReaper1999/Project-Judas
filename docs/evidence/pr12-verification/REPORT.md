# PR #12 independent Linux verification

Date: 2026-10-12. Baseline: `fc192c19e033c490d6ab63f5c35aad8218f9551e`.
Verified PR head: `83e3b94ec5d761fc8768bcaf0937c2246bb56f8b`.
Merge: `4d20eef90ef896e528f179404dbe93c032c83eb9`.

## Observations

- Dense 60 Hz trim fixture/new test linked against the pre-fix importer: 39 checks, one failure, `JudasModel: track times`. Final PR importer: 39 checks, zero failures. Reimport/aliases/dependencies/cancellation/root extraction remain covered by the unchanged surrounding checks.
- Pre-fix cookbook binary using the new example: 9 checks, two failures (VM callback and outcome). Final PR binary: 9 checks, zero failures; `unsocket:true`, followed by ordinary reattachment.
- Independent socket probe: 11 checks, zero failures. After undefined removal, directly inspect `RuntimeDefinition(102)` and assert the socket is absent; an invalid target throws TypeError without reattaching. This extends the PR's return-value proof to the actual runtime representation.
- Animation cookbook: 9 checks, zero failures.
- Actual VM public surface cookbook: 9 checks, zero failures. AST/types/live enumeration check: 345 symbols, 30 exports, 39 examples, three negative drift controls, TypeScript 5.9.3; passing.
- Release build of the two affected targets succeeded. It refreshed stale dependencies; pre-existing RuntimeWorld indentation warnings and dependency CMake warnings remain. No full production rerun, Windows run, or human visual claim.

## Review and documentation correction

Clip trimming keeps increasing rebased float times and the exact duration endpoint. The schedule retains source times for union pose sampling, preserving the prior shared sampling mechanism. No parser/physics/gameplay change.

Null/undefined socket removal dispatches the existing `socketClear`, preserving RuntimeWorld's stale-owner and lifetime checks. Non-null targets continue through `socketSet`.

Contrary to the PR description, the current reference explicitly rejected null, and declarations required an Entity. This follow-up adds the null/undefined removal overload, corrects both reference pages, and records both native operations in the inventory. No additional runtime behavior is introduced.

## Reproduction

From repository root:

```sh
cmake --build build --target judas_model_reimport_tests judasjs_examples_tests -j2
build/judas_model_reimport_tests /tmp/pr12-fresh-model-root
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy build/judasjs_examples_tests /tmp/pr12-cookbook developer-integration
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy build/judasjs_examples_tests /tmp/pr12-surface surface
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy build/judasjs_examples_tests /tmp/pr12-animation animation
node scripts/check_judasjs_api.mjs --typescript /absolute/path/to/typescript --runtime /tmp/pr12-surface/surface.json
```

The extra `socket-probe.cpp` is the cookbook fixture with a copied-script injection and two direct assertions, compiled with the existing cookbook target's flags/link dependencies. It changes only test-owned project copies. Logs and source identities are preserved; executables/build outputs remain local.

An initial extra probe used an unregistered relative wrapper module and failed its callbacks. `socket-proof.log` preserves that setup failure; the final probe injects into the already registered ordinary script and passes. No product correction was required for that probe failure.

Claude's reported background-source setup failure did not reproduce here: all 39 importer checks pass with the local source available. The unrelated imported-model example was outside this focused validation; no claim that its reported asset issue is repaired.

Protected historical evidence, fluid work and pre-existing untracked artifacts were not modified. M78 remains incomplete; this PR verification does not promote milestone human acceptance.
