# Support-continuity evidence provenance

This package contains original measurement records, source-only experiment inputs,
qualification tools, and selected final source snapshots. Presence is not a pass.
Binary/archive hashes in original receipts describe private artifacts; executable,
object and static/shared library bytes are intentionally excluded from this package.
The frozen manifest records all 517 original inputs. Selected baseline source
snapshots are included; unchanged files can be recovered from the recorded Git
checkpoint and verified against that manifest, rather than duplicating the entire
engine and four authored scenes in this evidence directory. The complete original
source archive remains preserved privately with its recorded hash.
CSV data is losslessly gzip-compressed with zero mtime and no embedded filename;
the manifest records both original uncompressed and stored-byte SHA-256 hashes.

## Temporal baseline source-monitor warning

`temporal/baseline900/AS_RUN.json` is preserved verbatim. It records the simulation
child exit 0, 900 completed steps, zero nonfinite rows, and unchanged binary, frozen
core and fixture. Its worktree source monitor reports `source_inputs_unchanged:
false`: production source was edited while the immutable baseline binary ran. The
outer runner therefore exited 1. This is not a clean source-monitor qualification
pass, nor an observed simulation crash; the frozen-binary physical observation is
retained with that exact qualification. No receipt is rewritten to remove it.

## Preserved qualification failures

`focused-provisional` includes a stale BendingTests object crash. The same object
also failed against the frozen original core; fresh BendingTests driver objects
passed both cores. Initial reused-object results remain provisional. Final
qualification belongs to the independently recorded fresh-driver checks.

`cold-public-invalid-fixture` records the first private probe's invalid save
metadata, not an engine persistence defect: its content identity was a human label
instead of the required 64-character SHA-256 identity. Exact original probe source
and diagnostic explanation are under qualification-tools/failures/. A later valid
fixture has a canonical authored scene fingerprint and explicit v4 content identity.
No unsupported universal bitwise warm-cache save-continuation promise is inferred.

## Scope and limitations

The ordinary support warm guess and completed actual contact observations are
different state. Event impulses are not serialized or copied into the next ordinary
support warm guess by this correction. Physical trajectories can change under the
same finite-pass solver when valid warm-guess history is retained; exact-parity
claims require the corresponding explicit comparison, not this packaging script.

Normal fixed-step timings, synthetic 900/1800-step throughput, temporal observers,
and actual live35 frame cadence have different scopes. Instrumented observer times
are not uninstrumented CPU percentages. CPU-PC sampling is main-thread time, not
hardware cycles, a call graph, total worker cost, or GPU timing.

The earlier PREALPHA_PHYSICS_COST_INVESTIGATION.txt and its evidence directory are
separate immutable baseline evidence; this tool does not modify or recopy them.
Missing/pending final groups are reported explicitly in PACKAGE.json.
