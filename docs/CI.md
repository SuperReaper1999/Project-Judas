# CI and qualification

## Routine feedback

`.github/workflows/prealpha.yml` builds one Alpha-labelled V2 engine/SDK on
Ubuntu 24.04 and the `judas_ci_tests` target. CTest's `routine` label selects
registered ordinary cases. `releaseOnly` cases require the full suite;
`workflow_dispatch` with `full=true` selects `judas_prealpha_tests` / `prealpha`.
Do not copy old hard-coded test counts into the selection policy.

`tools/validation/suites.json` owns cases, arguments, groups, explicit manual
findings, `releaseOnly` and audited `parallel` booleans. Each invocation has isolated
scratch/user data and validates the executable's embedded source identity before
running. Cold, portable and loss/recovery variants remain distinct when they
exercise different contracts. Use `scripts/prealpha_validate.py --list` and
`--check-manifest` for the current declaration.

`linux-vulkan.yml` runs on Experimental. Its Ubuntu 24.04 build exercises
OpenGL and Vulkan through the relocated SDK, with dedicated Vulkan synchronization
checks and a detached viewport run on each backend. The detached viewport case
is explicitly selected there and requires SDL X11 and present-fence capability
for its Vulkan path.

Compiler caches validate actual inputs and preserve source-path semantics.
Dependency keys include toolchain/dependency identities; SDKs are freshly staged.
A cache hit is not qualification. Local [storage budgets](WORKSPACE_STORAGE.md)
and bounded Actions retention own generated outputs; no tracked capture archive
is required to keep ordinary CI running.

## Complete candidate qualification

`linux-release-candidate.yml` prepares Linux candidates and never publishes.
Manual dispatch selects an exact Experimental SHA and channel. A push changing
the workflow definition also prepares that exact Experimental commit on the
Alpha channel, which registers the new workflow before its first manual dispatch.
Linux preparation includes coordinated native gates, SDK editor/tool and relocation
workflows, API checks and declared Ubuntu/ABI inspection.

Windows qualification and dependency bootstrap workflows are retired.

`ci-tooling.yml` covers packaging, updates, identity/routing and storage controls.
`current-docs.yml` checks managed source/publication summaries and current links.
The [documentation reference manifest](../tools/verification/current-docs.json)
contains current owned docs, not retired progress reports. Required license and
asset provenance files remain under their owners.

[Build identity](BUILD_RELEASES.md), [release authority](RELEASING.md),
[the harness](TEST_HARNESS.md) and [the readiness matrix](DEVELOPMENT_READINESS.md)
separate a build, unit/integration test, actual application, performance result,
platform qualification and human acceptance. An old green run cannot certify a
changed source or an unavailable device.
