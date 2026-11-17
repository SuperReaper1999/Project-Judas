# Generated workspace storage

This is a source-checkout developer workflow. Packaged SDK consumers keep the
checksum-verified SDK read-only and place their projects/exports outside it;
the source build/test commands below are not SDK consumer commands.

The operator cleared repository-local `.cache` and `build` on 2026-11-16 after
checking and safely unregistering their Git worktrees. Experiments that matter
belong in source commits/branches, tests, tracked assets and concise documentation.
Historical executables, screenshots, raw benchmarks and diagnostic dumps are
disposable. Do not archive every temporary campaign before clearing it.

`tools/storage-policy.json` sets allocated-byte budgets: 8 GiB for `.cache`,
14 GiB for `build`, 16 GiB combined, and 256 MiB per whole-engine capture.
These are workspace limits, not engine runtime memory or performance budgets.
The full native regression build plus atomic SDK preparation exceeded the first
12 GiB build allowance (12.09 GiB observed when stopped). The 14 GiB allowance
covers that measured temporary second payload; the combined ceiling stays 16 GiB.

```sh
python3 tools/workspace_storage.py check
python3 tools/workspace_storage.py run -- cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
python3 tools/workspace_storage.py run -- cmake --build build --target judas_prealpha_tests judas_linux_sdk -j2
python3 tools/workspace_storage.py run -- ctest --test-dir build -L prealpha --output-on-failure
```

The guard checks before launch, every five seconds, and after completion. On
budget exhaustion it terminates its own process group on Linux, returns failure
and leaves outputs for review. It never deletes, rotates, follows root symlinks or
touches other repositories. Sampling permits overshoot between checks; this is
not a filesystem quota. Windows termination is limited to the launched process;
native Windows process-tree enforcement remains unqualified. Manually bypassing
the wrapper bypasses its ongoing monitor.

The focused validation runner also checks the repository budget before creating
output and between test cases. The whole-engine capture driver rejects reused
output directories and stops a capture at its byte budget, checked every two
seconds. Run campaigns through the wrapper too, so repeated captures share the
repository-wide ceiling. Storage checks are diagnostic overhead; disclose them
in performance evidence. External output directories are not covered by the two
repository ceilings; a capture's own ceiling still applies. Moving an unbounded
campaign elsewhere is not a retention policy.

Reuse one build directory per active toolchain/configuration. Prefer focused
targets for changes and a periodic full integration run. Keep only the current
experiment campaign; record its owner and purpose in a small README. Do not use
timestamped output directories as indefinite archives. Do not place source
worktrees under generated roots: use ordinary Git branches and a source-owned
location outside them. Do not duplicate private models into every run; reuse
immutable inputs or independent copy-on-write staging, never writable hardlinks.

When the guard stops work, inspect exact owned paths, active processes and Git
worktrees. Preserve unique source changes/commits on explicit branches without
merging or accepting them. Review cleanup scope with the operator before any
further deletion; the one-time authorization does not authorize future automatic
purges. The guard contains no deletion command.

Do not raise the limits merely to pass a gate. Explain the workload's actual
minimum storage and review a revised budget when necessary. Commit compact
verified findings and reproducible tests, not temporary executables or raw
diagnostic history. Audit/qualification documentation must identify incomplete
runs rather than implying that discarded build artifacts passed.
